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
		USB_VERSION_2_0,	// bcdUSBVersion
		0,							// bDeviceClass
		0,							// bDeviceSubClass
		0,							// bDeviceProtocol
		64,						// bMaxPacketSize
		0x1D50,					// idVendor
		0x6015,					// idProduct
		0x0100,					// bcdDevice (serial number)
		0,							// iManufacturer
		0,							// iProduct
		0,							// iSerial
		1							// bNumConfigurations
	},
	{
		DL_CONFIGURATION,
		DT_CONFIGURATION,
		sizeof(usbdesc_configuration) + sizeof(usbdesc_interface) + sizeof(DFU_functional_descriptor),
		1,							// bNumInterfaces
		1,							// bConfigurationValue
		0,							// iConfiguration
		CA_BUSPOWERED,	// bmAttributes
		500 mA					// bMaxPower
	},
	{
		DL_INTERFACE,
		DT_INTERFACE,
		0,							// bInterfaceNumber
		0,							// bAlternate
		0,							// bNumEndpoints
		DFU_INTERFACE_CLASS,		// bInterfaceClass
		DFU_INTERFACE_SUBCLASS,		// bInterfaceSubClass
		DFU_INTERFACE_PROTOCOL_DFUMODE,		// bInterfaceProtocol
		0							// iInterface
	},
	{
		DL_DFU_FUNCTIONAL_DESCRIPTOR,
		DT_DFU_FUNCTIONAL_DESCRIPTOR,
		DFU_BMATTRIBUTES_WILLDETACH | DFU_BMATTRIBUTES_CANDOWNLOAD,
		500,						// wDetachTimeout - time in milliseconds between receiving detach request and issuing usb reset
		512,						// wTransferSize - the size of each packet of firmware sent from the host via control transfers
		DFU_VERSION_1_1	// bcdDFUVersion
	}
};

void DFU_init()
{
// 	usb_write_packet(0, &desc, sizeof(desc));
}

void DFU_controlTransfer(CONTROL_TRANSFER *control)
{
}

void EP0in()
{
}

void EP0out()
{
}

int DFU_complete()
{
	return 0;
}
