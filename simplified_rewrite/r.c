#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <stdbool.h>


#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <sys/time.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>




typedef uint64_t nat;

static nat 
	m = 5, 
	n = 0, 
	cm = 0, 
	cn = 0
;

struct word {
	char* data;
	nat count;
};

static struct word* text = NULL;

static inline char read_stdin(void) {
	char c = 0;
	read(0, &c, 1);
	return c;
}

static inline struct termios configure_terminal(void) {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	//raw.c_oflag &= ~( (unsigned long)OPOST );
	//raw.c_iflag &= ~( (unsigned long)BRKINT 
	//		| (unsigned long)ICRNL 
	//		| (unsigned long)INPCK 
	//		| (unsigned long)IXON );
	raw.c_lflag &= ~( (unsigned long)ECHO 
			| (unsigned long)ICANON 
			| (unsigned long)IEXTEN );
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}

static void move_right() {
	//if (cm == text[cn].count) { cm = 0; cn++; } else cm++;
}

static void insert(char c) {

	if (cn == n) {
		text = realloc(text, (n + 1) * sizeof *text);
		text[n].data = malloc(m);
		text[n].data[0] = c;
		text[n].count = 1;

		n++;
		cm++;

	} else if (cm == m)  {
		text[cn + 1].data = malloc(m);
		text[cn + 1].data[0] = c;
		text[cn + 1].count = 1;

		cn++; 
		cm = 0;

	} else if (text[cn].count < m) {
		memmove(text[cn].data + cm + 1, text[cn].data + cm, text[cn].count - cm);
		text[cn].data[cm] = c;
		text[cn].count++;

		cm++;

	} else {
		text = realloc(text, (n + 1) * sizeof *text);
		memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);

		text[cn + 1].data = malloc(m);
		memmove(text[cn + 1].data, text[cn].data + cm, text[cn].count - cm);
		text[cn + 1].count = text[cn].count - cm;

		text[cn].data[cm] = c;
		text[cn].count = cm + 1;

		n++;
		cm++;
	}
}


//		ABCDEFGHIx


static void debug_display() {

	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);
	for (nat i = 0; i < n; i++) {
		printf("%-3llu %c ", i, i == cn ? '*' : ':' );
		printf("%-3llu", text[i].count);
		for (nat j = 0; j < m; j++) {
			
			putchar(j == cm and i == cn ? '[' : ' ');
			
			if (j < text[i].count) 
				printf("%c", text[i].data[j]); 
			else 
				printf("-");

			putchar(j == cm and i == cn ? ']' : ' ');
		}
		puts(" | ");
	}
	puts(".");
}


static void string_display() {
	printf("\033[H\033[J");
	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);

	for (nat i = 0; i < n; i++) {
		for (nat j = 0; j < m; j++) {
			
			if (j == cm and i == cn) putchar('[');
			
			if (j < text[i].count) printf("%c", text[i].data[j]); 

			if (j == cm and i == cn) putchar(']');
		}
		
	}
	puts("");
}






int main() {

	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h\033[7l", 20);

	n = 4;
	text = malloc(sizeof *text * n);


	text[0].data = malloc(m);
	for (int i = 0; i < 5; i++) text[0].data[i] =  "beans"[i];
	text[0].count = 5;

	text[1].data = malloc(m);
	for (int i = 0; i < 5; i++) text[1].data[i] = " are "[i];
	text[1].count = 5;

	text[2].data = malloc(m);
	for (int i = 0; i < 5; i++) text[2].data[i] =  "cool "[i];
	text[2].count = 5;

	text[3].data = malloc(m);
	for (int i = 0; i < 5; i++) text[3].data[i] =  "so.."[i];
	text[3].count = 4;

	cn = 0;
	cm = 0;
loop:

	string_display();
	printf("(cm=%llu,cn=%llu): ", cm, cn);
here:;	char c = read_stdin(); 

	if (c == 'Q') goto done;
	else if (c == 'U') cn--;
	else if (c == 'D') cn++;
	else if (c == 'L') cm--;
	else if (c == 'R') cm++;
	else insert(c);

	goto loop;

	done: printf("quitting...\n");


	write(1, "\033[?1049l\033[?1000l\033[7h", 20);	
	tcsetattr(0, TCSAFLUSH, &terminal);





	

	


}












































//printf("0x%02hhx", text[i].data[j]);





/*

text = realloc(text, (n + 1) * sizeof *text);
memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
memmove(text[cn + 1].data, text[cn].data + cm, text[cn].count - cm);
text[cn + 1].count = text[cn].count - cm;
n++;

text[cn].data = malloc(m);
memcpy(text[cn].data, text[cn + 1].data, cm);

*/








/*
m = 7;

		ABC
		DEFG
*/


// memcpy(text[cn].data, text[cn + 1].data, cm);


