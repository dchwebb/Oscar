#pragma once

#include "initialisation.h"
#include "USBHandler.h"
#include "CDCHandler.h"
#include <list>

class USB;

class MidiHandler : public USBHandler {
public:
	MidiHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = xfer_buff;
	}
	void DataIn() override;
	void DataOut() override;
	void ActivateEP() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;
	uint32_t GetInterfaceDescriptor(const uint8_t** buffer) override;

	void SendData(uint8_t* buffer, uint32_t size);

	static const uint8_t Descriptor[];
	static constexpr uint8_t MidiClassDescSize = 50;		// size of just the MIDI class definition (excluding interface descriptors)

private:
	void MidiEvent(const uint32_t data);
	uint32_t xfer_buff[64];									// OUT Data filled in RxLevel Interrupt

	uint8_t sysEx[32];
	uint8_t sysExCount = 0;
};
