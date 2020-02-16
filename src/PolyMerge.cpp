#include "23volts.hpp"
#include "widgets/ports.hpp"

struct PolyMerge : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INPUTS, 8),
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	int activeInputs = 8;
	int voices = 2;

	PolyMerge() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs& args) override {
		
		if(outputs[POLY_OUTPUT].isConnected() == false) return;

		// First, find the last connected input
		int lastConnectedActiveInput = -1;
		for(int x = activeInputs - 1; x >= 0; x--) {
			if(inputs[INPUTS + x].isConnected() && lastConnectedActiveInput == -1) {
				lastConnectedActiveInput = x;
				x = -1;
			}
		}

		int currentOutputChannel = 0; // O

		for(int i = 0; i <= lastConnectedActiveInput; i++) {
			
			if(inputs[INPUTS + i].isConnected() == false) {
				for(int c = 0; c < voices; c++) {
					outputs[POLY_OUTPUT].setVoltage(0.f, currentOutputChannel);
					currentOutputChannel++;
				}
			}
			else {
				int inputChannels = inputs[INPUTS + i].getChannels();
				for(int c = 0; c < voices; c++) {
					float voltage = c <= inputChannels ? inputs[INPUTS + i].getVoltage(c) : 0.f;
					outputs[POLY_OUTPUT].setVoltage(voltage, currentOutputChannel);
					currentOutputChannel++;
				}
			}
		}

		outputs[POLY_OUTPUT].channels = currentOutputChannel + 1;
	}

	void setVoicePerChannel(int newVoices) {
		voices = newVoices;
		activeInputs = 16 / voices;
	}

	void onReset() override {
		activeInputs = 2;
		voices = 8;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "voices", json_integer(voices));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		setVoicePerChannel(json_integer_value(json_object_get(rootJ, "voices")));
	}
};

struct MergePolyVoiceItem : MenuItem {
	PolyMerge* module;
	int voices;
	void onAction(const event::Action& e) override {
		module->setVoicePerChannel(voices);
	}
};

struct PolyMergeWidget : ModuleWidget {

	PolyLightPort<8>* inputs[8];

	PolyMergeWidget(PolyMerge* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PolyMerge.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float yOffset = 58.f;
		for(int x = 0; x < 8; x++) {
			PolyLightPort<8>* input = createInputCentered<PolyLightPort<8>>(Vec(20, x * 33.5f + yOffset), module, PolyMerge::INPUTS + x);
			input->offset = 13;
			inputs[x] = input;
			addInput(input);
		}

		addOutput(createOutputCentered<PolyLightPort<16>>(mm2px(Vec(7.6, 111.18)), module, PolyMerge::POLY_OUTPUT));

	}

	void step() override {
		ModuleWidget::step();

		if(module) {
			PolyMerge* m = getModule();

			for(int x = 0; x < 8; x++) {
				if(x < m->activeInputs) {
					inputs[x]->activeChannels =  m->voices;
					inputs[x]->visible = true;	
				}
				else {
					inputs[x]->activeChannels = 0;
					inputs[x]->visible = false;
				}
			}
		}
	}

	PolyMerge* getModule() {
		return dynamic_cast<PolyMerge*>(this->module);
	}

	void appendContextMenu(Menu* menu) override {
		
		PolyMerge* module = getModule();

		menu->addChild(new MenuSeparator);

		MenuLabel* item = new MenuLabel;
	 	item->text = "Voices per input";
	 	menu->addChild(item);

	 	{
	 		MergePolyVoiceItem* item = new MergePolyVoiceItem;
			item->text = "2";
			item->voices = 2;
			item->rightText = CHECKMARK(module->voices == 2);
			item->module = module;
			menu->addChild(item);
	 	}
	 	{
	 		MergePolyVoiceItem* item = new MergePolyVoiceItem;
			item->text = "4";
			item->voices = 4;
			item->rightText = CHECKMARK(module->voices == 4);
			item->module = module;
			menu->addChild(item);
	 	}
	 	{
	 		MergePolyVoiceItem* item = new MergePolyVoiceItem;
			item->text = "8";
			item->voices = 8;
			item->rightText = CHECKMARK(module->voices == 8);
			item->module = module;
			menu->addChild(item);
	 	}
	}
};

Model* modelPolyMerge = createModel<PolyMerge, PolyMergeWidget>("PolyMerge");
