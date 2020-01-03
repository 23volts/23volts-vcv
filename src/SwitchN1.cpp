#include "23volts.hpp"
#include "widgets/ports.hpp"

struct SwitchN1 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		POLYIN_INPUT,
		CV_INPUT,
		STEPINC_INPUT,
		STEPDEC_INPUT,
		RANDOM_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MONOOUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::ClockDivider connectionUpdater;

	dsp::SchmittTrigger stepIncreaseTrigger;
	dsp::SchmittTrigger stepDecreaseTrigger;
	dsp::SchmittTrigger stepRandomTrigger;
	dsp::SchmittTrigger resetTrigger;

	int channels = 0;
	int activeChannel = -1;
	int step = 0;
	float offset = 0.f; // Offset by CV Input

	bool cvConnected = false;
	bool increaseConnected = false;
	bool decreaseConnected = false;
	bool randomConnected = false;
	bool resetConnected = false;

	SwitchN1() {
		connectionUpdater.setDivision(32);
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs& args) override {

		channels = inputs[POLYIN_INPUT].getChannels();

		if(connectionUpdater.process()) updateConnections();

		if(resetConnected && resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			step = 0;
		}

		if (increaseConnected && stepIncreaseTrigger.process(inputs[STEPINC_INPUT].getVoltage())) {
			step++;
			if(step >= channels) step = 0;
		}

		if (decreaseConnected && stepDecreaseTrigger.process(inputs[STEPDEC_INPUT].getVoltage())) {
			step--;
			if(step <= 0) step = channels - 1;
		}

		if (randomConnected && stepRandomTrigger.process(inputs[RANDOM_INPUT].getVoltage())) {
			step = std::floor(random::uniform() * channels);
		}

		if (cvConnected && channels > 1) {
			float cv = inputs[CV_INPUT].getVoltage();
			int stepOffset = std::floor(cv * (channels / 10.f));
			activeChannel = step + stepOffset;
			if(activeChannel < 0) activeChannel = channels + stepOffset;
			if(activeChannel >= channels) activeChannel = stepOffset - (channels - step);
		}
		else {
			activeChannel = step;
		}

		if(channels <= 0) activeChannel = -1;

		if(activeChannel > -1) {
			outputs[MONOOUT_OUTPUT].setVoltage(inputs[POLYIN_INPUT].getVoltage(activeChannel));
		}
	}

	void updateConnections() {
		cvConnected = inputs[CV_INPUT].isConnected();
		increaseConnected = inputs[STEPINC_INPUT].isConnected();
		decreaseConnected = inputs[STEPDEC_INPUT].isConnected();
		randomConnected = inputs[RANDOM_INPUT].isConnected();
		resetConnected = inputs[RESET_INPUT].isConnected();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "step", json_integer(step));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* stepJ = json_object_get(rootJ, "step");
		if(stepJ) {
			step = json_integer_value(stepJ);
		}
	}
};

struct SwitchN1Widget : ModuleWidget {
	PolyLightPort<16>* input;

	SwitchN1Widget(SwitchN1* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SwitchN1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		input = createInputCentered<PolyLightPort<16>>(mm2px(Vec(7.699, 22.179)), module, SwitchN1::POLYIN_INPUT);
		input->selectedColor = SCHEME_ORANGE_23V;
		addInput(input);

		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 38.200)), module, SwitchN1::RESET_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 51.100)), module, SwitchN1::CV_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 65.000)), module, SwitchN1::STEPINC_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 79.146)), module, SwitchN1::STEPDEC_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 92.700)), module, SwitchN1::RANDOM_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.699, 107.163)), module, SwitchN1::MONOOUT_OUTPUT));
	}

	void step() override {
		if(module) {
			SwitchN1* module =  dynamic_cast<SwitchN1*>(this->module);
			input->selectedChannel = module->activeChannel;
		}
		ModuleWidget::step();
	}
};

Model* modelSwitchN1 = createModel<SwitchN1, SwitchN1Widget>("SwitchN1");