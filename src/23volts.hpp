#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin *pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelCells;
extern Model* modelClockM8;
extern Model* modelMem;
extern Model* modelMerge4;
extern Model* modelMerge8;
extern Model* modelMidiPC;
extern Model* modelMonoPoly;
extern Model* modelMorph;
extern Model* modelMultimapK;
extern Model* modelMultimapS;
extern Model* modelPolyMerge;
extern Model* modelPolySplit;
extern Model* modelSplit4;
extern Model* modelSplit8;
extern Model* modelSwitchN1;
extern Model* modelSwitchN2;

static const NVGcolor SCHEME_ORANGE_23V = nvgRGB(246,139,73);