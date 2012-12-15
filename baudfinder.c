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

/*
 * Run with:
 * gcc -std=gnu99 -ICMSISv2p00_LPC17xx/src -lm -o baudfinder baudfinder.c && ./baudfinder
 */

#ifndef __LPC17XX__

#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"

#include "math.h"

typedef struct {
	uint32_t	baud;
	uint8_t	pd;
	uint16_t	dl;
	uint32_t	dx;
	uint8_t	mulval;
	uint8_t	divaddval;
} uart_regs;

uart_regs best;

uint32_t real_baud(uint32_t pclk, uint32_t dl, uint32_t divaddval, uint32_t mulval)
{
	return (uint32_t) (((double) pclk) / (16.0 * ((double) (dl & 65535)) * (1.0 + (((double) divaddval) / ((double) mulval)))) + 0.5);
}

static uint32_t const baud(uint32_t pclk, uint32_t dl, uint32_t divaddval, uint32_t mulval)
{
	// 65535 * 14 * 16 is less than 2**24 so we have a spare 8 bits of precision
	// we can use them to increase our accuracy quite a bit
	// pclk is less than 2**27, so we have 5 spare bits for the numerator
	// this means we can do (numerator * 2**5) / (denominator * 2**8) * 2**3 to get the highest accuracy possible with 32 bit integers
	// denominator is 16 * (dl * (1 + (divadd / mul)) which can be expanded to
	// dl*16 + dl*16*divadd/mul which gives far more opportunity for using all our precision
	uint32_t dx = ((dl * 16 * 32 * 8) + ((dl * 16 * divaddval * 32 * 8) / mulval));
	return ((pclk * 32) / dx) * 8;
}

static uint32_t const uabs(const uint32_t a, const uint32_t b)
{
	if (a>=b)
		return a-b;
	return b-a;
}

int baud_space_search(uint32_t target_baud, uint32_t SystemCoreClock, uart_regs *r)
{
	uint32_t pd, dl, mulval, divaddval;
	int i = 0;
	for (pd = ((target_baud < 1000000)?3:1); pd < 4; pd--)
	{
		uint32_t pclk = SystemCoreClock / (1<<pd);
		for (mulval = 1; mulval < 16; mulval++)
		{
			for (divaddval = 0; divaddval < mulval; divaddval++)
			{
				i++;
				// baud = pclk / (16 * dl * (1 + (DivAdd / Mul))
				// solving for dl, we get dl = mul * pclk / (16 * baud * (divadd + mul))
				// we double the numerator, add 1 to the result then halve to effectivel round up when dl % 1 > 0.5
				dl = (((2 * mulval * pclk) / (16 * target_baud * (divaddval + mulval))) + 1) / 2;

				// dl is a 16 bit field, if result needs more then we search again
				if (dl > 65535)
					continue;

				// datasheet says if DLL==DLM==0, then 1 is used instead since divide by zero is ungood
				if (dl == 0)
					dl = 1;

				// datasheet says if DIVADDVAL > 0 then DL must be >= 2
				if ((divaddval > 0) && (dl < 2))
					dl = 2;

// 				uint32_t b = baud(pclk, dl, divaddval, mulval);
				uint32_t dx = ((dl * 16 * 32 * 8) + ((dl * 16 * divaddval * 32 * 8) / mulval));
				uint32_t b = ((pclk * 32) / dx) * 8;
				if (uabs(b, target_baud) < uabs(r->baud, target_baud))
				{
					r->baud      = b;
					r->pd        = pd;
					r->dl        = dl;
					r->dx       = ((dl * 16 * 32 * 8) + ((dl * 16 * divaddval * 32 * 8) / mulval));
					r->mulval    = mulval;
					r->divaddval = divaddval;
// 					printf("\t\t{%7d,%4d,%6d,%3d,%3d},\t// Actual baud: %7d, error %c%4.2f%%, %d iterations\n", target_baud, 1<<r->pd, r->dl, r->mulval, r->divaddval, b, ((b > target_baud)?'+':((b < target_baud)?'-':' ')), (uabs(target_baud, b) * 100.0) / target_baud, i);
					if (b == target_baud)
						return i;
					// within 0.08%
					if ((uabs(r->baud, target_baud) * 1536 / target_baud) < 1)
						return i;
				}
			}
		}
		// don't check higher pclk if we're within 0.5%
		if ((uabs(r->baud, target_baud) * 200 / target_baud) < 1)
			return i;
	}
	return i;
}

uint32_t find_baud(uint32_t target_baud, uint32_t SystemCoreClock)
{
	int i = baud_space_search(target_baud, SystemCoreClock, &best);

	uint32_t b = real_baud(SystemCoreClock / (1<<best.pd), best.dl, best.divaddval, best.mulval);
// 	printf("%7d,%2d,%3d,%3d,%6d,%9d: %7d/%7d [%4.2f%%]\n", target_baud, best.pd, best.dl >> 8, best.dl & 255, best.mulval, best.divaddval, b, best.baud, (uabs(target_baud, b) * 100.0) / target_baud);
	printf("\t\t{%7d,%4d,%6d,%3d,%3d},\t// Actual baud: %7d, error %c%4.2f%%, %d iterations\n", target_baud, best.pd, best.dl, best.mulval, best.divaddval, b, ((b > target_baud)?'+':((b < target_baud)?'-':' ')), (uabs(target_baud, b) * 100.0) / target_baud, i);
}

#include <system_LPC17xx.c>

uint32_t main(uint32_t argc, char **argv)
{
// 	int N = ((PLL0CFG_Val >> 16) & 0xFF) + 1;
// 	int M = (PLL0CFG_Val & 0x3FF) + 1;

// 	uint32_t Fcco = (2ULL * M * XTAL) / N;
// 	printf("Fcco: %gMHz\n", Fcco / 1000000.0);

// 	uint32_t SystemCoreClock = Fcco / (CCLKCFG_Val + 1);
	uint32_t SystemCoreClock = __CORE_CLK;
// 	printf("CPU CLK: %gMHz\n", SystemCoreClock / 1000000.0);

// 	uint32_t usbclk = Fcco / (USBCLKCFG_Val + 1);
// 	printf("USB CLK: %gMHz\n", usbclk / 1000000.0);

// 	printf(" TARGET PD DLM DLL MULVAL DIVADDVAL:    BAUD/  IBAUD [ERROR]\n");
	printf("#if (__CORE_CLK) == %d\n", SystemCoreClock);
	printf("\ttypedef struct {\n\t\tuint32_t baud;\n\t\tuint8_t  pclk;\n\t\tuint16_t dl;\n\t\tuint8_t  mul;\n\t\tuint8_t  divadd;\n\t} BAUD_LUT;\n");
	printf("\tBAUD_LUT baud_lut[] = {\n");
	printf("\t\t//  BAUD PCLK     DL MUL DIV\n");

	find_baud(9600   , SystemCoreClock);
	find_baud(38400  , SystemCoreClock);
	find_baud(57600  , SystemCoreClock);
	find_baud(115200 , SystemCoreClock);
	find_baud(230400 , SystemCoreClock);
	find_baud(250000 , SystemCoreClock);
	find_baud(1000000, SystemCoreClock);
	find_baud(2000000, SystemCoreClock);
	find_baud(4000000, SystemCoreClock);

	printf("\t\t{%7d,%4d,%6d,%3d,%3d}\t// END\n", 0, 0, 0, 0, 0);
	printf("\t};\n");

	printf("#endif /* __CORE_CLK == %d */\n", SystemCoreClock);
	return 0;
}

#endif /* ifndef __LPC17XX__ */
