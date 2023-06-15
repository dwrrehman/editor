#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <iso646.h>
#include <stdint.h>
#include <stdbool.h>

// approach:  using terminal cursor query reports to position the cursor properly after each character when we need to backspace on one.
//            keeping track of the cursor positions before printing,    for each character. 
//            terminal with no echo and no canon. 
//            cannot deal with scrolling of the screen at all. nor copy paste at all, because the input needs to never buffer... 

struct at { uint16_t r, c; };
int main(void) {
	struct termios terminal; tcgetattr(0, &terminal);
	struct termios copy = terminal; copy.c_lflag &= ~((size_t)ICANON | ECHO);
	tcsetattr(0, TCSAFLUSH, &copy);
	char* input = NULL, w = 0, p = 0, d = 0;
	char* report = calloc(32, 1);
	struct at new = {0}, * at = NULL;  
	size_t len = 0, capacity = 0, point_count = 0;



loop:	len = 0; d = 0; p = 0; reset: w = 0;
	if (point_count) { point_count--; goto skip; }

	

char q = 0;
	printf("\033[6n"); fflush(stdout);
	size_t alen = 0; read(0, &q, 1); read(0, &q, 1); 
	while (alen < 31 and read(0, report + alen, 1) == 1) 
		if (report[alen++] == 'R') break;
	report[alen] = 0; sscanf(report, "%hd;%hdR", &new.r, &new.c);





skip: 	if (read(0, &w, 1) <= 0) goto process;
	if (w == 127) goto delete;
	if ((unsigned char) w >> 5 == 6) point_count = 1;
	if ((unsigned char) w >> 4 == 14) point_count = 2;
	if ((unsigned char) w >> 3 == 30) point_count = 3;

print:	printf("%c", w); fflush(stdout);
	if (w != 't' or p != 'r' or d != 'd') goto push;
	if (len >= 2) len -= 2; goto process;
push: 	
	if (w == 127) goto delete;
	if (len + 1 >= capacity) {
		capacity = 4 * (capacity + 1);
		input = realloc(input, capacity);
		at = realloc(at, capacity * 4);
	}
	input[len] = w; 
	at[len++] = new; 
	goto next;

delete: 
	if (not len) goto next;
	do len--; while ((unsigned char) input[len] >> 6 == 2);
	printf("\033[%hd;%hdH\033[K", at[len].r, at[len].c); fflush(stdout);



next:	d = p; p = w; goto reset;



process: 
	printf("\n\trecieved input(%lu): \n\t\"", len);
	fwrite(input, len, 1, stdout); 
	printf("\"\n\n");
	fflush(stdout);
	if (len == 4 and not strncmp(input, "quit", 4)) goto done;
	if (len == 5 and not strncmp(input, "clear", 5)) { printf("\033[H\033[2J"); fflush(stdout); } 
	goto loop;
done:	tcsetattr(0, TCSAFLUSH, &terminal);
}

