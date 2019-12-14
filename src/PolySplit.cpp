#include "23volts.hpp"
#include "widgets/ports.hpp"

struct PolySplit : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		POLY_IN,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(OUTPUTS, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	int activeOutputs = 8;
	int voices = 2;

	PolySplit() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs& args) override {
		
		if(inputs[POLY_IN].isConnected() == false) return;

		int inputChannels = inputs[POLY_IN].getChannels();

		int currentChannel = 0;
		for(int i = 0; i < activeOutputs; i++) {

			//if(outputs[OUTPUTS + i].isConnected() == false) continue;
			
			int realChannels = 0;

			for(int c = 0; c < voices && currentChannel < inputChannels; c++) {
				float voltage = inputs[POLY_IN].getVoltage(currentChannel);
				outputs[OUTPUTS + i].setVoltage(voltage, c);
				realChannels++;
				currentChannel++;
			}

			outputs[OUTPUTS + i].channels = realChannels;
		}

		for(int i = activeOutputs; i < 8; i++) {
			outputs[OUTPUTS + i].channels = 0;
		}
	}

	void setVoicePerChannel(int newVoices) {
		voices = newVoices;
		activeOutputs = 16 / voices;
	}
	
	void onReset() override {
		activeOutputs = 2;
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

struct SplitPolyVoiceItem : MenuItem {
	PolySplit* module;
	int voices;
	void onAction(const event::Action& e) override {
		module->setVoicePerChannel(voices);
	}
};

struct PolySplitWidget : ModuleWidget {

	PolyLightPort<8>* outputs[8];

	PolySplitWidget(PolySplit* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PolySplit.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PolyLightPort<16>>(mm2px(Vec(7.711, 20.000)), module, PolySplit::POLY_IN));

		float yOffset = 97.f;
		for(int x = 0; x < 8; x++) {
			PolyLightPort<8>* output = createOutputCentered<PolyLightPort<8>>(Vec(23, x * 33.5f + yOffset), module, PolySplit::OUTPUTS + x);
			output->offset = 2;
			outputs[x] = output;
			addOutput(output);
		}
	}

	void step() override {
		ModuleWidget::step();

		if(module) {
			PolySplit* m = getModule();

			for(int x = 0; x < 8; x++) {
				if(x < m->activeOutputs) {
					outputs[x]->activeChannels =  m->voices;
					outputs[x]->visible = true;	
				}
				else {
					outputs[x]->activeChannels = 0;
					outputs[x]->visible = false;
				}
			}
		}
		
	}

	PolySplit* getModule() {
		return dynamic_cast<PolySplit*>(this->module);
	}

	void appendContextMenu(Menu* menu) override {
		
		PolySplit* module = getModule();

		menu->addChild(new MenuSeparator);

		MenuLabel* item = new MenuLabel;
	 	item->text = "Voices per output";
	 	menu->addChild(item);

	 	{
	 		SplitPolyVoiceItem* item = new SplitPolyVoiceItem;
			item->text = "2";
			item->voices = 2;
			item->rightText = CHECKMARK(module->voices == 2);
			item->module = module;
			menu->addChild(item);
	 	}
	 	{
	 		SplitPolyVoiceItem* item = new SplitPolyVoiceItem;
			item->text = "4";
			item->voices = 4;
			item->rightText = CHECKMARK(module->voices == 4);
			item->module = module;
			menu->addChild(item);
	 	}
	 	{
	 		SplitPolyVoiceItem* item = new SplitPolyVoiceItem;
			item->text = "8";
			item->voices = 8;
			item->rightText = CHECKMARK(module->voices == 8);
			item->module = module;
			menu->addChild(item);
	 	}
	}
};

Model* modelPolySplit = createModel<PolySplit, PolySplitWidget>("PolySplit");
