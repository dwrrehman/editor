//  Daniel Warren Riaz Rehman's minimalist 
//        terminal-based text editor 
//
//     Designed with reliability, minimalism, 
//     simplicity, and ergonimics in mind.
//
//          tentatively named:   "t".
//
// -------------------------------------------
//
//    change log:
//          written on 2101177.005105
//           edited on 2111114.172631
//           edited on 2112116.194022
//         debugged on 2201252.173237
// 	   debugged on 2208022.211844
// 	   debugged on 2208151.002947
// 	   



/*

	things to add to make the editor useable full time:

		- tab completion for file names

		- simple file editor?

		- config file for constants 

		- be able to adjust wrap width, tab width,  etc,   within editor?  hmm, yeah... 


		- 



------------------- bugs ----------------------










[test]		- test if the confirmed() code  (ie, overwriting, and discarding file stuff) works.

[test]		- test if the copy paste code is still working. 









[bug/feature]		- scrolling down is not very smooth. we should be using the terminal scrolling mode. 
			use the escape code sequences 
			<ESC>[r    to enable scrolling on the whole screen
			<ESC>[{start};{end}r   to enable scrolling on a range of rows of the screen.
			<ESC>D    to scroll down
			<ESC>M   to scroll up







---------------- features ---------------


[feature]	- add tab completion for file names, and a small file veiwer, when saving/renaming. 
***


[feature]	- rework the way the      wrap_width        disabling/dynamic adjusting works. 
**


[optional]	- rebind the keybindings of the editor. 
**



[feature]	- init parameters from config file. 
*


[feature]	- implment my programming language in it. 
*


[optional]	- (BIG) redesign how we are drawing the screen, to be based on drawing text that is isolated by 
			whitespace, and jumping the (invisible) cursor around the screen, to draw those areas.  
			and clearing the screen before each frame of course,






------------------- fixed -----------------


[FIXED]		- tab width and wrap width major bugs.

[FIXED]		- the performance of our display function is not good. we are currently making an n^2 algorithm in order to draw the screen.


[FIXED]	 		- solve the two slow-unit-fffffff test cases that the fuzzer found. 


[FIXED]			- test how the editor handles unicode characters with the new tab/wrap/display code. 









*/



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
#include <stdint.h>
#include <signal.h>


#define is_fuzz_testing		0

#define memory_safe 	1

#define fuzz 		is_fuzz_testing
#define use_main    	not is_fuzz_testing

typedef ssize_t nat;

enum action_type {
	no_action,
	insert_action,
	delete_action,
	paste_text_action,
	cut_text_action,
	anchor_action,
};

struct line { 
	char* data; 
	nat count, capacity; 
};

struct buffer {
	struct line* lines;
	struct action* actions;

	nat     saved, mode, 
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
	nat     saved, line_number_width, 

		lcl, lcc, 	vcl, vcc,  	vol, voc, 
		vsl, vsc, 	vdc,    	lal, lac,

		wrap_width, tab_width
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
	nat* children; 
	char* text;
	nat parent;
	nat type;
	nat choice;
	nat count;
	nat length;
	struct logical_state pre;
	struct logical_state post;
};

static nat 
	window_rows = 0, 
	window_columns = 0;
static char* screen = NULL;
static struct textbox tb = {0};

static size_t fuzz_input_index = 0; 
static size_t fuzz_input_count = 0;
static const uint8_t* fuzz_input = NULL;

static struct buffer* buffers = NULL;
static nat buffer_count = 0, active_index = 0;

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

static inline bool stdin_is_empty() {
	if (fuzz) return fuzz_input_index >= fuzz_input_count;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);
	struct timeval timeout = {0};
	return select(1, &readfds, NULL, NULL, &timeout) != 1;
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


static inline nat compute_custom_vcc(nat given_lcc) {
	nat v = 0;
	for (nat c = 0; c < given_lcc; c++) {
		char k = lines[lcl].data[c];
		if (k == '\t') {
			if (v + tab_width - v % tab_width <= wrap_width)
				do v++; 
				while (v % tab_width);
			else v = 0;
		} else if (visual(k)) {
			if (v < wrap_width) v++; else v = 0;
		}
	}
	return v;
}

static inline void compute_vcc() {
	vcc = 0;
	for (nat c = 0; c < lcc; c++) {
		char k = lines[lcl].data[c];
		if (k == '\t') { 
			if (vcc + tab_width - vcc % tab_width <= wrap_width) 
				do vcc++;
				while (vcc % tab_width);  
			else vcc = 0;
		} else if (visual(k)) {
			if (vcc < wrap_width) vcc++; else vcc = 0;
		}
	}
}


static inline void move_left(bool change_desired) {

	if (not lcc) {
		if (not lcl) return;
		lcl--;
		lcc = lines[lcl].count;
 line_up: 	vcl--;
		if (vsl) vsl--;
		else if (vol) vol--;
		compute_vcc();
		if (vcc >= window_columns - 1 - line_number_width) { 
			vsc = window_columns - 1 - line_number_width;  voc = vcc - vsc; 
		} else { vsc = vcc; voc = 0; }
	} else {
		do lcc--; while (lcc and zero_width(lines[lcl].data[lcc]));
		if (lines[lcl].data[lcc] == '\t') { 
			const nat diff = tab_width - compute_custom_vcc(lcc) % tab_width;
			if (vcc < diff) goto line_up;
			vcc -= diff;
			if (vsc >= diff) vsc -= diff;
			else if (voc >= diff - vsc) { voc -= diff - vsc; vsc = 0; }
		} else {
			if (not vcc) goto line_up;
			vcc--;
			if (vsc) vsc--; else if (voc) voc--;
		}
	}

	if (change_desired) vdc = vcc;
}

static inline void move_right(bool change_desired) {
	if (lcl >= count) return;
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; lcc = 0; 
line_down:	vcl++; vcc = 0; voc = 0; vsc = 0;
		if (vsl + 1 < window_rows - show_status) vsl++; 
		else vol++;
	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
			if (vcc + tab_width - vcc % tab_width > wrap_width) goto line_down;
			do {
				vcc++; 
				if (vsc + 1 < window_columns - line_number_width) vsc++;
				else voc++;
			} while (vcc % tab_width);
			
		} else {
			do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
			if (vcc >= wrap_width) goto line_down;
			vcc++; 
			if (vsc + 1 < window_columns - line_number_width) vsc++; 
			else voc++;
		}
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
	if (vcc > window_columns - line_number_width) { vsc = window_columns - line_number_width; voc = vcc - vsc; } 
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
		// TODO: ^ WHAT IS THIS!?!?!?

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

static inline void record_logical_state(struct logical_state* pcond_out) {
	struct logical_state* p = pcond_out; 

	p->saved = buffer.saved;
	p->line_number_width = line_number_width;

	p->lcl = lcl;  p->lcc = lcc; 	
	p->vcl = vcl;  p->vcc = vcc;
  	p->vol = vol;  p->voc = voc;
	p->vsl = vsl;  p->vsc = vsc; 
	p->vdc = vdc;  p->lal = lal;
	p->lac = lac;

	p->wrap_width = wrap_width;
	p->tab_width = tab_width;
}

static inline void require_logical_state(struct logical_state* pcond_in) {  
	struct logical_state* p = pcond_in;

	buffer.saved = p->saved;
	line_number_width = p->line_number_width;

	lcl = p->lcl;  lcc = p->lcc; 	
	vcl = p->vcl;  vcc = p->vcc;
  	vol = p->vol;  voc = p->voc;
	vsl = p->vsl;  vsc = p->vsc; 
	vdc = p->vdc;  lal = p->lal;
	lac = p->lac;

	wrap_width = p->wrap_width;
	tab_width = p->tab_width;
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

	if (should_record and zero_width(c) 
		and not (
			actions[head].type == insert_action and 
			actions[head].text[0] != '\n' and
			actions[head].post.lcl == lcl and 
			actions[head].post.lcc == lcc and 
			actions[head].count == 0
		)) return; 

	struct action new_action = {0};
	if (should_record and visual(c)) record_logical_state(&new_action.pre);

	struct line* this = lines + lcl;
	if (c == '\n') {
		nat rest = this->count - lcc;
		this->count = lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, this->data + lcc, (size_t) rest);

		if (not memory_safe) {
			if (count + 1 >= capacity) 
				lines = realloc(lines, sizeof(struct line) * (size_t)(capacity = 8 * (capacity + 1)));
		} else {
			lines = realloc(lines, sizeof(struct line) * (size_t)(count + 1));
		}

		memmove(lines + lcl + 2, lines + lcl + 1, sizeof(struct line) * (size_t)(count - (lcl + 1)));
		lines[lcl + 1] = new;
		count++;

	} else {
		if (not memory_safe) {
			if (this->count + 1 >= this->capacity) 
				this->data = realloc(this->data, (size_t)(this->capacity = 8 * (this->capacity + 1)));
		} else {
			this->data = realloc(this->data, (size_t)(this->count + 1));
		}

		memmove(this->data + lcc + 1, this->data + lcc, (size_t) (this->count - lcc));
		this->data[lcc] = c;
		this->count++;
	}
	
	if (zero_width(c)) lcc++; 
	else move_right(1);

	buffer.saved = false;
	if (not should_record) return;
	lac = lcc; lal = lcl;

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

		if (not memory_safe) {
			if (new->count + this->count >= new->capacity)
				new->data = realloc(new->data, (size_t)(new->capacity = 8 * (new->capacity + this->count)));
		} else {
			new->data = realloc(new->data, (size_t)(new->count + this->count));
		}

		if (this->count) memcpy(new->data + new->count, this->data, (size_t) this->count);
		free(this->data);

		new->count += this->count;
		memmove(lines + lcl + 1, lines + lcl + 2, 
			sizeof(struct line) * (size_t)(count - (lcl + 2)));
		count--;

		if (not memory_safe) {
			// do nothing.
		} else {
			lines = realloc(lines, sizeof(struct line) * (size_t)count);
		}

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

		if (not memory_safe) {
			// do nothing.
		} else {
			this->data = realloc(this->data, (size_t)(this->count));
		}
	}

	buffer.saved = false;
	if (not should_record) return;
	lac = lcc; lal = lcl;

	record_logical_state(&new_action.post);
	new_action.type = delete_action;
	new_action.text = deleted_string;
	new_action.length = deleted_length;
	create_action(new_action);
}

static inline void adjust_window_size() {
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);

	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 15; window.ws_col = 50; }

	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen = realloc(screen, (size_t) (window_rows * window_columns * 4));
	}

	// 	TODO: 
    	//
	// 		redo this behaviour. make it so that you can explicitly disable wrapwidth (not just make it sudo-infinity), 
	// 		or explicitly make it wrap to screen width, and dynamically adjust as the screen width changes!!!
	// 

	if (not wrap_width) wrap_width = (window_columns - 1) - (line_number_width);

}


static inline void display() {
	nat length = 9; 
	memcpy(screen, "\033[?25l\033[H", 9);
	
	nat sl = 0, sc = 0; 
	nat vl = vol, vc = voc; 

	if (fuzz) {
		if (lcl < 0) abort();
		if (lcc < 0) abort();

		if (vol < 0) abort();
		if (voc < 0) abort();

		if (vcl < 0) abort();
		if (vcc < 0) abort();		
		
		if (vsl < 0) abort();
		if (vsc < 0) abort();
	}

	struct logical_state state = {0};
	record_logical_state(&state);
	while (1) { 
		if (vcl <= 0 and vcc <= 0) break;
		if (vcl <= state.vol and vcc <= state.voc) break;
		move_left(0);
	}

 	nat line = lcl, col = lcc; 
	require_logical_state(&state); 

	double f = floor(log10((double) count)) + 1;
	int line_number_digits = (int)f;
	line_number_width = show_line_numbers * (line_number_digits + 2);

	do {
		if (line >= count) goto next_visual_line;

		if (show_line_numbers and vl >= vol and vl < vol + window_rows - show_status) {
			if (not col or (not sc and not sl)) length += sprintf(screen + length, "\033[38;5;%ldm%*ld\033[0m  ", buffer.line_number_color + (line == lcl ? 5 : 0), line_number_digits, line);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}

		do {
			if (col >= lines[line].count) goto next_logical_line;  
			
			char k = lines[line].data[col++];

			if (k == '\t') {

				if (vc + (tab_width - vc % tab_width) > wrap_width) goto next_visual_line;

				do { 
					if (	vc >= voc and vc < voc + window_columns - line_number_width
					and 	vl >= vol and vl < vol + window_rows - show_status
					) {
						screen[length++] = ' '; sc++;
					}
					vc++;
				} while (vc % tab_width);

			} else {
				if (	vc >= voc and vc < voc + window_columns - line_number_width
				and 	vl >= vol and vl < vol + window_rows - show_status 
				and 	(sc or visual(k))
				) { 
					screen[length++] = k;
					if (visual(k)) sc++;	
				}
				if (visual(k)) {
					if (vc >= wrap_width) goto next_visual_line; 
					vc++; 
				} 
			}

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
		nat status_length = sprintf(screen + length, " %s [%ld][%ld %ld][%ld %ld][%ld %ld][%ld %ld][%ld %ld] %s %c  %s",
			datetime, 
			buffer.mode, 
			active_index, buffer_count,
			lcl, lcc, vcl, vcc, vsl, vsc, vol, voc, 
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
	if (tb.vs < window_columns - tb.prompt_length) tb.vs++; else tb.vo++;
	do tb.c++; while (tb.c < tb.count and zero_width(tb.data[tb.c]));
}

static inline void textbox_insert(char c) {

	if (not memory_safe) {
		if (tb.count + 1 >= tb.capacity) 
			tb.data = realloc(tb.data, (size_t)(tb.capacity = 8 * (tb.capacity + 1)));
	} else {
		tb.data = realloc(tb.data, (size_t)(tb.count + 1));
	}

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

	if (not memory_safe) {
		// do nothing.
	} else {
		tb.data = realloc(tb.data, (size_t)(tb.count));
	}
}

static inline void textbox_display(const char* prompt, nat prompt_color) {
	nat length = sprintf(screen, "\033[?25l\033[%ld;1H\033[38;5;%ldm%s\033[m", window_rows, prompt_color, prompt);
	nat col = 0, vc = 0, sc = tb.prompt_length;
	while (sc < window_columns and col < tb.count) {
		char k = tb.data[col];
		if (vc >= tb.vo and vc < tb.vo + window_columns - tb.prompt_length and (sc or visual(k))) {
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
	if (not fuzz) 
		write(1, screen, (size_t) length);
}


static inline void prompt(const char* prompt_message, nat color, char* out, nat out_size) {

	tb.prompt_length = (nat) strlen(prompt_message);
	do {
		adjust_window_size();
		textbox_display(prompt_message, color);
		char c = 0;
		
		if (fuzz) {
			if (fuzz_input_index >= fuzz_input_count) break;
			c = (char) fuzz_input[fuzz_input_index++];	
		} else {
			read(0, &c, 1);
		}

		if (c == '\r' or c == '\n') break;
		else if (c == '\t') { /*tab complete*/ }
		else if (c == 27 and stdin_is_empty()) { tb.count = 0; break; }
		else if (c == 27) {

			if (fuzz) {
				if (fuzz_input_index >= fuzz_input_count) break;
				c = (char) fuzz_input[fuzz_input_index++];	
			} else {
				read(0, &c, 1);
			}

			if (c == '[') {

				if (fuzz) {
					if (fuzz_input_index >= fuzz_input_count) break;
					c = (char) fuzz_input[fuzz_input_index++];	
				} else {
					read(0, &c, 1);
				}

				if (c == 'A') {}
				else if (c == 'B') {}
				else if (c == 'C') textbox_move_right();
				else if (c == 'D') textbox_move_left();
			}
		} else if (c == 127) textbox_delete();
		else textbox_insert(c);

		if (fuzz) { 
			if (fuzz_input_index >= fuzz_input_count) break;
		}

	} while (1);
	if (tb.count > out_size) tb.count = out_size;
	memcpy(out, tb.data, (size_t) tb.count);
	memset(out + tb.count, 0, (size_t) out_size - (size_t) tb.count);
	out[out_size - 1] = 0;
	free(tb.data);
	tb = (struct textbox){0};
}


static inline bool confirmed(const char* question, const char* yes_action, const char* no_action) {

	char prompt_message[4096] = {0}, invalid_response[4096] = {0};
	sprintf(prompt_message, "%s? (%s/%s): ", question, yes_action, no_action);
	sprintf(invalid_response, "please type \"%s\" or \"%s\".", yes_action, no_action);
	
	while (1) {
		char response[10] = {0};
		prompt(prompt_message, buffer.alert_prompt_color, response, sizeof response);
		
		if (not strcmp(response, yes_action)) return true;
		else if (not strcmp(response, no_action)) return false;
		else if (not strcmp(response, "")) return false;
		else print_above_textbox(invalid_response, buffer.default_prompt_color);

		if (fuzz) return true;
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

	show_status = 1; // init using file
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

static inline void emergency_save_to_file() {
	if (fuzz) return;

	char dt[16] = {0};
	get_datetime(dt);
	
	char local_filename[4096] = {0};
	sprintf(local_filename, "EMERGENCY_FILE_SAVE__%s_.txt", dt);

	FILE* file = fopen(local_filename, "w+");
	if (not file) {
		printf("emergency error: %s\n", strerror(errno));
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
		printf("emergency error: %s\n", strerror(errno));
		fclose(file);
		return;
	}

	fclose(file);

	printf("interrupt: emergency wrote %lldb;%ldl to %s\n", bytes, count, local_filename);
}





static void handle_signal_interrupt(int code) {

	if (fuzz) exit(1);

	printf(	"interrupt: caught signal SIGINT(%d), "
		"emergency saving...\n\r", 
		code);

	emergency_save_to_file();

	printf("press C to continue running process\n\r");
	int c = getchar(); 
	if (c != 'C') exit(1);
}




static inline void save() {
	if (fuzz) return;

	if (not strlen(filename)) {
	prompt_filename:
		prompt("save as: ", buffer.default_prompt_color, filename, sizeof filename);
		if (not strlen(filename)) { sprintf(message, "aborted save"); return; }
		if (not strrchr(filename, '.') and buffer.use_txt_extension_when_absent) strcat(filename, ".txt");
		if (file_exists(filename) and not confirmed("file already exists, overwrite", "overwrite", "no")) {
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

	if (file_exists(new) and not confirmed("file already exists, overwrite", "overwrite", "no")) {
		strcpy(new, ""); goto prompt_filename;
	}

	if (rename(filename, new)) sprintf(message, "error: %s", strerror(errno));
	else {
		strncpy(filename, new, sizeof new);
		sprintf(message, "renamed to \"%s\"", filename);
	}
}

static inline void interpret_escape_code() {

	char c = 0;
	
	if (fuzz) {
		if (fuzz_input_index >= fuzz_input_count) return;
		c = (char) fuzz_input[fuzz_input_index++];	
	} else {
		read(0, &c, 1);
	}
	
	if (c == '[') {


		if (fuzz) {
			if (fuzz_input_index >= fuzz_input_count) return;
			c = (char) fuzz_input[fuzz_input_index++];	
		} else {
			read(0, &c, 1);
		}

		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') move_right(1);
		else if (c == 'D') move_left(1);

		/*  TODO:   i need to completely redo the way that we scroll the screen, using mouse/trackpad scrolling. 

		else if (c == 'M') {
			
			char str[3] = {0};

			read(0, str + 0, 1); // b
			read(0, str + 1, 1); // x
			read(0, str + 2, 1); // y

			sprintf(message, "mouse reporting: [%d,%d,%d].", str[0],str[1],str[2]);
		} 


		else if (c == 77) {

			read(0, &c, 1);

			if (c == 97) {

				read(0, &c, 1); 
				read(0, &c, 1);

				// scroll_counter++;
				// if (scroll_counter == buffer.scroll_speed) {
					move_down();
				// 	scroll_counter = 0;
				// } else buffer.needs_display_update = 0;

			} else if (c == 96) {

				read(0, &c, 1); 
				read(0, &c, 1);

				// scroll_counter++;
				// if (scroll_counter == buffer.scroll_speed) {
					move_up();
				//	scroll_counter = 0;
				// } else buffer.needs_display_update = 0;
			} 
		}
	
		*/
	
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

static inline bool is_exit_sequence(char c, char p) {  //todo: make this configurable. 
	return c == 'w' and p == 'r' or c == 'f' and p == 'u';
}

static char* get_sel(nat* out_length, nat first_line, nat first_column, nat last_line, nat last_column) {
	
	char* string = malloc(256);
	nat length = 0;

	nat s_capacity = 256;

	nat line = first_line, column = first_column;

	while (line < last_line) {
		
		if (not memory_safe) {
			if (length + lines[line].count - column + 1 >= s_capacity) 
				string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + lines[line].count - column + 1)));
		} else {
			string = realloc(string, (size_t) (length + lines[line].count - column));
		}

		if (lines[line].count - column) 
			memcpy(string + length, lines[line].data + column, (size_t)(lines[line].count - column));

		length += lines[line].count - column;

		if (not memory_safe) {
			// do nothing. 
		} else {
			string = realloc(string, (size_t) (length + 1));
		}

		string[length++] = '\n';
		line++;
		column = 0;
	}

	if (not memory_safe) {
		if (length + last_column - column >= s_capacity) 
		 	string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + last_column - column)));
	} else {
		string = realloc(string, (size_t) (length + last_column - column));	
	}

	if (last_column - column) memcpy(string + length, lines[line].data + column, (size_t)(last_column - column));
	length += last_column - column;
	*out_length = length;
	return string;
}

static inline char* get_selection(nat* out) {
	if (lal < lcl) return get_sel(out, lal, lac, lcl, lcc);
	if (lcl < lal) return get_sel(out, lcl, lcc, lal, lac);
	if (lac < lcc) return get_sel(out, lal, lac, lcl, lcc);
	if (lcc < lac) return get_sel(out, lcl, lcc, lal, lac);
	*out = 0;
	return NULL;
}

static inline void paste() {

 if (not fuzz) {

	FILE* file = popen("pbpaste", "r");
	if (not file) { sprintf(message, "error: paste: popen(): %s", strerror(errno)); return; }
	struct action new = {0};
	record_logical_state(&new.pre);
	char* string = malloc(256);
	nat s_capacity = 256;
	nat length = 0;
	lac = lcc; 
	lal = lcl;
	nat c = 0;
	while ((c = fgetc(file)) != EOF) {

		if (not memory_safe) {
			if (length + 1 >= s_capacity) 
				string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + 1)));
		} else {
			string = realloc(string, (size_t) (length + 1));
		}

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


 } else {
 	struct action new = {0};
 	record_logical_state(&new.pre);
 	char* string = malloc(256);
 	nat length = 0;
 	lac = lcc;
 	lal = lcl;
 	string = realloc(string, (size_t) (length + 1));
 	string[length++] = (char) 'A';
 	insert((char)'A', 0);
 	sprintf(message, "pasted %ldb", length);
 	record_logical_state(&new.post);
 	new.type = paste_text_action;
 	new.text = string;
 	new.length = length;
 	create_action(new);
 }

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
	if (a.type == no_action) {}
	else if (a.type == insert_action or a.type == paste_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_text_action) cut_text();
	else if (a.type == anchor_action) {}
	else sprintf(message, "error: unknown action");
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {
	require_logical_state(&a.post);
	if (a.type == no_action) {}
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_text_action) {
		while (lcc > a.pre.lcc or lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == anchor_action) {}
	else sprintf(message, "error: unknown action");
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

static inline void anchor() {
	struct action new = {.type = anchor_action};
	record_logical_state(&new.pre);
	lal = lcl; lac = lcc;
	sprintf(message, "anchor %ld %ld", lal, lac);
	record_logical_state(&new.post);
	create_action(new);
}

static inline void recalculate_position() {
	nat save_lcl = lcl, save_lcc = lcc;
	move_top();
	adjust_window_size();
	jump_line(save_lcl);
	jump_column(save_lcc);
}



static inline void execute(char c, char p) {
	if (buffer.mode == 0) {

		if (is_exit_sequence(c, p)) { undo(); buffer.mode = 1; }
		else if (c == '\r') insert('\n', 1);
		else if (c == 127) delete(1);
		else if (c == 27 and stdin_is_empty()) buffer.mode = 1;
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

		else if (c == 'a') anchor();
		else if (c == 'c') copy();
		else if (c == 'r') cut();  
		else if (c == 'v') paste();    

		else if (c == 'g') move_to_previous_buffer();
		else if (c == 'y') move_to_next_buffer();
		else if (c == 'f') prompt_open();
    		else if (c == 'l') create_empty_buffer();
		else if (c == 'q') { if (buffer.saved or confirmed("discard unsaved changes", "discard", "no")) close_active_buffer(); }

		else if (c == 's') save();
		else if (c == 'S') rename_file();
	
		else if (c == '_') memset(message, 0, sizeof message);

		else if (c == '.') { wrap_width = 30; recalculate_position(); }
		else if (c == ':') { wrap_width = 20; recalculate_position(); }
		else if (c == '\\') { wrap_width = 0; recalculate_position(); }
		else if (c == ',') { tab_width = 4; recalculate_position(); }
		else if (c == ';') { tab_width = 8; recalculate_position(); }
		
		else if (c == '\t') {}
		else if (c == '\n') {}
		else if (c == ' ') {}

		else if (c == 27 and stdin_is_empty()) buffer.mode = 1;
		else if (c == 27) interpret_escape_code();

	} else if (buffer.mode == 2) {

		if (c == 't') buffer.mode = 0;
		else if (c == 'r') buffer.mode = 1;
		else if (c == 's') show_status = not show_status;  // temp
		else if (c == 'n') show_line_numbers = not show_line_numbers; // temp


	} else buffer.mode = 1;
}

static inline void editor(const uint8_t* _input, size_t _input_count) {

if ((0)) {

	FILE* file = fopen("timeout-cf5744ac30fcc23932eaf10a64f5db7a2b419b06", "r");
	if (not file) { perror("open"); exit(1); }
	fseek(file, 0, SEEK_END);
	size_t crash_length = (size_t) ftell(file);
	char* crash = malloc(sizeof(char) * crash_length);
	fseek(file, 0, SEEK_SET);
	fread(crash, sizeof(char), crash_length, file);
	fclose(file);
	_input = (const uint8_t*) crash;
	_input_count = crash_length;
	printf("\n\n\nstr = \"");
	for (size_t i = 0; i < _input_count; i++) printf("\\x%02hhx", _input[i]);
	printf("\";\n\n\n");

	exit(1);


} else {

	// const char* str = "\x09\x77\x72\x77\x2c\x7a";
	//const char* str = "\t5rw,z";
	//puts(str);
	//_input = (const uint8_t*) str;
	//_input_count = strlen(str);
	// _input_count = 1000;

}

	struct termios terminal;
	if (not fuzz) {
		terminal = configure_terminal();
		write(1, "\033[?1049h\033[?1000h\033[7l", 20);
		buffer.needs_display_update = 1;
	}
	
	if (fuzz) {
		fuzz_input_index = 0; 
		fuzz_input = _input;
		fuzz_input_count = _input_count;
	}

	char p = 0, c = 0;
	
loop:
	if (buffer.needs_display_update) {
		adjust_window_size();
		display();
	}
	if (fuzz) {
		if (fuzz_input_index >= fuzz_input_count) goto done;
		c = (char) fuzz_input[fuzz_input_index++];	
	} else {
		read(0, &c, 1);
	}
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
		write(1, "\033[?1049l\033[?1000l\033[7h", 20);	
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
	signal(SIGINT, handle_signal_interrupt);
	editor(NULL, 0);
}

#endif





// ---------------------------------------------------------------------------------------------------

















































// static nat scroll_counter = 0;

	// DONE:    TODO: make it so pressing escape once is sufficient.

	// if (stdin_is_empty()) sprintf(message, "stdin buffer is empty!");
	// also add mouse support so that you can click to reposition the cursor. 






		// todo: add    esc [ 2004 l       :   disable bracketed paste mode. because we arent babies, and dont care about copy paste attacks. 

		// note:  we already have:  1049h   : switch to the alternate screen. 

		//







// FILE* file = fopen("crash-b019392511ce17e97e5710b3e737866f804f99f3", "r");
	// if (not file) { perror("open"); exit(1); }
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

	// const char* str = "\x74\x43\x72\x77\x74\x92\x72\x77\x58\x78\x74\x72\x77\x78\x7a";

	// input = (const uint8_t*) str;
	// input_count = strlen(str);
	// input_count = 1000;






/*

	todo: 
--------------------------


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

	x 	- vdc not correct, with unicode.  



///////////////////////////////// old code //////////////////////////////////////////////

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


	// const char* str = "jufat\xa8ufrzE";   

	// c  <exit>  anchor   insertmode    <unicode>    <exit>      cut       undo      jump@col0



00	REDUCE cov: 1183 ft: 8920 corp: 1230/93Kb lim: 958 exec/s: 614 rss: 1452Mb L: 129/871 MS: 1 EraseBytes-
#833346	REDUCE cov: 1183 ft: 8920 corp: 1230/93Kb lim: 967 exec/s: 613 rss: 1452Mb L: 28/871 MS: 1 EraseBytes-
=================================================================
==58956==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x000116f40c72 at pc 0x000102b33900 bp 0x00016d2dd380 sp 0x00016d2dd378
READ of size 1 at 0x000116f40c72 thread T0
    #0 0x102b338fc in move_left main.c:189
    #1 0x102b31df8 in delete main.c:474
    #2 0x102b37474 in reverse_action main.c:1257
    #3 0x102b36678 in undo main.c:1269
    #4 0x102b27c4c in execute main.c:1335
    #5 0x102b225bc in editor main.c:1432
    #6 0x102b222d8 in LLVMFuzzerTestOneInput main.c:1454
    #7 0x102b5fa3c in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) FuzzerLoop.cpp:611
    #8 0x102b5f21c in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) FuzzerLoop.cpp:514
    #9 0x102b60898 in fuzzer::Fuzzer::MutateAndTestOne() FuzzerLoop.cpp:757
    #10 0x102b614d0 in fuzzer::Fuzzer::Loop(std::__1::vector<fuzzer::SizedFile, fuzzer::fuzzer_allocator<fuzzer::SizedFile> >&) FuzzerLoop.cpp:895
    #11 0x102b51bb4 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) FuzzerDriver.cpp:906
    #12 0x102b794f8 in main FuzzerMain.cpp:20
    #13 0x102d290f0 in start+0x204 (dyld:arm64+0x50f0)
    #14 0x8a16fffffffffffc  (<unknown module>)

0x000116f40c72 is located 0 bytes to the right of 2-byte region [0x000116f40c70,0x000116f40c72)
allocated by thread T0 here:
    #0 0x10301394c in wrap_realloc+0x94 (libclang_rt.asan_osx_dynamic.dylib:arm64+0x3f94c)
    #1 0x102b2e750 in insert main.c:407
    #2 0x102b37bac in reverse_action main.c:1261
    #3 0x102b36678 in undo main.c:1269
    #4 0x102b27c4c in execute main.c:1335
    #5 0x102b225bc in editor main.c:1432
    #6 0x102b222d8 in LLVMFuzzerTestOneInput main.c:1454
    #7 0x102b5fa3c in fuzzer::Fuzzer::ExecuteCallback(unsigned char const*, unsigned long) FuzzerLoop.cpp:611
    #8 0x102b5f21c in fuzzer::Fuzzer::RunOne(unsigned char const*, unsigned long, bool, fuzzer::InputInfo*, bool, bool*) FuzzerLoop.cpp:514
    #9 0x102b60898 in fuzzer::Fuzzer::MutateAndTestOne() FuzzerLoop.cpp:757
    #10 0x102b614d0 in fuzzer::Fuzzer::Loop(std::__1::vector<fuzzer::SizedFile, fuzzer::fuzzer_allocator<fuzzer::SizedFile> >&) FuzzerLoop.cpp:895
    #11 0x102b51bb4 in fuzzer::FuzzerDriver(int*, char***, int (*)(unsigned char const*, unsigned long)) FuzzerDriver.cpp:906
    #12 0x102b794f8 in main FuzzerMain.cpp:20
    #13 0x102d290f0 in start+0x204 (dyld:arm64+0x50f0)
    #14 0x8a16fffffffffffc  (<unknown module>)

SUMMARY: AddressSanitizer: heap-buffer-overflow main.c:189 in move_left
Shadow bytes around the buggy address:
  0x007022e08130: fa fa fd fa fa fa fd fa fa fa fd fd fa fa fa fa
  0x007022e08140: fa fa fa fa fa fa fd fa fa fa fa fa fa fa fd fa
  0x007022e08150: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007022e08160: fa fa fd fa fa fa fa fa fa fa fd fa fa fa fd fa
  0x007022e08170: fa fa fa fa fa fa fd fa fa fa fd fa fa fa fd fa
=>0x007022e08180: fa fa fd fa fa fa fd fa fa fa fa fa fa fa[02]fa
  0x007022e08190: fa fa fa fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007022e081a0: fa fa fd fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007022e081b0: fa fa 04 fa fa fa fd fa fa fa fd fa fa fa fd fa
  0x007022e081c0: fa fa fd fa fa fa fd fa fa fa 00 fa fa fa fd fa
  0x007022e081d0: fa fa fd fa fa fa fa fa fa fa fd fa fa fa fa fa
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
==58956==ABORTING
MS: 1 CrossOver-; base unit: 511a5fd4707ac55efabed80447aa2932a6e4a86c
0x9,0x6c,0xd1,0xd1,0x7f,0x21,0xd,0xff,0x86,0xff,0x2c,0x2d,0x54,0x2c,0x77,0x9,0x61,0x77,0x72,0x9,0x77,0x0,0x9,0x9,0x0,0x0,0x0,0x25,0xd1,0x7b,0x72,0x77,0x9,0x61,0x77,0x45,0x78,0x72,0x28,0x78,0x8,0x70,0x6f,0x72,0x74,0x7f,0x7f,0xd3,0x76,0x85,0x7a,0xfd,0x72,0x77,0x9d,0x46,0x0,0x7a,0x7a,0x77,0x9d,0x46,0xfd,0x72,0x77,0x9d,0x46,0x0,0x7a,0x7a,0x33,0x78,0x72,0x7a,0x72,0x77,0x67,0x67,0x7a,0x67,0x67,0x7a,0x77,0x9d,0x46,0xfd,0x72,0x77,0x9d,0x46,0x0,0x7a,0x7a,0x33,0x78,0x72,0x7a,0x7a,0x35,0x78,0x92,0x81,0x0,
\x09l\xd1\xd1\x7f!\x0d\xff\x86\xff,-T,w\x09awr\x09w\x00\x09\x09\x00\x00\x00%\xd1{rw\x09awExr(x\x08port\x7f\x7f\xd3v\x85z\xfdrw\x9dF\x00zzw\x9dF\xfdrw\x9dF\x00zz3xrzrwggzggzw\x9dF\xfdrw\x9dF\x00zz3xrzz5x\x92\x81\x00
artifact_prefix='./'; Test unit written to ./crash-e26e7bb6538311c641076a3de56cd36a03b67851
Base64: CWzR0X8hDf+G/ywtVCx3CWF3cgl3AAkJAAAAJdF7cncJYXdFeHIoeAhwb3J0f3/TdoV6/XJ3nUYAenp3nUb9cnedRgB6ejN4cnpyd2dnemdnenedRv1yd51GAHp6M3hyeno1eJKBAA==
zsh: abort      


	// const char* str = "ڗgrwet\x97rwzr";

	//  <unicode>    g      <exit>    word-left      insertmode      <unicode>    <exit>     undo    cut



















  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
==63825==ABORTING
MS: 2 ChangeASCIIInt-CopyPart-; base unit: 68cfcc9ba8ac3a3d18faeaee33f6f960c50db109
0xa,0x89,0xff,0x6e,0x73,0x74,0x92,0x72,0x77,0x58,0x50,0x43,0x9,0xa,0xa,0xa,0x73,0x74,0x92,0x72,0x77,0x58,0x50,0x43,0x9,0xff,0x3d,0xff,0xff,0xd,0x72,0x77,0x5a,0x7a,0x77,0xac,0x78,0x21,0x7a,0x5a,0x7a,0x77,0xac,0x78,0x21,0x7a,0x77,0xac,0x78,0x33,0x0,0x0,0x73,0x74,0x92,0x72,0x77,0x58,0xe2,0x50,0x43,0x9,0xff,0xff,0x77,0xac,0x78,0x33,0x0,0x0,0x73,0x74,0x92,0x72,0x77,0x58,0xe2,0x50,0x43,0x9,0xff,0xff,0xff,0xff,0xd,0x72,0x77,0x5a,0x7a,0x77,0xac,0x78,0x21,0x7a,0x77,0xac,0x78,0x32,0x0,0x0,0x0,0x0,0xac,0x78,0x21,0x7a,0x7f,0xbe,
\x0a\x89\xffnst\x92rwXPC\x09\x0a\x0a\x0ast\x92rwXPC\x09\xff=\xff\xff\x0drwZzw\xacx!zZzw\xacx!zw\xacx3\x00\x00st\x92rwX\xe2PC\x09\xff\xffw\xacx3\x00\x00st\x92rwX\xe2PC\x09\xff\xff\xff\xff\x0drwZzw\xacx!zw\xacx2\x00\x00\x00\x00\xacx!z\x7f\xbe
artifact_prefix='./'; Test unit written to ./crash-b019392511ce17e97e5710b3e737866f804f99f3
Base64: Con/bnN0knJ3WFBDCQoKCnN0knJ3WFBDCf89//8NcndaeneseCF6Wnp3rHgheneseDMAAHN0knJ3WOJQQwn//3eseDMAAHN0knJ3WOJQQwn/////DXJ3Wnp3rHgheneseDIAAAAArHghen++
zsh: abort      ./editor







	// const char* str = "\x2d\x2c\x72\x77\x61\x74\x7f\x7f\xd3\x76\x85\x72\x77\x72\x7a\x7a";






	// const char* str = "ttrwat\x7f\x7fgv\x85rwrzz";


	//0     char   char   <exit>    

	//1     anchor   insertmode    

	//0     <backspace>   <backspace>     char    char    <unicode>    <exit>

	//1     cut     undo   undo 








	// const char* str = "ttrwt\x92rwXxtrwxz";  

	//             char   char               exit   

	//        insertmode   unicode           exit    

	//        alternatedown    redo    

	//        insertmode                     exit   

	//        redo   undo 


*/








/*




static inline void move_left(bool change_desired) {
	if (not lcc) {
		if (not lcl) return;
		lcl--; 
		lcc = lines[lcl].count;
visual_line_up: compute_vcc();
visual_just_line_up: vcl--;
		if (vsl) vsl--;
		else if (vol) vol--;
		if (vcc > window_columns - line_number_width) { 
			vsc = window_columns - line_number_width; 
			voc = vcc - vsc; 
		} else { 
			vsc = vcc;
			voc = 0; 
		}
	} else {
		nat save_lcc = lcc;
		do lcc--; while (lcc and zero_width(lines[lcl].data[lcc]));
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



nat line = 0, col = 0; 
	// this is not correct!  ---> we need to compute the logical version of VOC and VOL, and start here.

	nat vl = vol, vc = voc; 
	// i think these should be initialized to    VOL and VOC,       i think. 

	nat sl = 0, sc = 0; 



*/






	// we cannot start out at zero, for the vc and vl,  or the line and col. 

	// we actually need to try to START AT THE CURSOR,
	//	(because we know that IT  is    AT LEAST     pretttttttyyyyy cloooossssseeeeee    to what we want. i think. 
	
	// and so, we can actually start at the cursor, and try to work backwards, (or search backwards, i mean)
	// to try to find the Logical Column and Logical Line that corresponds with VO(L/C) (Visual Origin line and col),  
	//  which is the crucial piece of information we need to start rendering. we need to know where in the text to 
	// start printing out text from, and we know that position must be where ever VOL lands, in logical coordinates. 

	// so yeah, thats how we get the performance of vim, in our scrolling, and display rendering, i think. that should do it. 


	// so how do we compute the logical coordinates of VO(L/C)?   (VOL and VOC)



	// this crucially releis on using the already existing piece of info,      vcc  and vcl 

	// these two variables are          the visual cursor line/col

	//      and they represent    the position of the cursor, in visual coordinates.  which the VOL and VOC are also based in. 

	//      and so, like, we know what  the visual coordinates of the logical cursor are now. 


	/// so heres the train of translations we need to do:

	//         logical/visual cursor --->  move backwards char-wise in logical space, and keep track of your visual coords while doing it, (utilize "compute_vcc()"!!!). once you notice that your visual coords hit "VOL" and "VOC",   then note down what resultant logical coords you had to go to in order to reach this visual position, from the cursor. 

	// that is what you need to initialize the variables "line" and "col" to. 


	// additionally,    "vc" and "vl"       should now be initialized to   VOC    and  VOL.      which makes sense. 

	// and sc and sl are still initialized to 0. thats totally good. 


	// yay! we figured it out, basically. cool. moving backwards is always more computationally expensive than moving forward, i think, but this is definitely the right way to go, i think. yayyyyyyyyyyy

	//  okay turns out i think we can literally use "move_left" to do most of the work for us!! as long as we revert back everything the way we found it lol. 







	// int n = 0;
	// if (ioctl(0, I_NREAD, &n) == 0) {
 //    		// we have exactly n bytes to read
	// 	sprintf(message, "info we have exactly %d bytes to read.",n);
	// }

	// n = 1;
	// if (ioctl(0, I_NREAD, &n) == 0 && n > 0) {
 //    		// we have exactly n bytes to read
	// 	sprintf(message, "info we have exactly %d bytes to read.",n);
	// }

	// n = 2;
	// if (ioctl(0, I_NREAD, &n) == 0 && n > 0) {
 //    		// we have exactly n bytes to read
	// 	sprintf(message, "info we have exactly %d bytes to read.",n);
	// }

	// n = 3;
	// if (ioctl(0, I_NREAD, &n) == 0 && n > 0) {
 //    		// we have exactly n bytes to read
	// 	sprintf(message, "info we have exactly %d bytes to read.",n);
	// }








//	n	n	n	n	n	n	n	n	n	n	n	n	n	n	n	n	n	n		
//ntntntntnttntnntntntntntnttntnntntntntntnttntnntntntntntnttntnntntntntntnttntnntntntntntnttntnntntntntntnttntnntntntntntnttntnntntntntntnttntnegegett	








/*




			//todo: simplify this?...


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













*/








//vt100 reference material: 
//
//    https://www2.ccs.neu.edu/research/gpc/VonaUtils/vona/terminal/vtansi.htm
//











// nat after_vcc = compute_custom_vcc(lcc);
			// nat before_vcc = compute_custom_vcc(save_lcc);
			// if (after_vcc > before_vcc) after_vcc = 0;      
			// =0, because:  to put back after to be on the same line. ie, before it wrapped back to the previous line.   
			// if "(after > before)", then it must be that our after_vcc is on the line above, 
			//                        and before_vcc are on the line below
			// ashtLNNN	ashoetn
			// compute_custom_vcc(lcc); 


			// int n = 0;
			// n++;



/*


	big bug with tabs and wraping:





okay, so i thought i figured out the bug, with this, and got the correct solution, and when i run it in the debugger, it seems to run pretty good, and do the correct thing, when going back on a tab that is being wrapped because of the wrap width, 

			but then i ran it in release mode, and it still goes into an infinite loop... 


				so i am not sure whats happening..... i think it has to do with the other movement commands?... not sure though... 

						hmmm


		








	editor`::__sanitizer_cov_trace_const_cmp8() at FuzzerTracePC.cpp:495:1 [opt]
    	frame #1: 0x000000010000d298 editor`move_left(change_desired=false) at main.c:241:10
    	frame #2: 0x0000000100008d64 editor`display at main.c:603:66
    	frame #3: 0x0000000100001ef8 editor`editor(_input="\t\t\t\t\t\xc1", _input_count=6) at main.c:1544:3
    	frame #4: 0x0000000100001dcc editor`LLVMFuzzerTestOneInput(input="\t\t\t\t\t\xc1", size=6) at main.c:1575:2


	




			if (vcc + 1 <= wrap_width) {
				vcc++; 
				if (vsc < window_columns - 1 - line_number_width) vsc++; 
				else voc++;
			} else {
				vcl++; vcc = 0; voc = 0; vsc = 0;
				if (vsl < window_rows - 1 - show_status) vsl++; 
				else vol++;
			}





							if (wrap_width <= tab_width) break; 
							
							if (vcc >= wrap_width) {
								vcl++; vcc = 0; voc = 0; vsc = 0;
								if (vsl < window_rows - 1 - show_status) vsl++; 
								else vol++;
							}










// if (vc >= wrap_width) goto next_visual_line;      // do a dictoch on "<=ww" here and in tab section too.

// delete me 









			if (vcc + (tab_width - vcc % tab_width) <= wrap_width) {
				do {
					vcc++; if (vsc < window_columns - 1 - line_number_width) vsc++; else voc++;
				} while (vcc % tab_width); 
			} else {
				vcl++; vcc = 0; voc = 0; vsc = 0;
				if (vsl < window_rows - 1 - show_status) vsl++; 
				else vol++;
			}




			if (vcc + 1 <= wrap_width) {
				vcc++; 
				if (vsc < window_columns - 1 - line_number_width) vsc++; 
				else voc++;
			} else {
				vcl++; vcc = 0; voc = 0; vsc = 0;
				if (vsl < window_rows - 1 - show_status) vsl++; 
				else vol++;
			}



*/

