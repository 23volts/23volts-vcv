#pragma once

#include "rack.hpp"

struct MidiInput : rack::midi::InputQueue {
	bool isConnected() {
		return driverId > -1 && deviceId > -1;
	}
};

struct MidiOutput : rack::dsp::MidiGenerator<rack::PORT_MAX_CHANNELS>, rack::midi::Output {
	bool isConnected() {
		return driverId > -1 && deviceId > -1;
	}

	void onMessage(const rack::midi::Message &message) override {
		rack::midi::Output::sendMessage(message);
	}

	void reset() {
		rack::midi::Output::reset();
		rack::dsp::MidiGenerator<rack::PORT_MAX_CHANNELS>::reset();
	}

	// Workaround for sending message on multiple channels
	void sendRawMessage(const rack::midi::Message& message) {
		//DEBUG("Midi Message sent %02x %02x %02x", message.bytes[0], message.bytes[1], message.bytes[2]);
		if (outputDevice) {
			outputDevice->sendMessage(message);
		}
	}
};

struct MidiInputOutput {
	MidiInput input;
	MidiOutput output;

	virtual void onPortChange() {}

	void reset() {
		input.setDeviceId(-1);
		output.setDeviceId(-1);
	}

	json_t* toJson() {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "midi_input", input.toJson());
		json_object_set_new(rootJ, "midi_output", output.toJson());
		return rootJ;
	}

	void fromJson(json_t* rootJ) {
		json_t* midiInputJ = json_object_get(rootJ, "midi_input");
		json_t* midiOutputJ = json_object_get(rootJ, "midi_output");
		if (midiInputJ) input.fromJson(midiInputJ);
		if (midiOutputJ) output.fromJson(midiOutputJ);
	}
};

struct MidiDriverValueItem : rack::ui::MenuItem {
	MidiInputOutput* midiIO;
	rack::midi::Port* port;
	int driverId;
	
	void onAction(const rack::event::Action& e) override {
		port->setDriverId(driverId);
		midiIO->onPortChange();
	}
};

struct MidiDriverItem : rack::ui::MenuItem {
	MidiInputOutput* midiIO;
	rack::midi::Port* port;
	
	rack::ui::Menu* createChildMenu() override;
};

struct MidiDeviceValueItem : rack::ui::MenuItem {
	MidiInputOutput* midiIO;
	rack::midi::Port* port;

	int deviceId;
	
	void onAction(const rack::event::Action& e) override {
		port->setDeviceId(deviceId);
		midiIO->onPortChange();
	}
};

struct MidiDeviceItem : rack::ui::MenuItem {
	MidiInputOutput* midiIO;
	rack::midi::Port* port;

	rack::ui::Menu* createChildMenu() override;
};

struct MidiChannelValueItem : rack::ui::MenuItem {
	MidiInputOutput* midiIO;
	rack::midi::Port* port;
	int channel;
	void onAction(const rack::event::Action& e) override {
		port->channel = channel;
		midiIO->onPortChange();
	}
};

struct MidiChannelItem : rack::ui::MenuItem {
	MidiInputOutput* midiIO;
	rack::midi::Port* port;

	rack::ui::Menu* createChildMenu() override;
};

struct MidiMenuBuilder {
	bool input = true;
	bool output = true;
	bool channel = true;
	void build(rack::ui::Menu* menu, MidiInputOutput* midiIO);
};

