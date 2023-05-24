#include <iso646.h>       // simplified rewrite of the text editor by dwrr on 2304307.024007
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

typedef uint64_t nat;
struct word { char* data; nat count; };
static nat m = 0, n = 0, cm = 0, cn = 0, mode = 0;
static struct word* text = NULL;

static bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2; }
static bool stdin_is_empty(void) {
	fd_set f; FD_ZERO(&f); FD_SET(0, &f);
	struct timeval timeout = {0};
	return select(1, &f, 0, 0, &timeout) != 1;
}

static struct termios configure_terminal(void) {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	raw.c_oflag &= ~((size_t)OPOST);
	raw.c_iflag &= ~((size_t)BRKINT | (size_t)ICRNL | (size_t)INPCK | (size_t)IXON);
	raw.c_lflag &= ~((size_t)ECHO | (size_t)ICANON | (size_t)IEXTEN);
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}

static void insert(char c) {
	 if (cm == m or cn == n) {
		text = realloc(text, (n + 1) * sizeof *text);
		memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
		if (cn < n) cn++; 
		text[cn].data = malloc(m);
		text[cn].data[0] = c;
		text[cn].count = 1;
		n++; cm = 1;
	} else if (text[cn].count < m) {
		memmove(text[cn].data + cm + 1, text[cn].data + cm, text[cn].count - cm);
		text[cn].data[cm++] = c;
		text[cn].count++;
	} else {
		text = realloc(text, (n + 1) * sizeof *text);
		memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
		n++;
		text[cn + 1].data = malloc(m);
		memmove(text[cn + 1].data, text[cn].data + cm, text[cn].count - cm);
		text[cn + 1].count = text[cn].count - cm;
		text[cn].data[cm++] = c;
		text[cn].count = cm;
	}
}

static char delete(void) {
	top: if (cm) {
		cm--;
		char c = text[cn].data[cm];
		text[cn].count--;
		memmove(text[cn].data + cm, text[cn].data + cm + 1, text[cn].count - cm);
		if (text[cn].count) goto r;
		n--;
		memmove(text + cn, text + cn + 1, (n - cn) * sizeof *text);
		text = realloc(text, n * sizeof *text);
		if (not n or not cn) goto r;
		cn--; cm = text[cn].count;
		r: return c;
	} else if (cn < n and text[cn].count) {
		if (not cn) return 0;
		cn--;
		cm = text[cn].count;
		goto top;
	} else return 0;
}

static void move_left(void) {
	if (not n) return;
	do if (cm) cm--; else if (cn) { cn--; cm = text[cn].count - 1; }
	while ( cm  < text[cn].count and zero_width(text[cn].data[cm]) or
		cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);
}

static void move_right(void) {
	if (not n) return;
	do if (cm < text[cn].count) cm++; else if (cn < n - 1) { cn++; cm = 1; }
	while ( cm  < text[cn].count and zero_width(text[cn].data[cm]) or
		cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);
}

static void display(nat display_mode) {
	static char* screen = NULL;
	static nat screen_size = 0, window_rows = 0, window_columns = 0, cursor_column = 0, cursor_row = 0, om = 0, on = 0;
	
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	if (not window.ws_row or not window.ws_col) { window.ws_row = 24; window.ws_col = 60; }
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen_size = 32 + (window_rows + 2) * (window_columns * 4 + 5);
		screen = realloc(screen, (size_t) screen_size);
	}
	bool shifted = false;
	nat screen_column = 0, screen_row = 0, start = window_columns * 4 + 5, on1 = 0, om1 = 0, i = on, j = om;
	int length = snprintf(screen, screen_size, "\033[?25l\033[H");

fill_screen: while (i <= n) {
		while (j <= m) {
			if (i == cn and j == cm) { cursor_row = screen_row; cursor_column = screen_column; }
			if (i >= n or j >= text[i].count) break;
			const char c = text[i].data[j];

			if (c == 10) { next_char_newline: j++; goto print_newline; }
			else if (c == 9) {
				do {
					if (screen_column >= window_columns) goto next_char_newline; screen_column++;
					length += snprintf(screen + length, screen_size, " ");
				} while (screen_column % 8);
			} else {
				if (zero_width(c)) goto print_char;
				if (screen_column >= window_columns) goto print_newline; screen_column++; 
				print_char: length += snprintf(screen + length, screen_size, "%c", c);
			}

			j++; continue;
			print_newline: length += snprintf(screen + length, screen_size, "\033[K");
			if (display_mode == 1 or screen_row >= window_rows - 1) goto print_cursor;
			length += snprintf(screen + length, screen_size, "\r\n");
			screen_row++; screen_column = 0;
			if (screen_row == 1) { on1 = i; om1 = j; }
		}
		i++; j = 0;
	}

	if (not shifted and cursor_row == window_rows - 1) {
		int t = 9; while (screen[t] != 10) t++; screen[t] = 13;

		 // we can cache the position of the 10 when we save on1 and om1. yay! 

		cursor_row--; screen_row--; screen_column = 0;
		on = on1; om = om1; shifted = true; goto fill_screen;
		
	} else if (not cursor_row) {
		
		

	}

	if (screen_row < window_rows) goto print_newline;
	print_cursor: length += snprintf(screen + length, screen_size, "\033[%llu;%lluH\033[?25h", cursor_row + 1, cursor_column + 1);
	write(1, screen, (size_t) length);
}

static void interpret_sequence(void) {
	char c = 0; read(0, &c, 1); 
	if (c != '[') return; read(0, &c, 1);
	if (c == 'D') move_left();
	else if (c == 'C') move_right();
	else if (c == 'M') {
		read(0, &c, 1);
		if (c == 97) { read(0, &c, 1); read(0, &c, 1); 
		} else if (c == 96) { read(0, &c, 1); read(0, &c, 1); 
		} else { char str[3] = {0}; read(0, str + 0, 1); read(0, str + 1, 1); }
	}
}





int main(int argc, const char** argv) {

	m = 10; n = 0; mode = 1;
	
	if (argc == 2) {

		FILE* file = fopen(argv[1], "r");
		if (not file) {
			printf("error: fopen: %s", strerror(errno));
			return 0;
		}

		fseek(file, 0, SEEK_END);        
	        size_t length = (size_t) ftell(file);
		char* local_text = malloc(sizeof(char) * length);
	        fseek(file, 0, SEEK_SET);
	        fread(local_text, sizeof(char), length, file);
		fclose(file);

		for (size_t i = 0; i < length; i++) insert(local_text[i]);

		cn = 0; cm = 0;

		free(local_text);
	}

	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h\033[7l\033[r", 23);

	char c = 0; 
loop:	display(0); 
	read(0, &c, 1);
	if (c == 27 and stdin_is_empty()) mode = 0;
	else if (c == 27) interpret_sequence(); 
	else if (c == 127) while (zero_width(delete()));
	else if (c == 13) insert(10);
	else insert(c);
	if (mode) goto loop;
	write(1, "\033[?1049l\033[?1000l\033[7h", 20);
	tcsetattr(0, TCSAFLUSH, &terminal);
}







/*

static void debug_display(void) {
	printf("\033[H\033[J");
	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);
	for (nat i = 0; i < n; i++) {
		printf("%-3llu %c ", i, i == cn ? '*' : ':' );
		printf("%-3llu", text[i].count);
		if (i and not text[i].count) abort();
		for (nat j = 0; j < m; j++) {
			putchar(j == cm and i == cn ? '[' : ' ');
			if (j < text[i].count) 
				{ if (((unsigned char)text[i].data[j]) >> 7) 
					printf("(%02hhx)", (unsigned char) text[i].data[j]); 
				else printf("%c", text[i].data[j]);  }
			else  printf("-");
			putchar(j == cm and i == cn ? ']' : ' ');
		}
		puts(" | ");
	}
	puts(".");
	printf("(cm=%llu,cn=%llu): ", cm, cn);
}

static void string_display(void) {
	printf("\033[H\033[J");
	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);
	for (nat i = 0; i < n; i++) {
		if (i and not text[i].count) abort();
		for (nat j = 0; j < m; j++) 
			if (j < text[i].count) printf("%c", text[i].data[j]); 
	}
	puts("");
}









static inline void open_file(const char* given_filename) {
	
	

	FILE* file = fopen(given_filename, "r");
	if (not file) {
		sprintf(message, "error: fopen: %s", strerror(errno));
		return;
	}

	fseek(file, 0, SEEK_END);        
        size_t length = (size_t) ftell(file);
	char* text = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(text, sizeof(char), length, file);
	fclose(file);

	for (size_t i = 0; i < length; i++) insert(text[i], 0);
	free(text); 
	saved = 1; 
	autosaved = 1; 
	mode = 1; 
	move_top();



	// open file 996      this overwrites.                 these should be     strlcpy's!!!!!!!!
	// execute 1677
	// editor 1812

	strlcpy(filename, 

		given_filename, 

		sizeof filename);

	//strlcpy(location, given_filename, sizeof filename); // todo:   seperate out these two things!!!
	//sprintf(message, "read %lub", length);
}






// go backwards from on and om   at most     window_width number of chars, backwards,    or until you hit a newline.
		// this is the string we want to put before the current screen string. 

		// prepend a line at the beginning by using the empty space, at the beginning, 
		// then subtract from start according to how many things you put there.
		// remove the last line, by doing length --, until you hit the newline of the second-to-last line. 




*/

















