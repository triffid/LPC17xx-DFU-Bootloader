/*****************************************************************************
 *                                                                            *
 * DFU/SD/SDHC Bootloader for LPC17xx                                         *
 *                                                                            *
 * by Triffid Hunter                                                          *
 *                                                                            *
 *                                                                            *
 * This firmware is Copyright (C) 2009-2010 Michael Moon aka Triffid_Hunter   *
 *                                                                            *
 * This program is free software; you can redistribute it and/or modify       *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software                *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

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

#include "lpc17xx_wdt.h"

#define ISP_BTN	P2_12

#ifndef DEBUG_MESSAGES
#define printf(...) do {} while (0)
#endif

FATFS	fat;
FIL		file;

const char *firmware_file = "firmware.bin";
const char *firmware_old  = "firmware.cur";

void setleds(int leds)
{
	GPIO_write(LED1, leds &  1);
	GPIO_write(LED2, leds &  2);
	GPIO_write(LED3, leds &  4);
	GPIO_write(LED4, leds &  8);
	GPIO_write(LED5, leds & 16);
}

int isp_btn_pressed(void)
{
	return GPIO_get(ISP_BTN);
}

void start_dfu(void)
{
	DFU_init();
	usb_init();
	usb_connect();
	while (DFU_complete() == 0)
		usb_task();
	usb_disconnect();
}

void check_sd_firmware(void)
{
	int r;
// 	printf("Check SD\n");
	f_mount(0, &fat);
	if ((r = f_open(&file, firmware_file, FA_READ)) == FR_OK)
	{
// 		printf("Flashing firmware...\n");
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

			setleds((address - USER_FLASH_START) >> 15);

// 			printf("\t0x%lx\n", address);

			write_flash((void *) address, (char *)buf, sizeof(buf));
			address += r;
		}
		f_close(&file);
		if (address > USER_FLASH_START)
		{
// 			printf("Complete!\n");
			r = f_unlink(firmware_old);
			r = f_rename(firmware_file, firmware_old);
		}
	}
	else
	{
// 		printf("open: %d\n", r);
	}
}

// this seems to fix an issue with handoff after poweroff
// found here http://knowledgebase.nxp.trimm.net/showthread.php?t=2869
typedef void __attribute__((noreturn))(*exec)();

static void boot(uint32_t a)
{
    uint32_t *start;

    __set_MSP(*(uint32_t *)USER_FLASH_START);
    start = (uint32_t *)(USER_FLASH_START + 4);
    ((exec)(*start))();
}

static uint32_t delay_loop(uint32_t count)
{
	volatile uint32_t j, del;
	for(j=0; j<count; ++j){
		del=j; // volatiles, so the compiler will not optimize the loop
	}
	return del;
}

static void new_execute_user_code(void)
{
	uint32_t addr=(uint32_t)USER_FLASH_START;
	// delay
	delay_loop(3000000);
	// relocate vector table
	SCB->VTOR = (addr & 0x1FFFFF80);
	// switch to RC generator
	LPC_SC->PLL0CON = 0x1; // disconnect PLL0
	LPC_SC->PLL0FEED = 0xAA;
	LPC_SC->PLL0FEED = 0x55;
	while (LPC_SC->PLL0STAT&(1<<25));
	LPC_SC->PLL0CON = 0x0;    // power down
	LPC_SC->PLL0FEED = 0xAA;
	LPC_SC->PLL0FEED = 0x55;
	while (LPC_SC->PLL0STAT&(1<<24));
	// disable PLL1
	LPC_SC->PLL1CON   = 0;
	LPC_SC->PLL1FEED  = 0xAA;
	LPC_SC->PLL1FEED  = 0x55;
	while (LPC_SC->PLL1STAT&(1<<9));

	LPC_SC->FLASHCFG &= 0x0fff;  // This is the default flash read/write setting for IRC
	LPC_SC->FLASHCFG |= 0x5000;
	LPC_SC->CCLKCFG = 0x0;     //  Select the IRC as clk
	LPC_SC->CLKSRCSEL = 0x00;
	LPC_SC->SCS = 0x00;		    // not using XTAL anymore
	delay_loop(1000);
	// reset pipeline, sync bus and memory access
	__asm (
		   "dmb\n"
		   "dsb\n"
		   "isb\n"
		  );
	boot(addr);
}

int main(void)
{
	WDT_Feed();

	GPIO_init(ISP_BTN); GPIO_input(ISP_BTN);

	GPIO_init(LED1); GPIO_output(LED1);
	GPIO_init(LED2); GPIO_output(LED2);
	GPIO_init(LED3); GPIO_output(LED3);
	GPIO_init(LED4); GPIO_output(LED4);
	GPIO_init(LED5); GPIO_output(LED5);

	// turn off heater outputs
	GPIO_init(P2_4); GPIO_output(P2_4); GPIO_write(P2_4, 0);
	GPIO_init(P2_5); GPIO_output(P2_5); GPIO_write(P2_5, 0);
	GPIO_init(P2_6); GPIO_output(P2_6); GPIO_write(P2_6, 0);
	GPIO_init(P2_7); GPIO_output(P2_7); GPIO_write(P2_7, 0);

	setleds(31);

	UART_init(UART_RX, UART_TX, APPBAUD);

	printf("Bootloader Start\n");

	// give SD card time to wake up
	for (volatile int i = (1UL<<12); i; i--);

	SDCard_init(P0_9, P0_8, P0_7, P0_6);
	if (SDCard_disk_initialize() == 0)
		check_sd_firmware();

	int dfu = 0;
	if (isp_btn_pressed() == 0)
	{
		printf("ISP button pressed, entering DFU mode\n");
		dfu = 1;
	}
	else if (WDT_ReadTimeOutFlag()) {
		WDT_ClrTimeOutFlag();
		printf("WATCHDOG reset, entering DFU mode\n");
		dfu = 1;
	} else if (*(uint32_t *)USER_FLASH_START == 0xFFFFFFFF) {
        printf("User flash empty, enabling DFU\n");
        dfu = 1;
    }

	if (dfu)
		start_dfu();

#ifdef WATCHDOG
	WDT_Init(WDT_CLKSRC_IRC, WDT_MODE_RESET);
	WDT_Start(1<<22);
#endif

	// grab user code reset vector
// #ifdef DEBUG
	unsigned *p = (unsigned *)(USER_FLASH_START +4);
	printf("Jumping to 0x%x\n", *p);
// #endif

	while (UART_busy());
	printf("Jump!\n");
	while (UART_busy());
	UART_deinit();

	new_execute_user_code();

    UART_init(UART_RX, UART_TX, APPBAUD);

	printf("This should never happen\n");

	while (UART_busy());

	for (volatile int i = (1<<18);i;i--);

	NVIC_SystemReset();
}


DWORD get_fattime(void)
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

void NMI_Handler() {
// 	printf("NMI\n");
	for (;;);
}
void HardFault_Handler() {
// 	printf("HardFault\n");
	for (;;);
}
void MemManage_Handler() {
// 	printf("MemManage\n");
	for (;;);
}
void BusFault_Handler() {
// 	printf("BusFault\n");
	for (;;);
}
void UsageFault_Handler() {
// 	printf("UsageFault\n");
	for (;;);
}
