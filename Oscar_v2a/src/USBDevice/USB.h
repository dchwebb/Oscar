#pragma once

#include "initialisation.h"
#include "USBHandler.h"
#include "CDCHandler.h"
#include "MidiHandler.h"
#include <functional>
#include <cstring>

// Enables capturing of debug data for output over STLink UART on dev boards
#define USB_DEBUG false
#if (USB_DEBUG)
//#include "uartHandler.h"
#define USB_DEBUG_COUNT 200
#endif


// USB Hardware Registers
#define USBx_PCGCCTL	*(__IO uint32_t *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_PCGCCTL_BASE)
#define USBx_DEVICE		((USB_OTG_DeviceTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)	((USB_OTG_INEndpointTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)	((USB_OTG_OUTEndpointTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_DFIFO(i)	*(uint32_t*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))

//#define LOBYTE(x)  ((uint8_t)(x & 0x00FF))
//#define HIBYTE(x)  ((uint8_t)((x & 0xFF00) >> 8))
static constexpr uint8_t LOBYTE(uint32_t x) {
	return (uint8_t)(x & 0x00FF);
}
static constexpr uint8_t HIBYTE(uint32_t x) {
	return (uint8_t)((x & 0xFF00) >> 8);
}

struct USB {
	friend class USBHandler;
public:
	enum Interface {NoInterface = -1, AudioInterface = 0, MidiInterface = 1, CDCCmdInterface = 2, CDCDataInterface = 3, interfaceCount = 4};
	enum EndPoint {Midi_In = 0x81, Midi_Out = 0x1, CDC_In = 0x82, CDC_Out = 0x2, CDC_Cmd = 0x83, };
	enum EndPointType { Control = 0, Isochronous = 1, Bulk = 2, Interrupt = 3 };
	enum class DeviceState {Default, Addressed, Configured, Suspended};
	enum RequestRecipient {RequestRecipientDevice = 0x0, RequestRecipientInterface = 0x1, RequestRecipientEndpoint = 0x2};
	enum RequestType {RequestTypeStandard = 0x0, RequestTypeClass = 0x20, RequestTypeVendor = 0x40};
	enum class Request {GetStatus = 0x0, SetAddress = 0x5, GetDescriptor = 0x6, SetConfiguration = 0x9};
	enum Descriptor {DeviceDescriptor = 0x1, ConfigurationDescriptor = 0x2, StringDescriptor = 0x3, InterfaceDescriptor = 0x4, EndpointDescriptor = 0x5, DeviceQualifierDescriptor = 0x6, IadDescriptor = 0xb, BosDescriptor = 0xF, ClassSpecificInterfaceDescriptor = 0x24};
	enum PacketStatus {GlobalOutNAK = 1, OutDataReceived = 2, OutTransferCompleted = 3, SetupTransComplete = 4, SetupDataReceived = 6};
	enum StringIndex {LangId = 0, Manufacturer = 1, Product = 2, Serial = 3, Configuration = 4, MassStorageClass = 5,
		CommunicationClass = 6, AudioClass = 7};

	static constexpr uint32_t requestTypeMask = 0x60;
	static constexpr uint32_t requestDirectionMask = 0x80;
	static constexpr uint32_t epAddressMask = 0xF;
	static constexpr uint8_t ep_maxPacket = 0x40;

	void InterruptHandler();
	void Init(bool softReset);
	void Disable();
	size_t SendData(const uint8_t *data, uint16_t len, uint8_t endpoint);
	void SendString(const char* s);
	void SendString(const std::string s);
	size_t SendString(const unsigned char* s, size_t len);
	void PauseEndpoint(USBHandler& handler);
	void ResumeEndpoint(USBHandler& handler);
	uint32_t StringToUnicode(const std::string_view desc, uint8_t* unicode);

	EP0Handler ep0   = EP0Handler(this, 0, 0, NoInterface);
	MidiHandler midi = MidiHandler(this, USB::Midi_In, USB::Midi_Out, MidiInterface);
	CDCHandler cdc   = CDCHandler(this,  USB::CDC_In,  USB::CDC_Out,  CDCCmdInterface);

	bool classPendingData = false;			// Set when class setup command received and data pending
	DeviceState devState;

private:
	enum class EP0State {Idle, Setup, DataIn, DataOut, StatusIn, StatusOut, Stall};

	static constexpr std::string_view manufacturerString 	= "Mountjoy Modular";
	static constexpr std::string_view productString 		= "Mountjoy Oscar";
	static constexpr std::string_view cfgString 			= "Mountjoy Oscar Config";
	static constexpr std::string_view cdcString 			= "Mountjoy Oscar Serial";
	static constexpr std::string_view midiString			= "Mountjoy Oscar MIDI";
	static constexpr uint8_t selfPowered = 0;				// Set to 1 if self powered
	static constexpr uint8_t usbSerialNoSize = 24;
	static constexpr uint32_t usbTimeout = 0xF000000;

	void ActivateEndpoint(uint8_t endpoint, const Direction direction, const EndPointType eptype);
	void DeactivateEndpoint(uint8_t endpoint, const Direction direction);

	void ReadPacket(const uint32_t* dest, uint16_t len, uint32_t offset);
	void WritePacket(const uint8_t* src, uint8_t endpoint, uint32_t len);
	void GetDescriptor();
	void StdDevReq();
	void EPStartXfer(const Direction direction, uint8_t endpoint, uint32_t xfer_len);
	void EP0In(const uint8_t* buff, uint32_t size);
	void CtlError();
	bool ReadInterrupts(const uint32_t interrupt);
	uint32_t MakeConfigDescriptor();
	void SerialToUnicode();

	std::array<USBHandler*, interfaceCount>classesByInterface;		// Lookup tables to get appropriate class handlers (set in handler constructor)
	std::array<USBHandler*, 3>classByEP;
	EP0State ep0State;
	bool transmitting;
	usbRequest req;

	uint8_t stringDescr[128];
	uint8_t configDescriptor[255];

	// USB standard device descriptor
	static constexpr uint16_t VendorID = 1155;	// STMicroelectronics
	static constexpr uint16_t ProductId = 65446;

	const uint8_t deviceDescr[0x12] = {
			0x12,								// bLength
			DeviceDescriptor,					// bDescriptorType
			0x01,								// bcdUSB  - 0x01 if LPM enabled
			0x02,
			0xEF,								// bDeviceClass: (Miscellaneous)
			0x02,								// bDeviceSubClass (Interface Association Descriptor- with below)
			0x01,								// bDeviceProtocol (Interface Association Descriptor)
			ep_maxPacket,  						// bMaxPacketSize
			LOBYTE(VendorID),					// idVendor
			HIBYTE(VendorID),					// idVendor
			LOBYTE(ProductId),					// idProduct
			HIBYTE(ProductId),					// idProduct
			0x00,								// bcdDevice rel. 2.00
			0x02,
			StringIndex::Manufacturer,			// Index of manufacturer  string
			StringIndex::Product,				// Index of product string
			StringIndex::Serial,				// Index of serial number string
			0x01								// bNumConfigurations
	};


	// Binary Object Store (BOS) Descriptor
	const uint8_t bosDescr[12] = {
			0x05,								// Length
			BosDescriptor,						// DescriptorType
			0x0C,								// TotalLength
			0x00, 0x01,							// NumDeviceCaps

			// USB 2.0 Extension Descriptor: device capability
			0x07,								// bLength
			0x10, 								// USB_DEVICE_CAPABITY_TYPE
			0x02,								// Attributes
			0x02, 0x00, 0x00, 0x00				// Link Power Management protocol is supported
	};


	// USB language indentifier descriptor
	static constexpr uint16_t languageIDString = 1033;
	const uint8_t langIDDescr[4] = {
			4,
			StringDescriptor,
			LOBYTE(languageIDString),
			HIBYTE(languageIDString)
	};

	const uint8_t deviceQualifierDescr[10] = {
			10,
			DeviceQualifierDescriptor,
			0x01,			// bcdUSB
			0x02,
			0xEF,			// bDeviceClass
			0x02,			// bDeviceSubClass
			0x01,			// bDeviceProtocol
			ep_maxPacket,	// bMaxPacketSize0
			0x01,			// bNumConfigurations
			0x00,			// bReserved
	};



public:
#if (USB_DEBUG)
	uint16_t usbDebugNo = 0;
	uint16_t usbDebugEvent = 0;

	struct usbDebugItem {
		uint16_t eventNo;
		uint32_t Interrupt;
		uint32_t IntData;
		usbRequest Request;
		uint8_t endpoint;
		uint8_t scsiOpCode;
		uint16_t PacketSize;
		uint32_t xferBuff[4];
	};
	usbDebugItem usbDebug[USB_DEBUG_COUNT];
	void OutputDebug();

	// Update the current debug record
	void USBUpdateDbg(uint32_t IntData, usbRequest request, uint8_t endpoint, uint16_t PacketSize, uint32_t scsiOpCode, uint32_t* xferBuff)
	{
		if (IntData) usbDebug[usbDebugNo].IntData = IntData;
		if (((uint32_t*)&request)[0]) usbDebug[usbDebugNo].Request = request;
		if (endpoint) usbDebug[usbDebugNo].endpoint = endpoint;
		if (PacketSize) usbDebug[usbDebugNo].PacketSize = PacketSize;
		if (scsiOpCode) usbDebug[usbDebugNo].scsiOpCode = scsiOpCode;
		if (xferBuff != nullptr) {
			usbDebug[usbDebugNo].xferBuff[0] = xferBuff[0];
			usbDebug[usbDebugNo].xferBuff[1] = xferBuff[1];
			usbDebug[usbDebugNo].xferBuff[2] = xferBuff[2];
			usbDebug[usbDebugNo].xferBuff[3] = xferBuff[3];
		}
	}
#else
#define USBUpdateDbg(IntData, request, endpoint, PacketSize, scsiOpCode, xferBuff)
#endif

};

extern USB usb;
