
#include "usbhw.h"
#include "usbcore.h"

#include "uart.h"

#include "SDCard.h"

#include "gpio.h"

#include "sbl_iap.h"
#include "sbl_config.h"

#include "ff.h"

#include "dfu.h"

#include "min-printf.h"

#define ISP_BTN	P2_10

FATFS	fat;
FIL		file;

typedef struct __attribute__ ((packed)) {
	uint8_t status;
	uint8_t chs_start[3];
	uint8_t type;
	uint8_t chs_last[3];
	uint32_t lba_start;
	uint32_t sectors;
} partition;

// uint8_t	sdbuf[512];
typedef struct __attribute__ ((packed)) {
	uint8_t ignore[446];
	partition part0;
	partition part1;
	partition part2;
	partition part3;
	uint16_t magic;
} MBR;

MBR mbr;

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
	UART_init(UART_RX, UART_TX, 115200);
	printf("Bootloader Start %u\n", 10);

	SDCard_init(P0_9, P0_8, P0_7, P0_6);
	if (SDCard_disk_initialize() == 0)
	{
		int a = 0;
		int b = 0;
		char c = ' ';
		if (SDCard_disk_sectors() > ((1UL<<30) / 512UL))
		{
			a = (SDCard_disk_sectors() / ((1UL << 30) / 512UL / 10));
			b = a % 10;
			a /= 10;
			c = 'G';
		}
		else if (SDCard_disk_sectors() > ((1UL << 20) / 512UL))
		{
			a = (SDCard_disk_sectors() * 512UL / ((1UL << 20) / 10));
			b = a % 10;
			a /= 10;
			c = 'M';
		}
		else
		{
			a = (SDCard_disk_sectors() * 512UL / ((1UL << 10) / 10));
			b = a % 10;
			a /= 10;
			c = 'K';
		}
		printf("SD card found: %lu sectors, %d.%d%cB\n", SDCard_disk_sectors(), a, b, c);

		SDCard_disk_read((uint8_t *) &mbr, 0);
		printf("Magic is %x\n", mbr.magic);
		printf("Partition 0:\n\tStatus: %d\nLBA start: %lu\nSectors: %lu\n", mbr.part0.status, mbr.part0.lba_start, mbr.part0.sectors);
// 		printf("Partition 1:\n\tStatus: %d\nLBA start: %lu\nSectors: %lu\n", mbr.part1.status, mbr.part1.lba_start, mbr.part1.sectors);
// 		printf("Partition 2:\n\tStatus: %d\nLBA start: %lu\nSectors: %lu\n", mbr.part2.status, mbr.part2.lba_start, mbr.part2.sectors);
// 		printf("Partition 3:\n\tStatus: %d\nLBA start: %lu\nSectors: %lu\n", mbr.part3.status, mbr.part3.lba_start, mbr.part3.sectors);

		SDCard_disk_read((uint8_t *) &mbr, mbr.part0.lba_start);
		for (b = 0; b < 512; b++)
		{
			printf("0x%x ", ((uint8_t *) &mbr)[b]);
			if ((b & 15) == 15)
				printf("\n");
		}

		a = f_mount(0, &fat);
		printf("f_mount returned %d\n", a);
		if (a == FR_OK)
		{
				a = f_open(&file, "firmware.bin", FA_READ);
			printf("f_open returned %d for firmware.bin\n", a);
			if (a == 0)
			{
				printf("it exists! let's rename to firmware.cur and see what happens\n");
				f_close(&file);
// 				printf("but first let's check for existence of firmware.cur\n");
				a = f_open(&file, "firmware.cur", FA_READ);
				printf("f_open firmware.cur returned %d\n", a);
				if (a == FR_OK)
				{
					f_close(&file);
					a = f_unlink("firmware.cur");
					printf("f_unlink returned %d\n", a);
// 					if (a == 0) printf("firmware.cur deleted\n");
				}
				a = f_rename("firmware.bin", "firmware.cur");
				printf("f_rename returned %d\n", a);
			}
			else if (a == FR_NO_FILE)
			{
				printf("open firmware.bin: file not found\n");
			}
		}

	}
// 	f_mount(0, &fat);

// 	if (isp_btn_pressed())
// 		start_dfu();
// 	if (user_code_present() == 0)
// 		start_dfu();
// 	check_sd_firmware();
	while (1);
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

int _write(int fd, const char *buf, int buflen)
{
	if (fd < 3)
	{
		while (UART_cansend() < buflen);
		return UART_send((const uint8_t *)buf, buflen);
	}
	return buflen;
}
