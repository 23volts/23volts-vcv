#include "23volts.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelCells);
	p->addModel(modelClockM8);
	p->addModel(modelMem);
	p->addModel(modelMerge4);
	p->addModel(modelMerge8);
	p->addModel(modelMidiPC);
	p->addModel(modelMonoPoly);
	p->addModel(modelMorph);
	p->addModel(modelMultimapK);
	p->addModel(modelMultimapS);
	p->addModel(modelPatchbay);
	p->addModel(modelPolySplit);
	p->addModel(modelPolyMerge);
	p->addModel(modelSplit4);
	p->addModel(modelSplit8);
	p->addModel(modelSwitchN1);
	p->addModel(modelSwitchN2);
	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}

