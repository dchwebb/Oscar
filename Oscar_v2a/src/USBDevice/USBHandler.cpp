#include <USBHandler.h>
#include "USB.h"

USBHandler::USBHandler(USB* usb, const uint8_t inEP, const uint8_t outEP, int8_t interface) : usb(usb), inEP(inEP), outEP(outEP), interface(interface) {
	if (interface >= 0) {
		usb->classesByInterface[interface] = this;
	}
	usb->classByEP[outEP] = this;
}

void USBHandler::EndPointTransfer(const Direction d, const uint8_t ep, const uint32_t len)
{
	usb->EPStartXfer(d, ep, len);
}


void USBHandler::EndPointActivate(const uint8_t ep, const Direction d, const EndPointType eptype)
{
	usb->ActivateEndpoint(ep, d, static_cast<USB::EndPointType>(eptype));
}

void USBHandler::SetupIn(const uint32_t size, const uint8_t* buff)
{
	usb->EP0In(buff, size);
}


void EP0Handler::ActivateEP()
{
}

void EP0Handler::DataIn()
{
}

void EP0Handler::DataOut()
{
}

void EP0Handler::ClassSetup(usbRequest& req)
{
}

void EP0Handler::ClassSetupData(usbRequest& req, const uint8_t* data)
{
}
