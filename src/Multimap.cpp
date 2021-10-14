#include "23volts.hpp"
#include "common/mapping.hpp"
#include "common/midi.hpp"
#include "widgets/buttons.hpp"
#include "widgets/knobs.hpp"
#include "widgets/labels.hpp"
#include "widgets/lights.hpp"
#include "widgets/ports.hpp"

struct ParameterSnapshot {
	float values[16] = {};
};

struct Snapshots {
	std::vector<ParameterSnapshot*> snapshots;

	int size = 0;

	~Snapshots() {
		reset();
	}

	void reset() {
		size = 0;
		for(ParameterSnapshot* s : snapshots) {
			delete s;
		}	
		snapshots.clear();
	}

	void init(int size_ = 1) {
		for(int x = 0; x < size_; x++) {
			ParameterSnapshot* snapshot = new ParameterSnapshot;
			addSnapshot(snapshot);
		}
	}

	bool exists(int index) {
		return index < size;
	}

	void addSnapshot(ParameterSnapshot *snapshot) {
		snapshots.push_back(snapshot);
		size++;
	}

	void replaceSnapshot(int index, ParameterSnapshot* snapshot) {
		delete snapshots[index];
		snapshots[index] = snapshot;
	}

	json_t* toJson() {
		json_t *snapshotsJ = json_array();
		for (int x = 0; x < size; x++) {
			json_t* snapshotJ = json_array();
			for(int y = 0; y < 16; y++) {
				json_t* value = json_real(snapshots[x]->values[y]);
				json_array_append_new(snapshotJ, value);
			}
			json_array_append_new(snapshotsJ, snapshotJ);
		}
		return snapshotsJ;
	}

	void fromJson(json_t* rootJ) {
		reset();
		int bankSize = json_array_size(rootJ);
		for(int x = 0; x < bankSize; x++) {
			ParameterSnapshot* snapshot = new ParameterSnapshot;
			json_t* snapshotJ = json_array_get(rootJ, x);
			for(int y = 0; y < 16; y++) {
				float value = json_real_value(json_array_get(snapshotJ, y));
				snapshot->values[y] = value;
			}
			addSnapshot(snapshot);
		}
	}
};

struct Multimap : Module {
	static const int MAX_BANK = 128;

	enum KnobModes {
		KNOB_JUMP,
		KNOB_CATCH
	};
	enum ParamIds {
		BANK_INC_BUTTON,
		BANK_DEC_BUTTON,
		ENUMS(KNOBS, 16),
		NUM_PARAMS
	};
	enum InputIds {
		RESET_INPUT,
		BANK_INC_INPUT,
		BANK_DEC_INPUT,
		BANK_CV_INPUT,
		POLY_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		POLY_CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	MidiInputOutput midiIO;
	MidiMapCollection midiMap;
	MultiHandleMapCollection handleMap;

	MappingProcessor mappingProcessor;

	dsp::SchmittTrigger bankIncTrigger;
	dsp::SchmittTrigger bankDecTrigger;
	dsp::SchmittTrigger resetTrigger;
	int currentBankIndex = 0;
	Snapshots snapshots;

	Multimap() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		init();
	}

	void init() {
		mappingProcessor.midiIO = &midiIO;
		mappingProcessor.midiMap = &midiMap;
		mappingProcessor.handleMap = &handleMap;

		for(int paramId = 0; paramId < NUM_PARAMS; paramId++) {
			mappingProcessor.params[paramId] = paramQuantities[paramId];
			midiMap.parameterIds.push_back(paramId);
			handleMap.parameterIds.push_back(paramId);
		}

		outputs[POLY_CV_OUTPUT].channels = 0;
		handleMap.init(MAX_BANK);
		snapshots.init(MAX_BANK);
	}

	void process(const ProcessArgs& args) override {
		
		processTriggers();

		if(inputs[BANK_CV_INPUT].isConnected()) {
			float inputValue = math::clamp(inputs[BANK_CV_INPUT].getVoltage(), 0.f, 10.f);
			int index = (int) rescale(inputValue, 0.f, 10.f, 0.f, 127.f);
			if(index != currentBankIndex) {
				storeCurrentSnapshot();
				currentBankIndex = index;
				handleMap.loadPage(currentBankIndex);
				restoreSnapshot(index);
			}
		}

		if(inputs[POLY_CV_INPUT].isConnected()) {
			// If CV input is connected, it overrides the MIDI Input, but still
			// sends midi feedback, so it can be used to convert CV -> MIDI CC
			mappingProcessor.processMidiInput = false;

			int channels = inputs[POLY_CV_INPUT].getChannels();

			// TODO : SIMD
			for(int c = 0; c < channels; c++) {
				float inputValue = math::clamp(inputs[POLY_CV_INPUT].getVoltage(c), 0.f, 10.f);
				float value = rescale(inputValue, 0.f, 10.f, 0.f, 1.f);
				params[KNOBS+c].setValue(value);
			}
		}
		else {
			mappingProcessor.processMidiInput = true;
		}

		mappingProcessor.process(args.frame);

		if(outputs[POLY_CV_OUTPUT].isConnected()) {
			outputs[POLY_CV_OUTPUT].channels = 16;
			for(int c = 0; c < 16; c++) { 
				// TODO : SIMD
				float value = rescale(params[KNOBS+c].getValue(), 0.f, 1.f, 0.f, 10.f);
				outputs[POLY_CV_OUTPUT].setVoltage(value, c);
			}
		}
	}

	void processTriggers() {
		if(resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			onBankReset();
		}

		float incValue = fmaxf(inputs[BANK_INC_INPUT].getVoltage(), params[BANK_INC_BUTTON].getValue());
		if(bankIncTrigger.process(incValue)) {
			onBankIncrease();
		}

		float decValue = fmaxf(inputs[BANK_DEC_INPUT].getVoltage(), params[BANK_DEC_BUTTON].getValue());
		if(bankDecTrigger.process(decValue)) {
			onBankDecrease();
		}
	}

	void onBankReset() {
		storeCurrentSnapshot();
		currentBankIndex = 0;
		handleMap.loadPage(currentBankIndex);
		restoreSnapshot(currentBankIndex);
	}

	void onBankIncrease() {
		if(currentBankIndex + 1 < MAX_BANK) {
			storeCurrentSnapshot();
			currentBankIndex++;
			handleMap.next();
			if(snapshots.exists(currentBankIndex)) {
				restoreSnapshot(currentBankIndex);
			}
			else {
				resetParameters();
			}
		}
	}

	void onBankDecrease() {
		if(currentBankIndex > 0) {
			storeCurrentSnapshot();
			currentBankIndex--;
			handleMap.previous();
			restoreSnapshot(currentBankIndex);
		}
	}

	void restoreSnapshot(int bankIndex) {
		ParameterSnapshot* snapshot = snapshots.snapshots[bankIndex];
		for(int x = 0; x < 16; x++) {
			params[KNOBS+x].setValue(snapshot->values[x]);
		}
	}

	void storeCurrentSnapshot() {
		ParameterSnapshot* snapshot;
		if(snapshots.exists(currentBankIndex)) {
			snapshot = snapshots.snapshots[currentBankIndex];
		}
		else {
			snapshot = new ParameterSnapshot;		
			snapshots.addSnapshot(snapshot);
		}
		
		for(int x = 0; x < 16; x++) {
			snapshot->values[x] = params[KNOBS+x].getValue();
		}
	}

	void resetParameters() {
		for(int x = 0; x < 16; x++) {
			params[KNOBS+x].setValue(0.f);
		}
	}
	
	void onReset() override {
		handleMap.clear();
		handleMap.init(MAX_BANK);
		midiMap.clear();
		snapshots.reset();
		snapshots.init(MAX_BANK);
		resetParameters();
		currentBankIndex = 0;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "midi_io", midiIO.toJson());

		storeCurrentSnapshot();
		json_object_set_new(rootJ, "current_bank", json_integer(currentBankIndex));
		json_object_set_new(rootJ, "snapshots", snapshots.toJson());
		json_object_set_new(rootJ, "midi_map", midiMap.toJson());
		json_object_set_new(rootJ, "handle_map", handleMap.toJson());

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t* midiIOJ = json_object_get(rootJ, "midi_io");
		if (midiIOJ) midiIO.fromJson(midiIOJ);

		json_t* bankIndexJ = json_object_get(rootJ, "current_bank");
		if (bankIndexJ) currentBankIndex = json_integer_value(bankIndexJ);

		json_t* snapshotsJ = json_object_get(rootJ, "snapshots");
		if(snapshotsJ) {
			snapshots.fromJson(snapshotsJ);
			restoreSnapshot(currentBankIndex);
		}

		json_t* midiMapJ = json_object_get(rootJ, "midi_map");
		if(midiMapJ) midiMap.fromJson(midiMapJ);

		json_t* handleMapJ = json_object_get(rootJ, "handle_map");
		if(handleMapJ) {
			handleMap.fromJson(handleMapJ);
			handleMap.loadPage(currentBankIndex);
		}
	}
};

struct MultimapDisplay : OpaqueWidget {
	Multimap* module;
	NVGcolor backgroundColor;
	NVGcolor strokeColor;
	TextLabel* line1;
	TextLabel* line2;
	TextLabel* line3;

	MultimapDisplay(math::Vec size) {
		box.size = size;
		backgroundColor = nvgRGB(0x00, 0x00, 0x00);
		strokeColor = nvgRGB(0x21, 0x21, 0x21);

		auto fontFileName = "res/fonts/Bebas-Regular.ttf";
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontFileName));

		line1 = new TextLabel(font);
		line1->box.pos = Vec(2.f, 2.f);
		line1->box.size = Vec(this->box.size.x - 4.f, this->box.size.y / 2);
		std::string testText1 = "IN : Midi Input";
		line1->setText(testText1);
		line1->setFontSize(16.f);
		line1->setColor(SCHEME_BLUE);
		addChild(line1);

		line2 = new TextLabel(font);
		line2->box.pos = Vec(2.f, this->box.size.y / 3);
		line2->box.size = Vec(this->box.size.x - 4.f, this->box.size.y / 3);
		std::string testText2 = "OUT : Midi Output";
		line2->setFontSize(16.f);
		line2->setText(testText2);
		line2->setColor(SCHEME_BLUE);
		addChild(line2);

		line3 = new TextLabel(font);
		line3->box.pos = Vec(2.f, (this->box.size.y / 3) * 2 + 6);
		line3->box.size = Vec(this->box.size.x - 4.f, this->box.size.y / 3);
		std::string testText3 = "Bank 0";
		line3->centered = false;
		line3->setFontSize(12.f);
		line3->setText(testText3);
		line3->setColor(SCHEME_BLUE);
		addChild(line3);

		TextLabel* learn = new TextLabel(font);
		learn->box.pos = Vec(76, (this->box.size.y / 3) * 2 + 6);
		learn->box.size = Vec(30.f, 14.f);
		std::string learnText = "Learn";
		learn->setFontSize(12.f);
		learn->setText(learnText);
		learn->setColor(SCHEME_BLUE);
		addChild(learn);

		TextLabel* assign = new TextLabel(font);
		assign->box.pos = Vec(125.5f, (this->box.size.y / 3) * 2 + 6);
		assign->box.size = Vec(30.f, 14.f);
		std::string assignText = "Assign";
		assign->setFontSize(12.f);
		assign->setText(assignText);
		assign->setColor(SCHEME_YELLOW);
		addChild(assign);
	}

	void step() override {
		if(module) {
			// Retrieve touched parameter
			int touchId = -1;
			if(module->midiMap.touchedParamId > -1) {
				touchId = module->midiMap.touchedParamId;
			}

			if(module->handleMap.touchedParamId > -1) {
				touchId = module->handleMap.touchedParamId;
			}

			std::string strLine1;
			std::string strLine2;
			if(touchId > -1 && module->handleMap.isAssigned(touchId)) {
				line1->setColor(SCHEME_YELLOW);
				strLine1 = module->handleMap.getMap(touchId)->moduleName;
				line2->setColor(SCHEME_YELLOW);
				strLine2 = module->handleMap.getMap(touchId)->paramName;
			}
			else {
				line1->setColor(SCHEME_BLUE);
				int inDeviceId = module->midiIO.input.deviceId;
				strLine1 = inDeviceId > -1 ?
					"IN : " + module->midiIO.input.getDeviceName(inDeviceId)
					: "IN : (No device)";
				line2->setColor(SCHEME_BLUE);
				int outDeviceId = module->midiIO.output.deviceId;
				strLine2 = outDeviceId > -1 ? 
					"OUT : " + module->midiIO.output.getDeviceName(outDeviceId)
					: "OUT : (No device)";
			}

			line1->setText(strLine1);
			line2->setText(strLine2);

			std::string bankText = "Bank " + std::to_string(module->currentBankIndex);
			line3->setText(bankText);
		}

		OpaqueWidget::step();
	}

	void draw(const DrawArgs &args) override {
		nvgFillColor(args.vg, backgroundColor);
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 2.f);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
		nvgFill(args.vg);
		nvgStroke(args.vg);
		OpaqueWidget::draw(args);
	}
};

template <class TParamType>
struct MultimapWidget : ModuleWidget {
	MultimapWidget(Multimap* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Multimap.svg")));

		float displayYpos = 35;
		{
			MultimapDisplay* display = new MultimapDisplay(Vec(170, 65));
			display->box.pos = Vec(5,displayYpos);
			display->module = module;
			addChild(display);
		}
		{
			ParamMapButton* mapButton = new ParamMapButton();
			if(module) mapButton->paramMap = &module->midiMap;
			mapButton->box.pos = Vec(110,displayYpos + 50);
			addChild(mapButton);
		}

		{
			ParamMapButton* mapButton = new ParamMapButton();
			if(module) mapButton->paramMap = &module->handleMap;
			mapButton->box.pos = Vec(160,displayYpos + 50);
			mapButton->light->borderColor = SCHEME_YELLOW;
			mapButton->light->setColor(SCHEME_YELLOW);
			addChild(mapButton);
		}

		auto fontFileName = "res/fonts/Bebas-Regular.ttf";
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontFileName));
		
		{
			MappableParameter<MomentaryTextLightButton>* button = createParam<MappableParameter<MomentaryTextLightButton>>(
				Vec(42, displayYpos + 50), 
				module, 
				Multimap::BANK_DEC_BUTTON);
			button->font = font;
			button->text = "<";
			button->paramId = Multimap::BANK_DEC_BUTTON;
			if(module) {
				button->midiMap = &module->midiMap;
			}
			addParam(button);
		}
		{
			MappableParameter<MomentaryTextLightButton>* button = createParam<MappableParameter<MomentaryTextLightButton>>(
				Vec(57, displayYpos + 50), 
				module, 
				Multimap::BANK_INC_BUTTON);
			button->font = font;
			button->text = ">";
			button->paramId = Multimap::BANK_INC_BUTTON;
			if(module) {
				button->midiMap = &module->midiMap;
			}
			addParam(button);
		}
		
		float paramYpos = 160.f;
		for(int x = 0; x < 4; x++) {
			createMappableParam<TParamType>(Vec(15, paramYpos + x * 40), Multimap::KNOBS + x * 4);
			createMappableParam<TParamType>(Vec(55, paramYpos + x * 40), Multimap::KNOBS + x * 4 + 1);
			createMappableParam<TParamType>(Vec(95, paramYpos + x * 40), Multimap::KNOBS + x * 4 + 2);
			createMappableParam<TParamType>(Vec(135, paramYpos + x * 40), Multimap::KNOBS + x * 4 + 3);
		}
		
		float inputXpos = 50.5f;
		float inputYpos = 110.5f;
		addInput(createInput<SmallPort>(Vec(0 + inputXpos, inputYpos), module, Multimap::RESET_INPUT));
		addInput(createInput<SmallPort>(Vec(34 + inputXpos, inputYpos), module, Multimap::BANK_DEC_INPUT));
		addInput(createInput<SmallPort>(Vec(68 + inputXpos, inputYpos), module, Multimap::BANK_INC_INPUT));
		addInput(createInput<SmallPort>(Vec(102 + inputXpos, inputYpos), module, Multimap::BANK_CV_INPUT));

		addInput(createInput<PolyLightPort<16>>(Vec(32, 328), module, Multimap::POLY_CV_INPUT));
		addOutput(createOutput<PolyLightPort<16>>(Vec(128, 328), module, Multimap::POLY_CV_OUTPUT));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
	
	template <class TParamWidget>
	void createMappableParam(math::Vec pos, int paramId) {
		Multimap* module = dynamic_cast<Multimap*>(this->module);
		MappableParameter<TParamWidget>* param = createParam<MappableParameter<TParamWidget>>(pos, module, paramId);
		param->paramId = paramId;
		if(module) {
			param->module = module;
			param->handleMap = &module->handleMap;
			param->midiMap = &module->midiMap;
		}
		addParam(param);
	}

	template <class TParamWidget>
	void createMidiParam(math::Vec pos, int paramId) {
		Multimap* module = dynamic_cast<Multimap*>(this->module);
		MappableParameter<TParamWidget>* param = createParam<MappableParameter<TParamWidget>>(pos, module, paramId);
		param->paramId = paramId;
		if(module) {
			param->midiMap = &module->midiMap;
		}
		addParam(param);
	}

	void appendContextMenu(Menu* menu) override {
		Multimap* module = dynamic_cast<Multimap*>(this->module);

		menu->addChild(new MenuSeparator);

		if(module) {
			MidiMenuBuilder menuBuilder;
			menuBuilder.channel = false;
			menuBuilder.build(menu, &module->midiIO);
		}
	}
};

Model* modelMultimapK = createModel<Multimap, MultimapWidget<KnobWhite32>>("MultimapK");
Model* modelMultimapS = createModel<Multimap, MultimapWidget<LedSwitch32>>("MultimapS");