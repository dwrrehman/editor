// sun.c: my solar powered text editor system.
// also testing file for interfaacing with the SSD1306 
// piOLED display, using the bcm2835 library.
// written by daniel rehman, on 2012255.212715
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <iso646.h>

#include "bcm2835.h"

// ASCII bit-mapped font. (monospace, 5x4)
static const uint8_t font[475] = { 
	 0, 0, 0, 0, 0, 2, 2, 2, 0, 2, 5, 5, 0, 0, 0, 5,15, 5,15, 5,
	14, 5, 6,10, 7, 3, 8, 6, 1,12, 2, 5, 2, 5,10, 2, 2, 0, 0, 0,
	 4, 2, 2, 2, 4, 2, 4, 4, 4, 2, 9, 6,15, 6, 9, 0, 2, 7, 2, 0,
	 0, 0, 0, 4, 2, 0, 0,15, 0, 0, 0, 0, 0, 0, 2, 4, 4, 2, 1, 1,
	 6,13,11, 9, 6, 2, 3, 2, 2, 7, 6, 9, 4, 2,15, 7, 8 ,6 ,8 ,7,
	 9, 9,15, 8, 8,15, 1, 7, 8, 7, 6, 1, 7, 9, 6,15, 8, 4, 2, 2,
	 6, 9, 6, 9, 6, 6, 9,14, 8, 8, 0, 2, 0, 2, 0, 0, 2, 0, 2, 1,
	 4, 2, 1, 2, 4, 0, 7, 0, 7, 0, 1, 2, 4, 2, 1, 3, 4, 2, 0, 2,
	 6, 9,13, 1,14, 6, 9,15, 9, 9, 7, 9, 7, 9, 7, 6, 9, 1, 9, 6,
	 7, 9, 9, 9, 7,15, 1, 7, 1,15,15, 1, 7, 1, 1, 6, 1,13, 9, 6,
	 9, 9,15, 9, 9, 7, 2, 2, 2, 7,15, 4, 4, 5, 2, 9, 5, 3, 5, 9,
	 1, 1, 1, 1,15, 9,15,15, 9, 9, 9,11,13, 9, 9, 6, 9, 9, 9, 6,
	 7, 9, 7, 1, 1, 6, 9, 9,13,14, 7, 9, 7, 5, 9,14, 1, 6, 8, 7,
	15, 2, 2, 2, 2, 9, 9, 9, 9, 6, 9, 9, 5, 5, 2, 9, 9, 9,15, 9,
	 9, 6, 6, 9, 9, 5, 5, 2, 2, 2,15, 8, 6, 1,15, 6, 2, 2, 2, 6,
	 1, 1, 2, 4, 4, 6, 4, 4, 4, 6, 2, 5, 0, 0, 0, 0, 0, 0, 0,15,
	 2, 4, 0, 0, 0, 0, 6, 9,13,10, 1, 1, 7, 9, 7, 0, 6, 1, 1, 6,
	 8, 8,14, 9,14, 0, 6, 9, 7,14,12, 2, 7, 2, 2, 6, 9,14, 8, 6,
	 1, 1, 7, 9, 9, 2, 0, 3, 2, 7, 4, 0, 4, 5, 2, 1, 5, 3, 3, 5,
	 3, 2, 2, 2,12, 0, 7,15, 9, 9, 0, 7, 9, 9, 9, 0, 6, 9, 9, 6,
	 0, 3, 5, 3, 1, 6, 5, 6, 4,12, 0,13, 3, 1, 1, 0,14, 1,14, 7,
	 2, 7, 2, 2, 4, 0, 9, 9, 9, 6, 0, 5, 5, 2, 2, 0, 9, 9,15, 6,
	 0, 9, 6, 6, 9, 0,10, 4, 5, 6, 0,15, 4, 2,15, 4, 2, 3, 2, 4,
	 2, 2, 2, 2, 2, 2, 4,12, 4, 2, 0,10, 5, 0, 0,
};

static inline struct termios configure_terminal() {
	struct termios terminal = {0};
	tcgetattr(0, &terminal);
	struct termios raw = terminal;
	raw.c_iflag &= ~((unsigned long)BRKINT | (unsigned long)ICRNL 
			| (unsigned long)INPCK | (unsigned long)IXON);
	raw.c_lflag &= ~((unsigned long)ECHO | (unsigned long)ICANON | (unsigned long)IEXTEN);
	tcsetattr(0, TCSAFLUSH, &raw);
	return terminal;
}

int main() {

	if (not bcm2835_init()) return 1;
	if (not bcm2835_i2c_begin()) return 1;	

	bcm2835_i2c_setSlaveAddress(0x3c);
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_148);

	const char initialization[] = { 
		0x80, 0xAE, 				// turn display off.
		0x80, 0x20, 0x80, 0x00,  		// set memory adress: horizontal mode. 
		0x80, 0x40, 				// set display start line to 0.
		0x80, 0xA1,  				// set segment remap: column addr 127 mapped to SEG0
		0x80, 0xA8, 0x80, 0x1F, 		// set multiplex ratio to 31 (height - 1)
		0x80, 0xC8,  				// set com out direction: scan from COM[N] to COM0
		0x80, 0xD3, 0x80, 0x00, 		// set display offset.
		0x80, 0xDA, 0x80, 0x02, 		// set com pin config.
		0x80, 0xD5, 0x80, 0x80, 		// display set clock divide.
		0x80, 0xD9, 0x80, 0xF1, 		// set precharge.
		0x80, 0xDB, 0x80, 0x30, 		// vcom desel.
		0x80, 0x81, 0x80, 0xFF, 		// set maximum contrast.
		0x80, 0xA4, 				// display follows ram contents.
		0x80, 0xA6, 				// set display as not inverted.
		0x80, 0x8D, 0x80, 0x14, 		// set charge pump.
		0x80, 0xAF, 				// turn display on.
	};
	
	bcm2835_i2c_write(initialization, sizeof initialization);

	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);	

	const char frame_header[] = {
		0x80, 0x21, // column address
		0x80, 0x00, // begin 0 
		0x80, 0x7F, // end 127 columns
		0x80, 0x22, // page address
		0x80, 0x00, // begin 0
		0x80, 0x03, // end 3 pages
	};
	
	char raw_frame[513];
	raw_frame[0] = 0x40;
	char* frame = raw_frame + 1;
	memset(frame, 0, 512);
	
	const int window_width = 128, window_height = 32; 
	const int window_column_count = 25, window_row_count = 5;
	int cursor_column = 0, cursor_line = 0;

	char text[5][25] = {0};
	memcpy(text[0], " !\"#$%&'()*+,-./012345678", 25);
	memcpy(text[1], "9:;<=>?@ABCDEFGHIJKLMNOPQ", 25);
	memcpy(text[2], "RSTUVWXYZ[\\]^_`abcdefghij", 25);
	memcpy(text[3], "klmnopqrstuvwxyz{|}~     ", 25);
	memcpy(text[4], "MYEDITOR(int x){exit(0);}", 25);
	
	while (1) {

		uint8_t segment = 0; // ranges from 0 to 127    (is per page)
		uint8_t common = 0; // ranges from 0 to 7       (is per page)
		uint8_t page = 0; // ranges from 0 to 3

		for (uint8_t line = 0; line < window_row_count; line++) {

			for (uint8_t slice = 0; slice < 5; slice++) {

				segment = 0;
				for (uint8_t column = 0; column < window_column_count; column++) {

					if (cursor_line == line and cursor_column == column)
						frame[segment + page * 128] |= 1 << common;
					else 
						frame[segment + page * 128] &= ~(1 << common);
					segment++;

					for (uint8_t bit = 0; bit < 4; bit++) {
						if (font[5 * (text[line][column] - 32) + slice] & (1 << bit))
							frame[segment + page * 128] |= 1 << common;
						else 
							frame[segment + page * 128] &= ~(1 << common);
						segment++;
					}
				}
				if (common == 7) { common = 0; page++; } else common++;
			}
			if (common == 7) { common = 0; page++; } else common++;
		}

		bcm2835_i2c_write(frame_header, sizeof frame_header);
		bcm2835_i2c_write(raw_frame, sizeof raw_frame);
		
		uint8_t c = 0; 
		read(0, &c, 1);
		
		if (c == 'q') break;
		
		else if (c == 'j') {
			if (cursor_column) cursor_column--;
		} else if (c == ';') {
			if (cursor_column < window_column_count) cursor_column++;
		} else if (c == 'i') {
			if (cursor_line < window_row_count) cursor_line++;
		} else if (c == 'o') {
			if (cursor_line) cursor_line--;
		}
		
		usleep(10000);
	}
	
cleanup:

	write(1, "\033[?1049l\033[?1000l", 16);
	tcsetattr(0, TCSAFLUSH, &terminal);
	
	char clear_display[] = { 0x80, 0xAE };
	bcm2835_i2c_write(clear_display, sizeof clear_display);

	bcm2835_i2c_end();
	bcm2835_close();
	return 0;
}
