#include "23volts.hpp"
#include "widgets/ports.hpp"

struct Merge4 : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(INPUTS_A,4),
		ENUMS(INPUTS_B,4),
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_OUT_A,
		POLY_OUT_B,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	int channels[2];

	Merge4() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onReset();
	}

	void onReset() override {
		channels[0] = -1;
		channels[1] = -1;
	}

	void process(const ProcessArgs& args) override {
		int lastChannelA = -1;
		int lastChannelB = -1;
		for (int c = 0; c < 4; c++) {
			float v = 0.f;
			
			if (inputs[INPUTS_A + c].isConnected()) {
				lastChannelA = c;
				v = inputs[INPUTS_A + c].getVoltage();
			}
			outputs[POLY_OUT_A].setVoltage(v, c);

			if (inputs[INPUTS_B + c].isConnected()) {
				lastChannelB = c;
				v = inputs[INPUTS_B + c].getVoltage();
			}
			outputs[POLY_OUT_B].setVoltage(v, c);
		}

		outputs[POLY_OUT_A].channels = (channels[0] >= 0) ? channels[0] : (lastChannelA + 1);
		outputs[POLY_OUT_B].channels = (channels[1] >= 0) ? channels[1] : (lastChannelB + 1);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "channels_A", json_integer(channels[0]));
		json_object_set_new(rootJ, "channels_B", json_integer(channels[1]));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* channelsAJ = json_object_get(rootJ, "channels_A");
		if (channelsAJ)
			channels[0] = json_integer_value(channelsAJ);
		json_t* channelsBJ = json_object_get(rootJ, "channels_B");
		if (channelsBJ)
			channels[1] = json_integer_value(channelsBJ);
	}
};

struct Merge4ChannelItem : MenuItem {
	Merge4* module;
	int channels;
	int index;
	void onAction(const event::Action& e) override {
		module->channels[index] = channels;
	}
};


struct Merge4ChannelsItem : MenuItem {
	Merge4* module;
	int index;

	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		for (int channels = -1; channels <= 4; channels++) {
			if(channels == 0) continue;
			Merge4ChannelItem* item = new Merge4ChannelItem;
			if (channels < 0)
				item->text = "Automatic";
			else
				item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels[index] == channels);
			item->module = module;
			item->channels = channels;
			item->index = index;
			menu->addChild(item);
		}
		return menu;
	}
};

struct Merge4Widget : ModuleWidget {
	Merge4Widget(Merge4* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Merge4.svg")));

		{
			for(int x = 0; x < 4; x++) {
				addInput(createInput<SmallPort>(Vec(10.f, 27.3f * x + 45.5f), module, Merge4::INPUTS_A + x));
			}
			PolyLightPort<4>* output = createOutput<PolyLightPort<4>>(Vec(10, 162), module, Merge4::POLY_OUT_A);
			output->offset = 13;
			addOutput(output);
		}

		{
			for(int x = 0; x < 4; x++) {
				addInput(createInput<SmallPort>(Vec(10.f, 27.3f * x + 200), module, Merge4::INPUTS_B + x));
			}	
			PolyLightPort<4>* output = createOutput<PolyLightPort<4>>(Vec(10, 316), module, Merge4::POLY_OUT_B);
			output->offset = 13;
			addOutput(output);
		}
		
		
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

	void appendContextMenu(Menu* menu) override {
		Merge4* module = dynamic_cast<Merge4*>(this->module);

		menu->addChild(new MenuEntry);

		{
			Merge4ChannelsItem* channelsItem = new Merge4ChannelsItem;
			channelsItem->index = 0;
			channelsItem->text = "Channels A";
			channelsItem->rightText = RIGHT_ARROW;
			channelsItem->module = module;
			menu->addChild(channelsItem);
		}
		{
			Merge4ChannelsItem* channelsItem = new Merge4ChannelsItem;
			channelsItem->index = 1;
			channelsItem->text = "Channels B";
			channelsItem->rightText = RIGHT_ARROW;
			channelsItem->module = module;
			menu->addChild(channelsItem);
		}
	}
};


Model* modelMerge4 = createModel<Merge4, Merge4Widget>("Merge4");