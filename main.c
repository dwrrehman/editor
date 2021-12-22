//  Daniel Warren Riaz Rehman's minimalist 
//        terminal-based text editor 
//
//     Designed with reliability, minimalism, 
//     simplicity, and ergonimics in mind.
//
//          written on 2101177.005105
//           edited on 2111114.172631
//           edited on 2112116.194022
//
//        tentatively named:   "t".
//
#include <iso646.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <math.h>  
#include <time.h>  
#include <sys/time.h> 
#include <errno.h>
#include <stdbool.h>

#define fuzz 1
#define use_main 0

typedef ssize_t nat;

enum action_type {
	no_action,
	insert_action,
	delete_action,
	paste_text_action,
	cut_text_action,
};

struct line { 
	char* data; 
	nat count, capacity; 
};

struct buffer {
	struct line* lines;
	struct action* actions;

	nat 
		saved, mode, 
		count, capacity, 

		line_number_width, needs_display_update,

		lcl, lcc, 	vcl, vcc,  	vol, voc, 	
		vsl, vsc, 	vdc,    	lal, lac,

		wrap_width, tab_width, 
		scroll_speed, show_status, show_line_numbers, 
		use_txt_extension_when_absent,
		line_number_color, status_bar_color, alert_prompt_color, 
		info_prompt_color, default_prompt_color,

		action_count, head
	;
	char message[4096];
	char filename[4096];
};

struct logical_state {
	nat 
		saved, line_number_width,

		lcl, lcc, 	vcl, vcc,  	vol, voc, 
		vsl, vsc, 	vdc,    	lal,  lac
	;
};

struct textbox {
	char* data;
	nat
		count, capacity, prompt_length, 
		c, vc, vs, vo
	;
};

struct action { 
	nat* children; 	// count: count    (array of indexes into action list)
	char* text; 	// count: length
	nat parent; 	// (index into action list)
	nat type;
	nat choice;
	nat count;
	nat length;
	struct logical_state pre;
	struct logical_state post;
};

// application global data:
static nat 
	window_rows = 0, 
	window_columns = 0;
static char* screen = NULL;
static struct textbox tb = {0};

// file buffer data:
static struct buffer* buffers = NULL;
static nat 
	buffer_count = 0,
	active_index = 0;

// active buffer's registers:
static struct buffer buffer = {0};
static struct line* lines = NULL;
static struct action* actions = NULL;
static nat 
	count = 0, capacity = 0, 
	line_number_width = 0, tab_width = 0, wrap_width = 0, 
	show_status = 0, show_line_numbers = 0,
	lcl = 0, lcc = 0, vcl = 0, vcc = 0, vol = 0,          
	voc = 0, vsl = 0, vsc = 0, vdc = 0, lal = 0, lac = 0,
	head, action_count;

static char message[4096] = {0};
static char filename[4096] = {0};

static inline bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2;  }
static inline bool visual(char c) { return not zero_width(c); }
static inline bool file_exists(const char* f) { return access(f, F_OK) != -1; }

static inline void get_datetime(char datetime[16]) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm* tm_info = localtime(&tv.tv_sec);
	strftime(datetime, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline struct termios configure_terminal() {
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

static inline void compute_vcc() {
	vcc = 0;
	for (nat c = 0; c < lcc; c++) {
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

static inline void move_left(bool change_desired) {
	if (not lcc) {
		if (not lcl) return;
		lcl--; 
		lcc = lines[lcl].count;
visual_line_up: compute_vcc();
visual_just_line_up: vcl--;
		if (vsl) vsl--;
		else if (vol) vol--;
		if (vcc > window_columns - 1 - line_number_width) { 
			vsc = window_columns - 1 - line_number_width; 
			voc = vcc - vsc; 
		} else { 
			vsc = vcc;
			voc = 0; 
		}
	} else {
		nat save_lcc = lcc;
		do lcc--; while (lcc and zero_width(
			lines[lcl].
			data[lcc]));
		if (lines[lcl].data[lcc] == '\t') {
			compute_vcc();
			nat old_vcc = vcc;
			for (nat c = lcc; c < save_lcc; c++) {
				char k = lines[lcl].data[c];
				if (old_vcc >= wrap_width) goto visual_just_line_up;
				if (k == '\t') {
					do {
						if (old_vcc >= wrap_width) goto visual_just_line_up; 
						old_vcc++; 
					} while (old_vcc % tab_width); 
				} else if (visual(k)) old_vcc++;
			}
			nat diff = old_vcc - vcc;
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

static inline void move_right(bool change_desired) {
	if (lcl >= count) return;
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; lcc = 0; 
		vcl++; vcc = 0; voc = 0; vsc = 0;
		if (vsl < window_rows - 1 - show_status) vsl++; 
		else vol++;
	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do {
				if (wrap_width <= tab_width) break; 
				
				if (vcc >= wrap_width) {
					vcl++; vcc = 0; voc = 0; vsc = 0;
					if (vsl < window_rows - 1 - show_status) vsl++; 
					else vol++;
				}
				vcc++;
				if (vsc < window_columns - 1 - line_number_width) vsc++;
				else voc++;

			} while (vcc % tab_width); 
		} else {
			if (vcc >= wrap_width) {
				vcl++; vcc = 0; voc = 0; vsc = 0;
				if (vsl < window_rows - 1 - show_status) vsl++; 
				else vol++;
			}
			vcc++; 
			if (vsc < window_columns - 1 - line_number_width) vsc++; 
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
	nat line_target = vcl - 1;
	while (vcc and vcl > line_target) move_left(0); 
	do move_left(0); while (vcc > vdc and vcl == line_target);
	if (vcc > window_columns - 1 - line_number_width) { vsc = window_columns - 1 - line_number_width; voc = vcc - vsc; } 
	else { vsc = vcc; voc = 0; }
}

static inline void move_down() {
	nat line_target = vcl + 1;
	while (vcl < line_target) { 
		if (lcl == count - 1 and lcc == lines[lcl].count) return;
		move_right(0);
	}
	while (vcc < vdc and lcc < lines[lcl].count) {
		if (lines[lcl].data[lcc] == '\t' and vcc + (tab_width - (vcc % tab_width)) > vdc) return;
		move_right(0);
	}
}

static inline void jump_line(nat line) {
	while (lcl < line and lcl < count) move_down();
	while (lcl > line and lcl) move_up();
}

static inline void jump_column(nat column) {
	while (lcc < column and lcc < lines[lcl].count) move_right(1);
	while (lcc > column and lcc) move_left(1);
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


static inline void record_logical_state(struct logical_state* pcond_out) { // get current state, fill into given pre/post-condtion.
	struct logical_state* p = pcond_out; // respelling.

	p->saved = buffer.saved;
	p->line_number_width = line_number_width;

	p->lcl = lcl;  p->lcc = lcc; 	
	p->vcl = vcl;  p->vcc = vcc;
  	p->vol = vol;  p->voc = voc;
	p->vsl = vsl;  p->vsc = vsc; 
	p->vdc = vdc;  p->lal = lal;
	p->lac = lac;
}

static inline void require_logical_state(struct logical_state* pcond_in) {   // set current state, based on a pre/post-condition.
	struct logical_state* p = pcond_in; // respelling.

	buffer.saved = p->saved;
	line_number_width = p->line_number_width;

	lcl = p->lcl;  lcc = p->lcc; 	
	vcl = p->vcl;  vcc = p->vcc;
  	vol = p->vol;  voc = p->voc;
	vsl = p->vsl;  vsc = p->vsc; 
	vdc = p->vdc;  lal = p->lal;
	lac = p->lac;
}

static inline void create_action(struct action new) {
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(nat) * (size_t) (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	head = action_count;
	actions[action_count++] = new;
}

static inline void insert(char c, bool should_record) { 

	if (should_record and zero_width(c) and (actions[head].type != insert_action or actions[head].text[0] == '\n')) return; 

	struct action new_action = {0};
	if (should_record and visual(c)) record_logical_state(&new_action.pre);

	struct line* this = lines + lcl;
	if (c == '\n') {
		nat rest = this->count - lcc;
		this->count = lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, this->data + lcc, (size_t) rest);

		// if (count + 1 > capacity) 
		// 	lines = realloc(lines, sizeof(struct line) * (size_t)(capacity = 8 * (capacity + 1)));
		if (not fuzz) abort();
		lines = realloc(lines, sizeof(struct line) * (size_t)(count + 1));

		memmove(lines + lcl + 2, lines + lcl + 1, sizeof(struct line) * (size_t)(count - (lcl + 1)));
		lines[lcl + 1] = new;
		count++;

	} else {
		// if (this->count + 1 > this->capacity) 
		// 	this->data = realloc(this->data, (size_t)(this->capacity = 8 * (this->capacity + 1)));
		if (not fuzz) abort();
		this->data = realloc(this->data, (size_t)(this->count + 1));

		memmove(this->data + lcc + 1, this->data + lcc, (size_t) (this->count - lcc));
		this->data[lcc] = c;
		this->count++;
	}
	
	if (zero_width(c)) lcc++; 
	else move_right(1);

	buffer.saved = false;

	if (not should_record) return;
	if (zero_width(c)) {
		actions[head].text = realloc(actions[head].text, (size_t) actions[head].length + 1);
		actions[head].text[actions[head].length++] = c;
		record_logical_state(&actions[head].post);
		return;
	}
	
	record_logical_state(&new_action.post);
	new_action.type = insert_action;
	new_action.text = malloc(1);
	new_action.text[0] = c;
	new_action.length = 1;
	create_action(new_action);
}

static inline void delete(bool should_record) {

	struct action new_action = {0};
	if (should_record) record_logical_state(&new_action.pre);

	char* deleted_string = NULL;
	nat deleted_length = 0;
	struct line* this = lines + lcl;

	if (not lcc) {
		if (not lcl) return;
		move_left(1);
		struct line* new = lines + lcl;

		// if (new->count + this->count > new->capacity)
		// 	new->data = realloc(new->data, (size_t)(new->capacity = 8 * (new->capacity + this->count)));
		if (not fuzz) abort();
		new->data = realloc(new->data, (size_t)(new->count + this->count));

		if (this->count) memcpy(new->data + new->count, this->data, (size_t) this->count);

		free(this->data);

		new->count += this->count;
		memmove(lines + lcl + 1, lines + lcl + 2, 
			sizeof(struct line) * (size_t)(count - (lcl + 2)));
		count--;

		if (not fuzz) abort();
		lines = realloc(lines, sizeof(struct line) * (size_t)count);

		if (should_record) {
			deleted_length = 1;
			deleted_string = malloc(1);
			deleted_string[0] = '\n';
		}

	} else {
		nat save = lcc;
		move_left(1);
		
		if (should_record) {
			deleted_length = save - lcc;
			deleted_string = malloc((size_t) deleted_length);
			memcpy(deleted_string, this->data + lcc, (size_t) deleted_length);
		}

		memmove(this->data + lcc, this->data + save, (size_t)(this->count - save));
		this->count -= save - lcc;

		if (not fuzz) abort();
		this->data = realloc(this->data, (size_t)(this->count));
	}

	buffer.saved = false;
	
	if (not should_record) return;

	record_logical_state(&new_action.post);
	new_action.type = delete_action;
	new_action.text = deleted_string;
	new_action.length = deleted_length;
	create_action(new_action);
}

static inline void adjust_window_size() {
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);

	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 20; window.ws_col = 40; }

	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col;
		screen = realloc(screen, (size_t) (window_rows * window_columns * 4));
	}

	if (not wrap_width) wrap_width = window_columns - 1 - line_number_width;
}

static inline void display() {
	nat length = 9; 
	memcpy(screen, "\033[?25l\033[H", 9);

	nat line = 0, col = 0;
	nat vl = 0, vc = 0;
	nat sl = 0, sc = 0;

	double f = floor(log10((double) count)) + 1;
	int line_number_digits = (int)f;
	line_number_width = show_line_numbers * (line_number_digits + 2);

	do {
		if (line >= count) goto next_visual_line;

		if (show_line_numbers and vl >= vol and vl < vol + window_rows - show_status) {
			if (not col) length += sprintf(screen + length, "\033[38;5;%ldm%*ld\033[0m  ", buffer.line_number_color, line_number_digits, line);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}
            
		do {
			if (col >= lines[line].count) goto next_logical_line;
			if (vc >= wrap_width) goto next_visual_line;

			char k = lines[line].data[col];
			if (k == '\t') {
				do { 
					if (vc >= wrap_width) goto next_visual_line;
					if (vc >= voc and vc < voc + window_columns - line_number_width
					and vl >= vol and vl < vol + window_rows - show_status) {
						screen[length++] = ' ';
						sc++;
					}
					vc++;
				} while (vc % tab_width);
			} else {
				if (vc >= voc and vc < voc + window_columns - line_number_width
				and vl >= vol and vl < vol + window_rows - show_status and
				(sc or visual(k))) {
					screen[length++] = k;
					if (visual(k)) { sc++; }
				}
				if (visual(k)) vc++; 
			}
			col++;

		} while (sc < window_columns - line_number_width or col < lines[line].count);

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

		char datetime[16] = {0};
		get_datetime(datetime);
		nat status_length = sprintf(screen + length, " %s %ld %ld %ld %ld %ld %s %c  %s",
			datetime, 
			buffer.mode, 
			active_index, buffer_count,
			lcl, lcc, 
			filename, 
			buffer.saved ? 's' : 'e', 
			message
		);
		length += status_length;

		for (nat i = status_length; i < window_columns; i++)
			screen[length++] = ' ';

		screen[length++] = '\033';
		screen[length++] = '[';
		screen[length++] = 'm';
	}
    
	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", vsl + 1, vsc + 1 + line_number_width);

	if (not fuzz) 
		write(1, screen, (size_t) length);
}


static inline void textbox_move_left() {
	if (not tb.c) return;
	do tb.c--; while (tb.c and zero_width(tb.data[tb.c]));
	tb.vc--; 
	if (tb.vs) tb.vs--; else if (tb.vo) tb.vo--;
}

static inline void textbox_move_right() {
	if (tb.c >= tb.count) return;
	tb.vc++; 
	if (tb.vs < window_columns - 1 - tb.prompt_length) tb.vs++; else tb.vo++;
	do tb.c++; while (tb.c < tb.count and zero_width(tb.data[tb.c]));
}

static inline void textbox_insert(char c) {
	if (tb.count + 1 > tb.capacity) 
		tb.data = realloc(tb.data, (size_t)(tb.capacity = 8 * (tb.capacity + 1)));
	///TODO: do a realloc, so that we can detect memory bugs with the textbox.

	memmove(tb.data + tb.c + 1, tb.data + tb.c, (size_t) (tb.count - tb.c));
	tb.data[tb.c] = c;
	tb.count++;
	if (zero_width(c)) tb.c++; else textbox_move_right();
}

static inline void textbox_delete() {
	if (not tb.c) return;
	nat save = tb.c;
	textbox_move_left();
	memmove(tb.data + tb.c, tb.data + save, (size_t)(tb.count - save));
	tb.count -= save - tb.c;
}

static inline void textbox_display(const char* prompt, nat prompt_color) {
	nat length = sprintf(screen, "\033[?25l\033[%ld;1H\033[38;5;%ldm%s\033[m", window_rows, prompt_color, prompt);
	nat col = 0, vc = 0, sc = tb.prompt_length;
	while (sc < window_columns and col < tb.count) {
		char k = tb.data[col];
		if (vc >= tb.vo and vc < tb.vo + window_columns - 1 - tb.prompt_length and (sc or visual(k))) {
			screen[length++] = k;
			if (visual(k)) { sc++; }
		}
		if (visual(k)) vc++; 
		col++;
	} 

	for (nat i = sc; i < window_columns; i++) 
		screen[length++] = ' ';

	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", window_rows, tb.vs + 1 + tb.prompt_length);

	if (not fuzz) 
		write(1, screen, (size_t) length);
}

static inline void print_above_textbox(char* write_message, nat color) {
	nat length = sprintf(screen, "\033[%ld;1H\033[K\033[38;5;%ldm%s\033[m", window_rows - 1, color, write_message);
	if (not fuzz) write(1, screen, (size_t) length);
}

static inline void prompt(const char* prompt_message, nat color, char* out, nat out_size) {
	if (fuzz) return;     ///TODO: make this code tested by the fuzzer by supplying the input to its read calls. somehow.

	tb.prompt_length = (nat) strlen(prompt_message);
	do {
		adjust_window_size();
		textbox_display(prompt_message, color);
		char c = 0;
		read(0, &c, 1);
		if (c == '\r' or c == '\n') break;
		else if (c == '\t') { /*tab complete*/ }
		else if (c == 27) {
			read(0, &c, 1);
			if (c == '[') {
				read(0, &c, 1);
				if (c == 'A') {}
				else if (c == 'B') {}
				else if (c == 'C') textbox_move_right();
				else if (c == 'D') textbox_move_left();
			} else if (c == 27) { tb.count = 0; break; }
		} else if (c == 127) textbox_delete();
		else textbox_insert(c);
	} while (1);
	if (tb.count > out_size) tb.count = out_size;
	memcpy(out, tb.data, (size_t) tb.count);
	memset(out + tb.count, 0, (size_t) out_size - (size_t) tb.count);
	out[out_size - 1] = 0;
	free(tb.data);
	tb = (struct textbox){0};
}

static inline bool confirmed(const char* question) {
	if (fuzz) return true;

	char prompt_message[4096] = {0};
	sprintf(prompt_message, "%s? (yes/no): ", question);
	while (1) {
		char response[10] = {0};
		prompt(prompt_message, buffer.alert_prompt_color, response, sizeof response);
		
		if (not strncmp(response, "yes", 4)) return true;
		else if (not strncmp(response, "no", 3)) return false;
		else if (not strncmp(response, "", 1)) return false;
		else print_above_textbox("please type \"yes\" or \"no\".", buffer.default_prompt_color);
	}
}

static inline void store_current_data_to_buffer() {
	if (not buffer_count) return;

	const nat b = active_index;
	
	buffers[b].saved = buffer.saved;
	buffers[b].mode = buffer.mode;

	buffers[b].wrap_width = wrap_width;
	buffers[b].tab_width = tab_width;
	buffers[b].line_number_width = line_number_width;
	buffers[b].needs_display_update = buffer.needs_display_update;
	buffers[b].show_status = show_status;
	buffers[b].show_line_numbers = show_line_numbers;

	buffers[b].capacity = capacity;
	buffers[b].count = count;
	buffers[b].lines = lines;

	buffers[b].lcl = lcl;  buffers[b].lcc = lcc; 
	buffers[b].vcl = vcl;  buffers[b].vcc = vcc; 
	buffers[b].vol = vol;  buffers[b].voc = voc; 
	buffers[b].vsl = vsl;  buffers[b].vsc = vsc; 
	buffers[b].vdc = vdc;  buffers[b].lal = lal;
	buffers[b].lac = lac;

	buffers[b].alert_prompt_color = buffer.alert_prompt_color;
	buffers[b].info_prompt_color = buffer.info_prompt_color;
	buffers[b].default_prompt_color = buffer.default_prompt_color;
	buffers[b].line_number_color = buffer.line_number_color;
	buffers[b].status_bar_color = buffer.status_bar_color;

	buffers[b].scroll_speed = buffer.scroll_speed;
	buffers[b].use_txt_extension_when_absent = buffer.use_txt_extension_when_absent;

	buffers[b].head = head;
	buffers[b].action_count = action_count;
	buffers[b].actions = actions;

	memcpy(buffers[b].message, message, sizeof message);
	memcpy(buffers[b].filename, filename, sizeof filename);
}

static inline void load_buffer_data_into_registers() {
	if (not buffer_count) return;

	struct buffer this = buffers[active_index];
	
	buffer.saved = this.saved;
	buffer.mode = this.mode;

	wrap_width = this.wrap_width;
	tab_width = this.tab_width;
	line_number_width = this.line_number_width;
	buffer.needs_display_update = this.needs_display_update;

	capacity = this.capacity;
	count = this.count;
	lines = this.lines;

	lcl = this.lcl;  lcc = this.lcc;
	vcl = this.vcl;  vcc = this.vcc;
	vol = this.vol;  voc = this.voc; 
	vsl = this.vsl;  vsc = this.vsc; 
	vdc = this.vdc;  lal = this.lal;
	lac = this.lac;

	buffer.alert_prompt_color = this.alert_prompt_color;
	buffer.info_prompt_color = this.info_prompt_color;
	buffer.default_prompt_color = this.default_prompt_color;
	buffer.line_number_color = this.line_number_color;
	buffer.status_bar_color = this.status_bar_color;

	head = this.head;
	action_count = this.action_count;
	actions = this.actions;
	
	show_status = this.show_status;
	this.scroll_speed = this.scroll_speed;
	show_line_numbers = this.show_line_numbers;
	buffer.use_txt_extension_when_absent = this.use_txt_extension_when_absent;

	memcpy(message, this.message, sizeof message);
	memcpy(filename, this.filename, sizeof filename);
}

static inline void zero_registers() {

	wrap_width = 0;
	tab_width = 0;
	line_number_width = 0;

	show_status = 0;
	show_line_numbers = 0;

	capacity = 0;
	count = 0;
	lines = NULL;

	lcl = 0; lcc = 0; vcl = 0; vcc = 0; vol = 0; 
	voc = 0; vsl = 0; vsc = 0; vdc = 0; lal = 0; lac = 0;

	head = 0;
	action_count = 0;
	actions = NULL;

	memset(message, 0, sizeof message);
	memset(filename, 0, sizeof filename);

	buffer = (struct buffer){0};

	buffers = NULL;
	buffer_count = 0;
	active_index = 0;	
}

static inline void initialize_registers() {
	
	buffer.saved = true;
	buffer.mode = 0;

	wrap_width = 0; // init using file
	tab_width = 8; // init using file
	line_number_width = 0;

	show_status = 0; // init using file
	show_line_numbers = 0; // init using file
	buffer.needs_display_update = 1;

	capacity = 1;
	count = 1;
	lines = calloc(1, sizeof(struct line));

	lcl = 0; lcc = 0; vcl = 0; vcc = 0; vol = 0; 
	voc = 0; vsl = 0; vsc = 0; vdc = 0; lal = 0; lac = 0;

	//TODO: make these initial default_values read from a config file or something.. 
	buffer.alert_prompt_color = 196; // init using file
	buffer.info_prompt_color = 45; // init using file
	buffer.default_prompt_color = 214; // init using file
	buffer.line_number_color = 236; // init using file
	buffer.status_bar_color = 245; // init using file
	
	buffer.scroll_speed = 4; // init using file
	buffer.use_txt_extension_when_absent = 1; // init using file

	head = 0;
	action_count = 1;
	actions = calloc(1, sizeof(struct action));

	memset(message, 0, sizeof message);
	memset(filename, 0, sizeof filename);
}

static inline void create_empty_buffer() {
	store_current_data_to_buffer();
	buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count + 1));
	buffers[buffer_count] = (struct buffer) {0};
	initialize_registers();
	active_index = buffer_count;
	buffer_count++;
	store_current_data_to_buffer();
}

static inline void close_active_buffer() {
	store_current_data_to_buffer();
	for (nat line = 0; line < buffers[active_index].count; line++) 
		free(buffers[active_index].lines[line].data);
	free(buffers[active_index].lines);

	for (nat a = 0; a < buffers[active_index].action_count; a++) {
		free(buffers[active_index].actions[a].text);
		free(buffers[active_index].actions[a].children);
	}
	free(buffers[active_index].actions);

	buffer_count--;
	memmove(buffers + active_index, buffers + active_index + 1, 
		sizeof(struct buffer) * (size_t)(buffer_count - active_index));
	if (active_index >= buffer_count) active_index = buffer_count - 1;
	buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count));
	load_buffer_data_into_registers();
}

static inline void move_to_next_buffer() {
	store_current_data_to_buffer(); 
	if (active_index) active_index--; 
	load_buffer_data_into_registers();
}

static inline void move_to_previous_buffer() {
	store_current_data_to_buffer(); 
	if (active_index < buffer_count - 1) active_index++; 
	load_buffer_data_into_registers();
}

static inline void open_file(const char* given_filename) {
	if (fuzz) return;
	if (not strlen(given_filename)) return;
	
	FILE* file = fopen(given_filename, "r");
	if (not file) {
		if (buffer_count) {
			sprintf(message, "error: fopen: %s", strerror(errno));
			return;
		}
		perror("fopen"); exit(1);
	}

	fseek(file, 0, SEEK_END);        
        size_t length = (size_t) ftell(file);
	char* text = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(text, sizeof(char), length, file);

	create_empty_buffer();
	adjust_window_size();
	for (size_t i = 0; i < length; i++) 
		insert(text[i], 0);

	free(text); 
	fclose(file);
	buffer.saved = true; 
	buffer.mode = 1; 
	move_top();
	strcpy(filename, given_filename);
	store_current_data_to_buffer();

	sprintf(message, "read %lub", length);
}

static inline void save() {
	if (fuzz) return;

	if (not strlen(filename)) {
	prompt_filename:
		prompt("save as: ", buffer.default_prompt_color, filename, sizeof filename);
		if (not strlen(filename)) { sprintf(message, "aborted save"); return; }
		if (not strrchr(filename, '.') and buffer.use_txt_extension_when_absent) strcat(filename, ".txt");
		if (file_exists(filename) and not confirmed("file already exists, overwrite")) {
			strcpy(filename, ""); goto prompt_filename;
		}
	}

	FILE* file = fopen(filename, "w+");
	if (not file) {
		sprintf(message, "error: %s", strerror(errno));
		strcpy(filename, "");
		return;
	}
	
	unsigned long long bytes = 0;
	for (nat i = 0; i < count; i++) {
		bytes += fwrite(lines[i].data, sizeof(char), (size_t) lines[i].count, file);
		if (i < count - 1) {
			fputc('\n', file);
			bytes++;
		}
	}

	if (ferror(file)) {
		sprintf(message, "error: %s", strerror(errno));
		strcpy(filename, "");
		fclose(file);
		return;
	}

	fclose(file);
	sprintf(message, "wrote %lldb;%ldl", bytes, count);
	buffer.saved = true;
}

static inline void rename_file() {
	if (fuzz) return;

	char new[4096] = {0};
	prompt_filename:
	prompt("rename to: ", buffer.default_prompt_color, new, sizeof new);
	if (not strlen(new)) { sprintf(message, "aborted rename"); return; }

	if (file_exists(new) and not confirmed("file already exists, overwrite")) {
		strcpy(new, ""); goto prompt_filename;
	}

	if (rename(filename, new)) sprintf(message, "error: %s", strerror(errno));
	else {
		strncpy(filename, new, sizeof new);
		sprintf(message, "renamed to \"%s\"", filename);
	}
}

static inline void interpret_escape_code() {
	if (fuzz) return;

	static nat scroll_counter = 0;
	char c = 0;
	read(0, &c, 1);      // TODO: make it so pressing escape once is sufficient. add mouse.
	if (c == 27) buffer.mode = 1;
	else if (c == '[') {
		read(0, &c, 1);
		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') move_right(1);
		else if (c == 'D') move_left(1);
		else if (c == 32) { 
			for (nat i = 0; i < 4; i++) {
				read(0, &c, 1);
			}
		} 
		else if (c == 77) {
			read(0, &c, 1);
			if (c == 97) {
				read(0, &c, 1); read(0, &c, 1);
				scroll_counter++;
				if (scroll_counter == buffer.scroll_speed) {
					move_down();
					scroll_counter = 0;
				} else buffer.needs_display_update = 0;
			} else if (c == 96) {
				read(0, &c, 1); read(0, &c, 1);
				scroll_counter++;
				if (scroll_counter == buffer.scroll_speed) {
					move_up();
					scroll_counter = 0;
				} else buffer.needs_display_update = 0;
			}
		}
	} 
}

static inline void prompt_open() {
	char new_filename[4096] = {0};
	prompt("open: ", buffer.default_prompt_color, new_filename, sizeof new_filename);
	if (not strlen(new_filename)) { sprintf(message, "aborted open"); return; }
	open_file(new_filename);
}

static inline void prompt_jump_line() {
	char string_number[128] = {0};
	prompt("line: ", buffer.default_prompt_color, string_number, sizeof string_number);
	nat line = atoi(string_number);
	if (line >= count) line = count - 1;
	jump_line(line);
	sprintf(message, "jumped to %ld %ld", lcl, lcc);
}

static inline void prompt_jump_column() {
	char string_number[128] = {0};
	prompt("column: ", buffer.default_prompt_color, string_number, sizeof string_number);
	nat column = atoi(string_number);
	if (column > lines[lcl].count) column = lines[lcl].count;
	jump_column(column);
	sprintf(message, "jumped to %ld %ld", lcl, lcc);
}

static inline bool is_exit_sequence(char c, char p) {
	return c == 'w' and p == 'r' or c == 'f' and p == 'u';
}

static char* get_sel(nat* out_length, nat first_line, nat first_column, nat last_line, nat last_column) {
	
	char* string = malloc(256);
	nat length = 0;
	nat s_capacity = 256;
	nat line = first_line, column = first_column;

	while (line < last_line) {

		// if (length + lines[line].count - column + 1 >= s_capacity) 
		// 	string = realloc(string, (size_t) (s_capacity = 2 * (s_capacity + length + lines[line].count - column + 1)));
		if (not fuzz) abort();
		string = realloc(string, (size_t) (length + lines[line].count - column));

		if (lines[line].count - column) 
			memcpy(string + length, lines[line].data + column, (size_t)(lines[line].count - column));

		length += lines[line].count - column;

		if (not fuzz) abort(); 
		string = realloc(string, (size_t) (length + 1));

		string[length++] = '\n';

		line++;
		column = 0;
	}

	// if (length + (last_column - column) >= s_capacity) 
	// 	string = realloc(string, (size_t) (s_capacity = 2 * (s_capacity + length + last_column - column)));
	if (not fuzz) abort();
	string = realloc(string, (size_t) (length + last_column - column));	

	if (last_column - column) memcpy(string + length, lines[line].data + column, (size_t)(last_column - column));
	length += last_column - column;
	*out_length = length;
	return string;
}

static inline bool anchor_is_invalid() {
	if (lal >= count) return true;
	if (lac > lines[lal].count) return true;
	return false;
}

static inline char* get_selection(nat* out) {
	if (anchor_is_invalid()) goto empty;
	if (lal < lcl) return get_sel(out, lal, lac, lcl, lcc);
	if (lcl < lal) return get_sel(out, lcl, lcc, lal, lac);
	if (lac < lcc) return get_sel(out, lal, lac, lcl, lcc);
	if (lcc < lac) return get_sel(out, lcl, lcc, lal, lac);
empty:	*out = 0;
	return NULL;
}

static inline void paste() {
	FILE* file = popen("pbpaste", "r");
	if (not file) { sprintf(message, "error: paste: popen(): %s", strerror(errno)); return; }

	struct action new = {0};
	record_logical_state(&new.pre);

	char* string = malloc(256);
	nat s_capacity = 256;
	nat length = 0;

	nat c = 0;
	while ((c = fgetc(file)) != EOF) {
		// if (length + 1 >= s_capacity) string = realloc(string, (size_t) (s_capacity = 2 * (s_capacity + length + 1)));
		if (not fuzz) abort(); 
		string = realloc(string, (size_t) (length + 1));

		string[length++] = (char) c;
		insert((char)c, 0);
	}

	pclose(file);
	sprintf(message, "pasted %ldb", length);

	record_logical_state(&new.post);
	new.type = paste_text_action;
	new.text = string;
	new.length = length;
	create_action(new);
}

static inline void cut_text() {
	if (lal < lcl) goto anchor_first;
	if (lcl < lal) goto cursor_first;
	if (lac < lcc) goto anchor_first;
	if (lcc < lac) goto cursor_first;
	return;
cursor_first:;
	nat line = lcl, column = lcc;
	while (lcl < lal or lcc < lac) move_right(0);
	lal = line; lac = column;
anchor_first:
	while (lal < lcl or lac < lcc) delete(0);
}

static inline void cut() { 
	if (anchor_is_invalid()) { sprintf(message, "?"); return; }

	struct action new = {0};
	record_logical_state(&new.pre);
	nat deleted_length = 0;
	char* deleted_string = get_selection(&deleted_length);
	cut_text();
	sprintf(message, "deleted %ldb", deleted_length);
	record_logical_state(&new.post);
	new.type = cut_text_action;
	new.text = deleted_string;
	new.length = deleted_length;
	create_action(new);
}

static inline void copy() {
	if (fuzz) return;
	if (anchor_is_invalid()) { sprintf(message, "?"); return; }

	FILE* file = popen("pbcopy", "w");
	if (not file) { sprintf(message, "error: copy: popen(): %s", strerror(errno)); return; }

	nat length = 0;
	char* selection = get_selection(&length);
	fwrite(selection, 1, (size_t)length, file);
	pclose(file);
	free(selection);
	sprintf(message, "copied %ldb", length);
}

static inline void replay_action(struct action a) {
	require_logical_state(&a.pre);
	if (a.type == no_action) return;
	else if (a.type == insert_action or a.type == paste_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_text_action) cut_text();
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {
	require_logical_state(&a.post);
	if (a.type == no_action) return;
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_text_action) { 
		while (lcc > a.pre.lcc or lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	}
	require_logical_state(&a.pre);
}

static inline void undo() {
	if (not head) return;
	reverse_action(actions[head]);
	sprintf(message, "undoing %ld ", actions[head].type);
	if (actions[head].count != 1) 
		sprintf(message + strlen(message), "%ld %ld", actions[head].choice, actions[head].count);
	head = actions[head].parent;
}

static inline void redo() {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	sprintf(message, "redoing %ld ", actions[head].type);
	if (actions[head].count != 1) 
		sprintf(message + strlen(message), "%ld %ld", actions[head].choice, actions[head].count);
	replay_action(actions[head]);
}

static inline void alternate_up() {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void alternate_down() {
	if (actions[head].choice) actions[head].choice--;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void execute(char c, char p) {
	if (buffer.mode == 0) {

		if (is_exit_sequence(c, p)) { undo(); buffer.mode = 1; }
		else if (c == '\r') insert('\n', 1);
		else if (c == 127) delete(1);
		else if (c == 27) interpret_escape_code();
		else insert(c, 1);

	} else if (buffer.mode == 1) {

		if (c == 't') buffer.mode = 0;
		else if (c == 'h') buffer.mode = 2;

		else if (c == 'n') move_left(1);
		else if (c == 'i') move_right(1);
		else if (c == 'p') move_up();
		else if (c == 'u') move_down();

		else if (c == 'e') move_word_left();
		else if (c == 'o') move_word_right();

		else if (c == 'N') move_begin();
		else if (c == 'I') move_end();

		else if (c == 'P') move_top();
		else if (c == 'U') move_bottom();

		else if (c == 'E') prompt_jump_column();
		else if (c == 'O') prompt_jump_line();

		else if (c == 'z') undo();
		else if (c == 'x') redo();
		else if (c == 'Z') alternate_up();
		else if (c == 'X') alternate_down();

		else if (c == 'a') { lal = lcl; lac = lcc; sprintf(message, "anchor %ld %ld", lal, lac); }

		else if (c == 'c') copy();
		else if (c == 'r') cut();  
		else if (c == 'v') paste();    

		else if (c == 'g') move_to_previous_buffer();
		else if (c == 'y') move_to_next_buffer();
		else if (c == 'f') prompt_open();
    		else if (c == 'l') create_empty_buffer();
		else if (c == 'q') { if (buffer.saved or confirmed("discard unsaved changes")) close_active_buffer(); }

		else if (c == 's') save();
		else if (c == 'S') rename_file();
		else if (c == 27) interpret_escape_code();

	} else if (buffer.mode == 2) {

		if (c == 't') buffer.mode = 0;
		else if (c == 'r') buffer.mode = 1;
		else if (c == 's') show_status = not show_status; 
		else if (c == 'n') show_line_numbers = not show_line_numbers;
		else if (c == 'd') memset(message, 0, sizeof message);
		
	} else buffer.mode = 1;
}

static inline void editor(const uint8_t* input, size_t input_count) {

	// FILE* file = fopen("crash-70bd435f53b8ff7b760a30590900997d5d8e7824", "r");
	// fseek(file, 0, SEEK_END);
	// size_t crash_length = (size_t) ftell(file);
	// char* crash = malloc(sizeof(char) * crash_length);
	// fseek(file, 0, SEEK_SET);
	// fread(crash, sizeof(char), crash_length, file);
	// fclose(file);
	// input = (const uint8_t*) crash;
	// input_count = crash_length;
	// printf("\n\n\nstr = \"");
	// for (size_t i = 0; i < input_count; i++) printf("\\x%02hhx", input[i]);
	// printf("\";\n\n\n");
	// exit(1);

	// const char*                 done:     str = "\x1f\x0a\xbb\x74\x7f\x72\x77\x70\x72\x7a\x7a";
	// move left bug: "\x7f\x1f\x0a\xbb\x43\x78\x74\x72\x77\x70\x7f\x72\x7a\x7a"
	// input = (const uint8_t*) str;
	// input_count = strlen(str);

	struct termios terminal;
	if (not fuzz) {
		terminal = configure_terminal();
		write(1, "\033[?1049h\033[?1000h", 16);
		buffer.needs_display_update = 1;
	}
	char p = 0, c = 0;
	size_t input_index = 0; 
loop:
	if (buffer.needs_display_update) {
		adjust_window_size();
		display();
	}
	if (fuzz) {
		if (input_index >= input_count) goto done;
		c = (char) input[input_index++];	
	} else read(0, &c, 1);
	buffer.needs_display_update = 1;
	execute(c, p);
	p = c;
	if (buffer_count) goto loop;
done:
	while (buffer_count) close_active_buffer();
	zero_registers();
	free(screen);
	screen = NULL;
	window_rows = 0;
	window_columns = 0;

	if (not fuzz) {
		write(1, "\033[?1049l\033[?1000l", 16);	
		tcsetattr(0, TCSAFLUSH, &terminal);
	}
}

#if fuzz && !use_main

int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {
	create_empty_buffer();
	editor(input, size);
	return 0;
}

#else

int main(const int argc, const char** argv) {
	if (argc <= 1) create_empty_buffer();
	else for (int i = 1; i < argc; i++) open_file(argv[i]);
	editor(NULL, 0);
}

#endif


/*

	todo: 
--------------------------
	

b	- vdc not correct, with unicode.  

f	- file manager and tab completion

f	- config file for init values.

f	- tc isa!!


		
	- make the machine code virtual machine interpreter:


		- implement the editor ISA, 
		- figure out how to allow for it to be TC.
		- make it ergonmic for a human to type, while being fast/efficient and minimalist.
		
			- - then later on, we can make the scripting language which will compile to the editor ISA machine code.


		- - easy to use by design,
		- - allow for repitions and macros by design,
		- - allow for commands with arbitary spelling
		- - extensible command context/enviornments,
		- - unified command system: buffer commands vs prompted commands. 




--------------------------------------------------------
			DONE:
--------------------------------------------------------


	features:
----------------------------


	x	- make the line numbers and column numbers 0-based everywhere. just do it. 

	x	- make scroll counter a local static variable in the internpret escape code function.

	x	- write the delete_buffers and delete_lines functions. 

	x	- we need to put most of the state for the registers that's not essential in a buffer data structure. 

	x	- make the code use "nat"'s instead of int's.    using the typedef:   // typedef nat ssize_t; 

	x	- redo the undo-tree code so that it uses a flat array datastructure- not a pointer based tree. 

	x	- make a simple and robust copy/paste system, that can support system clipboard.


	bugs:
----------------------------

	x 	- this editor has a HUGE memory leak. we need to fix this.

	x	- negative size param, delete(), from cut(). 

	x	- memory leak because of x:undo(); 

	x	- a possible crashing bug of undo()/move_left(), although i'm not sure yet...


*/



// static inline void recalculate_position() {    // used when we modify wrap or tab width. i think...
// 	int save_lcl = lcl, save_lcc = lcc;
// 	move_top();
// 	adjust_window_size();
// 	jump_line(save_lcl);
// 	jump_column(save_lcc);
// }



	// FILE* file = fopen("crash-c6c6ea3f9acfd5ca8ebcbb8e7e3e17846aca32bb", "r");
	// fseek(file, 0, SEEK_END);        
	// size_t crash_length = (size_t) ftell(file);
	// char* crash = malloc(sizeof(char) * crash_length);
	// fseek(file, 0, SEEK_SET);
	// fread(crash, sizeof(char), crash_length, file);
	// fclose(file);
	// input = (const uint8_t*) crash;
	// input_count = crash_length;
	// printf("\n\n\nstr = \"");
	// for (size_t i = 0; i < input_count; i++) printf("\\x%02hhx", input[i]);
	// printf("\";\n\n\n");
	// exit(1);

	// const char* str = "x\r\r\r\xfc\x01\x79\x72\x77\x58\x61\x65\x72\x65\x76\x72";
	// input = (const uint8_t*) str;
	// input_count = strlen(str);

/*


 ft: 7109 corp: 692/24Kb lim: 80 exec/s: 1459 rss: 675Mb L: 19/80 MS: 1 EraseBytes-
#110910	REDUCE cov: 1130 ft: 7109 corp: 692/24Kb lim: 80 exec/s: 1459 rss: 675Mb L: 33/80 MS: 1 EraseBytes-
#111029	NEW    cov: 1130 ft: 7110 corp: 693/24Kb lim: 80 exec/s: 1460 rss: 675Mb L: 75/80 MS: 4 InsertByte-ChangeBit-ShuffleBytes-CMP- DE: "z\x00\x00\x00"-
#111136	REDUCE cov: 1130 ft: 7110 corp: 693/24Kb lim: 80 exec/s: 1443 rss: 675Mb L: 48/80 MS: 2 CopyPart-EraseBytes-
=================================================================
==66313==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x000103305691 at pc 0x000100f6f990 bp 0x00016eea1340 sp 0x00016eea1338
READ of size 1 at 0x000103305691 thread T0
    #0 0x100f6f98c in move_left main.c:185
    #1 0x100f6e150 in delete main.c:457
    #2 0x100f73504 in reverse_action main.c:1188
    #3 0x100f72708 in undo main.c:1199
    #4 0x100f6404c in execute main.c:1256
    #5 0x100f5e9bc in editor main.c:1311
    #6 0x100f5e6d8 in LLVMFuzzerTestOneInput main.c:1333
    #7 0x100f9b9e4 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) FuzzerLoop.cpp:611
    #8 0x100f9b1c4 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) FuzzerLoop.cpp:514
    #9 0x100f9c840 in fuzzer::Fuzzer::MutateAndTestOne() FuzzerLoop.cpp:757
    #10 0x100f9d478 in fuzzer::Fuzzer::Loop(std::__1::vector<fuzzer::SizedFile, fuzzer::fuzzer_allocator<fuzzer::SizedFile> >&) FuzzerLoop.cpp:895
    #11 0x100f8db5c in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) FuzzerDriver.cpp:906
    #12 0x100fb54a0 in main FuzzerMain.cpp:20
    #13 0x1013590f0 in start+0x204 (dyld:arm64+0x50f0)
    #14 0x12527ffffffffffc  (<unknown module>)

0x000103305691 is located 0 bytes to the right of 1-byte region [0x000103305690,0x000103305691)
allocated by thread T0 here:
    #0 0x10143f6d8 in wrap_malloc+0x8c (libclang_rt.asan_osx_dynamic.dylib:arm64+0x3f6d8)
    #1 0x100f695dc in insert main.c:386
    #2 0x100f73c50 in reverse_action main.c:1192
    #3 0x100f72708 in undo main.c:1199
    #4 0x100f6404c in execute main.c:1256
    #5 0x100f5e9bc in editor main.c:1311
    #6 0x100f5e6d8 in LLVMFuzzerTestOneInput main.c:1333
    #7 0x100f9b9e4 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) FuzzerLoop.cpp:611
    #8 0x100f9b1c4 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) FuzzerLoop.cpp:514
    #9 0x100f9c840 in fuzzer::Fuzzer::MutateAndTestOne() FuzzerLoop.cpp:757
    #10 0x100f9d478 in fuzzer::Fuzzer::Loop(std::__1::vector<fuzzer::SizedFile, fuzzer::fuzzer_allocator<fuzzer::SizedFile> >&) FuzzerLoop.cpp:895
    #11 0x100f8db5c in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) FuzzerDriver.cpp:906
    #12 0x100fb54a0 in main FuzzerMain.cpp:20
    #13 0x1013590f0 in start+0x204 (dyld:arm64+0x50f0)
    #14 0x12527ffffffffffc  (<unknown module>)

SUMMARY: AddressSanitizer: heap-buffer-overflow main.c:185 in move_left
Shadow bytes around the buggy address:
  0x007020680a80: fa fa fd fa fa fa fa fa fa fa fd fa fa fa fa fa
  0x007020680a90: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007020680aa0: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007020680ab0: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007020680ac0: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
=>0x007020680ad0: fa fa[01]fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007020680ae0: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fd
  0x007020680af0: fa fa fd fa fa fa fd fa fa fa fa fa fa fa fd fa
  0x007020680b00: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007020680b10: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007020680b20: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==66313==ABORTING
MS: 1 CopyPart-; base unit: 8e5e1a31109078d9f8a45fdc4a0466da598b53b2
0x1a,0xa,0xa,0xa,0x2,0x5b,0xa,0xa,0xa,0xa,0xa,0xf6,0xa,0xa,0xa,0x9a,0x0,0x9,0x72,0x77,0x77,0xab,0x65,0x0,0x7a,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xf8,0x70,0xc,0x9,0x9,0x0,0x0,0x27,0x0,0x0,0x0,0x9,0x72,0x77,0x77,0xab,0x65,0x0,0x7a,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xf8,0x70,0xc,0x9,0x9,0x0,0x0,0x27,0x0,0x65,0x0,0x7a,0x0,0x0,0xa,0xa,0x9a,0x92,0x9a,
\x1a\x0a\x0a\x0a\x02[\x0a\x0a\x0a\x0a\x0a\xf6\x0a\x0a\x0a\x9a\x00\x09rww\xabe\x00z\x00\x00\x00\x00\x00\x00\x00\x00\xf8p\x0c\x09\x09\x00\x00'\x00\x00\x00\x09rww\xabe\x00z\x00\x00\x00\x00\x00\x00\x00\x00\xf8p\x0c\x09\x09\x00\x00'\x00e\x00z\x00\x00\x0a\x0a\x9a\x92\x9a
artifact_prefix='./'; Test unit written to ./crash-ecdc458c1f856e72f62017a778452e28c318dbb4
Base64: GgoKCgJbCgoKCgr2CgoKmgAJcnd3q2UAegAAAAAAAAAA+HAMCQkAACcAAAAJcnd3q2UAegAAAAAAAAAA+HAMCQkAACcAZQB6AAAKCpqSmg==
zsh: abort      ./editor
dwrr.editor: sub main.c

"\x1a\x0a\x0a\x0a\x02\x5b\x0a\x0a\x0a\x0a\x0a\xf6\x0a\x0a\x0a\x9a\x00\x09\x72\x77\x77\xab\x65\x00\x7a\x00\x00\x00\x00\x00\x00\x00\x00\xf8\x70\x0c\x09\x09\x00\x00\x27\x00\x00\x00\x09\x72\x77\x77\xab\x65\x00\x7a\x00\x00\x00\x00\x00\x00\x00\x00\xf8\x70\x0c\x09\x09\x00\x00\x27\x00\x65\x00\x7a\x00\x00\x0a\x0a\x9a\x92\x9a"
*/
















1 corp: 961/38Kb lim: 285 exec/s: 70 rss: 984Mb L: 68/258 MS: 4 InsertRepeatedBytes-CrossOver-ChangeBit-EraseBytes-
#347273	REDUCE cov: 1154 ft: 8371 corp: 961/37Kb lim: 293 exec/s: 70 rss: 984Mb L: 196/258 MS: 2 CopyPart-EraseBytes-
=================================================================
==32352==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x0001070e5ffa at pc 0x000104b5bbcc bp 0x00016b2b6380 sp 0x00016b2b6378
READ of size 1 at 0x0001070e5ffa thread T0
    #0 0x104b5bbc8 in move_left main.c:187
    #1 0x104b66e88 in jump_column main.c:288
    #2 0x104b669f8 in prompt_jump_column main.c:1063
    #3 0x104b504e8 in execute main.c:1282
    #4 0x104b4aec0 in editor main.c:1362
    #5 0x104b4abdc in LLVMFuzzerTestOneInput main.c:1384
    #6 0x104b879e4 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) FuzzerLoop.cpp:611
    #7 0x104b871c4 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) FuzzerLoop.cpp:514
    #8 0x104b88840 in fuzzer::Fuzzer::MutateAndTestOne() FuzzerLoop.cpp:757
    #9 0x104b89478 in fuzzer::Fuzzer::Loop(std::__1::vector<fuzzer::SizedFile, fuzzer::fuzzer_allocator<fuzzer::SizedFile> >&) FuzzerLoop.cpp:895
    #10 0x104b79b5c in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) FuzzerDriver.cpp:906
    #11 0x104ba14a0 in main FuzzerMain.cpp:20
    #12 0x104fd90f0 in start+0x204 (dyld:arm64+0x50f0)
    #13 0x4e0ffffffffffffc  (<unknown module>)

0x0001070e5ffa is located 0 bytes to the right of 42-byte region [0x0001070e5fd0,0x0001070e5ffa)
allocated by thread T0 here:
    #0 0x1050bf94c in wrap_realloc+0x94 (libclang_rt.asan_osx_dynamic.dylib:arm64+0x3f94c)
    #1 0x104b56a1c in insert main.c:399
    #2 0x104b5fe8c in reverse_action main.c:1221
    #3 0x104b5e944 in undo main.c:1228
    #4 0x104b50550 in execute main.c:1285
    #5 0x104b4aec0 in editor main.c:1362
    #6 0x104b4abdc in LLVMFuzzerTestOneInput main.c:1384
    #7 0x104b879e4 in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) FuzzerLoop.cpp:611
    #8 0x104b871c4 in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) FuzzerLoop.cpp:514
    #9 0x104b88840 in fuzzer::Fuzzer::MutateAndTestOne() FuzzerLoop.cpp:757
    #10 0x104b89478 in fuzzer::Fuzzer::Loop(std::__1::vector<fuzzer::SizedFile, fuzzer::fuzzer_allocator<fuzzer::SizedFile> >&) FuzzerLoop.cpp:895
    #11 0x104b79b5c in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) FuzzerDriver.cpp:906
    #12 0x104ba14a0 in main FuzzerMain.cpp:20
    #13 0x104fd90f0 in start+0x204 (dyld:arm64+0x50f0)
    #14 0x4e0ffffffffffffc  (<unknown module>)

SUMMARY: AddressSanitizer: heap-buffer-overflow main.c:187 in move_left
Shadow bytes around the buggy address:
  0x007020e3cba0: fa fa fd fd fd fd fd fd fa fa fa fa fa fa fa fa
  0x007020e3cbb0: fa fa fa fa fa fa fa fa fa fa fd fd fd fd fd fa
  0x007020e3cbc0: fa fa fd fd fd fd fd fd fa fa fa fa fa fa fa fa
  0x007020e3cbd0: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x007020e3cbe0: fa fa fd fd fd fd fd fd fa fa fd fd fd fd fd fa
=>0x007020e3cbf0: fa fa fd fd fd fd fd fd fa fa 00 00 00 00 00[02]
  0x007020e3cc00: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x007020e3cc10: fa fa fa fa fa fa fa fa fa fa fd fd fd fd fd fd
  0x007020e3cc20: fa fa fd fd fd fd fd fa fa fa fa fa fa fa fa fa
  0x007020e3cc30: fa fa fd fd fd fd fd fd fa fa fa fa fa fa fa fa
  0x007020e3cc40: fa fa fd fd fd fd fd fa fa fa fd fd fd fd fd fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==32352==ABORTING
MS: 4 InsertRepeatedBytes-PersAutoDict-PersAutoDict-PersAutoDict- DE: "domain-po"-"\x00\x00\x00R"-":JR.\x01\x00\x00\x00"-; base unit: 937d96d5d341db240b8ec0c29bd302c70f8d1767
0x7a,0x7a,0x76,0x31,0x72,0x7a,0x75,0x7a,0x76,0x7a,0x7a,0x72,0x7a,0x72,0x65,0xa,0xa,0x60,0x9d,0x9c,0x27,0x75,0x66,0x9d,0x24,0x61,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0x0,0x0,0x52,0xff,0xff,0xff,0xff,0xff,0xff,0x65,0x7e,0xb4,0x0,0x1e,0x52,0x74,0xa8,0x45,0x45,0xa,0x76,0x6a,0x75,0x66,0x9d,0x2c,0x64,0x6f,0x6d,0x61,0x69,0x6e,0x2d,0x70,0x6f,0xed,0x3a,0x4a,0x52,0x2e,0x1,0x0,0x0,0x0,0x52,0x74,0xa8,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0xff,0xff,0xff,0x45,0x45,0x45,0x45,0x45,0x45,0xa8,0x45,0x45,0x45,0x45,0x45,0x45,0x76,0x7a,0x75,0x66,0x72,0x7a,0x45,0x45,0x45,0x45,0x45,0x55,0x40,0x0,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x45,0x65,0x54,0x0,0x0,0x0,0x45,0x45,0xe,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x0,0x77,
zzv1rzuzvzzrzre\x0a\x0a`\x9d\x9c'uf\x9d$a\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00R\xff\xff\xff\xff\xff\xffe~\xb4\x00\x1eRt\xa8EE\x0avjuf\x9d,domain-po\xed:JR.\x01\x00\x00\x00Rt\xa8EEEEEEE\xff\xff\xffEEEEEE\xa8EEEEEEvzufrzEEEEEU@\x00EEEEEEEEEEEEeT\x00\x00\x00EE\x0ewwwwwwwww\x00w
artifact_prefix='./'; Test unit written to ./crash-9022d2bc16312f0c418c7a1cc75285d0a4237706
Base64: enp2MXJ6dXp2enpyenJlCgpgnZwndWadJGH/////////AAAAUv///////2V+tAAeUnSoRUUKdmp1Zp0sZG9tYWluLXBv7TpKUi4BAAAAUnSoRUVFRUVFRf///0VFRUVFRahFRUVFRUV2enVmcnpFRUVFRVVAAEVFRUVFRUVFRUVFRWVUAAAARUUOd3d3d3d3d3d3AHc=
zsh: abort      ./editor
dwrr.editor:  
