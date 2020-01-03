#include "23volts.hpp"
#include "widgets/ports.hpp"

struct Merge8 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INPUTS, 8),
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(CHANNEL_LIGHTS, 8),
		NUM_LIGHTS
	};

	int channels = -1;

	Merge8() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onReset();
	}

	void onReset() override {
		channels = -1;
	}

	void process(const ProcessArgs& args) override {
		int lastChannel = -1;
		for (int c = 0; c < 8; c++) {
			float v = 0.f;
			if (inputs[INPUTS + c].isConnected()) {
				lastChannel = c;
				v = inputs[INPUTS + c].getVoltage();
			}
			outputs[OUT_OUTPUT].setVoltage(v, c);
		}

		outputs[OUT_OUTPUT].channels = (channels >= 0) ? channels : (lastChannel + 1);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* channelsJ = json_object_get(rootJ, "channels");
		if (channelsJ)
			channels = json_integer_value(channelsJ);
	}
};

struct Merge8ChannelItem : MenuItem {
	Merge8* module;
	int channels;
	void onAction(const event::Action& e) override {
		module->channels = channels;
	}
};


struct Merge8ChannelsItem : MenuItem {
	Merge8* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (int channels = -1; channels <= 8; channels++) {
			Merge8ChannelItem* item = new Merge8ChannelItem;
			if (channels < 0)
				item->text = "Automatic";
			else
				item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels == channels);
			item->module = module;
			item->channels = channels;
			menu->addChild(item);
		}
		return menu;
	}
};

struct Merge8Widget : ModuleWidget {
	Merge8Widget(Merge8* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Merge8.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 18.445)), module, Merge8::INPUTS + 0));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 29.215)), module, Merge8::INPUTS + 1));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 39.67)), module, Merge8::INPUTS + 2));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 50.007)), module, Merge8::INPUTS + 3));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 60.345)), module, Merge8::INPUTS + 4));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 70.735)), module, Merge8::INPUTS + 5));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 81.452)), module, Merge8::INPUTS + 6));
		addInput(createInputCentered<SmallPort>(mm2px(Vec(6.7, 91.54)), module, Merge8::INPUTS + 7));

		PolyLightPort<8>* output = createOutputCentered<PolyLightPort<8>>(mm2px(Vec(7.6, 105.18)), module, Merge8::OUT_OUTPUT);
		output->offset = 11;
		addOutput(output);
	}

	void appendContextMenu(Menu* menu) override {
		Merge8* module = dynamic_cast<Merge8*>(this->module);

		menu->addChild(new MenuEntry);

		Merge8ChannelsItem* channelsItem = new Merge8ChannelsItem;
		channelsItem->text = "Channels";
		channelsItem->rightText = RIGHT_ARROW;
		channelsItem->module = module;
		menu->addChild(channelsItem);
	}
};


Model* modelMerge8 = createModel<Merge8, Merge8Widget>("Merge8");