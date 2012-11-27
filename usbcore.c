#include "usbcore.h"

#include "usbhw.h"

CONTROL_TRANSFER control;

uint8_t control_buffer[8];

void EP0setup()
{
	if (usb_read_packet(EP0IN, &control.setup, 8) == 8)
	{
		switch(control.setup.bRequest)
		{
			case REQ_GET_STATUS:
			{
				break;
			}
			case REQ_CLEAR_FEATURE:
			{
				break;
			}
			case REQ_SET_FEATURE:
			{
				break;
			}
			case REQ_SET_ADDRESS:
			{
				break;
			}
			case REQ_GET_DESCRIPTOR:
			{
				break;
			}
			case REQ_SET_DESCRIPTOR:
			{
				break;
			}
			case REQ_GET_CONFIGURATION:
			{
				break;
			}
			case REQ_SET_CONFIGURATION:
			{
				break;
			}
		}
	}
}
