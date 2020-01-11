#pragma once

#include "rack.hpp"
#include "midi.hpp"
#include "../widgets/knobs.hpp"
#include "../widgets/lights.hpp"
#include "../widgets/labels.hpp"

struct ParamMapCollection {
	std::vector<int> parameterIds;
	bool learningEnabled = false;
	int learningParamId = -1;
	int touchedParamId = -1;

	virtual void touch(int paramId) {
		touchedParamId = paramId;
	}

	virtual void untouch() {
		touchedParamId = -1;
	}

	bool isLearningEnabled() {
		return learningEnabled;
	}

	void setLearning(bool state) {
		learningEnabled = state;
	}

	void startLearning(int paramId) {
		learningParamId = paramId;
	}

	void cancelLearning() {
		learningParamId = -1;
	}

	bool isLearning(int paramId) {
		return learningParamId == paramId;
	}

	void learnNext()  {
		int learnIndex = getParameterIndex(learningParamId);

		if(learnIndex > -1 && learnIndex < (int) parameterIds.size() - 1) {
			learningParamId = parameterIds[learnIndex + 1];
		}
		else {
			cancelLearning();
		}
	}

	int getParameterIndex(int paramId) {
		for(int x = 0; x < (int) parameterIds.size(); x++) {
			if(parameterIds[x] == paramId) {
				return x;
			}
		}
		return -1;
	}

	virtual void unassign(int paramId) {}
	virtual bool isAssigned(int paramId) { return false; };
};

struct ParamMapping {
	std::string moduleName;
	std::string paramName;
	ParamHandle paramHandle;

	json_t* toJson() {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "module_name", json_string(moduleName.c_str()));
		json_object_set_new(rootJ, "param_name", json_string(paramName.c_str()));
		json_object_set_new(rootJ, "module_id", json_integer(paramHandle.moduleId));
		json_object_set_new(rootJ, "param_id", json_integer(paramHandle.paramId));
		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		json_t* moduleNameJ = json_object_get(rootJ, "module_name");
		if(moduleNameJ) moduleName = json_string_value(moduleNameJ);
		json_t* paramNameJ = json_object_get(rootJ, "param_name");
		if(paramNameJ) paramName = json_string_value(paramNameJ);
	}
};

struct HandleMapCollection : ParamMapCollection {
	std::map<int, ParamMapping> param2handle; // Key = Multimap's param id

	virtual ~HandleMapCollection() {
		clear();
	}

	void clear() {
		for (std::pair<int, ParamMapping> element : param2handle) {
			APP->engine->removeParamHandle(&param2handle[element.first].paramHandle);
		}
		param2handle.clear();
	}

	void unassign(int paramId) override {
		if(isAssigned(paramId)) {
			APP->engine->removeParamHandle(&param2handle[paramId].paramHandle);
			param2handle.erase(paramId);
		}
	}

	void touch(int paramId) override {
		untouch();
		if(isAssigned(paramId)) {
			if(isDeadParameterHandle(paramId)) {
				unassign(paramId);
			}
			else {
				param2handle[paramId].paramHandle.color = SCHEME_BLUE;
			}
		}	

		ParamMapCollection::touch(paramId);
	}

	bool isDeadParameterHandle(int paramId) {
		return param2handle[paramId].paramHandle.module == NULL;
	}

	void untouch() override {
		if(isAssigned(touchedParamId)) param2handle[touchedParamId].paramHandle.color = SCHEME_YELLOW;
		ParamMapCollection::untouch();
	}

	bool isAssigned(int paramId) override {
		return param2handle.find(paramId) != param2handle.end();
	}

	virtual void commitLearn(int paramId, int targetModuleId, int targetParamId) {

		if(! isAssigned(paramId)) {
			param2handle[paramId] = ParamMapping();
			param2handle[paramId].paramHandle.color = SCHEME_YELLOW;
			APP->engine->addParamHandle(&param2handle[paramId].paramHandle);
		}

		ParamMapping* mapping = &param2handle[paramId];

		APP->engine->updateParamHandle(&mapping->paramHandle, targetModuleId, targetParamId, true);

		ModuleWidget *moduleWidget = APP->scene->rack->getModule(targetModuleId);
		Module *targetModule = moduleWidget->module;
		ParamQuantity *paramQuantity = targetModule->paramQuantities[targetParamId];
		mapping->moduleName = moduleWidget->model->name;
		mapping->paramName = paramQuantity->label;

		learnNext();
	}

	virtual ParamMapping* getMap(int paramId) {
		return &param2handle[paramId];
	}

	virtual std::map<int, ParamMapping>* getMappedParameters() {
		return &param2handle;
	}

	json_t* toJson() {
		json_t* rootJ = json_object();
		auto iterator = param2handle.begin();
		while(iterator != param2handle.end())
		{
			int key = iterator->first;
			ParamMapping* mapping = &iterator->second;
			json_object_set_new(rootJ, std::to_string(key).c_str(), mapping->toJson());
			iterator++;
		}

		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		const char *key;
		json_t *value;

		json_object_foreach(rootJ, key, value) {
			int paramId = atoi(key);
		    param2handle[paramId] = ParamMapping();
		    ParamMapping* mapping = &param2handle[paramId];
			mapping->fromJson(value);
			mapping->paramHandle.color = SCHEME_YELLOW;
			APP->engine->addParamHandle(&mapping->paramHandle);
			APP->engine->updateParamHandle(
				&mapping->paramHandle, 
				json_integer_value(json_object_get(value, "module_id")),
				json_integer_value(json_object_get(value, "param_id")),
				true);
		}
	}
};

struct MultiHandleMapCollection : HandleMapCollection {
	std::vector<HandleMapCollection*> pages;
	int currentPage;
	int size;

	~MultiHandleMapCollection() {
		for(HandleMapCollection* e : pages) {
			delete e;
		}
	}

	void clear() {
		for(HandleMapCollection* e : pages) {
			delete e;
		}
		pages.clear();
		size = 0;
	}

	void init(int size_ = 1) {
		for(int x = 0; x < size_; x++) pages.push_back(new HandleMapCollection());
		size = size_;
		currentPage = 0;
	}

	void next() {
		if(currentPage >= size) {
			pages.push_back(new HandleMapCollection());
			size++;
		}
		loadPage(currentPage + 1);
	}

	void previous() {
		if(currentPage > 0) {
			loadPage(currentPage - 1);
		}
	}

	void loadPage(int page) {
		setCurrentPageHandleColor(nvgRGBA(0xf9, 0xdf, 0x1c, 0x42));
		currentPage = page;
		setCurrentPageHandleColor(SCHEME_YELLOW);
	}

	void setCurrentPageHandleColor(NVGcolor color) {
		HandleMapCollection* parameters = pages[currentPage];
		auto iterator = parameters->param2handle.begin();
		while(iterator != parameters->param2handle.end())
		{
			iterator->second.paramHandle.color = color;
			iterator++;
		}
	}

	void unassign(int paramId) override {
		pages[currentPage]->unassign(paramId);
	}

	void touch(int paramId) override {
		untouch();
		if(pages[currentPage]->isAssigned(paramId)) {
			if(pages[currentPage]->isDeadParameterHandle(paramId)) {
				pages[currentPage]->unassign(paramId);
			}
			else {
				pages[currentPage]->param2handle[paramId].paramHandle.color = SCHEME_BLUE;	
			}
		}
		ParamMapCollection::touch(paramId);
	}

	void untouch() override {
		if(pages[currentPage]->isAssigned(touchedParamId)) {
			pages[currentPage]->param2handle[touchedParamId].paramHandle.color = SCHEME_YELLOW;
		}
		ParamMapCollection::untouch();
	}

	bool isAssigned(int paramId) override {
		return pages[currentPage]->param2handle.find(paramId) != pages[currentPage]->param2handle.end();
	}

	void commitLearn(int paramId, int targetModuleId, int targetParamId) override {
		pages[currentPage]->commitLearn(paramId, targetModuleId, targetParamId);
		learnNext();
	}

	ParamMapping* getMap(int paramId) override {
		return pages[currentPage]->getMap(paramId);
	}

	std::map<int, ParamMapping>* getMappedParameters() override {
		return &pages[currentPage]->param2handle;
	}

	json_t* toJson() {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "current_page", json_integer(currentPage));
		json_t* pagesJ = json_array();
		for(int x = 0; x < size; x++) {
			json_array_append_new(pagesJ, pages[x]->toJson());
		}
		json_object_set_new(rootJ, "pages", pagesJ);
		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		json_t* currentPageJ =json_object_get(rootJ, "current_page");
		if(currentPageJ) currentPage = json_integer_value(currentPageJ);
		json_t* pagesJ = json_object_get(rootJ, "pages");
		if(pagesJ) {
			for(HandleMapCollection* e : pages) {
				delete e;
			}
			pages.clear();
			size = 0;
			int pSize = json_array_size(pagesJ);
			for(int x = 0; x < pSize; x++) {
				pages.push_back(new HandleMapCollection());
				size++;
				pages[x]->fromJson(json_array_get(pagesJ, x));
			}
		}
	}
};

struct MidiMapping {
	enum types {
		MIDI_CC,
		MIDI_NOTE
	};

	int port = 0;
	int type;
	uint8_t channel;
	uint8_t cc; // Or note number

	json_t* toJson() {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "port", json_integer(port));
		json_object_set_new(rootJ, "type", json_integer(type));
		json_object_set_new(rootJ, "channel", json_integer(channel));
		json_object_set_new(rootJ, "cc", json_integer(cc));
		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		json_t* portJ = json_object_get(rootJ, "port");
		if(portJ) port = json_integer_value(portJ);
		json_t* typeJ = json_object_get(rootJ, "type");
		if(typeJ) type = json_integer_value(typeJ);
		json_t* channelJ = json_object_get(rootJ, "channel");
		if(channelJ) channel = (uint8_t) json_integer_value(channelJ);
		json_t* ccJ = json_object_get(rootJ, "cc");
		if(ccJ) cc = (uint8_t) json_integer_value(ccJ);
	}
};

struct MidiMapCollection : ParamMapCollection {
	std::map<int,MidiMapping> param2midi; // ParamID -> Midi Mapping

	void clear() {
		param2midi.clear();
	}

	void unassign(int paramId) override {
		param2midi.erase(paramId);
	}

	bool isAssigned(int paramId) override {
		return param2midi.find(paramId) != param2midi.end();
	}

	int getMappedParamId(midi::Message& msg) {
		uint8_t channel = msg.getChannel();
		uint8_t number = msg.getNote();
		int type;

		switch (msg.getStatus()) {	
			case 0x9:
				type = MidiMapping::MIDI_NOTE;
				break;
			case 0xb:
				type = MidiMapping::MIDI_CC;
				break;
		}

		auto iterator = param2midi.begin();

		while(iterator != param2midi.end())
		{
			MidiMapping* mapping = &iterator->second;
			if(mapping->type == type && mapping->channel == channel && mapping->cc == number) {
				return iterator->first;
			}
			iterator++;
		}

		return -1;
	}

	void onMidiMessage(midi::Message& msg) {
		if(isLearningEnabled() && learningParamId > -1) {
			int assignedParamId = getMappedParamId(msg);
			if(assignedParamId > -1) {
				unassign(assignedParamId);
			}

			uint8_t channel = msg.getChannel();
			uint8_t number = msg.getNote();
			// Allow mapping CC & Notes
			switch (msg.getStatus()) {
				// NOTE ON
				case 0x9:
					if(msg.getValue() > 0) {
						MidiMapping mapping;
						mapping.channel = channel;
						mapping.cc = number;
						mapping.type = MidiMapping::MIDI_NOTE;
						param2midi.emplace(learningParamId, mapping);
					}
					commitLearn();
					break;
				// MIDI CC
				case 0xb: 
					MidiMapping mapping;
					mapping.channel = channel;
					mapping.cc = number;
					mapping.type = MidiMapping::MIDI_CC;
					param2midi.emplace(learningParamId, mapping);
					commitLearn();
					break;
			}
		}
		
	}

	void commitLearn() {
		learningParamId = -1;
	}

	MidiMapping* getMapping(int paramId) {
		return &param2midi[paramId];
	}

	std::map<int, MidiMapping>* getMappedParameters() {
		return &param2midi;
	}

	json_t* toJson() {
		json_t* rootJ = json_object();
		auto iterator = param2midi.begin();
		while(iterator != param2midi.end())
		{
			int key = iterator->first;
			MidiMapping* mapping = &iterator->second;
			json_object_set_new(rootJ, std::to_string(key).c_str(), mapping->toJson());
			iterator++;
		}

		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		const char *key;
		json_t *value;

		json_object_foreach(rootJ, key, value) {
			int paramId = atoi(key);
		    MidiMapping mapping;
			mapping.fromJson(value);
			param2midi.emplace(paramId, mapping);
		}
	}
};

struct MidiFeedbackCache {
	std::map<int,uint8_t> values;

	void updateCache(int paramId, uint8_t value) {
		values[paramId] = value;
	}

	bool changed(int paramId, uint8_t value) {
		return values[paramId] != value;
	}
};

struct MappingProcessor {
	MidiInputOutput* midiIO = NULL;
	MidiMapCollection* midiMap = NULL;
	HandleMapCollection* handleMap = NULL;

	dsp::ClockDivider divider;

	MidiFeedbackCache midiCache;
	std::map<int,ParamQuantity*> params;

	float scaledValues[128];

	bool processMidiInput = true;

	MappingProcessor() {
		for(int x = 0; x < 128; x++) {
			scaledValues[x] = (1.f / 127) * x;
		}
		divider.setDivision(32);
	}

	void process() {
		if(divider.process()) {
			if(midiIO && midiMap && processMidiInput && midiIO->input.isConnected()) processMidiQueue();
			if(handleMap) processHandledParameters();
			if(midiIO && midiMap && midiIO->output.isConnected()) processMidiFeedback();
		}
	}

	void processMidiQueue() {
		bool learning = midiMap->isLearningEnabled() && midiMap->learningParamId > -1;

		// Scan midi input and update params & button states
		midi::Message msg;
		while (midiIO->input.shift(&msg)) {

			if (learning == true) {
				midiMap->onMidiMessage(msg);
			}
			
			int paramId = midiMap->getMappedParamId(msg);
			
			if(paramId > -1) {
				uint8_t midiValue = msg.getValue();
				params[paramId]->setScaledValue(scaledValues[midiValue]);
				midiCache.updateCache(paramId, midiValue);
				midiMap->touch(paramId);
			}
			
		}
	}

	void processHandledParameters() {
		std::map<int,ParamMapping>* parameters = handleMap->getMappedParameters();

		auto iterator = parameters->begin();
		while(iterator != parameters->end())
		{
			int paramId = iterator->first;
			ParamHandle handle = iterator->second.paramHandle;
			updateHandledParameter(paramId, handle);
			iterator++;
		}
	}

	void updateHandledParameter(int paramId, ParamHandle& paramHandle) {
		float value = params[paramId]->getScaledValue();

		Module* module = paramHandle.module;
		
		if (!module) {
			return; 
		}

		int targetParamId = paramHandle.paramId;
		ParamQuantity* paramQuantity = module->paramQuantities[targetParamId];
		
		if (!paramQuantity) return;
		if (!paramQuantity->isBounded()) return;

		paramQuantity->setScaledValue(value);
	}


	void processMidiFeedback() {
		std::map<int, MidiMapping>* mappings = midiMap->getMappedParameters();

		auto iterator = mappings->begin();
		while(iterator != mappings->end())
		{
			int paramId = iterator->first;
			MidiMapping midiMapping = iterator->second;
			
			float value = params[paramId]->getScaledValue();

			uint8_t midiValue = (uint8_t) floor(value * 127);

			if(midiCache.changed(paramId, midiValue)) {
				midi::Message m;
				if(midiMapping.type == MidiMapping::MIDI_CC) m.setStatus(0xb);
				if(midiMapping.type == MidiMapping::MIDI_NOTE) m.setStatus(0x9);
				m.setChannel(midiMapping.channel);
				m.setNote(midiMapping.cc);
				m.setValue(midiValue);
				midiIO->output.sendRawMessage(m);
				midiCache.updateCache(paramId, midiValue);
			}

			iterator++;
		}
	}
};

struct ParamMapButton : OpaqueWidget {
	DynamicLight* light;
	ParamMapCollection* paramMap = NULL;

	ParamMapButton() {
		box.size = Vec(12.f, 12.f);
		light = new DynamicLight;
		light->bgColor = nvgRGBA(0, 0, 0, 0);
		light->borderColor = SCHEME_BLUE;
		light->box.size = Vec(12.f, 12.f);
		light->setColor(SCHEME_BLUE);
		light->setBrightness(0.f);
		addChild(light);
	}

	void step() override {
		if(paramMap) {
			light->setBrightness(paramMap->isLearningEnabled() ? 1.f : 0.f);
		}
		OpaqueWidget::step();
	}

	void onButton(const event::Button& e) override{
		if(paramMap) {
			if (e.action == GLFW_PRESS) {
				if(paramMap->isLearningEnabled()) {
					paramMap->setLearning(false);
					paramMap->cancelLearning();
				}
				else {
					paramMap->setLearning(true);
				}
				e.consume(this);
			}
		}
		OpaqueWidget::onButton(e);
	}
};

template <typename TBase = KnobWhite32>
struct MappableParameter : TBase {
	using TBase::box;
	using TBase::addChild;
	using TBase::paramQuantity;

	int paramId;
	Module* module = NULL;
	HandleMapCollection* handleMap = NULL;
	MidiMapCollection* midiMap = NULL;

	TextTag* midiLabel;

	MappableParameter() {
		auto fontFileName = "res/fonts/Bebas-Regular.ttf";
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontFileName));
		midiLabel = new TextTag(font);
		midiLabel->setBackgroundColor(SCHEME_BLUE);
		midiLabel->setColor(nvgRGB(0x21,0x21,0x21));
		midiLabel->visible = false;
		addChild(midiLabel);
	}

	void step() override {
		if(handleMap && handleMap->isLearning(paramId)) {
			ParamWidget *touchedParam = APP->scene->rack->touchedParam;
			if (touchedParam && touchedParam->paramQuantity->module != module) {
				APP->scene->rack->touchedParam = NULL;
				int targetModuleId = touchedParam->paramQuantity->module->id;
				int targetParamId = touchedParam->paramQuantity->paramId;
				handleMap->commitLearn(paramId, targetModuleId, targetParamId);
				paramQuantity->setScaledValue(touchedParam->paramQuantity->getScaledValue());
			}
		}
		TBase::step();
	}

	void draw(const Widget::DrawArgs &args) override {
		TBase::draw(args);
		if(handleMap) {
			if(handleMap->isLearningEnabled() && handleMap->isAssigned(paramId)) {
				nvgFillColor(args.vg, SCHEME_YELLOW);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, box.size.x-5.f, box.size.y-5.f, 5.f, 5.f);
				nvgFill(args.vg);
			}
			if(handleMap->isLearningEnabled() ) {
				if(handleMap->isLearning(paramId)) {
					nvgStrokeColor(args.vg, SCHEME_YELLOW);
				}
				else {
					nvgStrokeColor(args.vg, nvgRGB(0xd7,0xd7,0xd7));
				}
				nvgStrokeWidth(args.vg, 1.0f);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
				nvgStroke(args.vg);
			}
		}

		if(midiMap) {
			if(midiMap->isLearningEnabled() && midiMap->isAssigned(paramId)) {
				uint8_t cc = midiMap->param2midi[paramId].cc;
				uint8_t channel = midiMap->param2midi[paramId].channel + 1;
				std::string assignText = std::to_string(channel) + "/" + std::to_string(cc);
				midiLabel->setText(assignText);
				midiLabel->visible = true;
			}
			else {
				midiLabel->visible = false;
			}
			if(midiMap->isLearningEnabled()) {
				if(midiMap->isLearning(paramId)) {
					nvgStrokeColor(args.vg, SCHEME_BLUE);
				}
				else {
					nvgStrokeColor(args.vg, nvgRGB(0xd7,0xd7,0xd7));
				}
				nvgStrokeWidth(args.vg, 1.0f);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
				nvgStroke(args.vg);
			}
		}
	}

	bool isInLearningMode() {
		if(handleMap && handleMap->isLearningEnabled()) {
			return true;
		}
		
		if(midiMap && midiMap->isLearningEnabled()) {
			return true;
		}

		return false;
	}

	void onDragStart(const event::DragStart& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			if(handleMap) {
				if(handleMap->isLearningEnabled() && handleMap->isAssigned(paramId)) {
					handleMap->unassign(paramId);
				}
			}
			if(midiMap) {
				if(midiMap->isLearningEnabled() && midiMap->isAssigned(paramId)) {
					midiMap->unassign(paramId);
				}
			}
			e.stopPropagating();
			e.consume(this);
		}
		if(! isInLearningMode()) TBase::onDragStart(e);	
	}

	void onButton(const event::Button& e) override{
		if(isInLearningMode()) {
			e.stopPropagating();
			e.consume(this);
		}
		TBase::onButton(e);	
	}

	void onSelect(const event::Select &e) override {
		if(midiMap) {
			if(midiMap->isLearningEnabled()) {
				midiMap->startLearning(paramId);
				e.consume(this);
			}
		}
		if(handleMap) {
			if(handleMap->isLearningEnabled()) {
				APP->scene->rack->touchedParam = NULL;
				handleMap->startLearning(paramId);
				e.consume(this);
			}
		}
		TBase::onSelect(e);
	}

	void onDeselect(const event::Deselect &e) override {
		if(midiMap) {
			if(midiMap->isLearningEnabled() && midiMap->isLearning(paramId)) {
				midiMap->cancelLearning();
				e.consume(this);
			}
		}
		TBase::onDeselect(e);
	}

	void onHover(const event::Hover &e) override {
		e.consume(this);
		TBase::onHover(e);
	}

	void onEnter(const event::Enter &e) override {
		if(handleMap) handleMap->touch(paramId);
		TBase::onEnter(e);
	}

	void onLeave(const event::Leave &e) override {
		if(handleMap) handleMap->untouch();
		TBase::onLeave(e);
	}
};
