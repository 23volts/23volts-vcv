#include "23volts.hpp"
#include "widgets/ports.hpp"

struct Split4 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		POLY_IN_A,
		POLY_IN_B,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUTPUTS_A,4),
		ENUMS(OUTPUTS_B,4),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	Split4() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs& args) override {

		for (int c = 0; c < 4; c++) {
			float v = inputs[POLY_IN_A].getVoltage(c);
			outputs[OUTPUTS_A + c].setVoltage(v);
		}
		
		for (int c = 0; c < 4; c++) {
			float v = inputs[POLY_IN_B].getVoltage(c);
			outputs[OUTPUTS_B + c].setVoltage(v);
		}
	}
};


struct Split4Widget : ModuleWidget {
	Split4Widget(Split4* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Split4.svg")));

		{
			PolyLightPort<4>* input = createInput<PolyLightPort<4>>(Vec(10, 48), module, Split4::POLY_IN_A);
			addInput(input);

			for(int x = 0; x < 4; x++) {
				addOutput(createOutput<SmallPort>(Vec(13, 28 * x + 84), module, Split4::OUTPUTS_A + x));
			}
		}

		{
			PolyLightPort<4>* input = createInput<PolyLightPort<4>>(Vec(10, 200), module, Split4::POLY_IN_B);
			addInput(input);
			for(int x = 0; x < 4; x++) {
				addOutput(createOutput<SmallPort>(Vec(13, 28 * x + 239), module, Split4::OUTPUTS_B + x));
			}
		}
		

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
};


Model* modelSplit4 = createModel<Split4, Split4Widget>("Split4");