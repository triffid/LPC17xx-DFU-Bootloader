#include "dfu.h"

#include "usbcore.h"
#include "usbhw.h"

#include "descriptor.h"

#include "sbl_iap.h"

typedef struct
{
	usbdesc_device device;
	usbdesc_configuration configuration;
	usbdesc_interface	interface;
	DFU_functional_descriptor dfufunc;
} DFU_APP_Descriptor;

DFU_APP_Descriptor desc =
{
	{
		DL_DEVICE,
		DT_DEVICE,
		USB_VERSION_2_0,
		0,
		0,
		0,
		64,
		0x1D50,
		0x6015,
		0x0100,
		0,
		0,
		0,
		1
	},
	{
		DL_CONFIGURATION,
		DT_CONFIGURATION,
		sizeof(usbdesc_configuration) + sizeof(usbdesc_interface) + sizeof(DFU_functional_descriptor),
		1,
		1,
		0,
		CA_BUSPOWERED,
		500 mA
	},
	{
		DL_INTERFACE,
		DT_INTERFACE,
		0,
		0,
		0,
		0xFE,
		0x01,
		0x02,
		0
	},
	{
		DL_DFU_FUNCTIONAL_DESCRIPTOR,
		DT_DFU_FUNCTIONAL_DESCRIPTOR,
		DFU_BMATTRIBUTES_WILLDETACH | DFU_BMATTRIBUTES_CANDOWNLOAD,
		500,
		512,
		0x0101
	}
};

void DFU_init()
{
// 	usb_write_packet(0, &desc, sizeof(desc));
}

void DFU_EPIN()
{
}

void DFU_EPOUT()
{
}
