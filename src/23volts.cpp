#include "23volts.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelCells);
	p->addModel(modelClockM8);
	p->addModel(modelMem);
	p->addModel(modelMerge8);
	p->addModel(modelMonoPoly);
	p->addModel(modelMorph);
	p->addModel(modelPolyMerge);
	p->addModel(modelPolySplit);
	p->addModel(modelSplit8);
	p->addModel(modelSwitchN1);
	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}

