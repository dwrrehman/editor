#include <stdio.h>
#include <unistd.h>
#include <termios.h>



static struct termios configure_terminal(void) {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	raw.c_lflag &= ~((size_t)ECHO | (size_t)ICANON);
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}



int main(void) {

	


	char c = 0;


	struct termios terminal = configure_terminal();
	printf("\033[?1049h"); fflush(stdout);

	printf("bubbles and \nbeans!\033[6n");
	fflush(stdout);

	int row = 0, col = 0;
 	scanf("\033[%d;%dR", &row, &col);
	printf("done! received row = %d, column = %d\n", row, col);

	read(0, &c, 1);


	printf("\033[?1049l");
	tcsetattr(0, TCSAFLUSH, &terminal);	

}




/*
	do {
		read(0, &c, 1);
		printf("read = %d(%c)\n", (int) c, c);
	} while (c != 'R');
*/



