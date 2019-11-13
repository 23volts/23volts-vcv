#include <rack.hpp>

using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin *pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelCells;
extern Model* modelClockM8;
extern Model* modelMorph;
extern Model* modelMem;
extern Model* modelMerge8;
extern Model* modelSplit8;
extern Model* modelSwitchN1;

template <typename TBase = GrayModuleLightWidget>
struct TGreenBlueLight : TBase {
	TGreenBlueLight() {
		this->addBaseColor(SCHEME_GREEN);
		this->addBaseColor(SCHEME_BLUE);
	}
};
typedef TGreenBlueLight<> GreenBlueLight;

template <typename TBase = GrayModuleLightWidget>
struct TYellowBlueLight : TBase {
	TYellowBlueLight() {
		this->addBaseColor(SCHEME_YELLOW);
		this->addBaseColor(SCHEME_BLUE);
	}
};
typedef TYellowBlueLight<> YellowBlueLight;

template <typename LightType>
struct LightGridFactory {
	app::ModuleWidget* moduleWidget;
	int lights = 1;
	int colors = 1;
	int rows = 1;
	float startX = 1;
	float startY = 1;
	float Xseparation = 2;
	float Yseparation = 2;
	int base = 0;
	void make() {
 		for(int y = 0; y < rows; y++) {
			for(int x = 0; x < lights / rows; x++) {
				int lightOffset = ((lights / rows) * y + x) * colors; 
				math::Vec position = mm2px(Vec(startX + x * Xseparation, startY + y * Yseparation));
				engine::Module* module = moduleWidget->module;
				moduleWidget->addChild(createLight<LightType>(position, module, base + lightOffset));		
			}
		}
	}
};

struct InvisiblePort : SvgPort {
	InvisiblePort() {
		setSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/PJ3410.svg")));
		shadow->opacity = 0.01f;
		sw->visible = false;
	}
};

struct MonochromeGridDisplay : OpaqueWidget {
	int columns;
	int lines;
	int gridSize;
	NVGcolor color;
	NVGcolor backgroundColor;
	bool drawBackground = false;
	bool* cells;
	float gridWidth;
	float gridAlpha;

	MonochromeGridDisplay(int gridColumns, int gridLines) {
		columns = gridColumns;
		lines = gridLines;
		gridSize = gridColumns * gridLines;
		gridWidth = 1.f;
		gridAlpha = 0.1f;
		cells = new bool[gridSize];
		for(int i = 0; i < gridSize;i++) {
			cells[i] = false;
		}
		setColor(0xFF, 0xFF, 0xFF);
	}

	~MonochromeGridDisplay() {
		delete cells;
	}

	void setBackgroundColor(uint8_t red, uint8_t green, uint8_t blue) {
		backgroundColor = nvgRGB(red, green, blue);
		drawBackground = true;
	}

	void setColor(uint8_t red, uint8_t green, uint8_t blue) {
		color = nvgRGB(red,green,blue);
	}

	void setCells(const bool* newCells, int size) {
		for(int x = 0; x < size; x++) {
			cells[x] = newCells[x];
		}
	}

	void draw(const DrawArgs &args) override {

		// Background color
		if(drawBackground) {
			nvgFillColor(args.vg, backgroundColor);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
			nvgFill(args.vg);
		}

		// Grid contour
		nvgStrokeWidth(args.vg, gridWidth);
		nvgStrokeColor(args.vg, color::alpha(color, gridAlpha));
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgStroke(args.vg);

		float cellWidth = box.size.x / columns;
		float cellHeight = box.size.y / lines;
		
		// Draw the grid
		for (int i = 0; i < gridSize; i++) {
			
			int posY = i / columns;
			int posX = i % columns;
			
			nvgFillColor(args.vg, color);
			nvgStrokeWidth(args.vg, gridWidth);
			nvgStrokeColor(args.vg, color::alpha(color, gridAlpha));
			nvgBeginPath(args.vg);
			nvgRect(args.vg, posX * cellWidth, posY * cellHeight, cellWidth, cellHeight);
			nvgStroke(args.vg);
			if(cells[i]) nvgFill(args.vg);
		}

	}

};

struct MonochromeBackground : TransparentWidget {
	NVGcolor m_color;
	NVGcolor m_strokeColor;
	float m_strokeWidth = 1.f;
	bool doStroke = false;

	MonochromeBackground() {
		m_color = nvgRGB(0,0,0);
	}

	void setStrokeColor(NVGcolor color) {
		doStroke = true;
		m_strokeColor = color;
	}

	void setColor(NVGcolor color) {
		m_color = color;
	}

	void draw(const DrawArgs &args) override {
		nvgFillColor(args.vg, m_color);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
		nvgFill(args.vg);
		if(doStroke) {
			nvgStrokeColor(args.vg, m_strokeColor);
			nvgStrokeWidth(args.vg, m_strokeWidth);
			nvgStroke(args.vg);
		}
	}
};


struct MomentaryButton : SvgSwitch {
	MomentaryButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Button0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Button1.svg")));
		box.size = Vec(12, 12);
		momentary = true;
	}
};

struct ToggleButton : SvgSwitch {
	MultiLightWidget* light;

	ToggleButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Toggle0.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Toggle1.svg")));
		box.size = Vec(12, 12);
		light = new MultiLightWidget;
		light->bgColor = nvgRGB(0x5a, 0x5a, 0x5a);
		light->borderColor = nvgRGBA(0, 0, 0, 0x60);
		light->box.size = Vec(2,2);
		light->box.pos = Vec(5,5);
		light->addBaseColor(SCHEME_RED);
		fb->addChild(light);
	}

	void onChange(const event::Change& e) override {
		SvgSwitch::onChange(e);
		std::vector<float> brightnesses;
		brightnesses.push_back(paramQuantity->isMax() ? 1.0f : 0.f);
		light->setBrightnesses(brightnesses);
	}
};

// TODO : Generalize and template with light type
struct BlueLedButton : Switch {
	MultiLightWidget* light;

	BlueLedButton() {
		box.size = Vec(12, 12);
		light = new MultiLightWidget;
		light->bgColor = nvgRGB(0x5a, 0x5a, 0x5a);
		light->borderColor = nvgRGBA(0, 0, 0, 0x60);
		light->box.size = Vec(8,8);
		light->box.pos = Vec(2,2);
		light->addBaseColor(SCHEME_BLUE);
		addChild(light);
	}

	void onChange(const event::Change& e) override {
		Switch::onChange(e);
		std::vector<float> brightnesses;
		brightnesses.push_back(paramQuantity->isMax() ? 1.0f : 0.f);
		light->setBrightnesses(brightnesses);
	}
};

