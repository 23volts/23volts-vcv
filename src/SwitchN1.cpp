#include "23volts.hpp"

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
		NUM_INPUTS
	};
	enum OutputIds {
		MONOOUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHTS, 32), // 16 Green-Blue
		NUM_LIGHTS
	};

	dsp::ClockDivider lightUpdater;
	dsp::ClockDivider connectionUpdater;

	dsp::SchmittTrigger stepIncreaseTrigger;
	dsp::SchmittTrigger stepDecreaseTrigger;
	dsp::SchmittTrigger stepRandomTrigger;

	int channels = 0;
	int activeChannel = -1;
	int step = 0;
	float offset = 0.f; // Offset by CV Input

	bool cvConnected = false;
	bool increaseConnected = false;
	bool decreaseConnected = false;
	bool randomConnected = false;

	SwitchN1() {
		connectionUpdater.setDivision(32);
		lightUpdater.setDivision(512);
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs& args) override {
		
		channels = inputs[POLYIN_INPUT].getChannels();

		if(connectionUpdater.process()) updateConnections();

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

		if (lightUpdater.process()) updateLights();

		if(activeChannel > -1) {
			outputs[MONOOUT_OUTPUT].setVoltage(inputs[POLYIN_INPUT].getVoltage(activeChannel));
		}
	}

	void updateConnections() {
		cvConnected = inputs[CV_INPUT].isConnected();
		increaseConnected = inputs[STEPINC_INPUT].isConnected();
		decreaseConnected = inputs[STEPDEC_INPUT].isConnected();
		randomConnected = inputs[RANDOM_INPUT].isConnected();
	}

	void updateLights() {
		for(int x=0; x<32; x+=2) {

			for (int c = 0; c < 16; c++) {
				bool active = (c < channels);
				bool selected = c == activeChannel;
				if(selected && active) {
					lights[CHANNEL_LIGHTS + c * 2].setBrightness(1.f);
					lights[CHANNEL_LIGHTS + c * 2 + 1].setBrightness(0.f);	
				}
				if(active && ! selected) {
					lights[CHANNEL_LIGHTS + c * 2].setBrightness(0.f);	
					lights[CHANNEL_LIGHTS + c * 2 + 1].setBrightness(1.f);	
				}
				if(! active) {
					lights[CHANNEL_LIGHTS + c * 2].setBrightness(0.f);
					lights[CHANNEL_LIGHTS + c * 2 + 1].setBrightness(0.f);
				}
			}
		}
	}
};


struct SwitchN1Widget : ModuleWidget {
	SwitchN1Widget(SwitchN1* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SwitchN1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.699, 20.179)), module, SwitchN1::POLYIN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.699, 44.999)), module, SwitchN1::CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.699, 59.772)), module, SwitchN1::STEPINC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.699, 75.146)), module, SwitchN1::STEPDEC_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.699, 90.113)), module, SwitchN1::RANDOM_INPUT));

		LightGridFactory<TinyLight<GreenBlueLight>> lightFactory;
		lightFactory.moduleWidget = this;
		lightFactory.lights = 16;
		lightFactory.colors = 2;
		lightFactory.rows = 2;
		lightFactory.startX = 0.8;
		lightFactory.startY = 34;
		lightFactory.Xseparation = 1.8;
		lightFactory.Yseparation = 1.8;
		lightFactory.base = SwitchN1::CHANNEL_LIGHTS;
		lightFactory.make();

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.699, 107.163)), module, SwitchN1::MONOOUT_OUTPUT));
	}
};


Model* modelSwitchN1 = createModel<SwitchN1, SwitchN1Widget>("SwitchN1");