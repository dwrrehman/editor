#include <iso646.h>    // Daniel Warren Riaz Rehman's minimalist terminal-based text editor.
#include <stdint.h>
#include <stdio.h>     //     designed with reliability, minimalism, 
#include <stdlib.h>    //     simplicity, and ergonimics in mind.
#include <string.h>
#include <unistd.h>    //          written on 2101177.005105
#include <termios.h>
#include <sys/ioctl.h>

struct line { char* data; int count, capacity; };

static struct line* lines = NULL;
static int count = 0;
static int capacity = 0;
static int lcl = 0; // logical cursor line
static int lcc = 0; // logical cursor column
static int vcl = 0; // visual cursor line
static int vcc = 0; // visual cursor column
static int vol = 0; // visual origin line
static int voc = 0; // visual origin column
static int vsl = 0; // visual screen line
static int vsc = 0; // visual screen column
static int vdc = 0; // visual desired column
static int window_rows = 10;
static int window_columns = 34;
static int wrap_width = 60;
static int tab_width = 8;

static inline char zero_width(char c) { return (((uint8_t)c) >> 6) == 2; }
static inline char visual(char c) { return (((uint8_t)c) >> 6) != 2; }

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
		if (k == '\t') { do length++; while (length % tab_width); } 
		else if (visual(k)) length++;
	}
	return length;
}

static inline void move_left() {
	if (not lcc) {
		if (not lcl) return;
		lcl--; lcc = lines[lcl].count;
		vcl--; if (vsl) vsl--; else if (vol) vol--;
		vcc = compute_visual();
		if (vcc > window_columns - 1) { vsc = window_columns - 1; voc = vcc - vsc; } 
		else { vsc = vcc; voc = 0; }
	} else {
		do lcc--; while (lcc and zero_width(lines[lcl].data[lcc]));
		if (lines[lcl].data[lcc] == '\t') {
			int save = vcc;
			vcc = compute_visual();
			int diff = save - vcc;
			if (vsc >= diff) vsc -= diff; 
			else if (voc >= diff - vsc) { voc -= diff - vsc; vsc = 0; }
		} else {
			vcc--; if (vsc) vsc--; else if (voc) voc--;
		}
	}
}

static inline void move_right() {
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; lcc = 0;
		vcl++; vcc = 0; voc = 0; vsc = 0;
		if (vsl < window_rows - 1) vsl++; else vol++;
	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do { 
				vcc++; if (vsc < window_columns - 1) vsc++; else voc++;
			} while (vcc % tab_width); 
		} else {
			vcc++; if (vsc < window_columns - 1) vsc++; else voc++;
		}
		do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
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
	if (zero_width(c)) lcc++; else move_right();
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
	int length = 9;
	memcpy(screen, "\033[?25l\033[H", 9);

	int line = 0, column = 0;
	int visual_line = 0, visual_column = 0;
	int screen_line = 0, screen_column = 0;

	while (screen_line < window_rows) {

		if (visual_line >= vol and visual_line < vol + window_rows) {

			if (line < count) {

				while (screen_column < window_columns) {
		
					if (column >= lines[line].count) break;

					char c = lines[line].data[column];

					if (c == '\t') {
						do { 
							if (visual_column >= voc and visual_column < voc + window_columns) {
								screen[length++] = ' ';
								screen_column++; 
							}
							visual_column++;

						} while (visual_column % tab_width);
					} else {
						if (visual_column >= voc and visual_column < voc + window_columns){
							screen[length++] = c;
							if (visual(c)) { screen_column++; }
						}
						if (visual(c)) visual_column++; 
					}
					column++;
				}
			}
			
			screen[length++] = '\033';
			screen[length++] = '[';	
			screen[length++] = 'K';
			if (screen_line < window_rows - 1) {
				screen[length++] = '\r';
				screen[length++] = '\n';
			}
			screen_line++;
			screen_column = 0;
		}
		visual_line++;
		visual_column = 0;

		line++;
		column = 0;
	}

	/*length += sprintf(screen + length,
		"\033[K\n\rxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\033[K\n\r[%d,%d:(%d,%d){%d,%d}[%d,%d:%d,%d]\033[K\n\r",
	count, lines[lcl].count, lcl,lcc, vcl,vcc, vol,voc, vsl,vsc );*/

	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", vsl + 1, vsc + 1);

	write(1, screen, (size_t) length);
	free(screen);
}

int main() {
	lines = calloc((size_t) (count = 1), sizeof(struct line));
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);	

	do {
		adjust_window_size();
		display();
		char c = 0;
		read(0, &c, 1);
		if (c == 'Q') break;
		else if (c == 'J') move_left();
		else if (c == ':') move_right();
		else if (c == 127) delete();
		else insert(c);
	} while (1);

	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);
}
































// ------------------------------------------old code-----------------------------------------















































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



//  move left:

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
	//} else {
	// 	do {
	// 		file->column--;
	// 		if (file->screen_column) file->screen_column--;
	// 		else if (file->origin_column) file->origin_column--;
	// 	} while (file->column and 
	// 		is_unicode(file->lines[file->line].line[file->column]));



// move right:

	// 	file->line++;
	// 	file->column = 0;
	// 	file->origin_column = 0;
	// 	file->screen_column = 0;
	// 	if (file->screen_line < window_rows) file->screen_line++;
	// 	else file->origin_line++;
	// } else {
	// 	do {
	// 		file->column++;
	// 		if (file->screen_column < window_columns) file->screen_column++;
	// 		else file->origin_column++;
	// 	} while (file->column < file->lines[file->line].length and 
	// 		 is_unicode(file->lines[file->line].line[file->column]));



/*
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



*/


		// if (column >= wrap_width) goto next_line;
				// if (column < file->origin_column) {
				// 	column++;
				// 	continue;
				// }
			



/*while (visual_length < voc and column < lines[line].count) {

			char c = lines[line].data[column++];
	
			if (c == '\t') {
				do {
					if (not(visual_length < voc and column < lines[line].count)) break;
					
				} while (visual_length % tab_width);
			} else if (visual(c)) visual_length++;
		}*/





/*
static int lol = 0; // logical origin line
static int loc = 0; // logical origin column

static int lsl = 0; // logical screen line
static int lsc = 0; // logical screen column












for (int line = vol; line < count; line++) {

		if (screen_line >= window_rows - 2) break;

		int column = 0; 
		int visual_length = 0;

		for (; column < lines[line].count; column++) {

			if (screen_column >= window_columns) break;
		
			if (column == lcc and line == lcl) {
				cl = screen_line;
				cc = screen_column;
			}
		
			char c = lines[line].data[column];

			if (c == '\t') {
				do { 
					if (visual_length >= voc) screen[length++] = ' ';
					screen_column++;
					visual_length++;
				} while (screen_column % tab_width);
			} else {
				if (visual_length >= voc) screen[length++] = c;
				if (visual(c)) { screen_column++; visual_length++; }
			}
		}

		if (column == lcc and line == lcl) {
			cl = screen_line;
			cc = screen_column;
		}

		screen[length++] = '\033';
		screen[length++] = '[';	
		screen[length++] = 'K';
		if (screen_line < window_rows - 1) {
			screen[length++] = '\r';
			screen[length++] = '\n';
		}
		screen_line++;
		screen_column = 0;
	}

	for (; screen_line < window_rows - 2; screen_line++) {
		screen[length++] = '\033';
		screen[length++] = '[';	
		screen[length++] = 'K';
		if (screen_line < window_rows - 1) {
			screen[length++] = '\r';
			screen[length++] = '\n';
		}
	}






*/



