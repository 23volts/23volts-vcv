#pragma once

#include "rack.hpp"

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
		brightnesses.push_back(paramQuantity->isMax() ? 1.0f : 0.f);
		light->setBrightnesses(brightnesses);
	}
};
