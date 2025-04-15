//#include <MidiHandler.h>
#include "USB.h"

USB usb;

volatile bool debugStart = true;


// procedure to allow classes to pass configuration data back via endpoint 1 (eg CDC line setup, MSC MaxLUN etc)
void USB::EP0In(const uint8_t* buff, const uint32_t size)
{
	ep0.inBuffSize = std::min(size, static_cast<uint32_t>(req.Length));
	ep0.inBuff = buff;
	ep0State = EP0State::DataIn;

	USBUpdateDbg({}, {}, {}, ep0.inBuffSize, {}, (uint32_t*)ep0.inBuff);

	EPStartXfer(Direction::in, 0, ep0.inBuffSize);		// sends blank request back
}


void USB::InterruptHandler()						// In Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pcd.c
{
	uint32_t endpoint, epInterrupts, epInt;

	// Handle spurious interrupt
	if ((USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK) == 0) {
		return;
	}

	///////////		10			RXFLVL: RxQLevel Interrupt:  Rx FIFO non-empty Indicates that there is at least one packet pending to be read from the Rx FIFO.
	if (ReadInterrupts(USB_OTG_GINTSTS_RXFLVL))	{
		USB_OTG_FS->GINTMSK &= ~USB_OTG_GINTSTS_RXFLVL;

		const uint32_t receiveStatus = USB_OTG_FS->GRXSTSP;		// OTG status read and pop register: not shown in SFR, but read only (ie do not pop) register under OTG_FS_GLOBAL->FS_GRXSTSR_Device
		endpoint = receiveStatus & USB_OTG_GRXSTSP_EPNUM;			// Get the endpoint number
		const uint16_t packetSize = (receiveStatus & USB_OTG_GRXSTSP_BCNT) >> 4;

		USBUpdateDbg(receiveStatus, {}, endpoint, packetSize, {}, nullptr);

		if (((receiveStatus & USB_OTG_GRXSTSP_PKTSTS) >> 17) == OutDataReceived && packetSize != 0) {	// 2 = OUT data packet received
			ReadPacket(classByEP[endpoint]->outBuff, packetSize, classByEP[endpoint]->outBuffOffset);
			USBUpdateDbg({}, {}, {}, {}, {}, classByEP[endpoint]->outBuff + classByEP[endpoint]->outBuffOffset);
			if (classByEP[endpoint]->outBuffPackets > 1) {
				classByEP[endpoint]->outBuffOffset += (packetSize / 4);			// When receiving multiple packets increase buffer offset (packet size in bytes -> 32 bit ints)
			}
		} else if (((receiveStatus & USB_OTG_GRXSTSP_PKTSTS) >> 17) == SetupDataReceived) {				// 6 = SETUP data packet received
			ReadPacket(classByEP[endpoint]->outBuff, 8U, 0);
			USBUpdateDbg({}, {}, {}, {}, {}, classByEP[endpoint]->outBuff);
		} else if (((receiveStatus & USB_OTG_GRXSTSP_PKTSTS) >> 17) == OutTransferCompleted) {			// 3 = transfer completed
			classByEP[endpoint]->outBuffOffset = 0;
		}
		if (packetSize != 0) {
			classByEP[endpoint]->outBuffCount = packetSize;
		}
		USB_OTG_FS->GINTMSK |= USB_OTG_GINTSTS_RXFLVL;
	}

	/////////// 	80000 		OEPINT OUT endpoint interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_OEPINT)) {

		// Read the output endpoint interrupt register to ascertain which endpoint(s) fired an interrupt
		epInterrupts = ((USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK) & USB_OTG_DAINTMSK_OEPM_Msk) >> 16;

		// process each endpoint in turn incrementing the epnum and checking the interrupts (ep_intr) if that endpoint fired
		endpoint = 0;
		while (epInterrupts != 0) {
			if (epInterrupts & 1) {
				epInt = USBx_OUTEP(endpoint)->DOEPINT & USBx_DEVICE->DOEPMSK;

				USBUpdateDbg(epInt, {}, endpoint, {}, {}, nullptr);

				if (epInt & USB_OTG_DOEPINT_XFRC) {		// 0x01 Transfer completed
					USBx_OUTEP(endpoint)->DOEPINT = USB_OTG_DOEPINT_XFRC;				// Clear interrupt
					if (endpoint == 0) {
						if (devState == DeviceState::Configured && classPendingData) {
							if ((req.RequestType & requestTypeMask) == RequestTypeClass) {
								// Previous OUT interrupt contains instruction (eg host sending CDC LineCoding); next command sends data (Eg LineCoding data)
								classesByInterface[req.Index]->ClassSetupData(req, (uint8_t*)ep0.outBuff);
							}
							classPendingData = false;
							EPStartXfer(Direction::in, 0, 0);
						}

						ep0State = EP0State::Idle;
						USBx_OUTEP(endpoint)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
					} else {
						// Call appropriate data handler depending on endpoint of data
						if (classByEP[endpoint]->outBuffPackets <= 1) {
							EPStartXfer(Direction::out, endpoint, classByEP[endpoint]->outBuffCount);
						}
						classByEP[endpoint]->DataOut();
					}
				}

				if (epInt & USB_OTG_DOEPINT_STUP) {		// SETUP phase done: the application can decode the received SETUP data packet.
					// Parse Setup Request containing data in outBuff filled by RXFLVL interrupt
					req.loadData((uint8_t*)classByEP[endpoint]->outBuff);

					USBUpdateDbg({}, req, {}, {}, {}, nullptr);

					ep0State = EP0State::Setup;

					switch (req.RequestType & 0x1F) {		// originally USBD_LL_SetupStage
					case RequestRecipientDevice:
						StdDevReq();
						break;

					case RequestRecipientInterface:
						if ((req.RequestType & requestTypeMask) == RequestTypeClass) {	// 0xA1 & 0x60 == 0x20

							// req.Index holds interface - locate which handler this relates to
							if (req.Length > 0) {
								classesByInterface[req.Index]->ClassSetup(req);
							} else {
								EPStartXfer(Direction::in, 0, 0);
							}
						}
						break;

					case RequestRecipientEndpoint:
						break;

					default:
						CtlError();
						break;
					}

					USBx_OUTEP(endpoint)->DOEPINT = USB_OTG_DOEPINT_STUP;				// Clear interrupt
				}

				if (epInt & USB_OTG_DOEPINT_OTEPDIS) {			// OUT token received when endpoint disabled
					USBx_OUTEP(endpoint)->DOEPINT = USB_OTG_DOEPINT_OTEPDIS;			// Clear interrupt
				}
				if (epInt & USB_OTG_DOEPINT_OTEPSPR) {			// Status Phase Received interrupt
					USBx_OUTEP(endpoint)->DOEPINT = USB_OTG_DOEPINT_OTEPSPR;			// Clear interrupt
				}
				if (epInt & USB_OTG_DOEPINT_NAK) {										// 0x2000 OUT NAK interrupt
					USBx_OUTEP(endpoint)->DOEPINT = USB_OTG_DOEPINT_NAK;				// Clear interrupt
				}
			}
			endpoint++;
			epInterrupts >>= 1;
		}

	}

	///////////		40000 		IEPINT: IN endpoint interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_IEPINT)) {

		// Read in the device interrupt bits [initially 1]
		epInterrupts = (USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK) & USB_OTG_DAINTMSK_IEPM_Msk;

		// process each endpoint in turn incrementing the epnum and checking the interrupts (ep_intr) if that endpoint fired
		endpoint = 0;
		while (epInterrupts != 0) {
			if (epInterrupts & 1) {

				epInt = USBx_INEP(endpoint)->DIEPINT & (USBx_DEVICE->DIEPMSK | (((USBx_DEVICE->DIEPEMPMSK >> (endpoint & epAddressMask)) & 0x1) << 7));

				USBUpdateDbg(epInt, {}, endpoint, {}, {}, nullptr);

				if (epInt & USB_OTG_DIEPINT_XFRC) {						// 0x1 Transfer completed interrupt
					uint32_t fifoemptymsk = (0x1 << (endpoint & epAddressMask));
					USBx_DEVICE->DIEPEMPMSK &= ~fifoemptymsk;
					USBx_INEP(endpoint)->DIEPINT = USB_OTG_DIEPINT_XFRC;

					if (endpoint == 0) {
						if (ep0State == EP0State::DataIn && ep0.inBuffRem == 0) {
							ep0State = EP0State::StatusOut;								// After completing transmission on EP0 send an out packet [HAL_PCD_EP_Receive]
							EPStartXfer(Direction::out, 0, ep_maxPacket);
						} else if (ep0State == EP0State::DataIn && ep0.inBuffRem > 0) {	// For EP0 long packets are sent separately rather than streamed out of the FIFO
							ep0.inBuffSize = ep0.inBuffRem;
							ep0.inBuffRem = 0;
							USBUpdateDbg({}, {}, {}, ep0.inBuffSize, {}, (uint32_t*)ep0.inBuff);
							EPStartXfer(Direction::in, 0, ep0.inBuffSize);
						}
					} else {
						classByEP[endpoint]->DataIn();
						transmitting = false;
					}
				}

				if (epInt & USB_OTG_DIEPINT_TXFE) {						// 0x80 Transmit FIFO empty
					USBUpdateDbg({}, {}, {}, classByEP[endpoint]->inBuffSize, {}, (uint32_t*)classByEP[endpoint]->inBuff);

					if (endpoint == 0) {
						if (ep0.inBuffSize > ep_maxPacket) {
							ep0.inBuffRem = classByEP[endpoint]->inBuffSize - ep_maxPacket;
							ep0.inBuffSize = ep_maxPacket;
						}
						WritePacket(ep0.inBuff, endpoint, ep0.inBuffSize);
						ep0.inBuff += ep0.inBuffSize;					// Move pointer forwards
						const uint32_t fifoemptymsk = 1;
						USBx_DEVICE->DIEPEMPMSK &= ~fifoemptymsk;
					} else {
						// For regular endpoints keep writing packets to the FIFO while space available [PCD_WriteEmptyTxFifo]
						uint16_t len = std::min(classByEP[endpoint]->inBuffSize - classByEP[endpoint]->inBuffCount, (uint32_t)ep_maxPacket);
						uint16_t len32b = (len + 3) / 4;				// FIFO size is in 4 byte words

						// INEPTFSAV[15:0]: IN endpoint Tx FIFO space available: 0x0: Endpoint Tx FIFO is full; 0x1: 1 31-bit word available; 0xn: n words available
						while (((USBx_INEP(endpoint)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= len32b) && (classByEP[endpoint]->inBuffCount < classByEP[endpoint]->inBuffSize) && (classByEP[endpoint]->inBuffSize != 0)) {
							len = std::min(classByEP[endpoint]->inBuffSize - classByEP[endpoint]->inBuffCount, (uint32_t)ep_maxPacket);
							len32b = (len + 3) / 4;
							WritePacket(classByEP[endpoint]->inBuff, endpoint, len);
							classByEP[endpoint]->inBuff += len;
							classByEP[endpoint]->inBuffCount += len;
						}

						if (classByEP[endpoint]->inBuffSize <= classByEP[endpoint]->inBuffCount) {
							const uint32_t fifoemptymsk = (0x1 << (endpoint & epAddressMask));
							USBx_DEVICE->DIEPEMPMSK &= ~fifoemptymsk;
						}
					}
				}

				if (epInt & USB_OTG_DIEPINT_TOC) {						// Timeout condition
					USBx_INEP(endpoint)->DIEPINT = USB_OTG_DIEPINT_TOC;
				}
				if (epInt & USB_OTG_DIEPINT_ITTXFE) {					// IN token received when Tx FIFO is empty
					USBx_INEP(endpoint)->DIEPINT = USB_OTG_DIEPINT_ITTXFE;
				}
				if (epInt & USB_OTG_DIEPINT_INEPNE) {					// IN endpoint NAK effective
					USBx_INEP(endpoint)->DIEPINT = USB_OTG_DIEPINT_INEPNE;
				}
				if (epInt & USB_OTG_DIEPINT_EPDISD) {					// Endpoint disabled interrupt
					USBx_INEP(endpoint)->DIEPINT = USB_OTG_DIEPINT_EPDISD;
				}

			}
			endpoint++;
			epInterrupts >>= 1;
		}

	}


	/////////// 	800 		USBSUSP: Suspend Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_USBSUSP)) {
		if (USBx_DEVICE->DSTS & USB_OTG_DSTS_SUSPSTS) {
			devState = DeviceState::Suspended;
			USBx_PCGCCTL |= USB_OTG_PCGCCTL_STOPCLK;
		}
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_USBSUSP;
	}


	/////////// 	1000 		USBRST: Reset Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_USBRST))	{
		USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_RWUSIG;

		volatile uint32_t count = 0;
		USB_OTG_FS->GRSTCTL = (USB_OTG_GRSTCTL_TXFFLSH | (0x10 << USB_OTG_GRSTCTL_TXFNUM_Pos));			// Flush all TX Fifos
		while (count++ < usbTimeout && (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH));

		for (int i = 0; i < 6; i++) {				// hpcd->Init.dev_endpoints
			USBx_INEP(i)->DIEPINT = 0xFB7F;		// see p1177 for explanation: based on datasheet should be more like 0b10100100111011
			USBx_INEP(i)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
			USBx_OUTEP(i)->DOEPINT = 0xFB7F;
			USBx_OUTEP(i)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
		}
		USBx_DEVICE->DAINTMSK |= 0x10001;
		USBx_DEVICE->DOEPMSK |= USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM | USB_OTG_DOEPMSK_EPDM | USB_OTG_DOEPMSK_OTEPSPRM;			//  | USB_OTG_DOEPMSK_NAKM
		USBx_DEVICE->DIEPMSK |= USB_OTG_DIEPMSK_TOM | USB_OTG_DIEPMSK_XFRCM | USB_OTG_DIEPMSK_EPDM;
		USBx_DEVICE->DCFG &= ~USB_OTG_DCFG_DAD;		// Clear Device Address (will be set when address instruction received from host)

		// setup EP0 to receive SETUP packets
		if ((USBx_OUTEP(0)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) != USB_OTG_DOEPCTL_EPENA)	{
			// Set PKTCNT to 1, XFRSIZ to 24, STUPCNT to 3 (number of back-to-back SETUP data packets endpoint can receive)
			USBx_OUTEP(0)->DOEPTSIZ = (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | (3 * 8) | USB_OTG_DOEPTSIZ_STUPCNT;
		}
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_USBRST;
	}


	/////////// 	2000		ENUMDNE: Enumeration done Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_ENUMDNE)) {
		// Set the Maximum packet size of the IN EP based on the enumeration speed
		USBx_INEP(0)->DIEPCTL &= ~USB_OTG_DIEPCTL_MPSIZ;
		USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGINAK;		//  Clear global IN NAK

		// Assuming Full Speed USB and clock > 32MHz Set USB Turnaround time
		USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
		USB_OTG_FS->GUSBCFG |= (6 << USB_OTG_GUSBCFG_TRDT_Pos);

		ActivateEndpoint(0, Direction::out, Control);			// Open EP0 OUT
		ActivateEndpoint(0, Direction::in, Control);			// Open EP0 IN

		ep0State = EP0State::Idle;
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_ENUMDNE;
	}


	///////////		40000000	SRQINT: Connection event Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_SRQINT))	{
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_SRQINT;
	}


	/////////// 	80000000	WKUINT: Resume Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_WKUINT)) {
		USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_RWUSIG;				// Clear the Remote Wake-up Signaling
		if (devState == DeviceState::Suspended) {
			devState = DeviceState::Default;
		}
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_WKUINT;
	}


	/////////// OTGINT: Handle Disconnection event Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_OTGINT)) {
		uint32_t otgInt = USB_OTG_FS->GOTGINT;
		if (otgInt & USB_OTG_GOTGINT_SEDET) {
			devState = DeviceState::Default;
			Init(true);
		}
		USB_OTG_FS->GOTGINT |= otgInt;
	}

}


void USB::Init(bool softReset)
{
	volatile uint32_t count = 0;

	if (!softReset) {
		GpioPin::Init(GPIOA, 9, GpioPin::Type::Input);						// PA9: USB_OTG_HS_VBUS
		GpioPin::Init(GPIOA, 11, GpioPin::Type::AlternateFunction, 10);		// PA11: USB_OTG_HS_DM
		GpioPin::Init(GPIOA, 12, GpioPin::Type::AlternateFunction, 10);		// PA12: USB_OTG_HS_DP

		RCC->DCKCFGR2 |= RCC_DCKCFGR2_CK48MSEL;			// 0 = PLLQ Clock; 1 = PLLSAI_P
		RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;			// USB OTG FS clock enable
		RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;			// Enable system configuration clock: used to manage external interrupt line connection to GPIOs

		NVIC_SetPriority(OTG_FS_IRQn, 2);
		NVIC_EnableIRQ(OTG_FS_IRQn);
	}

	USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_PWRDWN;			// Activate the transceiver in transmission/reception. When reset, the transceiver is kept in power-down. 0 = USB FS transceiver disabled; 1 = USB FS transceiver enabled
	USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;		// Force USB device mode
	DelayMS(50);

	// Clear all transmit FIFO address and data lengths - these will be set to correct values below for endpoints 0,1 and 2
	for (uint8_t i = 0; i < 15; i++) {
		USB_OTG_FS->DIEPTXF[i] = 0;
	}

	USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBDEN; 			// Enable HW VBUS sensing
	USBx_DEVICE->DCFG |= USB_OTG_DCFG_DSPD;				// 11: Full speed using internal FS PHY

	USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFNUM_4;	// Select buffers to flush. 10000: Flush all the transmit FIFOs in device or host mode
	USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFFLSH;		// Flush the TX buffers
	while (++count < 100000 && (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH));

	USB_OTG_FS->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;		// Flush the RX buffers
	count = 0;
	while (++count < 100000 && (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH));

	USB_OTG_FS->GINTSTS = 0xBFFFFFFF;					// Clear pending interrupts (except SRQINT Session request/new session detected)

	// Enable interrupts
	USB_OTG_FS->GINTMSK = 0;							// Disable all interrupts
	USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_USBSUSPM |			// Receive FIFO non-empty mask; USB suspend
			USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |							// USB reset; Enumeration done
			USB_OTG_GINTMSK_IEPINT | USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_WUIM |	// IN endpoint; OUT endpoint; Resume/remote wakeup detected
			USB_OTG_GINTMSK_SRQIM | USB_OTG_GINTMSK_OTGINT;								// Session request/new session detected; OTG interrupt


	// NB - FIFO Sizes are in words NOT bytes. There is a total size of 320 (320x4 = 1280 bytes) available which is divided up thus:
	// FIFO		Start		Size
	// RX 		0			128
	// EP0 TX	128			64
	// EP1 TX	192			64
	// EP2 TX 	256			64

	USB_OTG_FS->GRXFSIZ = 128;		 					// Rx FIFO depth

	// Endpoint 0 Transmit FIFO size/address (as in device mode - this is also used as the non-periodic transmit FIFO size in host mode)
	USB_OTG_FS->DIEPTXF0_HNPTXFSIZ = (64 << USB_OTG_TX0FD_Pos) |		// IN Endpoint 0 Tx FIFO depth
			(128 << USB_OTG_TX0FSA_Pos);								// IN Endpoint 0 FIFO transmit RAM start address - this is offset from the RX FIFO (set above to 128)

	// Endpoint 1 FIFO size/address (address is offset from EP0 address+size above)
	USB_OTG_FS->DIEPTXF[0] = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |		// IN endpoint 1 Tx FIFO depth
			(192 << USB_OTG_DIEPTXF_INEPTXSA_Pos);  					// IN endpoint 1 FIFO transmit RAM start address

	// Endpoint 2 FIFO size/address (address is offset from EP1 address+size above)
	USB_OTG_FS->DIEPTXF[1] = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |		// IN endpoint 2 Tx FIFO depth
			(256 << USB_OTG_DIEPTXF_INEPTXSA_Pos);  					// IN endpoint 2 FIFO transmit RAM start address

    USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_SDIS;			// Activate USB
    USB_OTG_FS->GAHBCFG |= USB_OTG_GAHBCFG_GINT;		// Activate USB Interrupts

    transmitting = false;
}



void USB::Disable()
{
	volatile uint32_t count = 0;

    USBx_DEVICE->DCTL |= USB_OTG_DCTL_SDIS;				// Soft Disconnect
	DelayMS(50);

	USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_PWRDWN;			// Disable transceiver. When reset transceiver is kept in power-down. 0 = USB FS transceiver disabled; 1 = USB FS transceiver enabled
	USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_FDMOD;		// Disable USB device mode
	DelayMS(50);

	// Clear all transmit FIFO address and data lengths - these will be set to correct values below for endpoints 0,1 and 2
	for (uint8_t i = 0; i < 15; i++) {
		USB_OTG_FS->DIEPTXF[i] = 0;
	}

	USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBDEN; 			// Disable HW VBUS sensing

	USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFNUM_4;	// Select buffers to flush. 10000: Flush all the transmit FIFOs in device or host mode
	USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFFLSH;		// Flush the TX buffers
	while (++count < 100000 && (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH));

	USB_OTG_FS->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;		// Flush the RX buffers
	count = 0;
	while (++count < 100000 && (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH));

	USB_OTG_FS->GINTSTS = 0xBFFFFFFF;					// Clear pending interrupts (except SRQINT Session request/new session detected)
	USB_OTG_FS->GINTMSK = 0;							// Disable all interrupts

    USB_OTG_FS->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;		// Deactivate USB Interrupts
}


void USB::ActivateEndpoint(uint8_t endpoint, const Direction direction, const EndPointType eptype)
{
	endpoint = endpoint & 0xF;

	if (direction == Direction::in) {
		USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (1 << endpoint);

		if ((USBx_INEP(endpoint)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP) == 0) {
			USBx_INEP(endpoint)->DIEPCTL |=
					(ep_maxPacket & USB_OTG_DIEPCTL_MPSIZ) |
					((uint32_t)eptype << USB_OTG_DIEPCTL_EPTYP_Pos) |
					(endpoint << USB_OTG_DIEPCTL_TXFNUM_Pos) |
					USB_OTG_DIEPCTL_USBAEP;
		}
	} else {
		USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & ((1 << endpoint) << USB_OTG_DAINTMSK_OEPM_Pos);

		if (((USBx_OUTEP(endpoint)->DOEPCTL) & USB_OTG_DOEPCTL_USBAEP) == 0) {
			USBx_OUTEP(endpoint)->DOEPCTL |=
					(ep_maxPacket & USB_OTG_DOEPCTL_MPSIZ) |
					((uint32_t)eptype << USB_OTG_DOEPCTL_EPTYP_Pos) |
					USB_OTG_DOEPCTL_USBAEP;
		}
	}
}


void USB::DeactivateEndpoint(uint8_t endpoint, const Direction direction)
{
	endpoint = endpoint & 0xF;

	if (direction == Direction::in) {
		if (USBx_INEP(endpoint)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) {
			USBx_INEP(endpoint)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
			USBx_INEP(endpoint)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;
		}

		USBx_DEVICE->DEACHMSK &= ~(USB_OTG_DAINTMSK_IEPM & (uint32_t)(1 << (endpoint)));
		USBx_DEVICE->DAINTMSK &= ~(USB_OTG_DAINTMSK_IEPM & (uint32_t)(1 << (endpoint)));
		USBx_INEP(endpoint)->DIEPCTL &= ~(USB_OTG_DIEPCTL_USBAEP |
				USB_OTG_DIEPCTL_MPSIZ |
				USB_OTG_DIEPCTL_TXFNUM |
				USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
				USB_OTG_DIEPCTL_EPTYP);
	} else {
		if (USBx_OUTEP(endpoint)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) {
			USBx_OUTEP(endpoint)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
			USBx_OUTEP(endpoint)->DOEPCTL |= USB_OTG_DOEPCTL_EPDIS;
		}

		USBx_DEVICE->DEACHMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1 << endpoint) << USB_OTG_DAINTMSK_OEPM_Pos));
		USBx_DEVICE->DAINTMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1 << endpoint) << USB_OTG_DAINTMSK_OEPM_Pos));
		USBx_OUTEP(endpoint)->DOEPCTL &= ~(USB_OTG_DOEPCTL_USBAEP |
				USB_OTG_DOEPCTL_MPSIZ |
				USB_OTG_DOEPCTL_SD0PID_SEVNFRM |
				USB_OTG_DOEPCTL_EPTYP);
	}
}


void USB::ReadPacket(const uint32_t* dest, const uint16_t len, const uint32_t offset)
{
	// Read a packet from the RX FIFO
	uint32_t* pDest = (uint32_t*)dest + offset;
	const uint32_t count32b = ((uint32_t)len + 3) / 4;

	for (uint32_t i = 0; i < count32b; i++)	{
		*pDest = USBx_DFIFO(0);
		pDest++;
	}
}


void USB::WritePacket(const uint8_t* src, const uint8_t endpoint, const uint32_t len)
{
	uint32_t* pSrc = (uint32_t*)src;
	const uint32_t count32b = ((uint32_t)len + 3) / 4;

	for (uint32_t i = 0; i < count32b; i++) {
		USBx_DFIFO(endpoint) = *pSrc;
		pSrc++;
	}
}


void USB::GetDescriptor()
{
	uint32_t strSize;

	switch (req.Value >> 8)	{
	case DeviceDescriptor:
		return EP0In(deviceDescr, sizeof(deviceDescr));
		break;

	case ConfigurationDescriptor:
		return EP0In(configDescriptor, MakeConfigDescriptor());		// Construct config descriptor from individual classes
		break;

	case BosDescriptor:
		return EP0In(bosDescr, sizeof(bosDescr));
		break;

	case DeviceQualifierDescriptor:
		return EP0In(deviceQualifierDescr, sizeof(deviceQualifierDescr));
		break;

	case StringDescriptor:

		switch ((uint8_t)(req.Value)) {
		case StringIndex::LangId:				// 300
			return EP0In(langIDDescr, sizeof(langIDDescr));
			break;

		case StringIndex::Manufacturer:			// 301
			strSize = StringToUnicode(manufacturerString, stringDescr);
			return EP0In(stringDescr, strSize);
			break;

		case StringIndex::Product:				// 302
			strSize = StringToUnicode(productString, stringDescr);
			return EP0In(stringDescr, strSize);
			break;

		case StringIndex::Serial:				// 303
			SerialToUnicode();
			return EP0In(stringDescr, stringDescr[0]);				// length is 24 bytes (x2 for unicode padding) + 2 for header
			break;

		case StringIndex::Configuration:		// 304
	    	strSize = StringToUnicode(cfgString, stringDescr);
	    	return EP0In(stringDescr, strSize);
	    	break;

	    case StringIndex::CommunicationClass:	// 306
	    	strSize = StringToUnicode(cdcString, stringDescr);
	    	return EP0In(stringDescr, strSize);
			break;

	    case StringIndex::AudioClass:			// 307
	    	strSize = StringToUnicode(midiString, stringDescr);
	    	return EP0In(stringDescr, strSize);
			break;

		default:
			CtlError();
			return;
		}
		break;

		default:
			CtlError();
			return;
	}

	if (req.Length == 0) {
		EPStartXfer(Direction::in, 0, 0);			// FIXME - this never seems to happen
	}
}

uint32_t USB::MakeConfigDescriptor()
{
	// Construct the configuration descriptor from the various class descriptors with header
	static constexpr uint8_t descrHeaderSize = 9;
	uint32_t descPos = descrHeaderSize;
	for (auto c : classByEP) {
		if (c != nullptr) {
			const uint8_t* descBuff = nullptr;
			uint32_t descSize = c->GetInterfaceDescriptor(&descBuff);
			memcpy(&configDescriptor[descPos], descBuff, descSize);
			descPos += descSize;
		}
	}

	// Insert config descriptor header
	const uint8_t descriptorHeader[] = {
		0x09,								// bLength: Configuration Descriptor size
		ConfigurationDescriptor,			// bDescriptorType: Configuration
		LOBYTE(descPos),					// wTotalLength
		HIBYTE(descPos),
		interfaceCount,						// bNumInterfaces: 5 [1 MSC, 2 CDC, 2 MIDI]
		0x01,								// bConfigurationValue: Configuration value
		0x04,								// iConfiguration: Index of string descriptor describing the configuration
		0x80 | (selfPowered << 6),			// bmAttributes: self powered
		0x32,								// MaxPower 0 mA
	};
	memcpy(&configDescriptor[0], descriptorHeader, descrHeaderSize);

	return descPos;
}


uint32_t USB::StringToUnicode(const std::string_view desc, uint8_t* unicode)
{
	uint32_t idx = 2;
	for (auto c: desc) {
		unicode[idx++] = c;
		unicode[idx++] = 0;
	}
	unicode[0] = idx;
	unicode[1] = StringDescriptor;

	return idx;
}


void USB::SerialToUnicode()
{
	const uint32_t* uidAddr = (uint32_t*)UID_BASE;			// Location in memory that holds 96 bit Unique device ID register

	char uidBuff[usbSerialNoSize + 1];
	snprintf(uidBuff, usbSerialNoSize + 1, "%08lx%08lx%08lx", uidAddr[0], uidAddr[1], uidAddr[2]);

	stringDescr[0] = usbSerialNoSize * 2 + 2;				// length is 24 bytes (x2 for unicode padding) + 2 for header
	stringDescr[1] = StringDescriptor;
	for (uint8_t i = 0; i < usbSerialNoSize; ++i) {
		stringDescr[i * 2 + 2] = uidBuff[i];
	}
}


void USB::StdDevReq()
{
	uint8_t dev_addr;
	switch (req.RequestType & requestTypeMask)
	{
	case RequestTypeClass:
	case RequestTypeVendor:
		// pdev->pClass->Setup(pdev, req);
		break;

	case RequestTypeStandard:

		switch (static_cast<Request>(req.Request))
		{
		case Request::GetDescriptor:
			GetDescriptor();
			break;

		case Request::SetAddress:
			dev_addr = (uint8_t)(req.Value) & 0x7F;
			USBx_DEVICE->DCFG &= ~(USB_OTG_DCFG_DAD);
			USBx_DEVICE->DCFG |= ((uint32_t)dev_addr << 4) & USB_OTG_DCFG_DAD;
			ep0State = EP0State::StatusIn;
			EPStartXfer(Direction::in, 0, 0);
			devState = DeviceState::Addressed;
			break;

		case Request::SetConfiguration:
			if (devState == DeviceState::Addressed) {
				devState = DeviceState::Configured;

				for (auto c : classByEP) {
					c->ActivateEP();
				}

				ep0State = EP0State::StatusIn;
				EPStartXfer(Direction::in, 0, 0);
			}
			break;

		default:
			CtlError();
			break;
		}
		break;

		default:
			CtlError();
			break;
	}

}

void USB::EPStartXfer(const Direction direction, uint8_t endpoint, uint32_t xfer_len) {

	if (direction == Direction::in) {

		endpoint = endpoint & epAddressMask;					// Strip out 0x80 if endpoint passed eg as 0x81

		if (endpoint == 0 && xfer_len > ep_maxPacket) {		// If the transfer is larger than the maximum packet size send the maximum size and use the remaining flag to trigger a second send
			ep0.inBuffRem = xfer_len - ep_maxPacket;
			xfer_len = ep_maxPacket;
		}

		USBx_INEP(endpoint)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ | USB_OTG_DIEPTSIZ_PKTCNT);

		USBx_INEP(endpoint)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (((xfer_len + ep_maxPacket - 1) / ep_maxPacket) << USB_OTG_DIEPTSIZ_PKTCNT_Pos));
		USBx_INEP(endpoint)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & xfer_len);

		USBx_INEP(endpoint)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);		// EP enable, IN data in FIFO

		if (xfer_len > 0) {
			USBx_DEVICE->DIEPEMPMSK |= 1 << endpoint;		// Enable the Tx FIFO Empty Interrupt for this EP
		}
	} else { 		// OUT endpoint

		classByEP[endpoint]->outBuffPackets = 1;
		classByEP[endpoint]->outBuffOffset = 0;

		// If the transfer is larger than the maximum packet size send the total size and number of packets calculated from the end point maximum packet size
		if (xfer_len > ep_maxPacket) {
			classByEP[endpoint]->outBuffPackets = (xfer_len + ep_maxPacket - 1) / ep_maxPacket;
		}

		USBx_OUTEP(endpoint)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ | USB_OTG_DOEPTSIZ_PKTCNT);

		USBx_OUTEP(endpoint)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (classByEP[endpoint]->outBuffPackets << USB_OTG_DOEPTSIZ_PKTCNT_Pos));
		USBx_OUTEP(endpoint)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & xfer_len);

		USBx_OUTEP(endpoint)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);		// EP enable
	}
}


void USB::CtlError() {
	USBx_INEP(0)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
	USBx_OUTEP(0)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;

	// Set PKTCNT to 1, XFRSIZ to 24, STUPCNT to 3 (number of back-to-back SETUP data packets endpoint can receive)
	//USBx_OUTEP(0)->DOEPTSIZ = (1 << 19) | (3U * 8) | USB_OTG_DOEPTSIZ_STUPCNT;
}


bool USB::ReadInterrupts(uint32_t interrupt) {

#if (USB_DEBUG)
	if (((USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK) & interrupt) && usbDebugEvent < USB_DEBUG_COUNT && debugStart) {
		usbDebugNo = usbDebugEvent % USB_DEBUG_COUNT;
		usbDebug[usbDebugNo].eventNo = usbDebugEvent;
		usbDebug[usbDebugNo].Interrupt = USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK;
		usbDebugEvent++;
	}
#endif

	return ((USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK) & interrupt);
}


size_t USB::SendData(const uint8_t* data, const uint16_t len, uint8_t endpoint)
{
	endpoint &= epAddressMask;
	if (devState == DeviceState::Configured && !transmitting) {
		transmitting = true;
		classByEP[endpoint]->inBuff = data;
		classByEP[endpoint]->inBuffSize = len;
		classByEP[endpoint]->inBuffCount = 0;
		ep0State = EP0State::DataIn;
		EPStartXfer(Direction::in, endpoint, len);
		return len;
	} else {
		return 0;
	}
}


void USB::SendString(const char* s) {
	volatile uint16_t counter = 0;
	while (transmitting && counter < 10000) {
		++counter;
	}
	SendData((uint8_t*)s, strlen(s), CDC_In);
}


void USB::SendString(std::string s) {
	SendString(s.c_str());
}


size_t USB::SendString(const unsigned char* s, size_t len)
{
	volatile uint16_t counter = 0;
	while (transmitting && counter < 10000) {
		++counter;
	}
	return SendData((uint8_t*)s, len, CDC_In);
}

#if (USB_DEBUG)

#include <string>

std::string IntToString(const int32_t& v) {
	return std::to_string(v);
}

std::string HexToString(const uint32_t& v, const bool& spaces) {
	char buf[20];
	if (spaces) {
		if (v != 0) {
			uint8_t* bytes = (uint8_t*)&v;
			sprintf(buf, "%02X%02X%02X%02X", bytes[0], bytes[1], bytes[2], bytes[3]);
		} else {
			sprintf(buf, " ");
		}
	} else {
		sprintf(buf, "%X", (unsigned int)v);
	}
	return std::string(buf);

}

std::string HexByte(const uint16_t& v) {
	char buf[50];
	sprintf(buf, "%X", v);
	return std::string(buf);

}


void USB::OutputDebug() {

	uart.SendString("Event,Interrupt,Desc,Int Data,Desc,Endpoint,mRequest,Request,Value,Index,Length,Scsi,PacketSize,XferBuff\n");
	uint16_t evNo = usbDebugEvent % USB_DEBUG_COUNT;
	std::string interrupt, subtype;
	for (int i = 0; i < USB_DEBUG_COUNT; ++i) {
		switch (usbDebug[evNo].Interrupt) {
		case USB_OTG_GINTSTS_RXFLVL:
			interrupt = "RXFLVL";

			switch ((usbDebug[evNo].IntData & USB_OTG_GRXSTSP_PKTSTS) >> 17) {
				case OutDataReceived:			subtype = "Out packet rec";			break;		// 2 = OUT data packet received
				case OutTransferCompleted:		subtype = "Transfer completed";		break;		// 3 = Transfer completed
				case SetupDataReceived:			subtype = "Setup packet rec";		break;		// 6 = SETUP data packet received
				case SetupTransComplete:		subtype = "Setup comp";				break;		// 4 = SETUP comp
				default:						subtype = "";
			}
			break;

			case USB_OTG_GINTSTS_SRQINT:		interrupt = "SRQINT";			break;		// 0x40000000
			case USB_OTG_GINTSTS_USBSUSP:		interrupt = "USBSUSP";			break;		// 0x800
			case USB_OTG_GINTSTS_WKUINT:		interrupt = "WKUINT";			break;		// 0x80000000
			case USB_OTG_GINTSTS_USBRST:		interrupt = "USBRST";			break;		// 0x1000
			case USB_OTG_GINTSTS_ENUMDNE:		interrupt = "ENUMDNE";			break;		// 0x2000
			case USB_OTG_GINTSTS_OEPINT:		interrupt = "OEPINT";						// 0x80000

			switch (usbDebug[evNo].IntData) {
			case USB_OTG_DOEPINT_XFRC:
				subtype = "Transfer completed";
				break;
			case USB_OTG_DOEPINT_STUP:
				switch (usbDebug[evNo].Request.Value >> 8)	{
					case DeviceDescriptor:			subtype = "Device Descriptor";					break;
					case ConfigurationDescriptor:	subtype = "Configuration Descriptor";			break;
					case BosDescriptor:				subtype = "Bos Descriptor";						break;
					case DeviceQualifierDescriptor:	subtype = "Device Qualifier Descriptor";		break;
					case StringDescriptor:

						switch ((uint8_t)(usbDebug[evNo].Request.Value)) {
							case StringIndex::LangId:				subtype = "Lang Id";			break;
							case StringIndex::Manufacturer:			subtype = "Manufacturer";		break;
							case StringIndex::Product:				subtype = "Product";			break;
							case StringIndex::Serial:				subtype = "Serial";				break;
							case StringIndex::Configuration:		subtype = "Configuration";		break;
							case StringIndex::MassStorageClass:		subtype = "MassStorageClass";	break;
							case StringIndex::CommunicationClass:	subtype = "CommunicationClass";	break;
							case StringIndex::AudioClass:			subtype = "AudioClass";			break;
							default:				    			subtype = "Setup phase done";	break;
						}
						break;

					default: 									subtype = "Setup phase done";	break;
				}


//				subtype = "Setup phase done";
				break;
			default:
				subtype = "";
			}
			break;
		case USB_OTG_GINTSTS_IEPINT:
			interrupt = "IEPINT";

			switch (usbDebug[evNo].IntData) {
				case USB_OTG_DIEPINT_XFRC:			subtype = "Transfer completed";			break;
				case USB_OTG_DIEPINT_TXFE:			subtype = "Transmit FIFO empty";		break;
				default:							subtype = "";
			}
			break;
		default:
			interrupt = "";
		}

		if (usbDebug[evNo].Interrupt != 0) {
			uart.SendString(IntToString(usbDebug[evNo].eventNo) + ","
					+ HexToString(usbDebug[evNo].Interrupt, false) + "," + interrupt + ","
					+ HexToString(usbDebug[evNo].IntData, false) + "," + subtype + ","
					+ IntToString(usbDebug[evNo].endpoint) + ","
					+ HexByte(usbDebug[evNo].Request.RequestType) + ", "
					+ HexByte(usbDebug[evNo].Request.Request) + ", "
					+ HexByte(usbDebug[evNo].Request.Value) + ", "
					+ HexByte(usbDebug[evNo].Request.Index) + ", "
					+ HexByte(usbDebug[evNo].Request.Length) + ", "
					+ HexByte(usbDebug[evNo].scsiOpCode) + ", "
					+ IntToString(usbDebug[evNo].PacketSize) + ", "
					+ HexToString(usbDebug[evNo].xferBuff[0], true) + " "
					+ HexToString(usbDebug[evNo].xferBuff[1], true) + " "
					+ HexToString(usbDebug[evNo].xferBuff[2], true) + " "
					+ HexToString(usbDebug[evNo].xferBuff[3], true) + "\n");
		}
		evNo = (evNo + 1) % USB_DEBUG_COUNT;
	}
}


/*
startup sequence:
Event,Interrupt,Desc,Int Data,Desc,Endpoint,mRequest,Request,Value,Index,Length,Scsi,PacketSize,XferBuff
0,40000000,SRQINT,0,,0,0, 0, 0, 0, 0, 0, 0,
1,800,USBSUSP,0,,0,0, 0, 0, 0, 0, 0, 0,
2,1000,USBRST,0,,0,0, 0, 0, 0, 0, 0, 0,
3,2000,ENUMDNE,0,,0,0, 0, 0, 0, 0, 0, 0,
4,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060001 00004000
5,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
6,80000,OEPINT,8,Setup phase done,0,80, 6, 100, 0, 40, 0, 18, 12010102 EF020140 83042A57 00020102
7,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 18, 12010102 EF020140 83042A57 00020102
8,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
9,10,RXFLVL,A50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
10,10,RXFLVL,A70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
11,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
12,10,RXFLVL,AC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 00052A00
13,10,RXFLVL,A80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
14,80000,OEPINT,8,Setup phase done,0,0, 5, 2A, 0, 0, 0, 0,
15,10,RXFLVL,1EC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060001 00001200
16,10,RXFLVL,1E80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
17,80000,OEPINT,8,Setup phase done,0,80, 6, 100, 0, 12, 0, 18, 12010102 EF020140 83042A57 00020102
18,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 18, 12010102 EF020140 83042A57 00020102
19,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
20,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
21,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
22,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
23,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060002 0000FF00
24,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
25,80000,OEPINT,8,Setup phase done,0,80, 6, 200, 0, FF, 0, 32, 09022000 010104C0 32090400 00020806
26,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 32, 09022000 010104C0 32090400 00020806
27,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
28,10,RXFLVL,850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
29,10,RXFLVL,870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
30,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
31,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 8006000F 0000FF00
32,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
33,80000,OEPINT,8,Setup phase done,0,80, 6, F00, 0, FF, 0, 12, 050F0C00 01071002 02000000 1A030000
34,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 12, 050F0C00 01071002 02000000 1A030000
35,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
36,10,RXFLVL,850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
37,10,RXFLVL,870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
38,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
39,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060303 0904FF00
40,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
41,80000,OEPINT,8,Setup phase done,0,80, 6, 303, 409, FF, 0, 26, 1A033000 30003500 43003000 30003600
42,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A033000 30003500 43003000 30003600
43,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
44,10,RXFLVL,850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
45,10,RXFLVL,870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
46,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
47,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 0000FF00
48,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
49,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, FF, 0, 4, 04030904 0A060002 01000040 01000000
50,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01000000
51,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
52,10,RXFLVL,A50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
53,10,RXFLVL,A70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
54,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
55,10,RXFLVL,AC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
56,10,RXFLVL,A80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
57,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
58,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
59,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
60,10,RXFLVL,A50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
61,10,RXFLVL,A70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
62,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
63,10,RXFLVL,AC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060006 00000A00
64,10,RXFLVL,A80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
65,80000,OEPINT,8,Setup phase done,0,80, 6, 600, 0, A, 0, 0,
66,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000200
67,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
68,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 2, 0, 4, 04030904 0A060002 01000040 01001A03
69,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 2, 04030904 0A060002 01000040 01001A03
70,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
71,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
72,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
73,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
74,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000400
75,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
76,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 4, 0, 4, 04030904 0A060002 01000040 01001A03
77,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
78,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
79,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
80,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
81,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
82,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060303 09040200
83,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
84,80000,OEPINT,8,Setup phase done,0,80, 6, 303, 409, 2, 0, 26, 1A033000 30003500 43003000 30003600
85,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 2, 1A033000 30003500 43003000 30003600
86,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
87,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
88,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
89,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
90,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060303 09041A00
91,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
92,80000,OEPINT,8,Setup phase done,0,80, 6, 303, 409, 1A, 0, 26, 1A033000 30003500 43003000 30003600
93,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A033000 30003500 43003000 30003600
94,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
95,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
96,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
97,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
98,10,RXFLVL,16C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 00090100
99,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
100,80000,OEPINT,8,Setup phase done,0,0, 9, 1, 0, 0, 0, 0,
101,10,RXFLVL,18C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, A1FE0000 00000100
102,10,RXFLVL,1880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
103,80000,OEPINT,8,Setup phase done,0,A1, FE, 0, 0, 1, 0, 1,
104,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 1,
105,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
106,10,RXFLVL,1850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
107,10,RXFLVL,1870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
108,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
109,10,RXFLVL,18401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 20559EDE 24000000 80000612
110,10,RXFLVL,1860001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
111,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
112,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 36, 00800202 1F000000 53544D20 20202020
113,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
114,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 20559EDE
115,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
116,10,RXFLVL,18501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 B08479D9 24000000 80000612
117,10,RXFLVL,1870001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
118,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
119,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 36, 00800202 1F000000 53544D20 20202020
120,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
121,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 B08479D9
122,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
123,10,RXFLVL,18401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E05445E2 FC000000 80000A23
124,10,RXFLVL,1860001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
125,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 23, 0,
126,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 12, 00000008 00000031 02000200
127,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
128,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E05445E2 F0000000
129,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
130,10,RXFLVL,18501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E034FFE0 FF000000 80000612
131,10,RXFLVL,1870001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
132,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
133,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00800008 20202020 00800202 1F000000
134,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
135,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E034FFE0 F7000000
136,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
137,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 0000FF00
138,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
139,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, FF, 0, 4, 04030904 0A060002 01000040 01001A03
140,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
141,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
142,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
143,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
144,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
145,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 0000FF00
146,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
147,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, FF, 0, 4, 04030904 0A060002 01000040 01001A03
148,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
149,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
150,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
151,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
152,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
153,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060103 0904FF00
154,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
155,80000,OEPINT,8,Setup phase done,0,80, 6, 301, 409, FF, 0, 34, 22034D00 6F007500 6E007400 6A006F00
156,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 34, 22034D00 6F007500 6E007400 6A006F00
157,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
158,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
159,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
160,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
161,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060103 0904FF00
162,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
163,80000,OEPINT,8,Setup phase done,0,80, 6, 301, 409, FF, 0, 34, 22034D00 6F007500 6E007400 6A006F00
164,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 34, 22034D00 6F007500 6E007400 6A006F00
165,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
166,10,RXFLVL,250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
167,10,RXFLVL,270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
168,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
169,10,RXFLVL,2C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
170,10,RXFLVL,280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
171,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
172,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
173,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
174,10,RXFLVL,250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
175,10,RXFLVL,270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
176,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
177,10,RXFLVL,2C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
178,10,RXFLVL,280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
179,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
180,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
181,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
182,10,RXFLVL,250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
183,10,RXFLVL,270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
184,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
185,10,RXFLVL,2401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
186,10,RXFLVL,260001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
187,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
188,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
189,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
190,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
191,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
192,10,RXFLVL,2501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A008D3E8 C0000000 8000061A
193,10,RXFLVL,270001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
194,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 1A, 0,
195,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 23, 22000000 08120000
196,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
197,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A008D3E8 A9000000
198,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
199,10,RXFLVL,4401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 C0000000 8000061A
200,10,RXFLVL,460001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
201,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 1A, 0,
202,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 23, 22000000 08120000
203,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
204,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2 A9000000
205,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
206,10,RXFLVL,4501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E0F4B4EA 08000000 80000A25
207,10,RXFLVL,470001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
208,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
209,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
210,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
211,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E0F4B4EA
212,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
213,10,RXFLVL,4401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E0C4D5E3 24000000 80000612
214,10,RXFLVL,460001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
215,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
216,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 36, 00800202 1F000000 53544D20 20202020
217,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
218,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E0C4D5E3
219,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
220,10,RXFLVL,4501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 08000000 80000A25
221,10,RXFLVL,470001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
222,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
223,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
224,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
225,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
226,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
227,10,RXFLVL,6401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 08000000 80000A25
228,10,RXFLVL,660001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
229,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
230,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
231,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
232,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
233,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
234,10,RXFLVL,6501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 08000000 80000A25
235,10,RXFLVL,670001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
236,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
237,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
238,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
239,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
240,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
241,10,RXFLVL,6401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 00020000 80000A28
242,10,RXFLVL,660001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
243,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
244,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, EBFE904D 53444F53 352E3000 02010100
245,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
246,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
247,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
248,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
249,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
250,10,RXFLVL,8501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A068D0DB 00200000 80000A28
251,10,RXFLVL,870001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
252,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
253,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, EBFE904D 53444F53 352E3000 02010100
254,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
255,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
256,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
257,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, F8FFFFFF 0F000000
258,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
259,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
260,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
261,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, 53544D33 32202020 54585420
262,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
263,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
264,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
265,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
266,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
267,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
268,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
269,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
270,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
271,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
272,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
273,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
274,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
275,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
276,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
277,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
278,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
279,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
280,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
281,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
282,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
283,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
284,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
285,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
286,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
287,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
288,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
289,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
290,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
291,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
292,10,RXFLVL,16C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000400
293,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
294,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 4, 0, 4, 04030904 0A060002 01000040 01001A03
295,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
296,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
297,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
298,10,RXFLVL,1650000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
299,10,RXFLVL,1670000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
300,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
301,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
302,10,RXFLVL,16C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060103 0904FF00
303,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
304,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
305,80000,OEPINT,8,Setup phase done,0,80, 6, 301, 409, FF, 0, 34, 22034D00 6F007500 6E007400 6A006F00
306,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 34, 22034D00 6F007500 6E007400 6A006F00
307,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
308,10,RXFLVL,1650000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
309,10,RXFLVL,1670000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
310,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
311,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
312,40010,,16C0080,Transfer completed,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000400
313,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
314,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
315,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 4, 0, 4, 04030904 0A060002 01000040 01002203
316,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01002203
317,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
318,10,RXFLVL,1650000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
319,10,RXFLVL,1670000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
320,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
321,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
322,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
323,10,RXFLVL,18C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
324,10,RXFLVL,1880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
325,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
326,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
327,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
328,10,RXFLVL,1850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
329,10,RXFLVL,1870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
330,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
331,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
332,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
333,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
334,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
335,10,RXFLVL,18C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060006 00000A00
336,10,RXFLVL,1880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
337,80000,OEPINT,8,Setup phase done,0,80, 6, 600, 0, A, 0, 0,
338,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
339,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
340,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
341,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
342,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
343,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
344,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
345,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
346,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
347,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
348,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
349,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
350,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
351,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A068D0DB
352,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
353,10,RXFLVL,1E401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
354,10,RXFLVL,1E60001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
355,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
356,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200
357,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
358,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
359,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
360,10,RXFLVL,1E501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
361,10,RXFLVL,1E70001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
362,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
363,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200
364,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
365,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
366,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
367,10,RXFLVL,1E401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
368,10,RXFLVL,1E60001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
369,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
370,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200
371,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
372,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
373,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
374,10,RXFLVL,501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A0D8D9DB 00020000 80000A28
375,10,RXFLVL,70001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
376,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
377,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, 53544D33 32202020 54585420
378,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
379,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
380,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
381,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A0D8D9DB
382,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
383,10,RXFLVL,401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CCDE 08000000 80000A25
384,10,RXFLVL,60001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
385,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
386,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 54585420
387,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
388,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CCDE
389,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
390,10,RXFLVL,2501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A01897DD 08000000 80000A25
391,10,RXFLVL,270001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
392,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
393,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 54585420
394,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
395,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A01897DD
396,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
397,10,RXFLVL,2401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A068D0DB 08000000 80000A25
398,10,RXFLVL,260001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
399,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 2A, 13, 55534253 609400E2

*/

#endif
