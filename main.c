
#include "usbhw.h"
#include "usbcore.h"

#include "uart.h"

#include "SDCard.h"

#include "gpio.h"

#include "sbl_iap.h"
#include "sbl_config.h"

#include "ff.h"

#define ISP_BTN	P2_10

FATFS	fat;
FIL		file;

#include "lpc17xx_wdt.h"
void Watchdog_SystemReset()
{
	WDT_Init(WDT_CLKSRC_IRC, WDT_MODE_RESET);
	WDT_Start(256);
	while (1);
}

int isp_btn_pressed()
{
	GPIO_init(ISP_BTN);
	for (int i = 128; i; i--)
		GPIO_input(ISP_BTN);
	return GPIO_get(ISP_BTN);
}

void start_dfu()
{
	DFU_init();
	usb_init();
	while (1)
		usb_task();
}

void check_sd_firmware()
{
	if (f_open(&file, "/firmware.bin", FA_READ) == FR_OK)
	{
		uint8_t buf[512];
		unsigned int r = sizeof(buf);
		uint32_t address = USER_FLASH_START;
		while (r == sizeof(buf))
		{
			if (f_read(&file, buf, sizeof(buf), &r) != FR_OK)
			{
				f_close(&file);
				return;
			}
			write_flash((void *) address, (char *)buf, r);
			address += r;
		}
		if (address > USER_FLASH_START)
		{
			f_close(&file);
			if (f_open(&file, "/firmware.cur", FA_READ) == FR_OK)
			{
				f_close(&file);
				f_unlink("/firmware.cur");
			}
			f_rename("/firmware.bin", "/firmare.cur");
			NVIC_SystemReset();
		}
	}
}

int main()
{
	UART_init(UART_RX, UART_TX, 2000000);

	SDCard_init(p5,p6,p7,p8);
	f_mount(0, &fat);

	if (isp_btn_pressed())
		start_dfu();
	if (user_code_present() == 0)
		start_dfu();
	check_sd_firmware();
}


void __aeabi_unwind_cpp_pr0(void){}
void __libc_init_array(void){}

uint32_t get_fattime()
{
#define	YEAR	2012
#define MONTH	11
#define DAY		13
#define HOUR	20
#define MINUTE	13
#define SECOND	1
	return	((YEAR  & 127) << 25) |
			((MONTH &  15) << 21) |
			((DAY   &  31) << 16) |
			((HOUR  &  31) << 11) |
			((MINUTE & 63) <<  5) |
			((SECOND & 63) <<  0);
}