#pragma once

#include "rack.hpp"

struct DynamicLight : LightWidget {
	NVGcolor baseColor;

	float brightness = 1.f;
	bool hasHalo = true;

	DynamicLight() {
		this->bgColor = nvgRGB(0x5a, 0x5a, 0x5a);
		this->borderColor = nvgRGBA(0, 0, 0, 0x60);
	}

	void setColor(NVGcolor newColor) {
		baseColor = newColor;
		setBrightness(brightness);
	}

	void setBrightness(float newBrightness) {
		brightness = newBrightness;
		color = color::alpha(baseColor, brightness);
	}

	void drawHalo(const DrawArgs& args) override {
		if(hasHalo) LightWidget::drawHalo(args);
	}
};

struct SquareLight : DynamicLight {

	void drawLight(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);

		// Background
		if (bgColor.a > 0.0) {
			nvgFillColor(args.vg, bgColor);
			nvgFill(args.vg);
		}
		// Foreground
		if (color.a > 0.0) {
			nvgFillColor(args.vg, color);
			nvgFill(args.vg);
		}
		// Border
		if (borderColor.a > 0.0) {
			nvgStrokeWidth(args.vg, 0.5);
			nvgStrokeColor(args.vg, borderColor);
			nvgStroke(args.vg);
		}
	}
};