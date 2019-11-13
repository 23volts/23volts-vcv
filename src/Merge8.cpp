#include "23volts.hpp"


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


	dsp::ClockDivider clockLightDivider;
	int channels;

	Merge8() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		clockLightDivider.setDivision(512);
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

		if (clockLightDivider.process()) {
			for (int c = 0; c < 8; c++) {
				bool active = (c < outputs[OUT_OUTPUT].getChannels());
				lights[CHANNEL_LIGHTS + c].setBrightness(active);
			}
		}
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

struct MergeChannelItem : MenuItem {
	Merge8* module;
	int channels;
	void onAction(const event::Action& e) override {
		module->channels = channels;
	}
};


struct MergeChannelsItem : MenuItem {
	Merge8* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (int channels = -1; channels <= 8; channels++) {
			MergeChannelItem* item = new MergeChannelItem;
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

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 18.445)), module, Merge8::INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 29.215)), module, Merge8::INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 39.67)), module, Merge8::INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 50.007)), module, Merge8::INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 60.345)), module, Merge8::INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 70.735)), module, Merge8::INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 81.452)), module, Merge8::INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.7, 91.54)), module, Merge8::INPUTS + 7));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.6, 107.18)), module, Merge8::OUT_OUTPUT));

		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(4.805, 99.504)), module, Merge8::CHANNEL_LIGHTS + 0));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(6.393, 99.504)), module, Merge8::CHANNEL_LIGHTS + 1));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(7.98, 99.504)), module, Merge8::CHANNEL_LIGHTS + 2));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(9.568, 99.504)), module, Merge8::CHANNEL_LIGHTS + 3));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(4.805, 101.091)), module, Merge8::CHANNEL_LIGHTS + 4));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(6.393, 101.091)), module, Merge8::CHANNEL_LIGHTS + 5));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(7.98, 101.091)), module, Merge8::CHANNEL_LIGHTS + 6));
		addChild(createLight<TinyLight<BlueLight>>(mm2px(Vec(9.568, 101.091)), module, Merge8::CHANNEL_LIGHTS + 7));
	}

	void appendContextMenu(Menu* menu) override {
		Merge8* module = dynamic_cast<Merge8*>(this->module);

		menu->addChild(new MenuEntry);

		MergeChannelsItem* channelsItem = new MergeChannelsItem;
		channelsItem->text = "Channels";
		channelsItem->rightText = RIGHT_ARROW;
		channelsItem->module = module;
		menu->addChild(channelsItem);
	}
};


Model* modelMerge8 = createModel<Merge8, Merge8Widget>("Merge8");