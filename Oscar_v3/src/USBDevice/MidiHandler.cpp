#include <MidiEvents.h>
#include "MidiHandler.h"
#include "USB.h"

void MidiHandler::SendData(uint8_t* buffer, uint32_t size)
{
	usb->SendData(buffer, size, inEP);
}

void MidiHandler::DataIn()
{

}

void MidiHandler::DataOut()
{
	// Handle incoming midi command here
	for (uint32_t i = 0; i < outBuffCount; ++i) {
		uint8_t* buffBytes = (uint8_t*)outBuff;							// outBuff is uint32_t - need it as bytes
		midiEvents.QueueAdd(MidiEvents::QueueType::USB, buffBytes[i]);
	}

}


void MidiHandler::ActivateEP()
{
	EndPointActivate(USB::Midi_In,   Direction::in,  EndPointType::Bulk);
	EndPointActivate(USB::Midi_Out,  Direction::out, EndPointType::Bulk);

	EndPointTransfer(Direction::out, USB::Midi_Out, USB::ep_maxPacket);
}

void MidiHandler::ClassSetup(usbRequest& req)
{
}


void MidiHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{

}



// Descriptor definition here as requires constants from USB class
const uint8_t MidiHandler::Descriptor[] = {
	// B.3.1 Standard Audio Control standard Interface Descriptor
	0x09,									// length of descriptor in bytes
	USB::InterfaceDescriptor,				// interface descriptor type
	USB::AudioInterface,					// index of this interface
	0x00,									// alternate setting for this interface
	0x00,									// endpoints excl 0: number of endpoint descriptors to follow
	0x01,									// AUDIO
	0x01,									// AUDIO_Control
	0x00,									// bInterfaceProtocol
	USB::AudioClass,						// string index for interface

	// B.3.2 Class-specific AC Interface Descriptor
	0x09,									// length of descriptor in bytes
	USB::ClassSpecificInterfaceDescriptor,	// descriptor type
	0x01,									// header functional descriptor
	0x00, 0x01,								// bcdADC
	0x09, 0x00,								// wTotalLength
	0x01,									// bInCollection
	0x01,									// baInterfaceNr[1]

	// B.4 MIDIStreaming Interface Descriptors
	// B.4.1 Standard MS Interface Descriptor
	0x09,									// bLength
	USB::InterfaceDescriptor,				// bDescriptorType: interface descriptor
	USB::MidiInterface,						// bInterfaceNumber
	0x00,									// bAlternateSetting
	0x02,									// bNumEndpoints
	0x01,									// bInterfaceClass: Audio
	0x03,									// bInterfaceSubClass: MIDIStreaming
	0x00,									// InterfaceProtocol
	USB::AudioClass,						// iInterface: String Descriptor

	// B.4.2 Class-specific MS Interface Descriptor
	0x07,									// length of descriptor in bytes
	USB::ClassSpecificInterfaceDescriptor,	// bDescriptorType: Class Specific Interface Descriptor
	0x01,									// header functional descriptor
	0x0, 0x01,								// bcdADC
	MidiHandler::MidiClassDescSize, 0,		// wTotalLength

	// B.4.3 MIDI IN Jack Descriptor (Embedded)
	0x06,									// bLength
	USB::ClassSpecificInterfaceDescriptor,	// descriptor type
	0x02,									// bDescriptorSubtype: MIDI_IN_JACK
	0x01,									// bJackType: Embedded
	0x01,									// bJackID
	0x00,									// iJack: No String Descriptor

	// Table B4.4 Midi Out Jack Descriptor (Embedded)
	0x09,									// length of descriptor in bytes
	USB::ClassSpecificInterfaceDescriptor,	// descriptor type
	0x03,									// MIDI_OUT_JACK descriptor
	0x01,									// bJackType: Embedded
	0x02,									// bJackID
	0x01,									// No of input pins
	0x01,									// ID of the Entity to which this Pin is connected.
	0x01,									// Output Pin number of the Entity to which this Input Pin is connected.
	0x00,									// iJack

	//B.5.1 Standard Bulk OUT Endpoint Descriptor
	0x09,									// bLength
	USB::EndpointDescriptor,				// bDescriptorType = endpoint
	USB::Midi_Out,							// bEndpointAddress
	USB::Bulk,								// bmAttributes: 2:Bulk
	LOBYTE(USB::ep_maxPacket),				// wMaxPacketSize
	HIBYTE(USB::ep_maxPacket),
	0x00,									// bInterval in ms : ignored for bulk
	0x00,									// bRefresh Unused
	0x00,									// bSyncAddress Unused

	// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
	0x05,									// bLength of descriptor in bytes
	0x25,									// bDescriptorType (Audio Endpoint Descriptor)
	0x01,									// bDescriptorSubtype: MS General
	0x01,									// bNumEmbMIDIJack
	0x01,									// baAssocJackID ID of the Embedded MIDI IN Jack.

	//B.6.1 Standard Bulk IN Endpoint Descriptor
	0x09,									// bLength
	USB::EndpointDescriptor,				// bDescriptorType = endpoint
	USB::Midi_In,							// bEndpointAddress IN endpoint number 3
	USB::Bulk,								// bmAttributes: 2: Bulk, 3: Interrupt endpoint
	LOBYTE(USB::ep_maxPacket),				// wMaxPacketSize
	HIBYTE(USB::ep_maxPacket),
	0x00,									// bInterval in ms
	0x00,									// bRefresh
	0x00,									// bSyncAddress

	// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
	0x05,									// bLength of descriptor in bytes
	0x25,									// bDescriptorType
	0x01,									// bDescriptorSubtype
	0x01,									// bNumEmbMIDIJack
	0x02,									// baAssocJackID ID of the Embedded MIDI OUT Jack

};


uint32_t MidiHandler::GetInterfaceDescriptor(const uint8_t** buffer) {
	*buffer = Descriptor;
	return sizeof(Descriptor);
}

