#pragma once

#include "rack.hpp"
#include "lights.hpp"

// TODO : Generalize and template with light type
struct LedButton : Switch {
	MultiLightWidget* light;

	LedButton() {
		box.size = Vec(12, 12);
		light = new MultiLightWidget;
		light->bgColor = nvgRGB(0x5a, 0x5a, 0x5a);
		light->borderColor = nvgRGBA(0, 0, 0, 0x60);
		light->box.size = Vec(8,8);
		light->box.pos = Vec(2,2);
		light->addBaseColor(SCHEME_BLUE);
		addChild(light);
	}

	void onChange(const event::Change& e) override {
		Switch::onChange(e);
		std::vector<float> brightnesses;
		brightnesses.push_back(getParamQuantity()->isMax() ? 1.0f : 0.f);
		light->setBrightnesses(brightnesses);
	}
};

struct DynamicLedButton : Switch {
	DynamicLight* light;

	DynamicLedButton() {
		box.size = Vec(16.f, 16.f);
		light = new DynamicLight;
		light->bgColor = nvgRGB(186, 194, 205);
		light->borderColor = nvgRGBA(0, 0, 0, 0x60);
		light->box.size = Vec(16.f, 16.f);
		light->setColor(SCHEME_BLUE);
		addChild(light);
	}

	void onChange(const event::Change& e) override {
		Switch::onChange(e);
		std::vector<float> brightnesses;
		light->setBrightness(getParamQuantity()->isMax() ? 1.0f : 0.f);
	}
};

struct MomentaryLedButton : DynamicLedButton {
	MomentaryLedButton() {
		momentary = true;
	}
};


struct LedSwitch32 : DynamicLedButton {
	LedSwitch32() {
		box.size = Vec(32.f, 32.f);
		light->box.pos = Vec(2.f, 2.f);
		light->box.size = Vec(28.f, 28.f);
	}
};


struct TextLightButton : DynamicLedButton {
	std::string text;
	std::shared_ptr<Font> font;
	NVGcolor textColor;

	TextLightButton() {
		box.size = Vec(12.f, 12.f);
		light->box.size = Vec(12.f, 12.f);
		light->bgColor = nvgRGB(0,0,0);
		light->borderColor = light->baseColor;
		textColor = light->baseColor;
		text = "";
	}

	void onChange(const event::Change& e) override {
		DynamicLedButton::onChange(e);
		textColor = getParamQuantity()->isMax() ? light->bgColor : light->baseColor;
	}

	void draw(const DrawArgs &args) override {
		DynamicLedButton::draw(args);
		float bounds[4];
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 16.f);
		nvgFillColor(args.vg, textColor);
		nvgTextBounds(args.vg, 0.f, 0.f, text.c_str(), NULL, bounds);
		float xOffset = ((float) this->box.size.x - bounds[2]) / 2.f;
		nvgText(args.vg, xOffset, 10.5f, text.c_str(), NULL);
	}
};

struct MomentaryTextLightButton : TextLightButton {
	MomentaryTextLightButton() {
		momentary = true;
	}
};