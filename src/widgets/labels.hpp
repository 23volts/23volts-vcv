#pragma once

#include "rack.hpp"

struct TextLabel : rack::widget::TransparentWidget {

	bool centered = true;
	std::shared_ptr<Font> m_font;
	NVGcolor m_color;
	std::string m_text;
	float m_fontSize = 12.f;

	TextLabel(const std::shared_ptr<Font> font) {
		m_color = nvgRGB(0xFF, 0xFF, 0xFF);
		m_font = font;
	}

	void setText(std::string &text) {
		m_text = text;
	}

	void draw(const DrawArgs &args) override {
		float bounds[4];
		nvgFontFaceId(args.vg, m_font->handle);
		nvgFontSize(args.vg, m_fontSize);
		nvgTextAlign(args.vg, NVG_ALIGN_TOP);
		nvgFillColor(args.vg, m_color);
		
		if(centered) {
			nvgTextBounds(args.vg, 0.f, 0.f, m_text.c_str(), NULL, bounds);
			float xOffset = ((float) this->box.size.x - bounds[2]) / 2.f;
			nvgText(args.vg, xOffset, 0, m_text.c_str(), NULL);
		}
		else {
			nvgText(args.vg, 0, 0, m_text.c_str(), NULL);
		}
	}

	void setFontSize(float size) {
		m_fontSize = size;
	}

	void setColor(NVGcolor color) {
		m_color = color;
	}
};

struct TextTag : TextLabel {
	NVGcolor backgroundColor;

	TextTag(const std::shared_ptr<Font> font) : TextLabel(font) {
		backgroundColor = SCHEME_YELLOW;
	}

	void setBackgroundColor(NVGcolor color_) {
		backgroundColor = color_;
	}

	void draw(const DrawArgs &args) override {
		float bounds[4];
		nvgFontFaceId(args.vg, m_font->handle);
		nvgFontSize(args.vg, m_fontSize);
		nvgTextAlign(args.vg, NVG_ALIGN_TOP);
		nvgTextBounds(args.vg, 0.f, 0.f, m_text.c_str(), NULL, bounds);
		//DEBUG("bounds : %f, %f, %f, %f ", bounds[0], bounds[1], bounds[2], bounds[3]);
		nvgFillColor(args.vg, backgroundColor);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, bounds[2] + 2.f, bounds[3] + 1.f, 1.f);
		nvgFill(args.vg);
		nvgFillColor(args.vg, m_color);
		nvgText(args.vg, 1.f, 0, m_text.c_str(), NULL);
	}
};

struct LCDBackground : rack::widget::OpaqueWidget {

	NVGcolor m_color;
	NVGcolor strokeColor;

	LCDBackground() {
		m_color = nvgRGB(0x00, 0x00, 0x00);
		strokeColor = nvgRGB(0x21, 0x21, 0x21);
	}

	void draw(const DrawArgs &args) override {
		nvgFillColor(args.vg, m_color);
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.f);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 0.8f);
		nvgFill(args.vg);
		nvgStroke(args.vg);
	}

	void setColor(NVGcolor color) {
		m_color = color;
		strokeColor = nvgRGB(m_color.r + 0x21, m_color.g + 0x21, m_color.b + 0x21);
	}
};

struct LCDLabel : rack::widget::TransparentWidget {
	LCDBackground* background;
	TextLabel* label;

	LCDLabel(math::Vec size) {
		this->box.size = size;
		background = new LCDBackground();
		background->box.size = size;
		background->setColor(nvgRGB(0x15, 0x15, 0x15));
		addChild(background);
		
		auto fontFileName = "res/fonts/EHSMB.TTF";
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontFileName));
		label = new TextLabel(font);
		label->box.pos = Vec(0.f, 0.f);
		label->box.size = this->box.size;
		addChild(label);
	}

	void setFontSize(float size) {
		label->setFontSize(size);
	}

	void setColor(NVGcolor color) {
		label->setColor(color);
	}

	void setBackgroundColor(NVGcolor color) {
		background->setColor(color);
	}

	void setText(std::string &text) {
		label->setText(text);
	}

	void setLabelPos(math::Vec pos) {
		label->box.pos = pos;
	}
};
