#include "23volts.hpp"
#include "helpers.hpp"
#include "widgets/buttons.hpp"
#include "widgets/labels.hpp"
#include "widgets/knobs.hpp"
#include "widgets/ports.hpp"
#include "M8.hpp"

struct ClockFollower {

	static const int CLOCK_DRIFT_THRESEHOLD = 5;
	static const int CLOCK_STOP_THRESEHOLD = 1000;

	int m_samplerate;
	bool isRunning = false;
	bool isLearning = true;
	bool isTicking = false;

	uint64_t m_sampleCounter = 0;
	uint64_t m_lastClockTick = 0;
	uint64_t m_duration = 0;
	uint64_t m_nextExpectedClockTick = 0;
	uint64_t m_overdueCounter = 0;

	void tick() {
		isTicking = true;

		if(isRunning) {
			m_duration = m_sampleCounter - m_lastClockTick;
			isLearning = false;
			m_nextExpectedClockTick = m_sampleCounter + m_duration;
			m_overdueCounter = 0;
		}
		else {
			isRunning = true;
			// Is it a first start ?
			if(m_duration == 0) {
				start();
			}
			else {
				restart();
			}
		}

		m_lastClockTick = m_sampleCounter;
	}

	void start() {
		isLearning = true;
	}

	void restart() {
		m_nextExpectedClockTick = m_duration;
		m_nextExpectedClockTick = m_sampleCounter + m_duration;
		m_overdueCounter = 0;
	}

	void step() {
		isTicking = false; // We need to reinit this at each step
		m_sampleCounter++;

		if(isRunning && m_sampleCounter > m_nextExpectedClockTick ) {
			m_overdueCounter++;

			if(m_overdueCounter > CLOCK_STOP_THRESEHOLD && ! isLearning) {
				stop();
			}
		}
	}

	void stop() {
		isRunning = false;
	}

	void updateSamplerate(float samplerate) {
		// Update counters
		m_samplerate = samplerate;
	}

	uint64_t getDuration() {
		return m_duration;
	}

	json_t *toJson() {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "is_running", json_boolean(isRunning));
		json_object_set_new(rootJ, "is_learning", json_boolean(isRunning));
		json_object_set_new(rootJ, "is_ticking", json_boolean(isRunning));
		json_object_set_new(rootJ, "sample_counter", json_integer(m_sampleCounter));
		json_object_set_new(rootJ, "last_clock_tick", json_integer(m_lastClockTick));
		json_object_set_new(rootJ, "duration", json_integer(m_duration));
		json_object_set_new(rootJ, "next_expected_tick", json_integer(m_nextExpectedClockTick));
		json_object_set_new(rootJ, "overdue_counter", json_integer(m_overdueCounter));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		isRunning = json_is_true(json_object_get(rootJ, "is_running"));
		isLearning = json_is_true(json_object_get(rootJ, "is_learning"));
		isTicking = json_is_true(json_object_get(rootJ, "is_ticking"));
		m_sampleCounter = json_integer_value(json_object_get(rootJ, "sample_counter"));
		m_lastClockTick = json_integer_value(json_object_get(rootJ, "last_clock_tick"));
		m_duration = json_integer_value(json_object_get(rootJ, "duration"));
		m_nextExpectedClockTick = json_integer_value(json_object_get(rootJ, "next_expected_tick"));
		m_overdueCounter = json_integer_value(json_object_get(rootJ, "overdue_counter"));
	}
};

struct ClockModulator {

	static const int TRIGGER_LENGTH = 10;

	enum pulseModes {
		MODE_GATE,
		MODE_TRIGGER,
	};

	int pulseMode = MODE_GATE;

	bool isRunning = false;

	float m_samplerate;
	float m_ratio = 1.f;

	std::string ratioLitteral = "x1";

	uint64_t m_sampleCounter = 0; // Since last reset which will be t=0
	uint64_t m_nextpulse = 0;

	uint64_t m_tick_counter = 0; // Number of tick received since last reset
	uint64_t m_lastClockTick = 0; // last tick index, in samples

	float m_reset_tick = 0.f; // Tick index of received reset

	dsp::PulseGenerator clockPulse;
	ClockFollower* m_clockFollower;

	bool setRatio(float ratio) {
		bool ratioChanged = false;
		if(ratio != m_ratio) {
			// Stop current pulse if the ratio is faster, so we won't miss intended pulses
			if(ratio > m_ratio) {
				clockPulse.reset();
			}

			m_ratio = ratio;
			computeNextPulse();	
			ratioChanged = true;
		}
		return ratioChanged;
	}

	void setRatioLiterral(std::string ratio) {
		ratioLitteral = ratio;
	}

	float getRatio() const {
		return m_ratio;
	}

	void tick() {
		if(! isRunning) {
			reset();
		}

		m_tick_counter++;
		m_lastClockTick = m_sampleCounter;
		
		int tickIndex = m_tick_counter - 1;
		float elapsedDivisions = getElapsedDivisions(tickIndex);
		
		if(! hasDecimals(elapsedDivisions)) {
			m_nextpulse = m_sampleCounter;
		}
		else {
			computeNextPulse();
		}
	}

	float divisionDuration() {
		return 1.f / m_ratio;
	}

	// Return the number of divisions that have happened at given tick
	float getElapsedDivisions(int tickIndex) {
		return tickIndex / divisionDuration();
	}

	// Return the number of ticks to next division from tickIndex
	float getRemainingDivisionTick(int tickIndex) {
		float elapsedDivisions = getElapsedDivisions(tickIndex);
		float difference = elapsedDivisions - (int) elapsedDivisions;
		return divisionDuration() - difference * divisionDuration();
	}

	void computeNextPulse() {
		uint64_t tickDuration = m_clockFollower->getDuration();
		uint64_t duration = getIntervalDuration();
		if(duration <= 0 ||m_ratio == 1.f) return;

		float remaining = getRemainingDivisionTick(m_tick_counter - 1);

		if(remaining < 1) {
			// Calculate first pulse, in samples relative to the lastTick
			uint64_t firstPulse = m_lastClockTick + std::floor(remaining * tickDuration);

			if(m_sampleCounter < firstPulse) {
				m_nextpulse = firstPulse;
			}
			else {
				int elapsed = (m_sampleCounter - firstPulse) / duration;
				m_nextpulse = firstPulse + ((elapsed + 1) * duration);
			}
		}
		else {
			m_nextpulse = 0; // Just wait for next tick to pulse/compute
		}
	}

	void setClockFollower(ClockFollower* clockFollower) {
		m_clockFollower = clockFollower;
		computeNextPulse();
	}

	void reset() {
		// Reset 1st clock tick and output a new pulse
		m_sampleCounter = 0;
		m_nextpulse = 0;
		m_tick_counter = 0;
		isRunning = true;
		if(! m_clockFollower->isLearning) {
			nextPulse();
		}
	}

	void setPulseMode(int mode) {
		if(pulseMode == MODE_GATE && mode == MODE_TRIGGER) {
			clockPulse.reset();
		} 
		pulseMode = mode;
	}

	void nextPulse() {
		clockPulse.trigger(getPulseDuration());
	}

	uint64_t getIntervalDuration() {
		return std::floor((m_clockFollower->getDuration() * (1.f / m_ratio)));
	}

	uint64_t getPulseDuration() {
		return pulseMode == MODE_GATE ? std::floor(getIntervalDuration() / 2.f) : TRIGGER_LENGTH;
	}

	// Step one sample, must be called at each process() call of the module
	void step() {
		m_sampleCounter++;

		if(m_clockFollower->isTicking) {
			tick();
		}
		isRunning = m_clockFollower->isRunning;

		if(m_clockFollower->isRunning && m_sampleCounter == m_nextpulse) {
			nextPulse();
			computeNextPulse();
		}
	}

	float getValue() {
		return clockPulse.process(1.0f) ? 10.f : 0.f;
	}

	void updateSamplerate(float samplerate) {
		// Update counters
		m_samplerate = samplerate;
	}


	json_t *toJson() {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "is_running", json_boolean(isRunning));
		json_object_set_new(rootJ, "ratio", json_real(m_ratio));
		json_object_set_new(rootJ, "reset_tick", json_real(m_reset_tick));
		json_object_set_new(rootJ, "ratio_litteral", json_string(ratioLitteral.c_str()));
		json_object_set_new(rootJ, "sample_counter", json_integer(m_sampleCounter));
		json_object_set_new(rootJ, "next_pulse", json_integer(m_nextpulse));
		json_object_set_new(rootJ, "tick_counter", json_integer(m_tick_counter));
		json_object_set_new(rootJ, "last_clock_tick", json_integer(m_lastClockTick));

		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		isRunning = json_is_true(json_object_get(rootJ, "is_running"));
		m_ratio = json_real_value(json_object_get(rootJ, "ratio"));
		m_reset_tick = json_real_value(json_object_get(rootJ, "reset_tick"));
		ratioLitteral = json_string_value(json_object_get(rootJ, "ratio_litteral"));
		m_sampleCounter = json_integer_value(json_object_get(rootJ, "sample_counter"));
		m_nextpulse = json_integer_value(json_object_get(rootJ, "next_pulse"));
		m_tick_counter = json_integer_value(json_object_get(rootJ, "tick_counter"));
		m_lastClockTick = json_integer_value(json_object_get(rootJ, "last_clock_tick"));
	}
};

struct ClockM8 : M8Module {

	static const int POLY_CHANNELS = 16;

	enum ParamIds {
		ATTENUATOR_KNOB,
		MAIN_KNOB,
		BINARY_BUTTON,
		TRIPLETS_BUTTON,
		ODD_BUTTON,
		DOTTED_BUTTON,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		MOD_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CLOCK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float samplerate;

	dsp::ClockDivider connectionUpdater;
	dsp::ClockDivider channelUpdater;
	dsp::ClockDivider ratioUpdater;

	bool clockInputConnected = false;
	bool modInputConnected = false;
	bool resetInputConnected = false;
	bool clockOutputConnected = false;

	bool displayNeedsUpdate = true;

	ClockModulator clockModulators[M8_POLY_CHANNELS];
	ClockFollower clockFollowers[M8_POLY_CHANNELS];

	float parameters[POLY_CHANNELS]; // Added for expanders
	float modulations[POLY_CHANNELS];

	int clockChannels = -1; 
	int modChannels = -1; 
	int resetChannels = -1; 
	int outputChannels = -1;

	dsp::SchmittTrigger resetTriggers[M8_POLY_CHANNELS];
	dsp::SchmittTrigger clockTriggers[M8_POLY_CHANNELS];

	bool quantizeBinary = true;
	bool quantizeTernary = false;
	bool quantizeOdd = false;
	bool quantizeDotted = true;

	const float binaryValues[9] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
	const float ternaryValues[7] = {3, 6, 12, 24, 48, 96, 192};
	const float dottedBinaryMultiplicators[1] = {1.f/ (3.f/4.f)};
	const float dottedTernaryMultiplicators[2] = {1.5f, 2};
	const float dottedOddMultiplicators[1] = {2.5f};
	const float dottedBinaryDividers[8] = {1.5f, 3, 6, 12, 24, 48, 96, 192};
	const float dottedTernaryDividers[1] = {2.5f}; 
	const float oddValues[8] = {5,7,9,10,11,13,14,15};
	int BINARY_VALUES_COUNT = 9;
	int TERNARY_VALUES_COUNT = 7;
	int DOTTED_BIN_MULT_COUNT = 1;
	int DOTTED_TER_MULT_COUNT = 2;
	int DOTTED_ODD_MULT_COUNT = 1;
	int DOTTED_BIN_DIV_COUNT = 8;
	int DOTTED_TER_DIV_COUNT = 1;
	int ODD_VALUES_COUNT = 8;

	int outputMode = ClockModulator::MODE_GATE;

	std::vector<float> currentMultiplicators;
	std::vector<float> currentDividers;


	ClockM8() {

		moduleType = M8_CLOCK_SYSTEM;

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MAIN_KNOB, -10.0, 10.0, 0.0, string::f("Divide/Multiply"));
		configParam(ATTENUATOR_KNOB, -1.0, 1.0, 0.0, string::f("Mod Attenuverter"));

		configParam(BINARY_BUTTON, 0.0f, 1.0f, 1.0f, "Binary ratios");
		configParam(TRIPLETS_BUTTON, 0.0f, 1.0f, 1.0f, "Triplets ratios");	
		configParam(DOTTED_BUTTON, 0.0f, 1.0f, 1.0f, "Dotted ratios");	
		configParam(ODD_BUTTON, 0.0f, 1.0f, 1.0f, "Odd ratios");	

		channelUpdater.setDivision(32);
		ratioUpdater.setDivision(512);
		connectionUpdater.setDivision(128);

		samplerate = APP->engine->getSampleRate();
		onSampleRateChange();	
		updateCurrentRatios();

		for(int x = 0; x < M8_POLY_CHANNELS; x++) {
			channelLitterals[x] = &clockModulators[x].ratioLitteral;
			channelControls[x] = 0.0f;
		} 
	}

	void process(const ProcessArgs& args) override {
		updateExpanders();

		if(connectionUpdater.process()) updateConnections();
		if(channelUpdater.process()) updateChannels();
		if(ratioUpdater.process()) updateRatios();

		if(rightLinkActive && statusMessage->moduleType == M8_EXPANDER) {
			if(controlMessage->bankA == true) {
				for(int x = 0; x < 8; x++) {
					//
				}
			}
			if(controlMessage->bankB == true) {
				for(int x = 8; x < 16; x++) {

				}
			}
		}

		// The number of outputs channel will be defined by the number
		// of channels from the 3 inputs : Clock, Reset, Mod. Whichever
		// is the biggest defines the number of outputs. Missing voices
		// are copied from the last voice of the input

		// The number of active clock followers is defined by the number
		// of clock input channels. 
		// 
		// The number of active clock dividers is defined by the number
		// of clock output channels.

		
		for(int c = 0; c < outputChannels; c++) {
			// Process resets. 
			int r = c < resetChannels ? c : resetChannels - 1;

			if(resetTriggers[r].process(inputs[RESET_INPUT].getVoltage(r))) {
				clockModulators[c].reset();
			}

			int m = c < modChannels ? c : modChannels - 1;

			// Return an offseted value of the main knob
			float offset = getModulatedParameter(m);

			float ratio = getQuantizedRatio(offset);
			
			if(clockModulators[c].setRatio(ratio)) { 
				std::string litteral = getQuantizedRatioLitteral(offset);
				clockModulators[c].setRatioLiterral(litteral);
			}
		}

		// Step clock followers
		for(int c = 0; c < clockChannels; c++) {
			clockFollowers[c].step();

			if(clockTriggers[c].process(inputs[CLOCK_INPUT].getVoltage(c))) {
				clockFollowers[c].tick();
			}
		}

		// Process Outputs
		for(int c = 0; c < outputChannels; c++) {
			clockModulators[c].step();
			outputs[CLOCK_OUTPUT].setVoltage(clockModulators[c].getValue(), c);
		}


		if(rightLinkActive) {
			for(int c = 0; c < outputChannels; c++) {
				channelOutputs[c] = clockModulators[c].getValue();
			}
			sendStatusMessage();
		}
	}

	float getModulatedParameter(int modulationChannel) {
		float knobValue = params[MAIN_KNOB].getValue();
		float attenuatedModulation = getAttenuatedModulation(modulationChannel);
		return clamp(knobValue + attenuatedModulation, -10.f, 10.f);
	}

	float getAttenuatedModulation(int modulationChannel) {
		if(modulationChannel > (modChannels +1)) {
			return 0.f;
		} 

		float modulation = inputs[MOD_CV_INPUT].getVoltage(modulationChannel);
		float attenuation = params[ATTENUATOR_KNOB].getValue();
		return modulation * attenuation;
	}


	float getQuantizedRatio(float offset) {
		float ratio = getRawRatio(offset);

		if(offset >= 0.f) {
			return ratio;
		}
		else {
			return 1.f / ratio;
		}
	}

	std::string getQuantizedRatioLitteral(float offset) {
		float ratio = getRawRatio(offset);
		std::string ratioStr = hasDecimals(ratio) ? roundedLitteral(ratio, 2) : std::to_string((int)ratio) ;
		std::string litteral = ratio == 1 ? "x1" : offset >= 0.f ? "x" + ratioStr : "/" + ratioStr;
		return litteral;
	}

	std::string roundedLitteral(float ratio, int precision) {
		return std::to_string(ratio).substr(0, std::to_string(ratio).find(".") + precision + 1);
	}

	// Returns raw ratio for the offset
	float getRawRatio(float offset) {
		int index = getRatioIndex(offset);
		return offset >= 0 ? currentMultiplicators[index] : currentDividers[index];
	}

	int getRatioIndex(float offset) {
		float value = abs(offset);
		int size = offset >= 0 ? currentMultiplicators.size() : currentDividers.size();
		return value > 0 ? std::ceil((value / 10.f) * size) - 1 : 0;
	}

	void updateConnections() {
		clockInputConnected = inputs[CLOCK_INPUT].isConnected();
		modInputConnected = inputs[MOD_CV_INPUT].isConnected();	
		resetInputConnected = inputs[RESET_INPUT].isConnected();
		clockOutputConnected = outputs[CLOCK_OUTPUT].isConnected();
	}

	void updateChannels() {
		bool clockInputChanged = false;
		int newClockChannels = clockInputConnected ? inputs[CLOCK_INPUT].getChannels() : -1;
		if(newClockChannels != clockChannels) clockInputChanged = true;
		clockChannels = newClockChannels;

		int oldNumberOfOutputs = outputChannels;
		bool outputChanged = false;
		resetChannels = resetInputConnected ? inputs[RESET_INPUT].getChannels() : -1;
		modChannels = modInputConnected ? inputs[MOD_CV_INPUT].getChannels() : -1;
		outputChannels = clockChannels;
		if(clockChannels > 0 && resetChannels > outputChannels) outputChannels = resetChannels;
		if(clockChannels > 0 && modChannels > outputChannels) outputChannels = modChannels;

		if(oldNumberOfOutputs != outputChannels) outputChanged = true;

		// Once we have the correct number of channels for the current state
		// we can link the ClockFollowers to the corresponding ClockModulators
		
		if(clockInputChanged ||outputChanged) {
			for(int o = 0; o < outputChannels; o++) {
				// Process resets. 
				int c = o < clockChannels ? o : clockChannels - 1;
				clockModulators[o].setClockFollower(&clockFollowers[c]);
			}
		}

		if(clockOutputConnected) {
			outputs[CLOCK_OUTPUT].channels = outputChannels > 0 ? outputChannels : 1;
		}
	}	

	void updateRatios() {
		// Poll parameters and compare them with current module state. 
		// If change happened update the ratio list

		bool ratioChanged = false;

		if((bool) params[BINARY_BUTTON].getValue() != quantizeBinary) {
			quantizeBinary = (bool) params[BINARY_BUTTON].getValue();
			ratioChanged = true;
		}

		if((bool) params[TRIPLETS_BUTTON].getValue() != quantizeTernary) {
			quantizeTernary = (bool) params[TRIPLETS_BUTTON].getValue();
			ratioChanged = true;
		}

		if((bool) params[DOTTED_BUTTON].getValue() != quantizeDotted) {
			quantizeDotted = (bool) params[DOTTED_BUTTON].getValue();
			ratioChanged = true;
		}

		if((bool) params[ODD_BUTTON].getValue() != quantizeOdd) {
			quantizeOdd = (bool) params[ODD_BUTTON].getValue();
			ratioChanged = true;
		}

		if(ratioChanged) updateCurrentRatios();
	}

	void updateCurrentRatios() {
		currentDividers.clear();
		currentMultiplicators.clear();

		if(quantizeBinary) {
			for(int i=0; i<BINARY_VALUES_COUNT; i++) {
				currentDividers.push_back(binaryValues[i]);
				currentMultiplicators.push_back(binaryValues[i]);
			}
			// Add ODD and ternary dividers ?
		}
		if(quantizeTernary) {
			for(int i=0; i<TERNARY_VALUES_COUNT; i++) {
				currentDividers.push_back(ternaryValues[i]);
				currentMultiplicators.push_back(ternaryValues[i]);
			}
			// Add odd and binary dividers ?
		}
		if(quantizeDotted && quantizeBinary) {
			for(int i=0; i<DOTTED_BIN_MULT_COUNT; i++) {
				currentMultiplicators.push_back(dottedBinaryMultiplicators[i]);
			}
			for(int i=0; i<DOTTED_BIN_DIV_COUNT; i++) {
				currentDividers.push_back(dottedBinaryDividers[i]);
			}
		}
		if(quantizeDotted && quantizeTernary) {
			for(int i=0; i<DOTTED_TER_MULT_COUNT; i++) {
				currentMultiplicators.push_back(dottedTernaryMultiplicators[i]);
			}
			for(int i=0; i<DOTTED_TER_DIV_COUNT; i++) {
				currentDividers.push_back(dottedTernaryDividers[i]);
			}
		}
		if(quantizeDotted && quantizeOdd) {
			for(int i=0; i<DOTTED_ODD_MULT_COUNT; i++) {
				currentMultiplicators.push_back(dottedOddMultiplicators[i]);
			}
		}

		if(quantizeOdd) {
			for(int i=0; i<ODD_VALUES_COUNT; i++) {
				currentDividers.push_back(oddValues[i]);
				currentMultiplicators.push_back(oddValues[i]);
			}
		}

		// Always have X1 even if no quantization is selected
		currentDividers.push_back(1.f);
		currentMultiplicators.push_back(1.f);

		// remove duplicate elements
    	std::sort(currentDividers.begin(), currentDividers.end());
    	auto lastDivider = std::unique(currentDividers.begin(), currentDividers.end());
    	currentDividers.erase(lastDivider, currentDividers.end()); 


    	std::sort(currentMultiplicators.begin(), currentMultiplicators.end());
    	auto lastMultiplicator = std::unique(currentMultiplicators.begin(), currentMultiplicators.end());
    	currentMultiplicators.erase(lastMultiplicator, currentMultiplicators.end()); 
	}

	void onSampleRateChange() override {
		samplerate = APP->engine->getSampleRate();

		for(int c = 0; c < POLY_CHANNELS; c++) {
			clockModulators[c].updateSamplerate(samplerate);
			clockFollowers[c].updateSamplerate(samplerate);
		}
	}

	void setOutputMode(int mode) {
		outputMode = mode;
		for(int i = 0; i < POLY_CHANNELS; i++) {
			clockModulators[i].setPulseMode(mode);
		}
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "output_mode", json_integer(outputMode));

		json_t *followersJ = json_array();
		json_t *modulatorsJ = json_array();

		for (int i = 0; i < POLY_CHANNELS; i++) {
			json_array_append_new(followersJ, clockFollowers[i].toJson());
			json_array_append_new(modulatorsJ, clockModulators[i].toJson());
		}
		
		json_object_set_new(rootJ, "clock_followers", followersJ);
		json_object_set_new(rootJ, "clock_modulators", modulatorsJ);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		setOutputMode(json_integer_value(json_object_get(rootJ, "output_mode")));

		json_t *followersJ = json_object_get(rootJ, "clock_followers");
		json_t *modulatorsJ = json_object_get(rootJ, "clock_modulators");

		if (followersJ && modulatorsJ) {
			for (int i = 0; i < POLY_CHANNELS; i++) {
				clockFollowers[i].fromJson(json_array_get(followersJ, i));
				clockModulators[i].fromJson(json_array_get(modulatorsJ, i));
			}	
		}

	}
};

struct ClockModulatorDisplay : TransparentWidget {
	ClockM8* module;
	FramebufferWidget* fb;
	TextLabel* label;

	float currentRatio = 1.f;

	ClockModulatorDisplay(Vec size) {
		this->box.size = size;
		fb = new FramebufferWidget();
		fb->box.size = size;
		addChild(fb);
		auto fontFileName = "res/fonts/EHSMB.TTF";
		std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontFileName));
		label = new TextLabel(font);
		label->box.pos = Vec(-1.f, 3.f);
		label->box.size = size;
		label->setFontSize(14.f);
		std::string text = std::string("x1");
		label->setText(text);
		label->setColor(SCHEME_ORANGE_23V);
		fb->addChild(label);
		fb->dirty = true;
	}

	void step() override {
		if (module) {
			std::string text;
			if(module->outputChannels > 0) {
				float ratio = module->clockModulators[0].getRatio();
			
				if(ratio != currentRatio) {
					text = module->clockModulators[0].ratioLitteral;
					label->setText(text);	
					fb->dirty = true;
				}

				currentRatio = ratio;
			}
			else {
				if(currentRatio != 0.f) {
					text = "----";
					label->setText(text);	
					fb->dirty = true;
					currentRatio = 0.f;
				}
			}
		}
		else {
			std::string text = std::string("x1");
			label->setText(text);
		}

		TransparentWidget::step();
	}
};

struct OutputModeValueItem : MenuItem {
	int m_mode;
	ClockM8* module;

	OutputModeValueItem(int mode) {
		m_mode = mode;
	}

	void onAction(const event::Action& e) override {
		module->setOutputMode(m_mode);
	}
};

struct ClockM8Widget : ModuleWidget {
	ClockM8Widget(ClockM8* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ClockM8.svg")));

		{
			ClockModulatorDisplay *display = new ClockModulatorDisplay(Vec(38.1f, 27.f));
			display->module = module;
			display->box.pos = Vec(5.6f, 175.5f);
			addChild(display);
		}

		addInput(createInput<PJ301MPort>(Vec(10.5f, 42), module, ClockM8::CLOCK_INPUT));
		addInput(createInput<SmallPort>(Vec(13.f, 89.5f), module, ClockM8::RESET_INPUT));
		addInput(createInput<SmallPort>(Vec(5.f, 274.5f), module, ClockM8::MOD_CV_INPUT));
		
		addOutput(createOutput<PolyLightPort<16>>(Vec(10.5f, 315), module, ClockM8::CLOCK_OUTPUT));

		addParam(createParam<KnobWhite32>(Vec(6.f, 200.f), module, ClockM8::MAIN_KNOB));
		addParam(createParam<KnobDark26>(Vec(2.f, 243), module, ClockM8::ATTENUATOR_KNOB));

		addParam(createParam<LedButton>(Vec(25.f,131.0f), module, ClockM8::BINARY_BUTTON));
		addParam(createParam<LedButton>(Vec(25.f,141.0f), module, ClockM8::TRIPLETS_BUTTON));
		addParam(createParam<LedButton>(Vec(25.f,151.5f), module, ClockM8::DOTTED_BUTTON));
		addParam(createParam<LedButton>(Vec(25.f,161.5f), module, ClockM8::ODD_BUTTON));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

	}

	void appendContextMenu(Menu* menu) override {
		ClockM8* module = dynamic_cast<ClockM8*>(this->module);

		menu->addChild(new MenuSeparator);

		MenuLabel* item = new MenuLabel;
	 	item->text = "Output Mode ";
	 	menu->addChild(item);

	 	{
			OutputModeValueItem* menuItem = new OutputModeValueItem(ClockModulator::MODE_GATE);
			menuItem->text = "Gate";
			menuItem->module = module;
			menuItem->rightText = CHECKMARK(module->outputMode == ClockModulator::MODE_GATE);
			menu->addChild(menuItem);
		}
		{
			OutputModeValueItem* menuItem = new OutputModeValueItem(ClockModulator::MODE_TRIGGER);
			menuItem->text = "Trigger";
			menuItem->module = module;
			menuItem->rightText = CHECKMARK(module->outputMode == ClockModulator::MODE_TRIGGER);
			menu->addChild(menuItem);
		}

	}
};

Model* modelClockM8 = createModel<ClockM8, ClockM8Widget>("ClockM8");