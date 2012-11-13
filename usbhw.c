#include "usbhw.h"

#include <LPC17xx.h>

#include <lpc17xx_clkpwr.h>

#include "lpc17xx_usb.h"

#define EP(x)	(1UL<<EP2IDX(x))

/// pointers for callbacks to EP1-15 both IN and OUT
usb_callback_pointer EPcallbacks[30];

void usb_init()
{
	// enable USB hardware
	LPC_SC->PCONP |= CLKPWR_PCONP_PCUSB;

	// enable clocks
	LPC_USB->USBClkCtrl |= DEV_CLK_EN | AHB_CLK_EN;
	// wait for clocks to stabilise
	while (LPC_USB->USBClkSt != (DEV_CLK_ON | AHB_CLK_ON));

	// configure USBD+ and USBD-
	LPC_PINCON->PINSEL1 &= 0xc3ffffff;
	LPC_PINCON->PINSEL1 |= 0x14000000;

	SIE_Disconnect();

	// configure USB Connect
	LPC_PINCON->PINSEL4 &= 0xfffcffff;
	LPC_PINCON->PINSEL4 |= 0x00040000;
}

void usb_set_callback(uint8_t bEP, usb_callback_pointer callback)
{
	EPcallbacks[EP(bEP)] = callback;
}

void usb_realise_endpoint(uint8_t bEP, uint16_t packet_size)
{
	__disable_irq();
	LPC_USB->USBDevIntClr = EP_RLZD;
	LPC_USB->USBReEp |= EP(bEP);
	LPC_USB->USBEpInd = EP2IDX(bEP);
	LPC_USB->USBMaxPSize = packet_size;
	__enable_irq();
	while (!(LPC_USB->USBDevIntSt & EP_RLZD));
	LPC_USB->USBDevIntClr = EP_RLZD;
}

int usb_read_packet(uint8_t bEP, void *buffer, int buffersize)
{
	int i;
	uint32_t j = 0;
	__disable_irq();
	LPC_USB->USBCtrl = RD_EN | (EP2IDX(bEP) << 2);
	for (i=0; i<buffersize; i++) {
		if ((i & 3) == 0) {
			j = LPC_USB->USBRxData;
		}
		((uint8_t *) buffer)[i] = (j >> 24) & 0xFF;
		j <<= 8;
		if ((i & 3) == 3) {
			if (!(LPC_USB->USBCtrl & RD_EN))
			{
				SIE_SelectEndpoint(bEP);
				SIE_ClearBuffer();
				__enable_irq();
				return i;
			}
		}
	}
	if (LPC_USB->USBCtrl & RD_EN)
	i = LPC_USB->USBRxPLen;
	__enable_irq();
	return i;
}

int usb_write_packet(uint8_t bEP, void *data, int packetlen)
{
	int i;
	uint8_t *d = (uint8_t *) data;
	__disable_irq();
	LPC_USB->USBCtrl = WR_EN | (EP2IDX(bEP) << 2);
	LPC_USB->USBTxPLen = packetlen;
	for (i = 0;i < packetlen;)
	{
		LPC_USB->USBTxData = ((d[0]) << 0) | ((d[1]) << 8) | ((d[2]) << 16) | ((d[3]) << 24);
		data += 4;
		i += 4;
	}
	SIE_SelectEndpoint(bEP);
	SIE_ValidateBuffer();
	__enable_irq();
	return i;
}

void usb_task()
{
	if (LPC_USB->USBDevIntSt & FRAME)
	{
// 		USBEvent_Frame(SIEgetFrameNumber());
		LPC_USB->USBDevIntClr = FRAME;
	}
	if (LPC_USB->USBDevIntSt & DEV_STAT)
	{
		LPC_USB->USBDevIntClr = DEV_STAT;

		uint8_t devStat = SIE_GetDeviceStatus();

		if (devStat & SIE_DEVSTAT_SUS_CH)
		{
// 			USBEvent_suspendStateChanged(devStat & SIE_DEVSTAT_SUS);
		}

		if (devStat & SIE_DEVSTAT_RST)
		{
// 			USBEvent_busReset();

			usb_realise_endpoint(EP0IN , 64);
			usb_realise_endpoint(EP0OUT, 64);

			SIE_SetMode(SIE_MODE_INAK_CI | SIE_MODE_INAK_CO | SIE_MODE_INAK_BI | SIE_MODE_INAK_BO);
		}

		if (devStat & SIE_DEVSTAT_CON_CH)
		{
// 			USBEvent_connectStateChanged(devStat & SIE_DEVSTAT_CON);
		}
	}
	if (LPC_USB->USBDevIntSt & EP_SLOW)
	{
		if (LPC_USB->USBEpIntSt & EP(EP0OUT))
		{
			if (SIE_SelectEndpointClearInterrupt(EP0OUT) & SIE_EP_STP)
			{
				// this is a setup packet
				EP0setup();
			}
			else
			{
				EP0out();
			}
		}
		if (LPC_USB->USBEpIntSt & EP(EP0IN))
		{
			SIE_SelectEndpointClearInterrupt(EP0IN);
			EP0in();
		}
		if (LPC_USB->USBEpIntEn & ~(3UL))
		{

		}
		LPC_USB->USBDevIntClr = EP_SLOW;
	}
}

__attribute__ ((interrupt)) void USB_IRQHandler() {
	usb_task();
}
