#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>  // 2306202.165852:  
#include <stdlib.h> // a screen based simpler editor based on the ed-like one, 
#include <string.h> // uses a string ds, and is not modal. uses capital letters for commands! 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>         //  MAKE THE EDITOR ONLY USE ALPHA KEYS FOR KEYBINDINGSSSSSSSS PLEASEEEEEEEEE MODALLLLLL PLZ
#include <stdbool.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>    // todo:     - fix memory leaks       - write manual        x- test test test test test
#include <sys/ioctl.h>    //           - write feature list     x- redo readme.md     x- test with unicode                

static struct termios configure_terminal(void) {
	struct termios terminal;
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON | IEXTEN | ISIG);
	copy.c_iflag &= ~((size_t) BRKINT | IXON);
	tcsetattr(0, TCSAFLUSH, &copy);
	return terminal;
}

int main(void) {
	struct termios terminal = configure_terminal();

loop:;
	char c = 0;
	read(0, &c, 1);
	printf("received: %d\n", c);
	fflush(stdout);
	sleep(1);
	goto loop;
	printf("\033[H\033[2J");
	tcsetattr(0, TCSAFLUSH, &terminal);
}

