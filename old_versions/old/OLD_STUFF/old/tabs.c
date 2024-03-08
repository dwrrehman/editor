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
	copy.c_lflag &= ~((size_t)ICANON | ECHO);
	tcsetattr(0, TCSAFLUSH, &copy);

	printf("hi \033H bub");
	fflush(stdout);
	
	getchar();

	printf("no\033[ggug");
	fflush(stdout);

	getchar();


	tcsetattr(0, TCSAFLUSH, &terminal);
}