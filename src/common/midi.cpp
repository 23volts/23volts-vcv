#include "midi.hpp"


rack::ui::Menu* MidiDriverItem::createChildMenu() {
	rack::ui::Menu* menu = new rack::ui::Menu;

 	if (!port) {
 		rack::ui::MenuLabel* item = new rack::ui::MenuLabel;
 		item->text = "No Driver";
 		menu->addChild(item);
		return menu;
	}

	for (int driverId : rack::midi::getDriverIds()) {
		MidiDriverValueItem* item = new MidiDriverValueItem;
		item->port = port;
		item->midiIO = midiIO;
		item->driverId = driverId;
		item->text = rack::midi::getDriver(driverId)->getName();
		item->rightText = CHECKMARK(item->driverId == port->driverId);
		menu->addChild(item);
	}

	return menu;
}

rack::ui::Menu* MidiDeviceItem::createChildMenu() {
	rack::ui::Menu* menu = new rack::ui::Menu;

	if (!port) {
 		rack::ui::MenuLabel* item = new rack::ui::MenuLabel;
 		item->text = "(No device)";
 		menu->addChild(item);
		return menu;
	}

	MidiDeviceValueItem* item = new MidiDeviceValueItem;
	item->port = port;
	item->midiIO = midiIO;
	item->deviceId = -1;
	item->text = "(No device)";
	item->rightText = CHECKMARK(item->deviceId == port->deviceId);
	menu->addChild(item);
	
	for (int deviceId : port->getDeviceIds()) {
		MidiDeviceValueItem* item = new MidiDeviceValueItem;
		item->port = port;
		item->midiIO = midiIO;
		item->deviceId = deviceId;
		item->text = port->getDeviceName(deviceId);
		item->rightText = CHECKMARK(item->deviceId == port->deviceId);
		menu->addChild(item);
	}

	return menu;
}

rack::ui::Menu* MidiChannelItem::createChildMenu() {
	rack::ui::Menu* menu = new rack::ui::Menu;

	if (!port)
		return menu;

	for (int channel : port->getChannels()) {
		MidiChannelValueItem* item = new MidiChannelValueItem;
		item->port = port;
		item->midiIO = midiIO;
		item->channel = channel;
		item->text = port->getChannelName(channel);
		item->rightText = CHECKMARK(item->channel == port->channel);
		menu->addChild(item);
	}

	return menu;
}

void MidiMenuBuilder::build(rack::ui::Menu* menu, MidiInputOutput* midiIO) {

		rack::ui::MenuLabel* item = new rack::ui::MenuLabel;
	 	item->text = "MIDI Configuration";
	 	menu->addChild(item);

		if(input) {

			MidiDriverItem* midiInputDriverItem = new MidiDriverItem;
			midiInputDriverItem->text = "Input Driver";
			midiInputDriverItem->rightText = RIGHT_ARROW;
			midiInputDriverItem->midiIO = midiIO;
			midiInputDriverItem->port = &midiIO->input;
			menu->addChild(midiInputDriverItem);

			MidiDeviceItem* midiInputDeviceItem = new MidiDeviceItem;
			midiInputDeviceItem->text = "Input Device";
			midiInputDeviceItem->rightText = RIGHT_ARROW;
			midiInputDeviceItem->midiIO = midiIO;
			midiInputDeviceItem->port = &midiIO->input;
			menu->addChild(midiInputDeviceItem);

			if(channel) {
				MidiChannelItem* midiInputChannelItem = new MidiChannelItem;
				midiInputChannelItem->text = "Input Channel";
				midiInputChannelItem->rightText = RIGHT_ARROW;
				midiInputChannelItem->midiIO = midiIO;
				midiInputChannelItem->port = &midiIO->input;
				menu->addChild(midiInputChannelItem);
			}
		}

		if(output) {

			MidiDriverItem* midiOutputDriverItem = new MidiDriverItem;
			midiOutputDriverItem->text = "Output Driver";
			midiOutputDriverItem->rightText = RIGHT_ARROW;
			midiOutputDriverItem->midiIO = midiIO;
			midiOutputDriverItem->port = &midiIO->output;
			menu->addChild(midiOutputDriverItem);

			MidiDeviceItem* midiOutputDeviceItem = new MidiDeviceItem;
			midiOutputDeviceItem->text = "Output Device";
			midiOutputDeviceItem->rightText = RIGHT_ARROW;
			midiOutputDeviceItem->midiIO = midiIO;
			midiOutputDeviceItem->port = &midiIO->output;
			menu->addChild(midiOutputDeviceItem);

			if(channel) {
				MidiChannelItem* midiOutputChannelItem = new MidiChannelItem;
				midiOutputChannelItem->text = "Output Channel";
				midiOutputChannelItem->rightText = RIGHT_ARROW;
				midiOutputChannelItem->midiIO = midiIO;
				midiOutputChannelItem->port =& midiIO->output;
				menu->addChild(midiOutputChannelItem);
			}

		}

}