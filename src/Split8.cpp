#include "23volts.hpp"


struct Split8 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHTS, 8),
		NUM_LIGHTS
	};

	dsp::ClockDivider channelLightDivider;

	
	Split8() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		channelLightDivider.setDivision(512);
	}

	void process(const ProcessArgs& args) override {

		for (int c = 0; c < 8; c++) {
			float v = inputs[IN_INPUT].getVoltage(c);
			outputs[OUTPUTS + c].setVoltage(v);
		}

		if (channelLightDivider.process()) {
			for (int c = 0; c < 8; c++) {
				bool active = (c < inputs[IN_INPUT].getChannels());
				lights[CHANNEL_LIGHTS + c].setBrightness(active);
			}
		}
	}
};


struct Split8Widget : ModuleWidget {
	Split8Widget(Split8* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Split8.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.711, 23.000)), module, Split8::IN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 39.045)), module, Split8::OUTPUTS + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 49.814)), module, Split8::OUTPUTS + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 60.269)), module, Split8::OUTPUTS + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 70.607)), module, Split8::OUTPUTS + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 80.944)), module, Split8::OUTPUTS + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 91.334)), module, Split8::OUTPUTS + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 102.051)), module, Split8::OUTPUTS + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.9, 112.139)), module, Split8::OUTPUTS + 7));

		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(4.825, 28.261)), module, Split8::CHANNEL_LIGHTS + 0));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(6.412, 28.261)), module, Split8::CHANNEL_LIGHTS + 1));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(8.0, 28.261)), module, Split8::CHANNEL_LIGHTS + 2));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(9.587, 28.261)), module, Split8::CHANNEL_LIGHTS + 3));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(4.825, 29.849)), module, Split8::CHANNEL_LIGHTS + 4));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(6.412, 29.849)), module, Split8::CHANNEL_LIGHTS + 5));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(8.0, 29.849)), module, Split8::CHANNEL_LIGHTS + 6));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(9.587, 29.849)), module, Split8::CHANNEL_LIGHTS + 7));
	}
};


Model* modelSplit8 = createModel<Split8, Split8Widget>("Split8");