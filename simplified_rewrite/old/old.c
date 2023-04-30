
//    this code is now not used anymore.     dead code. 


// don't use this lol. 







// a simplified version of the editor, which uses a single string for the file contents. 

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

typedef size_t nat;


static char screen[1024] = {0};
static nat window_rows = 20;


struct datum {
	char* data;
	nat count;
	nat capacity;
};



static const nat max_count = 10;
//static const nat min_count = 5;


static struct datum* this = NULL;


static nat count = 0;

static nat mode = 1;

static nat atn = 0, atm = 0;


static inline bool stdin_is_empty(void) {
	fd_set f;
	FD_ZERO(&f);
	FD_SET(STDIN_FILENO, &f);
	struct timeval timeout = {0};
	return select(1, &f, NULL, NULL, &timeout) != 1;
}

static inline struct termios configure_terminal(void) {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	raw.c_oflag &= ~( (unsigned long)OPOST );
	raw.c_iflag &= ~( (unsigned long)BRKINT 
			| (unsigned long)ICRNL 
			| (unsigned long)INPCK 
			| (unsigned long)IXON );
	raw.c_lflag &= ~( (unsigned long)ECHO 
			| (unsigned long)ICANON 
			| (unsigned long)IEXTEN );
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}

static inline void insert(char c) {
	if (c == '\r') c = '\n';

	if (not count) {
		this = realloc(this, sizeof(struct datum));
		this->data = malloc(1);
		this->data[0] = c;
		this->count = 1;
		count = 1;
	} else {
		else if (this[atn].count < max_count) { 

			this[atn].data = realloc(this[atn].data, sizeof(char) * (1 + this[atn].count));

			struct datum* t = this + atn;

			if (t->count - atm) memmove(t->data + atm + 1, t->data + atm, t->count - atm);

			t->data[atm] = c;

			t->count++;

		} else if (this[atn].count >= max_count) {


		}

	}

	atm++;
}


static inline void delete() {
	if (not count) return;
}



static inline void interpret_escape_code() {
	char c = 0;
	read(0, &c, 1);

	if (c == '[') {
		read(0, &c, 1);

		//if (c == 'A') move_up();
		//else if (c == 'B') move_down();

		     if (c == 'C') {}
		else if (c == 'D') {}
	}
}


int main() {
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h\033[7l", 20);

	char c = 0;


loop:;	
	int l = 9;
	memcpy(screen, "\033[?25l\033[H", 9);

	nat line_count = 0;

	for (nat i = 0; i < count; i++) {
		//if (string[i] == 10) { l += sprintf(screen + l, "\033[K\r\n"); line_count++; }
		//else screen[l++] = string[i];


		l += sprintf(screen + l, "word %zu: {", i);
		for (nat j = 0; j < this[i].count; j++) {
			l += sprintf(screen + l, "%c", this[i].data[j]);
		}
		l += sprintf(screen + l, "}\033[K\r\n");
		l += sprintf(screen + l, "\033[K\r\n");

	}
	for (nat i = line_count; i < window_rows - 1; i++) 
		l += sprintf(screen + l, "\033[K\r\n");
	
	
	l += sprintf(screen + l, "\033[%d;%dH\033[?25h", 1, 1);
	write(1, screen, (size_t) l);


	read(0, &c, 1);
	if (c == 27) mode = 0;
	else if (c == 27 and stdin_is_empty()) mode = 0;
	else if (c == 27) interpret_escape_code();
	else if (c == 127) delete();
	else insert(c);

	if (mode) goto loop;

	write(1, "\033[?1049l\033[?1000l\033[7h", 20);	
	tcsetattr(0, TCSAFLUSH, &terminal);
}



// if (c == 10) { line++; column = 0; } else column++;





/*







*/




































/*

s w [
	u8 data[512];
	u16 count;
];


typedef uint8_t* word;         		// count of the word size   is   a 16-bit integer, 
					// represented, as two consequtive bytes at [0] : [1]




struct word {
	nat count;
	nat vc;
	nat vl;
	char* data;     // words are preallocated.      but!   there is exponential allocation for the array of words.
};
*/

	










// 1 1 1 1  1 1 1 1  



