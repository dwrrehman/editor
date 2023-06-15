/// a simpler more correct and beautiful version of my text editor.
/// more minimalist, and efficient as well.

#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

struct line {
	char* data;
	int count;
	int capacity;
};

static struct line* lines = NULL;
static int count = 0;
static int capacity = 0;
static int lcl = 0;
static int lcc = 0;
static int lol = 0;
static int loc = 0;
static int lsl = 0;
static int lsc = 0;
static int vcl = 0;
static int vcc = 0;
static int vol = 0;
static int voc = 0;
static int vsl = 0;
static int vsc = 0;
static int vdc = 0;
static int wrap_width = 60;
static int window_rows = 20;
static int window_columns = 40;
static int tab_width = 8;

static inline char is_unicode(char c) { // 0b10xxxxxx
	return (((uint8_t)c) >> 6) == 2;
}

static inline struct termios configure_terminal() {
	struct termios terminal = {0};
	tcgetattr(0, &terminal);
	struct termios raw = terminal;
	raw.c_oflag &= ~((unsigned long)OPOST);
	raw.c_iflag &= ~((unsigned long)BRKINT | (unsigned long)ICRNL 
			| (unsigned long)INPCK | (unsigned long)IXON);	
	raw.c_lflag &= ~((unsigned long)ECHO | (unsigned long)ICANON | (unsigned long)IEXTEN);
	tcsetattr(0, TCSAFLUSH, &raw);
	return terminal;
}

static inline void move_left() {
	if (not lcc) {
		if (not lcl) return;
	// 	file->line--;
	// 	file->column = file->lines[file->line].length;
	// 	if (file->screen_line) file->screen_line--;
	// 	else if (file->origin_line) file->origin_line--;
	// 	if (file->column > window_columns) {
	// 		file->screen_column = window_columns;
	// 		file->origin_column = file->column - window_columns;
	// 	} else {
	// 		file->screen_column = (i16) file->column;
	// 		file->origin_column = 0;
	// 	}
	} else {
	// 	do {
	// 		file->column--;
	// 		if (file->screen_column) file->screen_column--;
	// 		else if (file->origin_column) file->origin_column--;
	// 	} while (file->column and 
	// 		is_unicode(file->lines[file->line].line[file->column]));
	}
}

static inline void move_right() {
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
	// 	file->line++;
	// 	file->column = 0;
	// 	file->origin_column = 0;
	// 	file->screen_column = 0;
	// 	if (file->screen_line < window_rows) file->screen_line++;
	// 	else file->origin_line++;
	} else {
	// 	do {
	// 		file->column++;
	// 		if (file->screen_column < window_columns) file->screen_column++;
	// 		else file->origin_column++;
	// 	} while (file->column < file->lines[file->line].length and 
	// 		 is_unicode(file->lines[file->line].line[file->column]));
	}
}

static inline void insert(char c) {
	struct line* this = lines + lcl;
	if (c == 13) {
		int rest = this->count - lcc;
		this->count = lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, this->data + lcc, (size_t) rest);
		this->data = realloc(this->data, (size_t) (this->count)); // debug
		lines = realloc(lines, sizeof(struct line) * (size_t)(count + 1)); // debug
		// if (file->count + 1 > file->capacity) 
		// file->lines = realloc(file->lines, sizeof(struct line) 
		// * (size_t)(file->capacity = 8 * (file->capacity + 1)));

		memmove(lines + lcl + 2, lines + lcl + 1, sizeof(struct line) * (size_t)(count - (lcl + 1)));
		lines[lcl + 1] = new;
		count++;
	} else {
		this->data = realloc(this->data, (size_t) (this->count + 1)); // debug
		// if (this->length + 1 > this->capacity) this->line = realloc(this->line, 
		// (size_t)(this->capacity = 8 * (this->capacity + 1)));
		memmove(this->data + lcc + 1, this->data + lcc, (size_t) (this->count - lcc));
		this->data[lcc] = c;
		this->count++;
	}
	if (is_unicode(c)) { 
		lcc++;
		// move along: loc, lol, lsc, lsl.
	} else move_right();
}

static inline void delete() {
	struct line* this = lines + lcl;
	if (not lcc) {
		if (not lcl) return;
		move_left();
		struct line* new = lines + lcl;
		new->data = realloc(new->data, (size_t)(new->count + this->count)); // debug
		if (this->count) memcpy(new->data + new->count, this->data, (size_t) this->count);
		new->count += this->count;
		memmove(lines + lcl + 1, lines + lcl + 2, 
			sizeof(struct line) * (size_t)(count - (lcl + 2)));
		count--;
		lines = realloc(lines, sizeof(struct line) * (size_t)(count)); // debug

	} else {
		int save = lcc;
		move_left();
		memmove(this->data + lcc, this->data + save, (size_t)(this->count - save));
		this->count -= save - lcc;
		this->data = realloc(this->data, (size_t) (this->count)); // debug
	}
}

static inline void adjust_window_size() {
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	window_rows = window.ws_row - 1;
	window_columns = window.ws_col - 1;
}

static inline void display() {
	char* screen = malloc(sizeof(char) * (size_t) (window_rows * window_columns * 4));
	int length = 9;
	memcpy(screen, "\033[?25l\033[H", 9);
	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", 1, 1);
	write(1, screen, (size_t) length);
	free(screen);
}

int main() {
	lines = calloc((size_t) (count = 1), sizeof(struct line));
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);	
	while (1) {
		adjust_window_size();
		display();
		char c = 0;
		read(0, &c, 1);
		if (c == 'q') break;
		else if (c == 'J') move_left();
		else if (c == ':') move_right();
		else if (c == 127) delete();
		else insert(c);
	}
	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);
}




























// ------------------------------------------old code-----------------------------------------------















































///NOTE:   after you get display working perfectly, code up the MOVE up and down functions!!! 
//   		then do word movement, and maybe also try for word wrapping..?!?!? that would be amazing.
///        also try to see what other features from text edit, you are actually still missing!!




				// if (column >= file->lines[line].length) break;
				// if (column >= wrap_width) goto next_line;
				// if (column < file->origin_column) {
				// 	column++;
				// 	continue;
				// }

				// if (column == file->column and line == file->line) {
				// 	cursor_screen_line = screen_line; 
				// 	cursor_screen_column = screen_column;
				// }

				// i8 c = file->lines[line].line[column++];

				// if (c == '\t') {
				// 	do { 
				// 		if (screen_column >= window_columns or 
				// 		    screen_column >= wrap_width) break;
				// 		screen[length++] = ' ';
				// 		screen_column++;
				// 	} while (screen_column % tab_width);
				// } else {
				// 	screen[length++] = (char) c;
				// 	if (!is_unicode(c)) screen_column++;
				// }

			// while (column < file->lines[line].length) {
			// 	if (column == wrap_width) goto next_line;
			// 	column++;
			// 	screen_column++;
			// }



			// if (column == file->column and line == file->line) {
			// 	cursor_screen_line = screen_line; 
			// 	cursor_screen_column = screen_column;
			// }




/*
lcl
lcc
lol
loc
lsl
lsc
vcl
vcc
vol
voc
vsl
vsc
vdc

*/


	// i32 line = file->origin_line, column = 0;
	// i32 wrap_line = 0, wrap_column = 0;
	// i16 screen_line = 0, screen_column = 0;
	// i16 cursor_screen_line = 0, cursor_screen_column = 0;

	// while (screen_line < window_rows) {

	// 	if (line >= file->count) goto next_screen_line;

	// 	while (screen_column < window_columns) {

	// 		if (column >= file->lines[line].length) goto next_line;

	// 		i8 c = file->lines[line].line[column];

	// 		if (c == '\t') {
	// 			do { 
	// 				if (screen_column >= window_columns or 
	// 				    screen_column >= wrap_width) break;
	// 				screen[length++] = ' ';
	// 				screen_column++;
	// 			} while (screen_column % tab_width);
	// 		} else {
	// 			screen[length++] = (char) c;
	// 			if (not is_unicode(c)) screen_column++;
	// 		}
    
	// 		column++;
	// 	}

	// next_line:
	// 	line++;
	// 	column = 0;

	// next_screen_line:
	// 	screen[length++] = '\033';
	// 	screen[length++] = '[';
	// 	screen[length++] = 'K';
	// 	if (screen_line < window_rows - 1) {
	// 		screen[length++] = '\r';
	// 		screen[length++] = '\n';
	// 	}
	// 	screen_line++;
	// 	screen_column = 0;
	// }



		// while (column < file->lines[line].length) {
		// 	column++;
		// if (column >= wrap_width) goto next_line;
		// 			if (column < file->origin_column) {
		// 				column++;
		// 				continue;
		// 			}
		// 		i
		// }



/*

static inline void move_left(struct file* file) {
	if (not file->column) {
		if (not file->line) return;
		file->line--;
		file->column = file->lines[file->line].length;
		if (file->screen_line) file->screen_line--;
		else if (file->origin_line) file->origin_line--;
		if (file->column > window_columns) {
			file->screen_column = window_columns;
			file->origin_column = file->column - window_columns;
		} else {
			file->screen_column = (i16) file->column;
			file->origin_column = 0;
		}
	} else {
		do {
			file->column--;
			if (file->screen_column) file->screen_column--;
			else if (file->origin_column) file->origin_column--;
		} while (file->column and 
			is_unicode(file->lines[file->line].line[file->column]));
	}
}

static inline void move_right(struct file* file) {
	if (file->column >= file->lines[file->line].length) {
		if (file->line + 1 >= file->count) return;
		file->line++;
		file->column = 0;
		file->origin_column = 0;
		file->screen_column = 0;
		if (file->screen_line < window_rows) file->screen_line++;
		else file->origin_line++;
	} else {
		do {
			file->column++;
			if (file->screen_column < window_columns) file->screen_column++;
			else file->origin_column++;
		} while (file->column < file->lines[file->line].length and 
			 is_unicode(file->lines[file->line].line[file->column]));
	}
}


*/

