#include "23volts.hpp"
#include "widgets/knobs.hpp"
#include "widgets/ports.hpp"

struct SwitchN2 : Module {
	enum ParamIds {
		PROBABILITY_KNOB,
		NUM_PARAMS
	};
	enum InputIds {
		POLYIN_A_INPUT,
		POLYIN_B_INPUT,
		CV_INPUT,
		STEPINC_INPUT,
		STEPDEC_INPUT,
		RANDOM_INPUT,
		RESET_INPUT,
		PROBABILITY_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		MONOOUT_A_OUTPUT,
		MONOOUT_B_OUTPUT,
		MIN_OUTPUT,
		MAX_OUTPUT,
		TRIG_A_OUTPUT,
		TRIG_B_OUTPUT,
		DICE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		A_ACTIVE_LIGHT,
		B_ACTIVE_LIGHT,
		NUM_LIGHTS
	};

	dsp::ClockDivider connectionUpdater;
	dsp::ClockDivider lightUpdater;

	dsp::SchmittTrigger stepIncreaseTrigger;
	dsp::SchmittTrigger stepDecreaseTrigger;
	dsp::SchmittTrigger stepRandomTrigger;
	dsp::SchmittTrigger resetTrigger;

	dsp::PulseGenerator cvTrigger[2];

	int channels[2] = {0};

	int selectedVoice = 0;

	int activeChannels[2] = {-1};

	int step[2] = {0};
	int stepOffset[2] = {0};

	bool inputConnected[2] = {false};
	bool cvConnected = false;
	bool increaseConnected = false;
	bool decreaseConnected = false;
	bool randomConnected = false;
	bool resetConnected = false;
	bool probabilityConnected = false;

	SwitchN2() {
		connectionUpdater.setDivision(32);
		lightUpdater.setDivision(512);
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PROBABILITY_KNOB, 0.0, 1.0, 0.5, string::f("A/B Probability"));
	}

	void process(const ProcessArgs& args) override {

		if(connectionUpdater.process()) updateConnections();

		int initialStep[2];

		for(int c = 0; c < 2; c++) {
			channels[c] = inputs[POLYIN_A_INPUT + c].getChannels();
			initialStep[c] = step[c] + stepOffset[c];
		}
		
		bool trigged = false;

		if(resetConnected && resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			step[0] = 0;
			step[1] = 0;
			trigged = true;
		}

		if (increaseConnected && stepIncreaseTrigger.process(inputs[STEPINC_INPUT].getVoltage())) {
			step[0]++;
			step[1]++;
			if(step[0] >= channels[0]) step[0] = 0;
			if(step[1] >= channels[1]) step[1] = 0;
			trigged = true;
		}

		if (decreaseConnected && stepDecreaseTrigger.process(inputs[STEPDEC_INPUT].getVoltage())) {
			step[0]--;
			step[1]--;
			if(step[0] < 0) step[0] = channels[0] - 1;
			if(step[1] < 0) step[1] = channels[1] - 1;
			trigged = true;
		}

		if (randomConnected && stepRandomTrigger.process(inputs[RANDOM_INPUT].getVoltage())) {
			step[0] = std::floor(random::uniform() * channels[0]);
			step[1] = std::floor(random::uniform() * channels[1]);
			trigged = true;
		}

		if (cvConnected) {
			float cv = inputs[CV_INPUT].getVoltage();
			for(int c = 0; c < 2; c++) {
				stepOffset[c] = std::floor(cv * (channels[c] / 10.f));
				
				if(step[c] + stepOffset[c] != initialStep[c]) {
					cvTrigger[c].trigger(32);
					trigged = true;
				}
			}
		}
		else {
			stepOffset[0] = 0;
			stepOffset[1] = 0;
		}

		for(int c = 0; c < 2; c++) {
			if(step[c] + stepOffset[c] != initialStep[c]) {
				updateActiveChannels(c);
			}
		}

		if(trigged) {
			updateSelectedVoice();
		}

		float voltageA = inputs[POLYIN_A_INPUT].getVoltage(activeChannels[0]);
		float voltageB = inputs[POLYIN_B_INPUT].getVoltage(activeChannels[1]);

		// Main Outs
		if(activeChannels[0] > -1) {
			outputs[MONOOUT_A_OUTPUT].setVoltage(voltageA);
		}
		if(activeChannels[1] > -1) {
			outputs[MONOOUT_B_OUTPUT].setVoltage(voltageB);
		}
		outputs[DICE_OUTPUT].setVoltage(selectedVoice == 0 ? voltageA : voltageB);
		
		// Min
		outputs[MIN_OUTPUT].setVoltage(fmin(voltageA, voltageB));

		// Max
		outputs[MAX_OUTPUT].setVoltage(fmax(voltageA, voltageB));

		// Trig -> Output max voltage of trig inputs [STEP+,STEP-,RND,RESET]
		float maxVoltage = 0.f;
		for(int x = STEPINC_INPUT; x <= RESET_INPUT; x++) {
			float voltage = inputs[x].getVoltage();
			if(voltage > maxVoltage) {
				maxVoltage = voltage;
			}
		}

		outputs[TRIG_A_OUTPUT].setVoltage(fmax(maxVoltage, cvTrigger[0].process(1.f) ? 10.f : 0.f));
		outputs[TRIG_B_OUTPUT].setVoltage(fmax(maxVoltage, cvTrigger[1].process(1.f) ? 10.f : 0.f));

		if(lightUpdater.process()) updateLights();
	}

	void updateActiveChannels(int c) {
		int activeChannel = step[c] + stepOffset[c];
		if(activeChannel < 0) activeChannel = channels[c] + stepOffset[c];
		if(activeChannel >= channels[c]) activeChannel = stepOffset[c] - (channels[c] - step[c]);
		
		if(channels[c] <= 0) {
			activeChannels[c] = -1;
		}
		else {
			activeChannels[c] = activeChannel;
		}
	}

	void updateSelectedVoice() {
		float probability = params[PROBABILITY_KNOB].getValue();
		
		if(probabilityConnected) {
			float cv = clamp(inputs[PROBABILITY_INPUT].getVoltage(), -5.f, 5.f);
			float offset = rescale(cv, -5.f, 5.f, -1.f, 1.f);
			probability = clamp(cv + offset, 0.f, 1.f);
		}

		float dice = random::uniform();
		selectedVoice = dice < probability ? 1 : 0;
	}

	void updateLights() {
		lights[A_ACTIVE_LIGHT].setBrightness(selectedVoice == 0 ? 1.f : 0.f);
		lights[B_ACTIVE_LIGHT].setBrightness(selectedVoice == 1 ? 1.f : 0.f);
	}

	void updateConnections() {
		inputConnected[0] = inputs[POLYIN_A_INPUT].isConnected();
		inputConnected[1] = inputs[POLYIN_B_INPUT].isConnected();
		cvConnected = inputs[CV_INPUT].isConnected();
		increaseConnected = inputs[STEPINC_INPUT].isConnected();
		decreaseConnected = inputs[STEPDEC_INPUT].isConnected();
		randomConnected = inputs[RANDOM_INPUT].isConnected();
		resetConnected = inputs[RESET_INPUT].isConnected();
		probabilityConnected = inputs[PROBABILITY_INPUT].isConnected();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "step_0", json_integer(step[0]));
		json_object_set_new(rootJ, "step_1", json_integer(step[1]));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		{
			json_t* stepJ = json_object_get(rootJ, "step_0");
			if(stepJ) {
				step[0] = json_integer_value(stepJ);
			}
		}
		{
			json_t* stepJ = json_object_get(rootJ, "step_1");
			if(stepJ) {
				step[1] = json_integer_value(stepJ);
			}
		}
	}
};


struct SwitchN2Widget : ModuleWidget {
	PolyLightPort<16>* inputA;
	PolyLightPort<16>* inputB;
	
	SwitchN2Widget(SwitchN2* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SwitchN2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		
		inputA = createInputCentered<PolyLightPort<16>>(mm2px(Vec(7.699, 18.379)), module, SwitchN2::POLYIN_A_INPUT);
		inputA->selectedColor = SCHEME_ORANGE_23V;
		addInput(inputA);

		inputB = createInputCentered<PolyLightPort<16>>(mm2px(Vec(22.699, 18.379)), module, SwitchN2::POLYIN_B_INPUT);
		inputB->selectedColor = SCHEME_ORANGE_23V;
		addInput(inputB);

		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 38.200)), module, SwitchN2::RESET_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 51.100)), module, SwitchN2::CV_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 65.000)), module, SwitchN2::STEPINC_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 79.146)), module, SwitchN2::STEPDEC_INPUT));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(7.699, 92.700)), module, SwitchN2::RANDOM_INPUT));

		addParam(createParamCentered<KnobDark26>(mm2px(Vec(15.25, 28.3)), module, SwitchN2::PROBABILITY_KNOB));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(23, 38.200)), module, SwitchN2::PROBABILITY_INPUT));

		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(24, 51.100)), module, SwitchN2::MIN_OUTPUT));
		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(24, 65.0)), module, SwitchN2::MAX_OUTPUT));
		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(24, 79.146)), module, SwitchN2::TRIG_A_OUTPUT));
		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(24, 92.700)), module, SwitchN2::TRIG_B_OUTPUT));

		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(6, 112.0)), module, SwitchN2::MONOOUT_A_OUTPUT));
		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(15.2, 112.0)), module, SwitchN2::DICE_OUTPUT));
		addOutput(createOutputCentered<SmallPort>(mm2px(Vec(24, 112.0)), module, SwitchN2::MONOOUT_B_OUTPUT));

		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(11.5, 117.5)), module, SwitchN2::A_ACTIVE_LIGHT));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(18, 117.5)), module, SwitchN2::B_ACTIVE_LIGHT));
	}

	void step() override {
		if(module) {
			SwitchN2* module =  dynamic_cast<SwitchN2*>(this->module);
			inputA->selectedChannel = module->activeChannels[0];
			inputB->selectedChannel = module->activeChannels[1];
		}
		ModuleWidget::step();
	}
};


Model* modelSwitchN2 = createModel<SwitchN2, SwitchN2Widget>("SwitchN2");