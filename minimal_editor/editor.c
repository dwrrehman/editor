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

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;

static i16 window_rows = 20;
static i16 window_columns = 40;
static i32 wrap_width = 60;
static i8 tab_width = 8;

struct line {
	i8* line;
	i32 length;
	i32 capacity;
};

struct file {
	struct line* lines;
	i32 count;
	i32 capacity;

	i32 line;
	i32 column;

	i32 origin_line;
	i32 origin_column;

	i32 desired;

	i16 screen_line;
	i16 screen_column;
};

__attribute__((always_inline)) static inline i8 is_unicode(i8 c) {
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

static inline void insert(struct file* file, i8 c) {
	struct line* this = file->lines + file->line;
	if (c == 13) {
		i32 rest = this->length - file->column;
		this->length = file->column;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.line, this->line + file->column, (size_t) rest);
		this->line = realloc(this->line, (size_t) (this->length)); // debug
		file->lines = realloc(file->lines, sizeof(struct line) * (size_t)(file->count + 1)); // debug
		// if (file->count + 1 > file->capacity) 
		// file->lines = realloc(file->lines, sizeof(struct line) 
		// * (size_t)(file->capacity = 8 * (file->capacity + 1)));
		memmove(file->lines + file->line + 2,
			file->lines + file->line + 1, 
			sizeof(struct line) * (size_t)(file->count - (file->line + 1)));
		file->lines[file->line + 1] = new;
		file->count++;
	} else {
		this->line = realloc(this->line, (size_t) (this->length + 1)); // debug
		// if (this->length + 1 > this->capacity) this->line = realloc(this->line, 
		// (size_t)(this->capacity = 8 * (this->capacity + 1)));
		memmove(this->line + file->column + 1, 
			this->line + file->column,
			(size_t) (this->length - file->column));
		this->line[file->column] = c;
		this->length++;
	}
	if (is_unicode(c)) file->column++; else move_right(file);
}

static inline void delete(struct file* file) {
	struct line* this = file->lines + file->line;
	if (not file->column) {
		if (not file->line) return;
		move_left(file);
		struct line* new = file->lines + file->line;
		new->line = realloc(new->line, (size_t)(new->length + this->length)); // debug
		if (this->length) memcpy(new->line + new->length, this->line, (size_t) this->length);
		new->length += this->length;
		memmove(file->lines + file->line + 1, 
			file->lines + file->line + 2, 
			sizeof(struct line) * (size_t)(file->count - (file->line + 2)));
		file->count--;
		file->lines = realloc(file->lines, sizeof(struct line) * (size_t)(file->count)); // debug

	} else {
		i32 save = file->column;
		move_left(file);
		memmove(this->line + file->column, 
			this->line + save, 
			(size_t)(this->length - save));
		this->length -= save - file->column;
		this->line = realloc(this->line, (size_t) (this->length)); // debug
	}
}

static inline void adjust_window_size() {
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	window_rows = (i16) window.ws_row - 1;
	window_columns = (i16) window.ws_col - 1;
}

static inline void display(struct file* file) {

	char* screen = malloc(sizeof(char) * (size_t) (window_rows * window_columns * 4));

	i32 length = 9;
	memcpy(screen, "\033[?25l\033[H", 9);

	i32 line = file->origin_line, column = 0;
	i16 screen_line = 0, screen_column = 0;
	i16 cursor_screen_line = 0, cursor_screen_column = 0;

	while (screen_line < window_rows) {

		if (line < file->count) {

			while (screen_column < window_columns) {
	
				if (column >= file->lines[line].length) break;
				if (column >= wrap_width) goto next_line;
				if (column < file->origin_column) {
					column++;
					continue;
				}
				
				if (column == file->column and line == file->line) {
					cursor_screen_line = screen_line; 
					cursor_screen_column = screen_column;
				}

				i8 c = file->lines[line].line[column++];

				if (c == '\t') {
					do { 
						if (screen_column >= window_columns or 
						    screen_column >= wrap_width) break;
						screen[length++] = ' ';
						screen_column++;
					} while (screen_column % tab_width);
				} else {
					screen[length++] = (char) c;
					screen_column++;
				}
			}

			if (column == file->column and line == file->line) {
				cursor_screen_line = screen_line; 
				cursor_screen_column = screen_column;
			}

			while (column < file->lines[line].length) {
				if (column == wrap_width) goto next_line;
				column++;
				screen_column++;
			}
			line++;
			column = 0;
		}

		next_line:

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
	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", cursor_screen_line + 1, cursor_screen_column + 1);
	write(1, screen, (size_t) length);
	free(screen);
}

static inline struct file* create_empty_file() {
	struct file* file = calloc(1, sizeof(struct file));
	file->count = 1;
	file->lines = calloc((size_t) file->count, sizeof(struct line));
	return file;
}

int main() {
	struct file* file = create_empty_file();
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);	
	while (1) {
		adjust_window_size();
		display(file);
		i8 c = 0;
		read(0, &c, 1);
		if (c == 'q') break;
		else if (c == 'J') move_left(file);
		else if (c == ':') move_right(file);		
		else if (c == 127) delete(file);
		else insert(file, c);
	}
	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);
}

