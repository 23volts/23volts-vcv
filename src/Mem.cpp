#include "23volts.hpp"
//#include "vector"
//#include "string"
#include "widgets/ports.hpp"

struct MemoryBank {
	private:
		int m_max_size;
		float* m_data;

		void initData() {
			m_memory = new std::vector<float>;
			m_memory->reserve(m_max_size);
			m_data = m_memory->data();
		}

	public:
		std::vector<float>* m_memory;
		
		MemoryBank(int maxSize) 
		{
			m_max_size = maxSize;
			initData();
		}
		~MemoryBank() 
		{
			delete m_memory;
		}

		float* data() const 
		{
			return m_data;
		}

		int size() const
		{
			return m_memory->size();
		}

		float getValue(int position) const
		{
			return m_data[position];
		}

		void clear() 
		{
			delete m_memory;
			initData();
		}

		/**
		 * Fill the internal data with data from another vector. 
		 * Source if truncated if the size of source vector > m_max_size
		 */
		void fill(std::vector<float>* sourceVector)
		{
			float* data = sourceVector->data();
			for (int x=0; x < (int) sourceVector->size() && x < m_max_size; x++) {
				m_memory->push_back(data[x]);
			}
		}

};

struct PureNoiseSource {
	float getValue() {
		return 2.0 * random::normal();
	}
};

struct StatisticNoiseSource {
	int m_max_size;
	
	MemoryBank *m_memory;
	std::vector<float> noiseTable;

	StatisticNoiseSource(int maxSize) {
		m_memory = new MemoryBank(maxSize);
	}

	~StatisticNoiseSource() {
		delete m_memory;
	}

	float getValue() {
		return 0.f;
	}

	/**
	 * Append memory to stats, after a clear for example, or each x samples
	 */
	void append(MemoryBank &addedMemory) {
		if(addedMemory.size() + m_memory->size() > m_max_size) return;

		// Append memory
	}

	void generateNewNoiseTable(float noiseAmount, int size) {
		// Create a new noise table
	}
};

struct Mem : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		CLEAR_INPUT,
		INPUT_INPUT,
		TRIG_INPUT,
		RESET_INPUT,
		WRITE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		WRITE_LIGHT,
		READ_LIGHT,
		NOISE_LIGHT,
		NUM_LIGHTS
	};

	static const int MEMORY_OPTIONS = 23;
	const int MEMORY_SIZES[23] = {1,2,3,4,6,7,8,12,13,16,23,32,42,48,64,96,128,256,1024,2048,4096,8192,16384};
	static const int DEFAULT_MEMORY_SIZE = 16; //_INDEX

	static const int PROCESS_BUFFER_SIZE = 32;
	static const int IDLE_BUFFER_SIZE = 1152000;

	int processBuffer = 0;
	int idleBuffer = 0;

	dsp::SchmittTrigger trigTrigger;
	dsp::SchmittTrigger clearTrigger;
	dsp::SchmittTrigger resetTrigger;

	bool isIdle = true;

	bool isReading = false;
	bool isWriting = false;
	bool isRandomizing = false;

	bool isInputConnected = false;
	bool isWriteConnected = false;

	MemoryBank* memory;
	int memorySize = MEMORY_SIZES[DEFAULT_MEMORY_SIZE];
	float currentOutputValue = 0.f;
	int position = -1;

	PureNoiseSource noiseSource;
	StatisticNoiseSource* statisticSource;

	Mem() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		memory = new MemoryBank(MEMORY_SIZES[DEFAULT_MEMORY_SIZE]);
		statisticSource = new StatisticNoiseSource(MEMORY_SIZES[MEMORY_OPTIONS - 2]);
	}
	
	~Mem() {
		delete memory;
		delete statisticSource;
	}

	void reset() {
		position = -1;
		isReading = false;
		isRandomizing = false;
		isWriting = false;
	}

	void process(const ProcessArgs &args) override {

		if(processBuffer == 0) processTimely(args);
		if(idleBuffer == 0) processIdle(args);

		bool writeGate = isWriteConnected && inputs[WRITE_INPUT].getNormalVoltage(0.f) >= 1.f;

		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage())) {
			// TODO Add to stats
			memory->clear();
			reset();
		}

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			reset();
		}

		if (trigTrigger.process(inputs[TRIG_INPUT].getVoltage())) {
			isIdle = false;
			idleBuffer = IDLE_BUFFER_SIZE;
			position++;

			if((memory->size() > position) && ! writeGate) {
				isReading = true;
				isWriting = false;
				isRandomizing = false;
				currentOutputValue = memory->getValue(position);
			}
			else {
				isReading = false;
				if(memorySize > position) {
					isWriting = true;
					// we have still some place left
					if(isInputConnected) {
						currentOutputValue = inputs[INPUT_INPUT].getVoltage();
					}
					else {
						isRandomizing = true;
						currentOutputValue = noiseSource.getValue();
					}
					memory->m_memory->push_back(currentOutputValue);
				}
				else {
					isWriting = false;
					isRandomizing = true;
					currentOutputValue = noiseSource.getValue();
				}
			}
			
		}
		else {
			idleBuffer--;
		}

		lights[WRITE_LIGHT].setBrightness(isWriting ? 1.f : 0.f);
		lights[READ_LIGHT].setBrightness(isReading ? 1.f : 0.f);
		lights[NOISE_LIGHT].setBrightness(isRandomizing && ! isIdle ? 1.f : 0.f);

		outputs[OUTPUT_OUTPUT].setVoltage(currentOutputValue);

		processBuffer--;
	}


	/**
	 * Part of the process only executed every PROCESS_BUFFER samples
	 */
	void processTimely(const ProcessArgs &args) {

		// Check states of connected cables
		isWriteConnected = inputs[WRITE_INPUT].isConnected();
		isInputConnected = inputs[INPUT_INPUT].isConnected();

		// swap noise statistics taken 

		processBuffer = PROCESS_BUFFER_SIZE;
	}

	/**
	 * Process some time consuming stuff, like writing to disk, when no clock has
	 * been received for a long time (>10-20s)
	 */
	void processIdle(const ProcessArgs &args) {

		idleBuffer = IDLE_BUFFER_SIZE;
		isIdle = true;
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "outputValue", json_real(currentOutputValue));
		json_object_set_new(rootJ, "position", json_integer(position));
		
		uint8_t *memoryData;
		memoryData = reinterpret_cast<uint8_t*>(memory->data());
		int memoryByteSize = memory->size() * 8;
		std::string memoryStr = string::toBase64(memoryData, memoryByteSize);
		json_object_set_new(rootJ, "memory", json_string(memoryStr.c_str()));
		json_object_set_new(rootJ, "memorySize", json_integer(memorySize));
		json_object_set_new(rootJ, "memoryByteSize", json_integer(memoryByteSize));

		json_object_set_new(rootJ, "isWriting", json_boolean(isWriting));
		json_object_set_new(rootJ, "isReading", json_boolean(isReading));
		json_object_set_new(rootJ, "isRandomizing", json_boolean(isReading));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		currentOutputValue = json_real_value(json_object_get(rootJ, "outputValue"));
		position = json_integer_value(json_object_get(rootJ, "position"));
		memorySize = json_integer_value(json_object_get(rootJ, "memorySize"));

		// memory
		unsigned long memoryByteSize = json_integer_value(json_object_get(rootJ, "memoryByteSize"));
		std::string memoryStr = json_string_value(json_object_get(rootJ, "memory"));
		std::vector<uint8_t> memoryVector = string::fromBase64(memoryStr);
		float* actualData = reinterpret_cast<float*>(memoryVector.data());

		memory = new MemoryBank(memorySize);
		for(unsigned long x = 0; x < memoryByteSize / 8; x++) {
			memory->m_memory->push_back(actualData[x]);
		}

		isWriting = json_is_true(json_object_get(rootJ, "isWriting"));
		isReading = json_is_true(json_object_get(rootJ, "isReading"));
		isRandomizing = json_is_true(json_object_get(rootJ, "isRandomizing"));
	}

	void setMemorySize(int size) {
		MemoryBank* newMemory = new MemoryBank(size);
		newMemory->fill(memory->m_memory);
		delete memory;
		memory = newMemory;
		memorySize = size;
	}
};

struct MemorySizeValueItem : MenuItem {
	int m_size;
	Mem* module;

	MemorySizeValueItem(int size) {
		m_size = size;
	}

	void onAction(const event::Action& e) override {
		module->setMemorySize(m_size);
	}
};

struct MemWidget : ModuleWidget {

	MemWidget(Mem *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Mem.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.7, 16.72)), module, Mem::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.7, 62.758)), module, Mem::INPUT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.7, 77.234)), module, Mem::CLEAR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.7, 91.862)), module, Mem::RESET_INPUT));
		addInput(createInputCentered<InvisiblePort>(mm2px(Vec(10.69, 51.23)), module, Mem::WRITE_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.7, 107.503)), module, Mem::OUTPUT_OUTPUT));

		addChild(createLight<MediumLight<GreenLight>>(mm2px(Vec(10, 36.6)), module, Mem::READ_LIGHT));
		addChild(createLight<MediumLight<RedLight>>(mm2px(Vec(10, 40.4)), module, Mem::WRITE_LIGHT));
		addChild(createLight<MediumLight<BlueLight>>(mm2px(Vec(10, 44.2)), module, Mem::NOISE_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		Mem* module = dynamic_cast<Mem*>(this->module);

		menu->addChild(new MenuSeparator);

		// TODO Context menu -> add "Enable noise statistics"
		
		MenuLabel* item = new MenuLabel;
	 	item->text = "Memory Size";
	 	menu->addChild(item);

		// Context menu -> add memory size select list
		for(int x = 0; x < module->MEMORY_OPTIONS; x++) {
			MemorySizeValueItem* menuItem = new MemorySizeValueItem(module->MEMORY_SIZES[x]);
			std::string label = std::to_string(module->MEMORY_SIZES[x]) + " samples" ;
			menuItem->text = label;
			menuItem->module = module;
			menuItem->rightText = CHECKMARK(module->MEMORY_SIZES[x] == module->memorySize);
			menu->addChild(menuItem);
		}

	}
};

Model *modelMem = createModel<Mem, MemWidget>("Mem");
