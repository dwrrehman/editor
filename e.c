#include <iso646.h>    // Daniel Warren Riaz Rehman's minimalist terminal-based text editor.
#include <termios.h>   //
#include <stdio.h>     //     designed with reliability, minimalism, 
#include <stdlib.h>    //     simplicity, and ergonimics in mind.
#include <string.h>    //
#include <unistd.h>    //          written on 2101177.005105
#include <sys/ioctl.h> //         finished on 2101181.131817
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

struct line { char* data; int count, capacity; };

static int window_rows = 0;
static int window_columns = 0;
static char* screen = NULL;

static char message[4096] = {0};
static char filename[4096] = {0};

static int saved = 0;
static int mode = 0;
static struct line* lines = NULL;
static int count = 0;
static int capacity = 0;
static int scroll_counter = 0;
static int lcl = 0;
static int lcc = 0;
static int vcl = 0;
static int vcc = 0;
static int vol = 0;
static int voc = 0;
static int vsl = 0;
static int vsc = 0;
static int vdc = 0;

static int wrap_width = 200;
static int tab_width = 8;
static int scroll_speed = 4;
static int show_status = 1;



static inline char zero_width(char c) { 
	return (((unsigned char)c) >> 6) == 2; 
}

static inline char visual(char c) { 
	return (((unsigned char)c) >> 6) != 2; 
}


static inline int file_exists(const char* f) {
    return access(f, F_OK) != -1;
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

static inline void compute_vcc() {
	vcc = 0;
	for (int c = 0; c < lcc; c++) {
		char k = lines[lcl].data[c];
		if (vcc >= wrap_width) vcc = 0;
		if (k == '\t') { 
			do {
				if (vcc >= wrap_width) vcc = 0;
				vcc++; 
			} while (vcc % tab_width); 
		} else if (visual(k)) vcc++;
	}
}

static inline void move_left(int change_desired) {
	if (not lcc) {
		if (not lcl) return;
		lcl--; 
		lcc = lines[lcl].count;
visual_line_up: compute_vcc();
visual_just_line_up: vcl--;
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
		int save_lcc = lcc;
		do lcc--; while (lcc and zero_width(lines[lcl].data[lcc]));
		if (lines[lcl].data[lcc] == '\t') {
			compute_vcc();
			int old_vcc = vcc;
			for (int c = lcc; c < save_lcc; c++) {
				char k = lines[lcl].data[c];
				if (old_vcc >= wrap_width) goto visual_just_line_up;
				if (k == '\t') {
					do {
						if (old_vcc >= wrap_width) goto visual_just_line_up; 
						old_vcc++; 
					} while (old_vcc % tab_width); 
				} else if (visual(k)) old_vcc++;
			}
			int diff = old_vcc - vcc;
			if (vsc >= diff) vsc -= diff;
			else if (voc >= diff - vsc) { voc -= diff - vsc; vsc = 0; }

		} else {
			if (not vcc) goto visual_line_up;
			vcc--; 
			if (vsc) vsc--; 
			else if (voc) voc--;
		}
	}
	if (change_desired) vdc = vcc;
}

static inline void move_right(int change_desired) {
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; lcc = 0; 
		vcl++;  vcc = 0; voc = 0; vsc = 0;
		if (vsl < window_rows - 1 - show_status) vsl++; 
		else vol++;

	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do {
				if (vcc >= wrap_width) {
					vcl++; vcc = 0; voc = 0; vsc = 0;
					if (vsl < window_rows - 1 - show_status) vsl++; 
					else vol++;
				}
				vcc++;
				if (vsc < window_columns - 1) vsc++;
				else voc++;

			} while (vcc % tab_width); 
		} else {
			if (vcc >= wrap_width) {
				vcl++; vcc = 0; voc = 0; vsc = 0;
				if (vsl < window_rows - 1 - show_status) vsl++; 
				else vol++;
			}
			vcc++; 
			if (vsc < window_columns - 1) vsc++; 
			else voc++;
		}
		do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
	}	
	if (change_desired) vdc = vcc;
}

static inline void move_up() {
	if (not vcl) {
		lcl = 0; lcc = 0;
		vcl = 0; vcc = 0;
		vol = 0; voc = 0;
		vsl = 0; vsc = 0;
		return;
	}
	int line_target = vcl - 1;
	while (vcc and vcl > line_target) move_left(0); 
	do move_left(0); while (vcc > vdc and vcl == line_target);
	if (vcc > window_columns - 1) { vsc = window_columns - 1; voc = vcc - vsc; } 
	else { vsc = vcc; voc = 0; }
}

static inline void move_down() {
	int line_target = vcl + 1;
	while (vcl < line_target) { 
		if (lcl == count - 1 and lcc == lines[lcl].count) return;
		move_right(0);
	} 
	while (vcc < vdc and lcc < lines[lcl].count) move_right(0);
	if (vcc != vdc and lcc and lines[lcl].data[lcc - 1] != '\t') move_left(0);
}

static inline void move_begin() {
	while (vcc) move_left(1);
}

static inline void move_end() {
	while (lcc < lines[lcl].count and vcc < wrap_width) move_right(1); 
}

static inline void move_top() {
	lcl = 0; lcc = 0;
	vcl = 0; vcc = 0;
	vol = 0; voc = 0;
	vsl = 0; vsc = 0;
	vdc = 0;
}

static inline void move_bottom() {
	while (lcl < count - 1 or lcc < lines[lcl].count) move_down(); 
	vdc = vcc;
}

static inline void move_word_left() {
	do move_left(1);
	while (not(
		(not lcl and not lcc) or (
			(lcc < lines[lcl].count and isalnum(lines[lcl].data[lcc]))  and 
			(not lcc or not isalnum(lines[lcl].data[lcc - 1]))
		)
	));
}

static inline void move_word_right() {
	do move_right(1);
	while (not(
		(lcl >= count - 1 and lcc >= lines[lcl].count) or (
			(lcc >= lines[lcl].count or not isalnum(lines[lcl].data[lcc]))  and 
			(lcc and isalnum(lines[lcl].data[lcc - 1]))
		)
	));
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
	else move_right(1);
	saved = 0;
}

static inline void delete() {
	struct line* this = lines + lcl;
	if (not lcc) {
		if (not lcl) return;
		move_left(1);
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
		move_left(1);
		memmove(this->data + lcc, this->data + save, (size_t)(this->count - save));
		this->count -= save - lcc;
		this->data = realloc(this->data, (size_t) (this->count)); // debug
	}
	saved = 0;
}

static inline void adjust_window_size() {
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col;
		screen = realloc(screen, sizeof(char) * (size_t) (window_rows * window_columns * 4));
	}
}

static inline void get_datetime(char buffer[16]) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm* tm_info = localtime(&tv.tv_sec);
	strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline void get_full_datetime(char buffer[32]) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm* time = localtime(&tv.tv_sec);
	strftime(buffer, 31, "CE%Y%m%d%u.%H%M%S", time);
}

static inline void display() {
	
	int length = 9; memcpy(screen, "\033[?25l\033[H", 9);

	int line = 0, col = 0;
	int vl = 0, vc = 0;
	int sl = 0, sc = 0;

	do {
		if (line >= count) goto next_visual_line;
		do {
			if (col >= lines[line].count) goto next_logical_line;
			if (vc >= wrap_width) goto next_visual_line;

			char k = lines[line].data[col];
			if (k == '\t') {
				do { 
					if (vc >= wrap_width) goto next_visual_line;
					if (vc >= voc and vc < voc + window_columns
					and vl >= vol and vl < vol + window_rows - show_status) {
						screen[length++] = ' ';
						sc++;
					}
					vc++;
				} while (vc % tab_width);
			} else {
				if (vc >= voc and vc < voc + window_columns
				and vl >= vol and vl < vol + window_rows - show_status and
				(sc or visual(k))) {
					screen[length++] = k;
					if (visual(k)) { sc++; }
				}
				if (visual(k)) vc++; 
			}
			col++;

		} while (sc < window_columns or col < lines[line].count);

	next_logical_line:
		line++; col = 0;

	next_visual_line:
		if (vl >= vol and vl < vol + window_rows - show_status) {
			screen[length++] = '\033';
			screen[length++] = '[';	
			screen[length++] = 'K';
			if (sl < window_rows - 1) {
				screen[length++] = '\r';
				screen[length++] = '\n';
			}
			sl++; sc = 0;
		}

		vl++; vc = 0; 

	} while (sl < window_rows - show_status);

	if (show_status) {
		length += sprintf(screen + length, "\033[7m\033[38;5;246m");

		char datetime[16] = {0};
		get_datetime(datetime);

		int status_length = sprintf(screen + length, " %s %d %d %d %s %c  %s",
			datetime,
			mode,
			lcl + 1, lcc + 1,
			filename,
			saved ? 's' : 'e',
			message
		);

		length += status_length;

		for (int i = status_length; i < window_columns; i++)
			screen[length++] = ' ';

		screen[length++] = '\033';
		screen[length++] = '[';
		screen[length++] = 'm';
	}
    
	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", vsl + 1, vsc + 1);
	write(1, screen, (size_t) length);
}

static void interpret_escape_code() {
    char c = 0;
    read(0, &c, 1);
    if (c == '[') {
        read(0, &c, 1);
        if (c == 'A') move_up();
        else if (c == 'B') move_down();
        else if (c == 'C') move_right(1);
        else if (c == 'D') move_left(1);
        else if (c == 32) { read(0, &c, 1); read(0, &c, 1); }
        else if (c == 77) {
            read(0, &c, 1);
            if (c == 97) {
                read(0, &c, 1); read(0, &c, 1);
                scroll_counter++;
                if (scroll_counter == scroll_speed) {
                    move_down();
                    scroll_counter = 0;
                }
            } else if (c == 96) {
                read(0, &c, 1); read(0, &c, 1);
                
                scroll_counter++;
                if (scroll_counter == scroll_speed) {
                    move_up();
                    scroll_counter = 0;
                }
            }
        }
    } else if (c == 27) mode = 0;
}

static inline void open_file(const char* given_filename) {
	if (not strlen(given_filename)) return;
	
	FILE* file = fopen(given_filename, "r");
	if (not file) {
		perror("fopen");
		exit(1);
	}

	strcpy(filename, given_filename);
	char* line = NULL;
	size_t line_capacity = 0;
	ssize_t line_length;
	
	saved = 1;
	mode = 0;

	while ((line_length = getline(&line, &line_capacity, file)) >= 0) {
		if (line_length and line[line_length - 1] == '\n') line_length--;
		char* line_copy = malloc((size_t) line_length);
		memcpy(line_copy, line, (size_t) line_length);
		lines = realloc(lines, sizeof(struct line) * (size_t)(count + 1)); // debug
		lines[count++] = (struct line) {line_copy, (int) line_length, (int) line_length};
	}

	if (not count) lines = calloc((size_t) (count = 1), sizeof(struct line));

	fclose(file);
	free(line);
}

static inline void save() {
	sprintf(message, "error: save unimplemented.");
	saved = 1;
}

int main(const int argc, const char** argv) {

	if (argc == 1) lines = calloc((size_t) (count = 1), sizeof(struct line));
	else open_file(argv[1]);

	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);	

loop:	adjust_window_size();
	display();
	char c = 0;
	read(0, &c, 1);

	if (not mode) {
		if (c == 'q') { if (saved) goto done; }
		if (c == 'Q') { goto done; }
		else if (c == 'f') mode = 1;
		else if (c == 27) interpret_escape_code();
		else if (c == 'W') { wrap_width++; }
		else if (c == 'E') { if (wrap_width > tab_width) wrap_width--; }

		else if (c == 'T') { tab_width++; }
		else if (c == 'R') { if (tab_width > 1) tab_width--; }

		else if (c == 's') show_status = !show_status;
		// else if (c == 'd') show_line_numbers = !show_line_numbers;

		else if (c == 'j') move_left(1);
		else if (c == ';') move_right(1);
		else if (c == 'o') move_up();
		else if (c == 'i') move_down();
		
		else if (c == 'J') move_word_left();
		else if (c == ':') move_word_right();

		else if (c == 'k') move_begin();
		else if (c == 'l') move_end();
		else if (c == 'O') move_top();
		else if (c == 'I') move_bottom();

		else if (c == 'w') save();

	} else if (mode == 1) {
		if (c == 27) interpret_escape_code();
		else if (c == 127) delete();
		else insert(c);
	}
	goto loop;
done:
	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);
}




