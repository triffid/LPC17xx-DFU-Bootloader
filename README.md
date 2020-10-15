Hold P2.12 low during reset to force DFU mode even if application firmware is present.

Pinouts for the SD card are at https://github.com/triffid/LPC17xx-DFU-Bootloader/blob/develop/main.c#L211

A basic instruction on using dfu-util: https://github.com/Smoothieware/Smoothieware/blob/edge/src/makefile#L126-L127

LED pins are set as outputs and toggled during bootloader: https://github.com/triffid/LPC17xx-DFU-Bootloader/blob/develop/pins.h#L47-L51

Some other pins are set output/low ( https://github.com/triffid/LPC17xx-DFU-Bootloader/blob/develop/main.c#L196-L200 ) due to this bootloader being predominantly designed for Smoothieboard.

If the bootloader finds a file named "firmware.bin" on the FAT32-formatted SD card's first partition, it will flash it to the application region (0x4000, +16k - see https://github.com/triffid/LPC17xx-DFU-Bootloader/blob/develop/lpc1769.ld#L34-L35 ), and after successful flashing, rename the file to "firmware.cur" - 'cur' meaning 'current'.