#include "23volts.hpp"
#include "helpers.hpp"
#include "widgets/knobs.hpp"
#include "widgets/ports.hpp"

struct MonoPoly : Module {

	static const int CHANNELS = 3;
	static const int INITIAL_STEPS = 15;

	enum ParamIds {
		ENUMS(STEP_KNOBS, CHANNELS),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(MONO_INPUTS, CHANNELS),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(POLY_OUTPUTS, CHANNELS),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::ClockDivider connectionUpdater;

	int steps[CHANNELS];

	bool inputConnected[CHANNELS];
	bool outputConnected[CHANNELS];
	
	MonoPoly() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for(int x = 0; x < CHANNELS; x++) {
			// Config step chanNel param
			configParam(STEP_KNOBS + x, 0.0f, 15.0f, (float) INITIAL_STEPS, "Number of channels");
			inputConnected[x] = false;
			outputConnected[x] = false;
		}
		connectionUpdater.setDivision(32);
		onReset();
	}

	void process(const ProcessArgs& args) override {
		if(connectionUpdater.process()) {
			updateConnections();
			updateSteps();
		}

		for(int x = 0; x < CHANNELS; x++) {
			if(outputConnected[x] && inputConnected[x]) {
				for(int c = 0; c <= steps[x]; c++) {
					outputs[POLY_OUTPUTS + x].setVoltage(inputs[MONO_INPUTS + x].getVoltage(), c);
				}
			}
		}
	}

	void updateSteps() {
		for(int x = 0; x < CHANNELS; x++) {
			steps[x] = std::floor(params[STEP_KNOBS + x].getValue());
			outputs[x].setChannels(steps[x] + 1);	
		}
	}

	void updateConnections() {
		for(int x = 0; x < CHANNELS ; x++) {
			inputConnected[x] = inputs[MONO_INPUTS + x].isConnected();
			outputConnected[x] = outputs[POLY_OUTPUTS + x].isConnected();
		}
	}
	
	/** Called when user clicks Initialize in the module context menu. */
	void onReset() override {
		for(int x = 0; x < CHANNELS; x++) {
			steps[x] = INITIAL_STEPS;
		}
	}

	/** Called when user clicks Randomize in the module context menu. */
	void onRandomize() override {
		for(int x = 0; x < CHANNELS; x++) {
			steps[x] = randomInteger(0, 15);
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		for(int x = 0; x < CHANNELS; x++) {
			std::string name = "steps_" + std::to_string(x);
			json_object_set_new(rootJ, name.c_str(), json_integer(steps[x]));
		}	
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		for(int x = 0; x < CHANNELS; x++) {
			std::string name = "steps_" + std::to_string(x);
			json_t *stepJ = json_object_get(rootJ, name.c_str());
			if(stepJ) {
				steps[x] = json_integer_value(stepJ);
			}
		}
	}
};


struct MonoPolyWidget : ModuleWidget {

	MonoPolyWidget(MonoPoly* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MonoPoly.svg")));

		float yOffset = 42.f;

		for(int x = 0; x < MonoPoly::CHANNELS; x++) {
			float y = yOffset + x * 105;
			addInput(createInput<PJ301MPort>(Vec(10.5, y), module, MonoPoly::MONO_INPUTS + x));
			
			PolyLightPort<16>* polyOutput = createOutput<PolyLightPort<16>>(Vec(10.5f, y + 35), module, MonoPoly::POLY_OUTPUTS + x);
			addOutput(polyOutput);
			
			addParam(createParamCentered<SnapKnobDark26>(Vec(box.size.x / 2.f, y + 78), module, MonoPoly::STEP_KNOBS + x));
		}

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

};


Model* modelMonoPoly = createModel<MonoPoly, MonoPolyWidget>("MonoPoly");