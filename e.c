#include <iso646.h>    // Daniel Warren Riaz Rehman's minimalist terminal-based text editor.
#include <termios.h>   //
#include <stdio.h>     //     designed with reliability, minimalism, 
#include <stdlib.h>    //     simplicity, and ergonimics in mind.
#include <string.h>    //
#include <unistd.h>    //          written on 2101177.005105
#include <sys/ioctl.h> //

struct line { char* data; int count, capacity; };
static struct line* lines = NULL;
static int count = 0;
static int capacity = 0;
static int lcl = 0;
static int lcc = 0;
static int vcl = 0;
static int vcc = 0;
static int vol = 0;
static int voc = 0;
static int vsl = 0;
static int vsc = 0;
static int vdc = 0;
static int window_rows = 10;
static int window_columns = 34;
static int wrap_width = 60;
static int tab_width = 8;

static inline char zero_width(char c) { 
	return (((unsigned char)c) >> 6) == 2; 
}

static inline char visual(char c) { 
	return (((unsigned char)c) >> 6) != 2; 
}

static inline struct termios configure_terminal() {
	struct termios terminal = {0};
	tcgetattr(0, &terminal);
	struct termios raw = terminal;
	raw.c_oflag &= ~( (unsigned long)OPOST );
	raw.c_iflag &= ~( (unsigned long)BRKINT 
			| (unsigned long)ICRNL 
			| (unsigned long)INPCK 
			| (unsigned long)IXON );	
	raw.c_lflag &= ~( (unsigned long)ECHO 
			| (unsigned long)ICANON 
			| (unsigned long)IEXTEN );
	tcsetattr(0, TCSAFLUSH, &raw);
	return terminal;
}

static inline int compute_visual() {
	int length = 0;
	for (int c = 0; c < lcc; c++) {
		char k = lines[lcl].data[c];
		if (k == '\t') { 
			do length++; 
			while (length % tab_width); 
		} else if (visual(k)) length++;
	}
	return length;
}

static inline void move_left() {
	if (not lcc) {
		if (not lcl) return;
		lcl--; vcl--; 
		lcc = lines[lcl].count;
		vcc = compute_visual();
		if (vsl) vsl--; 
		else if (vol) vol--;
		if (vcc > window_columns - 1) { 
			vsc = window_columns - 1; 
			voc = vcc - vsc; 
		} else { 
			vsc = vcc; 
			voc = 0; 
		}
	} else {
		do lcc--; 
		while (lcc and zero_width(lines[lcl].data[lcc]));
		if (lines[lcl].data[lcc] == '\t') {
			int save = vcc;
			vcc = compute_visual();
			int diff = save - vcc;
			if (vsc >= diff) vsc -= diff; 
			else if (voc >= diff - vsc) { 
				voc -= diff - vsc; 
				vsc = 0; 
			}
		} else {
			vcc--; 
			if (vsc) vsc--; 
			else if (voc) voc--;
		}
	}
}

static inline void move_right() {
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; vcl++; 
		lcc = 0; vcc = 0; 
		voc = 0; vsc = 0;
		if (vsl < window_rows - 1) vsl++; 
		else vol++;
	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do { 
				vcc++; 
				if (vsc < window_columns - 1) vsc++; 
				else voc++;
			} while (vcc % tab_width); 
		} else {
			vcc++; 
			if (vsc < window_columns - 1) vsc++; 
			else voc++;
		}
		do lcc++; 
		while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
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
		// if (file->count + 1 > file->capacity) // enable if release mode.
		// file->lines = realloc(file->lines, sizeof(struct line) 
		// * (size_t)(file->capacity = 8 * (file->capacity + 1)));
		memmove(lines + lcl + 2, lines + lcl + 1, 
			sizeof(struct line) * (size_t)(count - (lcl + 1)));
		lines[lcl + 1] = new;
		count++;
	} else {
		this->data = realloc(this->data, (size_t) (this->count + 1)); // debug
		// enable if release mode.
		// if (this->length + 1 > this->capacity) this->line = realloc(this->line, 
		// (size_t)(this->capacity = 8 * (this->capacity + 1)));
		memmove(this->data + lcc + 1, this->data + lcc, (size_t) (this->count - lcc));
		this->data[lcc] = c;
		this->count++;
	}
	if (zero_width(c)) lcc++; 
	else move_right();
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
	window_rows = window.ws_row;
	window_columns = window.ws_col;
}

static inline void display() {
	char* screen = malloc(sizeof(char) * (size_t) (window_rows * window_columns * 4));
	int length = 9; memcpy(screen, "\033[?25l\033[H", 9);

	int line = 0, col = 0;
	int vl = 0, vc = 0;
	int sl = 0, sc = 0;

	do {
		if (vl < vol or vl >= vol + window_rows) goto next_logical_line;
		if (line >= count) goto next_screen_line;

		do {
			if (col >= lines[line].count) goto next_screen_line;
			char k = lines[line].data[col];
			if (k == '\t') {
				do { 
					if (vc >= voc and vc < voc + window_columns) {
						screen[length++] = ' ';
						sc++; 
					}
					vc++;

				} while (vc % tab_width);
			} else {
				if (vc >= voc and 
				vc < voc + window_columns and 
				(sc or visual(k))) {
					screen[length++] = k;
					if (visual(k)) { sc++; }
				}
				if (visual(k)) vc++; 
			}
			col++;

		} while (sc < window_columns);

	next_screen_line:
		screen[length++] = '\033';
		screen[length++] = '[';	
		screen[length++] = 'K';
		if (sl < window_rows - 1) {
			screen[length++] = '\r';
			screen[length++] = '\n';
		}
		sl++;
		sc = 0;

	next_logical_line:
		line++;
		col = 0;

	next_visual_line:
		vl++;
		vc = 0;

	} while (sl < window_rows);

	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", vsl + 1, vsc + 1);
	write(1, screen, (size_t) length);
	free(screen);
}

int main() {
	lines = calloc((size_t) (count = 1), sizeof(struct line));
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);	

begin:	adjust_window_size();
	display();
	char c = 0;
	read(0, &c, 1);
	if (c == 'J') move_left();
	else if (c == ':') move_right();
	else if (c == 127) delete();
	else insert(c);
	if (c != 'Q') goto begin;

	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);
}

