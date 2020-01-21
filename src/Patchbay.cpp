#include "23volts.hpp"
#include "widgets/ports.hpp"
#include "widgets/labels.hpp"

struct Patchbay : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INPUTS,8),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUTPUTS,8),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	std::string labels[8];
	bool inputConnected[8];

	dsp::ClockDivider connectionDivider;

	Patchbay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		connectionDivider.setDivision(128);
	}

	void process(const ProcessArgs& args) override {
		if(connectionDivider.process()) {
			for(int x = 0; x < NUM_INPUTS; x++) {
				inputConnected[x] = inputs[INPUTS + x].isConnected();
			}
		}

		for(int x = 0; x < NUM_OUTPUTS; x++) {
			if(inputConnected[x]) {
				int channels = inputs[INPUTS + x].getChannels();

				for(int c = 0; c < channels; c++) {
					outputs[OUTPUTS + x].setVoltage(inputs[INPUTS + x].getVoltage(c), c);
				}
			}
		}
	}
	
	void onReset() override {
		for(int x = 0; x < 8; x++) {
			labels[x] = "";
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* labelsJ = json_array();
		for(int x = 0; x < 8; x++) {
			json_t* labelJ = json_string(labels[x].c_str());
			json_array_append_new(labelsJ, labelJ);
		}
		json_object_set_new(rootJ, "labels", labelsJ);
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *labelsJ = json_object_get(rootJ, "labels");
		if (labelsJ) {
			for (int x = 0; x < 8; x++) {
				json_t *labelJ = json_array_get(labelsJ, x);
				if (labelJ)
					labels[x] = json_string_value(labelJ);
			}
		}
	}
};

struct PatchbayWidget : ModuleWidget {
	PatchbayWidget(Patchbay* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Patchbay.svg")));

		float yOffset = 45.f;
		for(int x = 0; x < 8; x++) {
			float yPos = x * 40 + yOffset;
			addInput(createInput<SmallPort>(Vec(5, yPos), module, Patchbay::INPUTS + x));
			addOutput(createOutput<SmallPort>(Vec(127, yPos), module, Patchbay::OUTPUTS + x));

			LCDTextField* textField = new LCDTextField();
			textField->box.pos = Vec(30, yPos - 3);
			textField->box.size = Vec(90, 25);
			if(module) {
				textField->linkedText = &module->labels[x];
			}
			addChild(textField);
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};


Model* modelPatchbay = createModel<Patchbay, PatchbayWidget>("Patchbay");