#include "gpio.h"

#include "LPC17xx.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

void GPIO_init(PinName pin) {
    GPIO_setup(pin);
}

void GPIO_setup(PinName pin) {
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
	PinCfg.Pinmode = PINSEL_PINMODE_TRISTATE;
	PinCfg.Portnum = PORT(pin);
	PinCfg.Pinnum = PIN(pin);
	PINSEL_ConfigPin(&PinCfg);
}

void GPIO_set_direction(PinName pin, uint8_t direction) {
	FIO_SetDir(PORT(pin), 1UL << PIN(pin), direction);
}

void GPIO_output(PinName pin) {
	GPIO_set_direction(pin, 1);
}

void GPIO_input(PinName pin) {
	GPIO_set_direction(pin, 0);
}

void GPIO_write(PinName pin, uint8_t value) {
	GPIO_output(pin);
	if (value)
		GPIO_set(pin);
	else
		GPIO_clear(pin);
}

void GPIO_set(PinName pin) {
	FIO_SetValue(PORT(pin), 1UL << PIN(pin));
}

void GPIO_clear(PinName pin) {
	FIO_ClearValue(PORT(pin), 1UL << PIN(pin));
}

uint8_t GPIO_get(PinName pin) {
	return (FIO_ReadValue(PORT(pin)) & (1UL << PIN(pin)))?255:0;
}
