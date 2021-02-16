#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <iso646.h>

//   !"#$%&'()*+,-./012345678
//  9:;<=>?@ABCDEFGHIJKLMNOPQ
//  RSTUVWXYZ[\]^_`abcdefghij
//  klmnopqrstuvwxyz{|}~     

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

static struct termios terminal = {0};

static inline void restore_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049l", 8);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
}

static inline void configure_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049h", 8);
    
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        abort();
    }
    
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        abort();
    }
}


int main() {

	configure_terminal();	 // terminal window is  [ cols=25 x rows=5 ]
	
	const int window_width = 128, window_height = 32; // using SSD1306 piOLED display.
	const int window_column_count = window_width / (4 + 1); // 25
	const int window_row_count = window_height / (5 + 1); // 5
	int cursor_line = 0, cursor_column = 0;

	const char** text = malloc(sizeof(const char*) * window_row_count);
	text[0] = " !\"#$%&'()*+,-./012345678";
	text[1] = "9:;<=>?@ABCDEFGHIJKLMNOPQ";
	text[2] = "RSTUVWXYZ[\\]^_`abcdefghij";
	text[3] = "klmnopqrstuvwxyz{|}~     ";
	text[4] = "MYEDITOR(int x){exit(0);}";

	while (1) {

		printf("\033[H\033[2J");

		for (int line = 0; line < window_row_count; line++) {

			for (int slice = 0; slice < 5; slice++) {

				for (int column = 0; column < window_column_count && text[line][column]; column++) {

					printf(cursor_line == line && cursor_column == column 
							? "\033[38;5;255m██\033[0m" 
							: "  ");

					for (int bit = 0; bit < 4; bit++) {
						printf((font[5 * (text[line][column] - 32) + slice] & (1 << bit)) 
							? "\033[38;5;255m██\033[0m" 
							: "  ");
					}
				}
				puts("");
			}
			puts("");
		}

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
	}
	restore_terminal();
}
