#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

int main(void) {
	struct termios terminal; 
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t)ICANON); 
	tcsetattr(0, TCSAFLUSH, &copy);

loop:;	size_t len = 0, capacity = 0;
	char* input = NULL, w = 0, p = 0, d = 0;
rc:	if (read(0, &w, 1) <= 0) goto process;
	if (w == 127) goto delete;
	if (w != 't' or p != 'r' or d != 'd') goto push;
	if (len >= 2) len -= 2;
	goto process;
push:	if (len + 1 >= capacity) {
		capacity = 4 * (capacity + 1);
		input = realloc(input, capacity);
	}
	input[len++] = w;
	goto next;
delete: if (not len) goto next;
	// delete the escape char that got printed, 
	// and then delete the next character...
next:	d = p; p = w; goto rc;

process: 
	printf("\n\trecieved input(%lu): \n\n\t\t\"", len);
	fwrite(input, len, 1, stdout); 
	printf("\"\n\n\n");
	fflush(stdout);

	if (len == 4 and not strncmp(input, "quit", 4)) goto done;
	if (len == 5 and not strncmp(input, "clear", 5)) { printf("\033[H\033[2J"); fflush(stdout); } 
	goto loop;
done:	tcsetattr(0, TCSAFLUSH, &terminal);
}

