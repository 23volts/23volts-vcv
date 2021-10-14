#include "23volts.hpp"
#include "common/midi.hpp"
#include "widgets/buttons.hpp"
#include "widgets/knobs.hpp"
#include "widgets/labels.hpp"
#include "widgets/lights.hpp"
#include "widgets/ports.hpp"

struct MidiPC : Module {

	enum ParamIds {
		PROGRAM_KNOB,
		SEND_PC_BUTTON,
		SEND_ON_LOAD_BUTTON,
		NUM_PARAMS
	};
	enum InputIds {
		PROGRAM_CV_INPUT,
		SEND_PC_TRIG,
		NUM_INPUTS
	};
	enum OutputIds {
		PROGRAM_CV_OUTPUT,
		CHANGE_TRIG_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		NUM_LIGHTS
	};

	enum MidiPorts {
		MIDI_INPUT,
		MIDI_OUTPUT
	};

	MidiInputOutput midiIO;

	bool midiConnected[2] = {false};
	bool midiActivity[2] = {false};

	uint8_t targetProgram = 0x00;
	uint8_t currentProgram = 0x00;

	bool sendOnLoad = false;

	dsp::ClockDivider midiDivider;
	
	dsp::PulseGenerator pcPulse;
	dsp::PulseGenerator midiInputPulse;
	dsp::PulseGenerator midiOutputPulse;

	dsp::SchmittTrigger sendPCTrigger;
	
	MidiPC() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PROGRAM_KNOB, 0.0f, 127.0f, 0.f, "MIDI Program");
		midiDivider.setDivision(32);
	}

	void process(const ProcessArgs& args) override {

		if(inputs[PROGRAM_CV_INPUT].isConnected()) {
			float inputValue = clamp(inputs[PROGRAM_CV_INPUT].getVoltage(), 0.f, 10.f);
			targetProgram = (uint8_t) rescale(inputValue, 0.f, 10.f, 0.f, 127.f);
		}
		else {
			targetProgram = (uint8_t) params[PROGRAM_KNOB].getValue();
		}

		if(sendPCTrigger.process(fmax(params[SEND_PC_BUTTON].getValue(), inputs[SEND_PC_TRIG].getVoltage()))) {
			sendProgram(targetProgram);
			pcPulse.trigger();
		}
		
		if(midiDivider.process()) {
			updateMidiStatus();
			if(midiIO.input.isConnected()) {
				midi::Message msg;
				while (midiIO.input.tryPop(&msg, args.frame)) {
					processMessage(msg);
				}
			}
		}

		outputs[CHANGE_TRIG_OUTPUT].setVoltage(pcPulse.process(1) ? 10.f : 0.f);

		if(outputs[PROGRAM_CV_OUTPUT].isConnected()) {
			float outputValue = rescale(currentProgram, 0.f, 127.f, 0.f, 10.f);
			outputs[PROGRAM_CV_OUTPUT].setVoltage(outputValue);
		}
	}
	
	void updateMidiStatus() {
		midiConnected[MIDI_INPUT] = midiIO.input.isConnected();
		midiConnected[MIDI_OUTPUT] = midiIO.output.isConnected();
		midiActivity[MIDI_INPUT] = midiInputPulse.process(1);
		midiActivity[MIDI_OUTPUT] = midiOutputPulse.process(1);
	}

	void processMessage(midi::Message msg) {
		if(msg.getStatus() == 0xC) {
			uint8_t program = msg.getNote();
			setProgram(program);
			pcPulse.trigger();
			midiInputPulse.trigger(100.f);
		}
	}

	void setProgram(uint8_t program) {
		currentProgram = program;
	}

	void sendProgram(uint8_t program) {
		setProgram(program);
		midi::Message m;
		m.setStatus(0xC);
		m.setNote(program);
		m.setValue(0x0);
		midiIO.output.sendMessage(m);
		midiOutputPulse.trigger(1000.f);
	}

	void onReset() override {
		midiIO.reset();
		currentProgram = 0x00;
		targetProgram = 0x00;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "midi_io", midiIO.toJson());
		json_object_set_new(rootJ, "current_program", json_integer(currentProgram));
		json_object_set_new(rootJ, "target_program", json_integer(targetProgram));
		json_object_set_new(rootJ, "send_on_load", json_boolean(params[SEND_ON_LOAD_BUTTON].getValue()));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t* midiJ = json_object_get(rootJ, "midi_io");
		if (midiJ)
			midiIO.fromJson(midiJ);
		json_t* currentProgramJ = json_object_get(rootJ, "current_program");
		if(currentProgramJ) currentProgram = json_integer_value(currentProgramJ);
		json_t* targetProgramJ = json_object_get(rootJ, "target_program");
		if(targetProgramJ) targetProgram = json_integer_value(targetProgramJ);
		json_t* sendOnLoadJ = json_object_get(rootJ, "send_on_load");
		bool restoreProgram = false;
		if(sendOnLoadJ) restoreProgram = json_boolean_value(sendOnLoadJ);
		if(restoreProgram) {
			sendProgram(currentProgram);
		}
	}
};

struct MidiActivityWidget : TransparentWidget {
	MidiPC* module;
	DynamicLight* light;
	int portIndex = -1;

	MidiActivityWidget() {
		light = new DynamicLight;
		light->setColor(SCHEME_BLUE);
		light->setBrightness(1.0f);
		light->box.size = Vec(5,5);
		light->box.pos = Vec(2,2);
		addChild(light);
	}

	void step() override {
		if(module && portIndex > -1) {
			if(module->midiConnected[portIndex]) {
				light->setBrightness(1.f);
				light->setColor(SCHEME_BLUE);
				if(module->midiActivity[portIndex]) {
					light->setColor(SCHEME_GREEN);
				}
			}	
			else {
				light->setBrightness(0.f);
			}
		}
		TransparentWidget::step();
	}
};

struct MidiPCWidget : ModuleWidget {
	LCDLabel* currentLabel;
	LCDLabel* targetLabel;

	MidiPCWidget(MidiPC* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Midi-PC.svg")));

		MidiActivityWidget* midiInLight = new MidiActivityWidget;
		midiInLight->module = module;
		midiInLight->portIndex = MidiPC::MIDI_INPUT;
		midiInLight->box.pos = Vec(16,66);
		midiInLight->box.size = Vec(10,10);
		midiInLight->light->bgColor = nvgRGBA(0, 0, 0, 30);
		addChild(midiInLight);

		MidiActivityWidget* midiOutLight = new MidiActivityWidget;
		midiOutLight->module = module;
		midiOutLight->portIndex = MidiPC::MIDI_OUTPUT;
		midiOutLight->box.pos = Vec(43,66);
		midiOutLight->box.size = Vec(10,10);
		midiOutLight->light->bgColor = nvgRGBA(0, 0, 0, 30);
		addChild(midiOutLight);

		std::string defaultText = "127";
		currentLabel = new LCDLabel(Vec(46, 18));
		currentLabel->box.pos = Vec(7, 95);
		currentLabel->setText(defaultText);
		currentLabel->setLabelPos(Vec(0,2));
		currentLabel->setFontSize(16.f);
		currentLabel->setColor(SCHEME_ORANGE_23V);
		addChild(currentLabel);

		targetLabel = new LCDLabel(Vec(46, 18));
		targetLabel->box.pos = Vec(7, 185);
		targetLabel->setText(defaultText);
		targetLabel->setLabelPos(Vec(0, 2));
		targetLabel->setFontSize(16.f);
		targetLabel->setColor(SCHEME_BLUE);
		addChild(targetLabel);

		addInput(createInput<SmallPort>(Vec(34.f, 141.f), module, MidiPC::SEND_PC_TRIG));
		MomentaryLedButton* button = createParam<MomentaryLedButton>(Vec(34.5f, 115.5f), module, MidiPC::SEND_PC_BUTTON);
		button->light->box.size = Vec(18,18);
		addParam(button);

		addInput(createInput<SmallPort>(Vec(5.f, 212.f), module, MidiPC::PROGRAM_CV_INPUT));
		addParam(createParam<KnobDark26>(Vec(31.f, 208.f), module, MidiPC::PROGRAM_KNOB));

		addParam(createParam<LedButton>(Vec(41.f, 264.f), module, MidiPC::SEND_ON_LOAD_BUTTON));
		
		addOutput(createOutput<PJ301MPort>(Vec(5.f, 312), module, MidiPC::PROGRAM_CV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(31.f, 312), module, MidiPC::CHANGE_TRIG_OUTPUT));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}

	void appendContextMenu(Menu* menu) override {
		MidiPC* module = dynamic_cast<MidiPC*>(this->module);
		menu->addChild(new MenuSeparator);
		if(module) {
			MidiMenuBuilder menuBuilder;
			menuBuilder.build(menu, &module->midiIO);
		}
	}

	void updateText() {
		MidiPC* module = dynamic_cast<MidiPC*>(this->module);

		std::string programText = std::to_string(module->currentProgram);
		currentLabel->setText(programText);

		std::string targetText = std::to_string(module->targetProgram);
		targetLabel->setText(targetText);
	}

	void step() override {
		if(module) {
			updateText();
		}
		ModuleWidget::step();
	}
};

Model* modelMidiPC = createModel<MidiPC, MidiPCWidget>("MidiPC");