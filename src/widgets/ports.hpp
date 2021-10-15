#pragma once

#define F_PI float(3.14159265358979323846264338327950288) 

#include "rack.hpp"
#include "lights.hpp"

struct InvisiblePort : SvgPort {
	InvisiblePort() {
		setSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/PJ3410.svg")));
		shadow->opacity = 0.01f;
		sw->visible = false;
	}
};

struct SmallPort : SvgPort {
	SmallPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallPort.svg")));
	}
};

template <typename TBase, typename TLightBase>
struct LightPort : TBase {
	rack::app::ModuleLightWidget* light;
	bool active = true;

	LightPort() {
		light = new rack::componentlibrary::SmallLight<TLightBase>;
		light->box.pos = Vec(19,20);
		this->addChild(light); 
	}

	void step() override {
		TBase::step();
		std::vector<float> brightnesses; 
		brightnesses.push_back(active ? 10.f : 0.f);
		light->setBrightnesses(brightnesses);
	}
};


template <int TChannels>
struct PolyLightPort : rack::app::SvgPort {

	TinyLight<DynamicLight>* lights[TChannels];

	int offset = 0;

	// Keep track of number of channels at last step
	int oldChannels = 0; 

	// Allows to define dinamically how many channels LEDS are visible
	int activeChannels = TChannels;  

	NVGcolor selectedColor = SCHEME_GREEN;
	int selectedChannel = -1;
	int oldSelectedChannel = -1;

	bool created = false;

	PolyLightPort() {
		setSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/PJ301M.svg")));
	}

	void setActiveChannels(int channels) {
		activeChannels = channels <= TChannels ? channels : TChannels;
	}

	void createLights() {
		float div = (F_PI * 2) / 16; 
		for (int i = 0; i < TChannels; i++) {

			int index = i + offset > 16 ? i - 16 + offset : i + offset;

			float cosDiv = cos(div * index);
			float sinDiv = sin(div * index);
			float xPos  = sinDiv * 14.1f;
			float yPos  = cosDiv * 14.1f;

			TinyLight<DynamicLight>* light = new TinyLight<DynamicLight>;
			light->box.pos = Vec(xPos + 10.5f, 10.75f - yPos);
			lights[i] = light;
			this->addChild(light);
		}
		created = true;
	}

	void draw(const DrawArgs& args) override{
		if (created == false) createLights();
		rack::app::SvgPort::draw(args);
		for (int i = 0; i < TChannels && i < activeChannels; i++) {
			DrawArgs childCtx = args;
			// Intersect child clip box with self
			childCtx.clipBox = childCtx.clipBox.intersect(lights[i]->box);
			childCtx.clipBox.pos = childCtx.clipBox.pos.minus(lights[i]->box.pos);

			nvgSave(args.vg);
			nvgTranslate(args.vg, lights[i]->box.pos.x, lights[i]->box.pos.y);
			lights[i]->draw(childCtx);
			nvgRestore(args.vg);
		}
	}

	void step() override {
		if (created == false) createLights();
		rack::app::SvgPort::step();

		int channels; 
		
		if (type == rack::engine::Input::OUTPUT) {
			channels = module ? module->outputs[portId].getChannels() : TChannels;
		}
		else {
			channels = module ? module->inputs[portId].getChannels() : TChannels;
		}

		if(channels != oldChannels ||oldSelectedChannel != selectedChannel) {
			for(int i = 0; i < TChannels; i++) {
				lights[i]->setColor(i == selectedChannel ? selectedColor : SCHEME_BLUE);
				lights[i]->setBrightness(channels > i ? 1.f : 0.f);
			}
			oldChannels = channels;
		}
	}
};
