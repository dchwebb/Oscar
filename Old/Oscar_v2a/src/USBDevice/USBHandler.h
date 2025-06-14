#pragma once
#include "initialisation.h"

class USB;

enum class Direction {in, out};
enum EndPointType {Control = 0, Isochronous = 1, Bulk = 2, Interrupt = 3};

struct usbRequest {
	uint8_t RequestType;
	uint8_t Request;
	uint16_t Value;
	uint16_t Index;
	uint16_t Length;

	void loadData(const uint8_t* data) {
		RequestType = data[0];
		Request = data[1];
		Value = (uint16_t)(data[2]) + (data[3] << 8);
		Index = (uint16_t)(data[4]) + (data[5] << 8);
		Length = (uint16_t)(data[6]) + (data[7] << 8);
	}
} ;

// interface for USB class handlers
class USBHandler {
public:
	USB* usb;
	USBHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface);

	uint8_t inEP;
	uint8_t outEP;
	int8_t interface;

	uint32_t* outBuff;			// Pointer to end point's OUT (receive from host) buffer [Previously xfer_buff]
	uint32_t outBuffCount;		// Number of bytes received in OUT packet [Previously xfer_count]
	uint32_t outBuffPackets;	// If transfer is larger than maximum packet size store remaining byte count [Previously xfer_packets]
	uint32_t outBuffOffset;		// When receiving multiple packets in RxLevel Interrupt this applies offset [Previously xfer_offset]

	const uint8_t* inBuff;		// Pointer to IN buffer (device to host)
	uint32_t inBuffSize;
	uint32_t inBuffRem;			// If transfer is larger than maximum packet size store remaining byte count
	uint32_t inBuffCount;		// Number of bytes already sent to host from a large packet

	virtual void DataIn() = 0;
	virtual void DataOut() = 0;
	virtual void ActivateEP() = 0;
	virtual void ClassSetup(usbRequest& req) = 0;
	virtual void ClassSetupData(usbRequest& req, const uint8_t* data) = 0;
	virtual uint32_t GetInterfaceDescriptor(const uint8_t** buffer) = 0;
protected:
   // Proxy functions to allow access to USB private methods
   void EndPointTransfer(Direction d, uint8_t ep, uint32_t len);
   void EndPointActivate(const uint8_t ep, const Direction d, const EndPointType eptype);
   void SetupIn(uint32_t size, const uint8_t* buff);
};

class EP0Handler : public USBHandler {
public:
	EP0Handler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = ep0OutBuff;
	}

	void DataIn() override;
	void DataOut() override;
	void ActivateEP() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;
	uint32_t GetInterfaceDescriptor(const uint8_t** buffer) override {return 0;};

private:
	uint32_t ep0OutBuff[64];		// EP0 OUT Data filled in RxLevel Interrupt
};
