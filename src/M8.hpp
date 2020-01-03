#pragma once

#include "rack.hpp"

#define M8_POLY_CHANNELS 16

// Definitions for M8 Modular Clock System modules
#define M8_NO_MODULE -1
#define M8_EXPANDER 0
#define M8_TRANS_SYSTEM 1
#define M8_CLOCK_SYSTEM 2
#define M8_TIME_SYSTEM 3
#define M8_DELAY_SYSTEM 4

// utility macros 
#define isM8ExpanderModule(x) false/*x->model == modelM8X*/
#define isM8SystemModule(x) /*x->model == modelTransM8 ||*/ x->model == modelClockM8 /*|| x->model == modelTimeM8 || x->model == modelDelayM8*/
//#define isM8Module(x) x->model == modelM8X || x->model == modelTransM8 || x->model == modelClockM8 || x->model == modelTimeM8 || x->model == modelDelayM8
#define isM8Module(x) /*x->model == modelM8X ||*/ x->model == modelClockM8

// Message from the master transport that should be carried
// all the way from left to right. Let's keep in mind that message
// mechanism has a 1 sample delay for each module.
// One solution could be for TransM8 to buffer the transport
// and do latency compensation at the same time so everything could 
// be in sync. 
// 
struct M8TransportMessage {
	float phases[M8_POLY_CHANNELS];
	bool running = false;
	bool start = false;
	bool stop = false;
	bool reset = false;

	M8TransportMessage() {}

	M8TransportMessage(const M8TransportMessage &other) {
		running = other.running;
		start = other.start;
		stop = other.stop;
		reset = other.reset;
		for(int x = 0; x < M8_POLY_CHANNELS; x++) {
			phases[x] = other.phases[x];
		}
	}
};

// Status message that will carry clock information left to right until 
// last M8 module is reached. 
struct M8StatusMessage {

	// M8 Module type that the status is originated from. This will
	// prevent a module from misinterpreting empty data.
	int moduleType = M8_NO_MODULE;

	// Expander passing this message will increase the count, therefore
	// allowing to display correct sets of value, write in the correct
	// control slots
	int expanderCount = 0;

	// Number of active channels
	int activeChannels = 0;

	// Channel litterals
	std::string* litterals[M8_POLY_CHANNELS];

	// Would be used when an expander is connected to an M8 module, so the
	// parameter values can be set to reflect current state. 
	float values[M8_POLY_CHANNELS];

	// Channel output raw value
	float outputs[M8_POLY_CHANNELS];

	M8StatusMessage() {
		for(int x = 0; x < M8_POLY_CHANNELS; x++) {
			values[x] = 0.f;
			outputs[x] = 0.f;
		}
	}

	M8StatusMessage(const M8StatusMessage &other) {
		moduleType = other.moduleType;
		expanderCount = other.expanderCount;
		activeChannels = other.activeChannels;
		for(int x = 0; x < M8_POLY_CHANNELS; x++) {
			values[x] = other.values[x];
			outputs[x] = other.outputs[x];
		}
	}

};

// Control message structure that will be passed right to left until a M8 Module is reached
// Another M8X will mutate this to fill the controller values it manages
struct M8ControlMessage {

	bool bankA = false;	// Set to true if 1-8 bank is controlled
	bool bankB = false; // Set to true if 9-16 bank is controlled

	float controllerValues[M8_POLY_CHANNELS];

	// Can serve at determining the number of needed channels
	bool outputConnected[M8_POLY_CHANNELS];

	M8ControlMessage() {
		for(int x = 0; x < M8_POLY_CHANNELS; x++) {
			controllerValues[x] = 0.f;
			outputConnected[x] = false;
		}
	}

	M8ControlMessage(const M8ControlMessage &other) {
		bankA = other.bankA;
		bankB = other.bankB;
		for(int x = 0; x < M8_POLY_CHANNELS; x++) {
			controllerValues[x] = other.controllerValues[x];
			outputConnected[x] = other.outputConnected[x];
 		}
	}
};

// Just to keep things tidy as we have to use the same exact structure between module/expanders
struct M8Message {
	M8StatusMessage status;
	M8ControlMessage control;
	M8TransportMessage transport;
};


struct M8Module : Module {
	int moduleType = M8_NO_MODULE;

	bool leftLinkActive = false;
	bool rightLinkActive = false;

	int leftExpanderType = M8_NO_MODULE;
	int rightExpanderType = M8_NO_MODULE;

	int activeChannels = 0;
	std::string* channelLitterals[M8_POLY_CHANNELS];
	float channelOutputs[M8_POLY_CHANNELS];
	float channelControls[M8_POLY_CHANNELS];

	M8Message leftMessages[2][1];
	M8Message rightMessages[2][1];

	M8TransportMessage* transportMessage;
	M8StatusMessage* statusMessage;
	M8ControlMessage* controlMessage;

	M8Module() {
		leftExpander.consumerMessage = leftMessages[0];
		leftExpander.producerMessage = leftMessages[1];
		rightExpander.consumerMessage = rightMessages[0];
		rightExpander.producerMessage = rightMessages[1];

		transportMessage = &leftMessages[0]->transport;
		statusMessage = &leftMessages[0]->status;
		controlMessage = &rightMessages[0]->control;
	}

	void updateExpanders() {
		leftLinkActive = leftExpander.module ? isM8Module(leftExpander.module) ? true : false : false;
		rightLinkActive = rightExpander.module ? isM8Module(rightExpander.module) ? true : false : false;

		leftExpanderType = leftLinkActive ? reinterpret_cast<M8Module*>(leftExpander.module)->moduleType : M8_NO_MODULE;
		rightExpanderType = rightLinkActive ? reinterpret_cast<M8Module*>(rightExpander.module)->moduleType : M8_NO_MODULE;
	}

	void sendStatusMessage() {
		M8Message *message = (M8Message*)(rightExpander.module->leftExpander.producerMessage);
		message->status = *statusMessage;
		message->transport = *transportMessage;

		if(isM8SystemModule(this)) {
			message->status.moduleType = moduleType;

			for(int x = 0; x < M8_POLY_CHANNELS; x++) {
				message->status.litterals[x] = channelLitterals[x];
				message->status.outputs[x] = channelOutputs[x];
			}
		}
		else {
			message->status.expanderCount++;
		}

		rightExpander.module->leftExpander.messageFlipRequested = true;
	}

	void sendControlMessage() {
		M8Message *message = (M8Message*)(leftExpander.module->rightExpander.producerMessage);	
		message->control = *controlMessage;	

		leftExpander.module->rightExpander.messageFlipRequested = true;	
	}

};
