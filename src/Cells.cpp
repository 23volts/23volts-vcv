#include "23volts.hpp"


// Helper functions

int rangeToIndex(float value, int size, float rangeMin, float rangeMax) {
	float range = rangeMax - rangeMin;
	float ratio = value / range;
	return std::ceil(ratio * size) - 1;
}


struct CellularAlgorithm {
	std::string m_name = "Default";
	std::vector<int> m_birth;
	std::vector<int> m_survival;
	bool survives(int n) {
		return n > 0 ? m_survival[n-1] : false;
	}
	bool breed(int n) {
		return n > 0 ? m_birth[n-1] : false;
	}
};

struct ConwayAlgorithm : CellularAlgorithm {
	ConwayAlgorithm() {
		m_name = "Conway's";
		m_birth = {0, 0, 1, 0, 0, 0, 0, 0};
		m_survival= {0, 1, 1, 0, 0, 0, 0, 0};
	}
};

struct HighlifeAlgorithm : CellularAlgorithm {
	HighlifeAlgorithm() {
		m_name = "HighLife";
		m_birth = {0, 0, 1, 0, 0, 1, 0, 0};
		m_survival= {0, 1, 1, 0, 0, 0, 0, 0};	
	}
};

struct DaynightAlgorithm : CellularAlgorithm {
	DaynightAlgorithm() {
		m_name = "DayNight";
		m_birth = {0, 0, 1, 0, 0, 1, 1, 1};
		m_survival= {0, 0, 1, 1, 0, 1, 1, 1};	
	}
};

struct Organism {

	static const int ORG_MIN_SIZE = 3;
	static const int ORG_MAX_SIZE = 6;

	int sizeX;
	int sizeY;
	int size;
	bool* cells;

	Organism(float density) {
		sizeX = randomSize();
		sizeY = randomSize();
	
		size = sizeX * sizeY;
		cells = new bool[size];
		for(int x = 0; x < size; x++) {
			cells[x] = random::uniform() < density;
		}
	}

	~Organism() {
		delete cells;
	}

	int randomSize() {
		return std::floor(random::uniform() * (ORG_MAX_SIZE - ORG_MIN_SIZE)) + ORG_MIN_SIZE;
	}
};

struct State {
	int m_size;
	std::vector<bool> cells;

	State(int size, bool* data) {
		m_size = size;
		cells.reserve(size);
		for(int i = 0; i < size; i++) {
			cells.push_back(data[i]);
		}
	}

	bool empty() {
		for(int i = 0; i < m_size; i++) {
			if(cells[i]) return true;
		}
		return false;
	}

	bool operator==( const State &otherState ) const { 
		if(m_size != otherState.m_size) return false;
		
		for(int i = 0; i < m_size; i++) {
			if(cells[i] != otherState.cells[i]) return false;
		}
		return true;
	}
};

struct Cells : Module {

	static const int GRID_LINES = 15;
	static const int GRID_COLUMNS = 21;
	static const int GRID_SIZE = GRID_LINES * GRID_COLUMNS;
	static const int GATE_OUTPUTS_TOTAL = (GRID_COLUMNS / 3 - 1) * (GRID_LINES / 3 - 1);
	static const int GATE_OUTPUTS_PER_LINE = GRID_COLUMNS / 3 - 1;
	static const int EOL_DETECTOR_DEPTH = 7;

	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		TICK_INPUT,
		SPAWN_INPUT,
		CLEAR_INPUT,
		RESET_INPUT,
		ALGO_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, GATE_OUTPUTS_TOTAL),
		DENSITY_CV_OUTPUT,
		INFINITE_LOOP_GATE_OUTPUT,
		OR_GATE_OUTPUT,
		NOT_GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	bool cells[GRID_SIZE];

	bool algoConnected = false;
	bool densityConnected = false;
	bool infiniteLoopConnected = false;
	bool connectedOutputs[GATE_OUTPUTS_TOTAL];
	dsp::ClockDivider connectionClock;

	dsp::SchmittTrigger tickTrigger; // Next generation
	dsp::SchmittTrigger resetTrigger; // Reset to last spawn
	dsp::SchmittTrigger clearTrigger; // Clear the whole grid
	dsp::SchmittTrigger spawnTrigger; // Spawn a protolifeform

	dsp::PulseGenerator eolPulse;

	bool displayNeedsUpdate = true;

	State* initialState;
	std::deque<State*> previousStates;

	std::vector<CellularAlgorithm> algorithms;
	CellularAlgorithm* currentAlgorithm;
	int currentAlgorithmIndex = 0;

	Cells() {
		clear();
		connectionClock.setDivision(512);
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		initAlgorithms();
	}

	~Cells() {
		for(State* &state : previousStates) {
			delete state;
		}
		delete initialState;
	}

	void initAlgorithms() {
		algorithms.push_back(ConwayAlgorithm());
		algorithms.push_back(HighlifeAlgorithm());
		algorithms.push_back(DaynightAlgorithm());
		currentAlgorithm = &algorithms[0];
	}

	void setAlgorithm(int index) {
		currentAlgorithm = &algorithms[index];	
		currentAlgorithmIndex = index;
	}

	void process(const ProcessArgs& args) override {

		if(connectionClock.process()) {
			pollOutputs();
		}

		if (clearTrigger.process(inputs[CLEAR_INPUT].getVoltage())) {
			clear();
			displayNeedsUpdate = true;
		}

		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			reset();
			displayNeedsUpdate = true;
		}

		if (spawnTrigger.process(inputs[SPAWN_INPUT].getVoltage())) {
			spawn();
			displayNeedsUpdate = true;
		}

		if (tickTrigger.process(inputs[TICK_INPUT].getVoltage())) {
			tick();
			displayNeedsUpdate = true;
		}

		processConnectedOutputs();
	}

	void processConnectedOutputs() {

		bool isNot = true;
		bool isOrTick = false;
		bool isOrGate = false;

		for(int x = 0; x < GATE_OUTPUTS_TOTAL; x++) {
			if(connectedOutputs[x]) {
				int count = countCableNeighbours(x);
				if(count < 1) {
					outputs[GATE_OUTPUTS + x].setVoltage(0.f);
				}
				if(count == 1) {
					isNot = false;
					isOrTick = true;
					outputs[GATE_OUTPUTS + x].setVoltage(inputs[TICK_INPUT].getVoltage());
				}
				if(count > 1) {
					isNot = false;
					isOrGate = true;
					outputs[GATE_OUTPUTS + x].setVoltage(10.f);
				}
			}
		}

		outputs[NOT_GATE_OUTPUT].setVoltage(isNot ? 10.f : 0.f);
		outputs[OR_GATE_OUTPUT].setVoltage(isOrGate ? 10.f : isOrTick ? inputs[TICK_INPUT].getVoltage() : 0.f);

		if(densityConnected) outputs[DENSITY_CV_OUTPUT].setVoltage(10.f * (getCellCount() / GRID_SIZE));

		if(infiniteLoopConnected) outputs[INFINITE_LOOP_GATE_OUTPUT].setVoltage(eolPulse.process(1.0f) ? 10.f : 0.f);
		
	}

	int countCableNeighbours(int cableIndex) {
		int count = 0;
		int orgY = std::floor(cableIndex / (GATE_OUTPUTS_PER_LINE)) * 3 + 2;
		int orgX = (cableIndex % GATE_OUTPUTS_PER_LINE + 1) * 3 - 1;
		for(int y = 0; y < 2; y++) {
			for(int x = 0; x < 2; x++) {
				count += cells[relativeCoordinatesToIndex(orgX + x, orgY + y)] ? 1 : 0;
			}
		}

		return count;
	}

	void pollOutputs() {
		for(int x = 0; x < GATE_OUTPUTS_TOTAL; x++) {
			connectedOutputs[x] = outputs[GATE_OUTPUTS + x].isConnected();		
		}

		densityConnected = outputs[DENSITY_CV_OUTPUT].isConnected();
		infiniteLoopConnected = outputs[INFINITE_LOOP_GATE_OUTPUT].isConnected();
		algoConnected = inputs[ALGO_CV_INPUT].isConnected();
	}

	void spawn() {
		float density = random::uniform() * 0.6f + 0.4f;
		Organism organism = Organism(density);

		int orgX = std::floor(random::uniform() * GRID_COLUMNS);
		int orgY = std::floor(random::uniform() * GRID_LINES);

		for(int x = 0; x < organism.sizeX; x++) {
			for(int y = 0; y < organism.sizeY; y++) {
				int cellIndex = relativeCoordinatesToIndex(orgX + x, orgY + y);
				cells[cellIndex] = organism.cells[y * organism.sizeX + x];
			}
		}

		// Save State as initial State
		initialState = stateFromCurrentGrid();
	}

	void tick() {
		
		// Check current algorithm
		if(algoConnected) {
			float cv = clamp(inputs[ALGO_CV_INPUT].getVoltage(), 0.0f, 10.f);
			//setAlgorithm(std::floor((cv / 10.f) * algorithms.size()));
			setAlgorithm(rangeToIndex(cv, algorithms.size(), 0.0f, 10.f));
		}

		bool newGeneration[GRID_SIZE];

		for(int y = 0; y < GRID_LINES; y++) {
			for(int x = 0; x < GRID_COLUMNS; x++) {
				int count = countNeighbours(x, y);
				int index = (y * GRID_COLUMNS) + x;

				bool alive = cells[index];
				bool newCell;
				if(alive) {
					newCell = currentAlgorithm->survives(count);
				}
				else {
					newCell = currentAlgorithm->breed(count);
				}
				newGeneration[index] = newCell;
			}
		}

		setCells(newGeneration);

		pushStateMemory();

		if(isGridEOL()) {
			eolPulse.trigger(25.f);
		}
	}

	void pushStateMemory() {
		previousStates.push_back(stateFromCurrentGrid());

		if(previousStates.size() > EOL_DETECTOR_DEPTH) {
			delete previousStates.front();
			previousStates.pop_front();
		}
	}

	bool isGridEOL() {		
		if(previousStates.size() < 2) return false;

		State *currentState = previousStates.back();
		
		for(uint64_t i=0; i < previousStates.size() - 1; i++) {
			if (*previousStates[i] == *currentState) return true;
		}

		return false;
	}

	State* stateFromCurrentGrid() {
		return new State(GRID_SIZE, cells);
	}

	void setGridFromState(State *state) {
		for(int x = 0; x < GRID_SIZE; x++) {
			cells[x] = state->cells[x];
		}
	}

	void restoreState(State &state) {
		for(int x = 0; x < GRID_SIZE; x++) {
			cells[x] = state.cells[x];
		}
	}

	void reset() {
		setGridFromState(initialState);
	}

	void setCells(const bool* newCells) {
		for(int x = 0; x < GRID_SIZE; x++) {
			cells[x] = newCells[x];
		}
	}

	int countNeighbours(int x, int y) {
		int counter = 0;

		for(int deltaX = -1; deltaX < 2; deltaX++) {
			for(int deltaY = -1; deltaY < 2; deltaY++) {
				if(deltaX == 0 && deltaY == 0) continue;	// Don't count the center cell
				int coordX = x + deltaX;
				int coordY = y + deltaY;
				if(cells[relativeCoordinatesToIndex(coordX, coordY)]) counter++;
			}
		}

		return counter;
	}

	int relativeCoordinatesToIndex(int x, int y) {
		if(x < 0) x = GRID_COLUMNS - 1;
		if(x >= GRID_COLUMNS) x = 0;
		if(y < 0) y = GRID_LINES - 1;
		if(y >= GRID_LINES) y = 0;
		return y * GRID_COLUMNS + x;
	}

	void clear() {
		for(int x = 0; x < GRID_SIZE; x++) {
			cells[x] = false;
			initialState =  stateFromCurrentGrid();
		}
		for(int x = 0; x < GATE_OUTPUTS_TOTAL; x++) {
			connectedOutputs[x] = false;
		}

	}

	int getCellCount() {
		int count = 0;
		for(int x = 0; x < GRID_SIZE; x++) {
			count+= cells[x] ? 1 : 0;
		}
		return count;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// Initial state
		json_t *initialStateJ = json_array();
		for (int i = 0; i < GRID_SIZE; i++) {
			json_t *cellJ = json_integer((int) initialState->cells[i]);
			json_array_append_new(initialStateJ, cellJ);
		}
		json_object_set_new(rootJ, "initial_state", initialStateJ);

		// Current State
		json_t *stateJ = json_array();
		for (int i = 0; i < GRID_SIZE; i++) {
			json_t *cellJ = json_integer((int) cells[i]);
			json_array_append_new(stateJ, cellJ);
		}
		json_object_set_new(rootJ, "current_state", stateJ);

		// Selected algorithm
		json_object_set_new(rootJ, "algorithm", json_integer(currentAlgorithmIndex));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		// Initial state
		bool initial[GRID_SIZE];
		json_t *initialStateJ = json_object_get(rootJ, "initial_state");
		if (initialStateJ) {
			for (int i = 0; i < GRID_SIZE; i++) {
				json_t *cellJ = json_array_get(initialStateJ, i);
				if (cellJ)
					initial[i] = !!json_integer_value(cellJ);
			}	
			initialState = new State(GRID_SIZE, initial);
		}

		// Current State
		json_t *stateJ = json_object_get(rootJ, "current_state");
		if (stateJ) {
			for (int i = 0; i < GRID_SIZE; i++) {
				json_t *cellJ = json_array_get(stateJ, i);
				if (cellJ)
					cells[i] = !!json_integer_value(cellJ);
			}
		}
		
		// Selected algorithm
		setAlgorithm(json_integer_value(json_object_get(rootJ, "algorithm")));
	}
};

struct CellsBackground : TransparentWidget {
	void draw(const DrawArgs &args) override {
		nvgFillColor(args.vg, nvgRGB(20, 30, 33));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);

		float cellHeight = box.size.y / Cells::GRID_LINES;
		float cellWidth = box.size.x  / Cells::GRID_COLUMNS;

		for(int x = 0; x < Cells::GATE_OUTPUTS_TOTAL; x++) {
			float xPos = (cellWidth * 3) * (x % Cells::GATE_OUTPUTS_PER_LINE + 1);
			float yPos = (cellHeight * 3) * (std::floor(x / Cells::GATE_OUTPUTS_PER_LINE) + 1);
			nvgFillColor(args.vg, color::alpha(nvgRGB(0xE2, 0xEE, 0xEF), 0.07f));
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, xPos, yPos, 12.f);
			//nvgStrokeWidth(args.vg, 2.f);
			nvgFill(args.vg);
		}

	}
};

struct CellsDisplay : OpaqueWidget {
	Cells* module;
	FramebufferWidget* fb;

	CellsBackground* background;
	MonochromeGridDisplay* gridDisplay;

	CellsDisplay(int width, int height) {
		fb = new FramebufferWidget();
		addChild(fb);

		background = new CellsBackground();
		fb->addChild(background);

		gridDisplay = new MonochromeGridDisplay(width, height);
		fb->addChild(gridDisplay);
	}

	void setSize() {
		background->box.pos = Vec(0,0);
		background->box.size = this->box.size;
		gridDisplay->box.pos = Vec(0,0);
		gridDisplay->box.size = this->box.size;
		gridDisplay->setColor(0xE2, 0xEE, 0xEF);
		fb->box.size = gridDisplay->box.size;
		fb->dirty = true;
	}

	void updateCells() {
		gridDisplay->setCells(module->cells, Cells::GRID_SIZE);
	}
	
	void step() override {
		if (module && module->displayNeedsUpdate) {
			updateCells();
			fb->dirty = true;
			module->displayNeedsUpdate = false;
		}
		OpaqueWidget::step();
	}
};

struct AlgorithmValueItem : MenuItem {
	int m_index;
	Cells* module;

	AlgorithmValueItem(int index) {
		m_index = index;
	}

	void onAction(const event::Action& e) override {
		module->setAlgorithm(m_index);
	}
};

struct CellsWidget : ModuleWidget {

	static const int DISPLAY_TOP_OFFSET = 23;

	CellsWidget(Cells* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Cells.svg")));

		{
			CellsDisplay *display = new CellsDisplay(Cells::GRID_COLUMNS, Cells::GRID_LINES);
			display->box.pos = Vec(0, DISPLAY_TOP_OFFSET);
			display->box.size = Vec(box.size.x, box.size.y - 95);
			display->setSize();
			display->module = module;
			if(module) {
				display->gridDisplay->setCells(module->cells, Cells::GRID_SIZE);
			}
			addChild(display);
		}

		float cellHeight = (box.size.y - 95) / Cells::GRID_LINES;
		float cellWidth = box.size.x  / Cells::GRID_COLUMNS;

		for(int x = 0; x < Cells::GATE_OUTPUTS_TOTAL; x++) {
			float xPos = (cellWidth * 3) * (x % Cells::GATE_OUTPUTS_PER_LINE + 1);
			float yPos = (cellHeight * 3) * (std::floor(x / Cells::GATE_OUTPUTS_PER_LINE) + 1);
			addOutput(createOutputCentered<InvisiblePort>(Vec(xPos, yPos + DISPLAY_TOP_OFFSET), module, Cells::GATE_OUTPUTS + x));
		}

		addInput(createInput<PJ301MPort>(Vec(176.1f, 329.f), module, Cells::TICK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(62.1f, 346.2f), module, Cells::SPAWN_INPUT));
		addInput(createInput<PJ301MPort>(Vec(133.1f, 317.f), module, Cells::CLEAR_INPUT));
		addInput(createInput<PJ301MPort>(Vec(62.1f, 317.f), module, Cells::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(133.1f, 346.2f), module, Cells::ALGO_CV_INPUT));
		
		addOutput(createOutput<PJ301MPort>(Vec(291.1f, 317.f), module, Cells::DENSITY_CV_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(291.1f, 346.2f), module, Cells::INFINITE_LOOP_GATE_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(219.1f, 317.f), module, Cells::OR_GATE_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(219.1f, 346.2f), module, Cells::NOT_GATE_OUTPUT));


		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

	void appendContextMenu(Menu* menu) override {
		Cells* module = dynamic_cast<Cells*>(this->module);

		menu->addChild(new MenuSeparator);

		MenuLabel* item = new MenuLabel;
	 	item->text = "Algorithm";
	 	menu->addChild(item);

		// Context menu -> add memory size select list
		for(int x = 0; x < (int) module->algorithms.size(); x++) {
			AlgorithmValueItem* menuItem = new AlgorithmValueItem(x);
			menuItem->text = module->algorithms[x].m_name;
			menuItem->module = module;
			menuItem->rightText = CHECKMARK(module->currentAlgorithmIndex == x);
			menu->addChild(menuItem);
		}

	}
};

Model* modelCells = createModel<Cells, CellsWidget>("Cells");
