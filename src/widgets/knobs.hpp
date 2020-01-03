#pragma once

#include "rack.hpp"

struct KnobWhite23 : rack::RoundKnob {
	KnobWhite23() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KnobWhite23.svg")));
	}
};

struct SnapKnobWhite23 : KnobWhite23 {
	SnapKnobWhite23() {
		snap = true;
	}
};

struct KnobWhite26 : rack::RoundKnob {
	KnobWhite26() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KnobWhite26.svg")));
	}
};

struct KnobWhite32 : rack::RoundKnob {
	KnobWhite32() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KnobWhite32.svg")));
	}
};

struct KnobWhite48 : rack::RoundKnob {
	KnobWhite48() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KnobWhite48.svg")));
	}
};

struct SnapKnobWhite26 : KnobWhite26 {
	SnapKnobWhite26() {
		snap = true;
	}
};

struct KnobDark26 : rack::RoundKnob {
	KnobDark26() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/KnobDark26.svg")));
	}
};

struct SnapKnobDark26 : KnobDark26 {
	SnapKnobDark26() {
		snap = true;
	}
};
