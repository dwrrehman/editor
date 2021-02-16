// testing file for interfaacing with the SSD1306 piOLED display, using the bcm2835 library.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <iso646.h>

#include "bcm2835.h"
 
enum { slave_address = 0x3c };
enum { mode_read, mode_write };
enum { i2c_no_action, i2c_begin, i2c_end };

enum SSD1306_commands {
	SET_CONTRAST = 0x81,
	SET_ENTIRE_DISPLAY_ON = 0xA4,
	SET_NORMAL_INVERSION = 0xA6,
	SET_DISPLAY = 0xAE,
	SET_MEMORY_ADDRESS = 0x20,
	SET_COLUMN_ADDRESS = 0x21,
	SET_PAGE_ADDRESS = 0x22,
	SET_DISPLAY_START_LINE = 0x40,
	SET_SEG_REMAP = 0xA0,
	SET_MULTIPLEX_RATIO = 0xA8,
	SET_COM_OUT_DIR = 0xC0,
	SET_DISPLAY_OFFSET = 0xD3,
	SET_COM_PIN_CONFIG = 0xDA,
	SET_DISP_CLOCK_DIV = 0xD5,
	SET_PRECHARGE = 0xD9,
	SET_VCOM_DESEL = 0xDB,
	SET_CHARGE_PUMP = 0x8D,
};

int main() {

	uint8_t init = i2c_no_action;	
 	uint8_t mode = mode_read;
	uint16_t clk_div = BCM2835_I2C_CLOCK_DIVIDER_148;
 
	if (not bcm2835_init()) return 1;
   	
	if (init == i2c_no_action) return 0;
	else if (init == i2c_begin and not bcm2835_i2c_begin()) return 1;
	else if (init == i2c_end) bcm2835_i2c_end();
 
	bcm2835_i2c_setSlaveAddress(slave_address);
	bcm2835_i2c_setClockDivider(clk_div);
	
	if (mode == mode_read) {

		char buffer[32] = {0};
		int buffer_length = 32;

		uint8_t r = bcm2835_i2c_read(buffer, buffer_length);
		printf("read(%d): { \n", r);

		for (int i = 0; i < buffer_length; i++) {
			printf("%x ", buffer[i]);
		}
		printf("}\n");
	}

	if (mode == mode_write) {

		char buffer[32] = {0};
		int buffer_length = 1;

		uint8_t r = bcm2835_i2c_write(buffer, buffer_length);
		printf("write(%d): \n", r);
	}

	if (init == i2c_end) bcm2835_i2c_end();

	bcm2835_close();
	return 0;
}








// static inline void show_usage() {
//     printf("i2c \n");
//     printf("Usage: \n");
//     printf("  i2c [options] len [rcv/xmit bytes]\n");
//     printf("\n");
//     printf("  Invoking i2c results in an I2C transfer of a specified\n");
//     printf("    number of bytes.  Additionally, it can be used to set the appropriate\n");
//     printf("    GPIO pins to their respective I2C configurations or return them\n");
//     printf("    to GPIO input configuration.  Options include the I2C clock frequency,\n");
//     printf("    initialization option (i2c_begin and i2c_end).  i2c must be invoked\n");
//     printf("    with root privileges.\n");
//     printf("\n");
//     printf("  The following are the options, which must be a single letter\n");
//     printf("    preceded by a '-' and followed by another character.\n");
//     printf("    -dx where x is 'w' for write and 'r' is for read.\n");
//     printf("    -ix where x is the I2C init option, b[egin] or e[nd]\n");
//     printf("      The begin option must be executed before any transfer can happen.\n");
//     printf("        It may be included with a transfer.\n");
//     printf("      The end option will return the I2C pins to GPIO inputs.\n");
//     printf("        It may be included with a transfer.\n");
//     printf("    -cx where x is the clock divider from 250MHz. Allowed values\n");
//     printf("      are 150 through 2500.\n");
//     printf("      Corresponding frequencies are specified in bcm2835.h.\n");
//     printf("\n");
//     printf("    len: The number of bytes to be transmitted or received.\n");
//     printf("    The maximum number of bytes allowed is %d\n", MAX_LEN);
//     printf("\n");
//     printf("\n");
//     printf("\n");
//     exit(1);
// }
 


/*******************************************************************************
*
*   i2c.c
*
*
*   Description:
*   i2c is a command-line utility for executing i2c commands with the 
*   Broadcom bcm2835.  It was developed and tested on a Raspberry Pi single-board
*   computer model B.  The utility is based on the bcm2835 C library developed
*   by Mike McCauley of Open System Consultants, http://www.open.com.au/mikem/bcm2835/.
*
*   Invoking spincl results in a read or write I2C transfer.  Options include the
*   the I2C clock frequency, read/write, address, and port initialization/closing
*   procedures.  The command usage and command-line parameters are described below
*   in the showusage function, which prints the usage if no command-line parameters
*   are included or if there are any command-line parameter errors.  Invoking i2c 
*   requires root privilege.
*
*   This file contains the main function as well as functions for displaying
*   usage and for parsing the command line.
*
*   Open Source Licensing GNU GPLv3
*
*   Building:
* After installing bcm2835, you can build this 
* with something like:
* gcc -o i2c i2c.c -l bcm2835
* sudo ./i2c
*
* Or you can test it before installing with:
* gcc -o i2c -I ../../src ../../src/bcm2835.c i2c.c
* sudo ./i2c

*
*      Examples:
*
*           Set up ADC (Arduino: ADC1015)
*           sudo ./i2c -s72 -dw -ib 3 0x01 0x44 0x00 (select config register, setup mux, etc.)
*           sudo ./i2c -s72 -dw -ib 1 0x00 (select ADC data register)
*
*           Bias DAC (Arduino: MCP4725) at some voltage
*           sudo ./i2c -s99 -dw -ib 3 0x60 0x7F 0xF0 (FS output is with 0xFF 0xF0)
*           Read ADC convergence result
*           sudo ./i2c -s72 -dr -ib 2 (FS output is 0x7FF0 with PGA1 = 1)
*  
*/