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


========================= REMAINING REQ. FEATURES ========================





IMPL.
===========

	[ ] 	- make the editor use less globals.
	[ ]	- make textbox use a buffer.
	[ ]	- make the anchor system be stateful, and utilize "recent-cursor" behaviour.

	[ ]  	- make the editor use C11 Atomics instead of mutexes!!!


FILE VIEW:

===========
	
	[ ]	- seperate out path and filename, when saving. 
	[ ]	- - seperate out moving a file, and renaming a file. 
	

CONFIG FILE:
===========
	[ ] 	- read and write to config file to store parameters
	[ ] 	- be able to adjust config parameters within editor using "set" command-line command.


UI:
===========
	[ ] 	- be able to adjust config parameters within editor using "set" command-line command.
	


DOC:
==================

	[ ] 	- document the new keybinding of the editor in a manual_v2.txt file!

------------------- bugs ----------------------


[bug]			- add word wrap support. 


[bug]			- rework the way the      wrap_width        disabling/dynamic adjusting works. 
***


[bug/feature]		- scrolling down is not very smooth. we should be using the terminal scrolling mode. 
			use the escape code sequences 
			<ESC>[r    to enable scrolling on the whole screen
			<ESC>[{start};{end}r   to enable scrolling on a range of rows of the screen.
			<ESC>D    to scroll down
			<ESC>M   to scroll up




---------------- features ---------------



[feature]	- implement a history for the textbox? oh! just by using multiple lines. of course. beautiful. 
			(note: wrap width is infinity for the textbox)


	

*
[feature]	- determine file format of config file, as a list of commands, that set the values? yes.
		- init parameters from config file. 


*
[feature]	- determine language isa for my programming language. 
		- implment my programming language in it. 



[optional]	- (BIG) redesign how we are drawing the screen, to be based on drawing text that is isolated by 
			whitespace, and jumping the (invisible) cursor around the screen, to draw those areas.  
			and clearing the screen before each frame of course,






















------------------- done/fixed -----------------


[FIXED]		- tab width and wrap width major bugs.

[FIXED]		- the performance of our display function is not good. we are currently making an n^2 algorithm in order to draw the screen.


[FIXED]	 	- solve the two slow-unit-fffffff test cases that the fuzzer found. 


[FIXED]		- test how the editor handles unicode characters with the new tab/wrap/display code. 

[FIXED]		- test if the confirmed() code  (ie, overwriting, and discarding file stuff) works.


[FIXED]		- test if the copy paste code is still working. 

[FIXED]		- test if word movement is working

	
SAFETY: [x]	- make an autosave feature, that saves the file to an autosave directory every time it changes. i think. or at least, when theres downtime. yeah. dont do it while typing, obviously. dont wait until user saves to write the file to SOMEWHERE in disk. we need to not loose anything. 


FILE VIEW:
	[x]	- simple file viewer
	[x]	- tab completion for file names



DONE: [optional]	x- rebind the keybindings of the editor. 
**


DONE: [feature]		x - add tab completion for file names, and a small file veiwer, when saving/renaming. 
***

[x] 	- implement a command line, that parses arguments via whitespace..? 
			single char commands only, for built in ones, i think? 













	// todo:   make the textbox actually just a full-on buffer  in its own right
	//         and then thus allow for  different modes,   and also undo redo,  etc. just, dont allow for upward movement, or new lines, though. simple as that.    alsooo keep the way we are rendering the textbox. that can stay. but the actual logic of the textbox should just use the already existing buffer logic. not use its own.  so yeah.   and then we only use the first line, of this "textbox buffer", of course.











	// also i want to implement word wrapping!!!!









	// 	TODO: 
    	//
	// 		redo this behaviour. make it so that you can explicitly disable wrapwidth (not just make it sudo-infinity), 
	// 		or explicitly make it wrap to screen width, and dynamically adjust as the screen width changes!!!
	// 









	todo:   add     restrict    to all pointers that we use!

	





*/



#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdatomic.h>
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






#define is_fuzz_testing		0

#define memory_safe 	1
#define fuzz 		is_fuzz_testing
#define use_main    	not is_fuzz_testing

typedef ssize_t nat;


// todo: make these part of the config file / parameters.

static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";

static const nat autosave_frequency = 8;     // in seconds




enum action_type {
	no_action,
	insert_action,
	delete_action,
	paste_action,
	cut_action,
	anchor_action,
};

struct line { 
	char* data; 
	nat count, capacity; 
};



// if ww_fix,    then ww = 0,  ie, same as window width.     

struct buffer {
	struct line* lines;
	struct action* actions;

	nat     saved, mode, autosaved, selecting, ww_fix,    
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
	nat     saved, autosaved, selecting, ww_fix,

		lcl, lcc, 	vcl, vcc,  	vol, voc, 
		vsl, vsc, 	vdc,    	
		lal, lac, 	lrl, lrc,

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








// these three will be glogals, actually.        i dont really want to thread these through the call stack lol. 

static size_t fuzz_input_index = 0;
static size_t fuzz_input_count = 0;
static const uint8_t* fuzz_input = NULL;




static nat 
	window_rows = 0, 
	window_columns = 0;     // i think these will stay as globals. they arent part of a buffer. 








					
//static struct textbox tb = {0};      // this should be just a particular buffer, actually. 
				     // its just simpler that way. but it will be rendered differently.










// these 4 will be variables in main, i think.     


static struct buffer* buffers = NULL;

static nat buffer_count = 0, active_index = 0; // these only need to be passed to the functions which perform a buffer switch.

static struct buffer buffer = {0};   // the current buffer.    this will be named   "_"   probably. 

static struct buffer textbox = {0};  // the textbox. 








// all of these will actually be put into the current buffer variable "buffer"!    i think. and "buffer" will be named    "this"...?


static struct line* lines = NULL;

static struct action* actions = NULL;

static nat 
	count = 0, capacity = 0, 
	line_number_width = 0, tab_width = 0, wrap_width = 0, 
	show_status = 0, 
	lcl = 0, lcc = 0,  lal = 0, lac = 0,
	vcl = 0, vcc = 0,  vol = 0, voc = 0,  vsl = 0, vsc = 0, 
	vdc = 0,
	head, action_count;

static char message[4096] = {0};

static char filename[4096] = {0};

static char user_selection[4096] = {0};    // rename this 	selected_file

static char current_path[4096] = {0};      // rename this to     cwd








static pthread_mutex_t mutex;		// this needs to be global..? i think. 









static inline bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2;  }
static inline bool visual(char c) { return not zero_width(c); }
static inline bool file_exists(const char* f) { return access(f, F_OK) != -1; }

/*

static inline bool is_regular_file(const char *path) {
	struct stat s;
	stat(path, &s);
	return S_ISREG(s.st_mode);
}

*/


static inline nat unicode_strlen(const char* string) {
	nat i = 0, length = 0;
	while (string[i]) {
		if (visual(string[i])) length++;
		i++;
	}
	return length;
}

static inline bool equals(const char* s1, const char* s2) {
	if (strlen(s1) != strlen(s2)) return false;
	else return not strcmp(s1, s2);
}

static inline bool is_directory(const char *path) {
	struct stat s;
	if (stat(path, &s)) return false;
	return S_ISDIR(s.st_mode);
}

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





/*
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
/// consolidate these two functions:
*/

static inline nat compute_vcc(nat _unused__unused__unused) {
	v = 0;
	for (nat c = 0; c < lcc; c++) {
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
}

static inline void move_left() {
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
}

static inline void move_right() {

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
	while (vcc and vcl > line_target) move_left(); 
	do move_left(); while (vcc > vdc and vcl == line_target);
	if (vcc > window_columns - line_number_width) { vsc = window_columns - line_number_width; voc = vcc - vsc; } 
	else { vsc = vcc; voc = 0; }
}

static inline void move_down() {
	nat line_target = vcl + 1;
	while (vcl < line_target) { 
		if (lcl == count - 1 and lcc == lines[lcl].count) return;
		move_right();
	}
	while (vcc < vdc and lcc < lines[lcl].count) {

		if (lines[lcl].data[lcc] == '\t' and vcc + (tab_width - (vcc % tab_width)) > vdc) return;

		// TODO: ^ WHAT IS THIS!?!?!?

		move_right();
	}
}

static inline void jump_line(nat line) {
	while (lcl < line and lcl < count) move_down();
	while (lcl > line and lcl) move_up();
}

static inline void jump_column(nat column) {
	while (lcc < column and lcc < lines[lcl].count) move_right();
	while (lcc > column and lcc) move_left();
	vdc = vcc;
}

static inline void jump_to(nat line, nat column) {
	jump_line(line);
	jump_column(column);
}

static inline void move_begin() {
	while (vcc) move_left();
	vdc = vcc;
}

static inline void move_end() {
	while (lcc < lines[lcl].count and vcc < wrap_width) move_right(); 
	vdc = vcc;
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
	do move_left();
	while (not(
		(not lcl and not lcc) or (
			(lcc < lines[lcl].count and isalnum(lines[lcl].data[lcc]))  and 
			(not lcc or not isalnum(lines[lcl].data[lcc - 1]))
		)
	));
	vdc = vcc;
}

static inline void move_word_right() {
	do move_right();
	while (not(
		(lcl >= count - 1 and lcc >= lines[lcl].count) or (
			(lcc >= lines[lcl].count or not isalnum(lines[lcl].data[lcc]))  and 
			(lcc and isalnum(lines[lcl].data[lcc - 1]))
		)
	));
	vdc = vcc;
}

static inline void record_logical_state(struct logical_state* pcond_out) {
	struct logical_state* p = pcond_out; 

	p->saved = buffer.saved;
	p->autosaved = buffer.autosaved;
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
	buffer.autosaved = p->autosaved;
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
	else { move_right(); vdc = vcc; }

	buffer.saved = false;
	buffer.autosaved = false;
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
		move_left(); vdc = vcc;
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
		move_left();
		vdc = vcc;
		
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
	buffer.autosaved = false;
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


	//if (not wrap_width) wrap_width = (window_columns - 1) - (line_number_width);

}


static inline void display() {


	static char* screen = NULL;   //

	adjust_window_size(screen);


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
		move_left();
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

		nat status_length = sprintf(screen + length, " %ld %ld %ld %ld %ld %s %c%c %s",
			buffer.mode, 
			active_index, buffer_count,
			lcl, lcc, 

			filename, 
			
			buffer.saved ? 's' : 'e', 

			buffer.autosaved ? ' ' : '*',

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

/*
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
*/

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

static inline void clear_above_textbox() {
	nat length = sprintf(screen, "\033[%ld;1H\033[K", window_rows - 1);
	if (not fuzz) 
		write(1, screen, (size_t) length);
}






static inline void prompt(const char* prompt_message, nat color, char* out, nat out_size) {


	tb.prompt_length = unicode_strlen(prompt_message);
	do {
		adjust_window_size();
		textbox_display(prompt_message, color);
		char c = 0;
		
		if (fuzz) {
			if (fuzz_input_index >= fuzz_input_count) break;
			c = (char) fuzz_input[fuzz_input_index++];	
		} else read(0, &c, 1);

		if (c == '\r' or c == '\n') break;
		else if (c == '\t') {  // tab complete :   simply paste the user selection on tab?...
			
			const nat path_length = (nat) strlen(user_selection);
			for (nat i = 0; i < path_length; i++) 
				textbox_insert(user_selection[i]);
			
		}
		else if (c == 27 and stdin_is_empty()) { tb.count = 0; break; }
		else if (c == 27) {

			if (fuzz) {
				if (fuzz_input_index >= fuzz_input_count) break;
				c = (char) fuzz_input[fuzz_input_index++];	
			} else read(0, &c, 1);

			if (c == '[') {

				if (fuzz) {
					if (fuzz_input_index >= fuzz_input_count) break;
					c = (char) fuzz_input[fuzz_input_index++];	
				} else read(0, &c, 1);

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
		char response[4096] = {0};
		prompt(prompt_message, buffer.alert_prompt_color, response, sizeof response);
		clear_above_textbox();

		if (equals(response, yes_action)) return true;
		else if (equals(response, no_action)) return false;
		else if (equals(response, "")) return false;
		else print_above_textbox(invalid_response, buffer.default_prompt_color);

		if (fuzz) return true;
	}
}

static inline void store_current_data_to_buffer() {
	if (not buffer_count) return;

	const nat b = active_index;
	
	buffers[b].saved = buffer.saved;
	buffers[b].autosaved = buffer.autosaved;
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
	buffer.autosaved = this.autosaved;
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

static inline void zero_registers() {    // does this need to exist?... 

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
	buffer.autosaved = true;
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
	buffer.autosaved = true; 
	buffer.mode = 1; 
	move_top();
	strcpy(filename, given_filename);
	store_current_data_to_buffer();

	sprintf(message, "read %lub", length);
}

static inline void generate_hex_string(char id[32]) {
	sprintf(id, "%08x%08x", rand(), rand());
}

static inline void emergency_save_to_file() {
	
	if (fuzz) return;

	char dt[16] = {0};
	get_datetime(dt);

	char id[32] = {0};
	generate_hex_string(id);
	
	char local_filename[4096] = {0};
	sprintf(local_filename, "EMERGENCY_FILE_SAVE__%s__%s__.txt", dt, id);

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

	printf("interrupt: emergency wrote %lldb;%ldl to %s\n\r", bytes, count, local_filename);
}

static inline void emergency_save_all_buffers() {

	store_current_data_to_buffer();

	nat saved_active_index = active_index;
	for (int i = 0; i < buffer_count; i++) {
		
		active_index = i;
		load_buffer_data_into_registers();
		emergency_save_to_file(); 

		sleep(1);
	}
	
	active_index = saved_active_index;
	load_buffer_data_into_registers();
}


static inline void autosave() {

	if (fuzz) return;

	char dt[16] = {0};
	get_datetime(dt);

	char local_filename[4096] = {0};
	sprintf(local_filename, "%sautosave_%s_.txt", autosave_directory, dt);


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

	buffer.autosaved = true;
}

static void handle_signal_interrupt(int code) {

	if (fuzz) exit(1);

	printf(	"interrupt: caught signal SIGINT(%d), "
		"emergency saving...\n\r", 
		code);

	srand((unsigned)time(NULL));
	emergency_save_all_buffers();

	printf("press '.' to continue running process\n\r");
	int c = getchar(); 
	if (c != '.') exit(1);
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

	static nat scroll_counter = 0;

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
		else if (c == 'C') { move_right(); vdc = vcc; }
		else if (c == 'D') { move_left(); vdc = vcc; }

		//  TODO:   i need to completely redo the way that we scroll the screen, using mouse/trackpad scrolling. 

		else if (c == 'M') {

			read(0, &c, 1);

			if (c == 97) {

				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "scroll reporting: [%d:%d:%d]", c, str[0], str[1]);

				if (not scroll_counter) {
					move_down();
				}
				scroll_counter = (scroll_counter + 1) % 4;
				

			} else if (c == 96) {

				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "scroll reporting: [%d:%d:%d]", c, str[0], str[1]);

	
				if (not scroll_counter) {
					move_up();
				}
				scroll_counter = (scroll_counter + 1) % 4;
				
			} else {
				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y

				//sprintf(message, "mouse reporting: [%d:%d:%d].", c, str[0], str[1]);
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
	nat s_capacity = 256, length = 0, c = 0;

	lac = lcc; lal = lcl; // todo: why are we doing this here!?

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
 	lac = lcc; lal = lcl;
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

static void insert_string(const char* string, nat length) {
	struct action new = {0};
	record_logical_state(&new.pre);

	for (nat i = 0; i < length; i++) 
		insert((char) string[i], 0);
	
	record_logical_state(&new.post);
	new.type = paste_text_action;
	new.text = strndup(string, (size_t) length);
	new.length = length;
	create_action(new);
}

static inline void insertdt() {
	char datetime[16] = {0};
	get_datetime(datetime);
	insert_string(datetime, 14);
}


/*
static void insert_printable_string(const char* string, nat length) {
	struct action new = {0};
	record_logical_state(&new.pre);

	for (nat i = 0; i < length; i++) {
		if (isprint((char) string[i])) insert((char) string[i], 0);
	}
	
	record_logical_state(&new.post);
	new.type = paste_text_action;
	new.text = strndup(string, (size_t) length);
	new.length = length;
	create_action(new);
}
*/

/*
static void insert_cstring(const char* string) { insert_string(string, (nat) strlen(string)); }
*/

static inline void cut_selection() {
	if (lal < lcl) goto anchor_first;
	if (lcl < lal) goto cursor_first;
	if (lac < lcc) goto anchor_first;
	if (lcc < lac) goto cursor_first;
	return;
cursor_first:;
	nat line = lcl, column = lcc;
	while (lcl < lal or lcc < lac) move_right();
	lal = line; lac = column;
anchor_first:
	while (lal < lcl or lac < lcc) delete(0);
}

static inline void cut() { 
	struct action new = {0};
	record_logical_state(&new.pre);
	nat deleted_length = 0;
	char* deleted_string = get_selection(&deleted_length);
	cut_selection();
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

static inline void run_shell_command(const char* command) {
	FILE* f = popen(command, "r");
	if (not f) {
		sprintf(message, "error: could not run command \"%s\": %s\n", command, strerror(errno));
		return;
	}

	char line[4096] = {0};
	nat length = 0;
	char* output = NULL;

	while (fgets(line, sizeof line, f)) {
		const nat line_length = (nat) strlen(line);
		output = realloc(output, (size_t) length + 1 + (size_t) line_length); 
		sprintf(output + length, "%s", line);
		length += line_length;

		// sprintf(message, "LINE(len=%ld): ll=%ld", length, line_length);
	}
	pclose(f);
	lal = lcl; lac = lcc;
	sprintf(message, "output %ldb", length);
	insert_string(output, length);
}



// note:  for commands that output to stderr,   append   	 2>&1 		to the end of the command.

static inline void prompt_run() {
	char command[4096] = {0};
	prompt("run: ", 85, command, sizeof command);
	if (not strlen(command)) { sprintf(message, "aborted run"); return; }
	sprintf(message, "running: %s", command);
	run_shell_command(command);
}


static inline void replay_action(struct action a) {
	require_logical_state(&a.pre);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action or a.type == paste_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_text_action) cut_selection();
	else sprintf(message, "error: unknown action");
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {
	require_logical_state(&a.post);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_text_action) {
		while (lcc > a.pre.lcc or lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else sprintf(message, "error: unknown action");
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

static inline void undo_silent() {
	if (not head) return;
	reverse_action(actions[head]);
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

static inline void alternate_incr() {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void alternate_decr() {
	if (actions[head].choice) actions[head].choice--;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void anchor() {

	// struct action new = {.type = anchor_action};

	// record_logical_state(&new.pre);

	// all this should do is just set   STATE_should_recent_anchor = false;      thats it. 

	// lal = lcl; lac = lcc;    // it shouldnt do this. i think...?


	// sprintf(message, "anchor %ld %ld", lal, lac);

	// record_logical_state(&new.post);
	// create_action(new);
}

static inline void recalculate_position() {
	nat save_lcl = lcl, save_lcc = lcc;
	move_top();
	adjust_window_size();
	jump_to(save_lcl, save_lcc);
}

static inline void open_directory() {
	
	DIR* directory = opendir(current_path);
	if (not directory) { 
		sprintf(message, "couldnt open path=%s, reason=%s", current_path, strerror(errno));	
		return;
	}

	struct dirent *e = NULL;

	nat length = (nat)strlen(current_path) + 2;
	char* menu = calloc((size_t) length + 1, sizeof(char));
	sprintf(menu, ":%s\n", current_path);
	
	while ((e = readdir(directory))) {

		char path[4096] = {0};
		strlcpy(path, current_path, sizeof path);
		strlcat(path, e->d_name, sizeof path);

		if (is_directory(path)) {
			menu = realloc(menu, sizeof(char) * ((size_t) length + 3 + strlen(e->d_name))); 
			sprintf(menu + length, "%s/\n", e->d_name);
			length += 2 + strlen(e->d_name);
		} else {
			menu = realloc(menu, sizeof(char) * ((size_t) length + 2 + strlen(e->d_name))); 
			sprintf(menu + length, "%s\n", e->d_name);
			length += 1 + strlen(e->d_name);
		}
	}

	closedir(directory);

	lal = lcl; lac = lcc;
	insert_string(menu, length);
	jump_to(lal, lac);	
}

static inline void change_directory() {

	char* selection = strndup(lines[lcl].data, (size_t) lines[lcl].count);

	if (equals(selection, "../") ) {
		if (not equals(current_path, "/")) {
			current_path[strlen(current_path) - 1] = 0;
			*(1+strrchr(current_path, '/')) = 0;
		} else {
			sprintf(message, "error: at root /");
		}
		
	} else if (equals(selection, "./") ) {
		// do nothing.

	} else {
		char path[4096] = {0};
		strlcpy(path, current_path, sizeof path);
		strlcat(path, selection, sizeof path);

		if (is_directory(path)) {
			strlcat(current_path, selection, sizeof current_path);
		} else {
			sprintf(message, "error: not a directory");
		}
	}
}

static inline void file_select() {
	char* line = strndup(lines[lcl].data, (size_t) lines[lcl].count);
	strlcpy(user_selection, current_path, sizeof user_selection);
	strlcat(user_selection, line, sizeof user_selection);
	free(line);
	sprintf(message, "selected: %s", user_selection);
}

static inline char** split(char* string, char delim, int* array_count) {

	int a_count = 0;
	char** array = NULL;
	int start = 0, i = 0;
	const int length = (int)strlen(string);

	for (; i < length; i++) {
		if (string[i] == delim) {
			string[i] = 0;
			if (strlen(string + start)) {
				array = realloc(array, sizeof(char*) * (size_t)(a_count + 1));
				array[a_count++] = strdup(string + start);
			}
			start = i + 1;
		}
	}

	if (strlen(string + start)) {
		array = realloc(array, sizeof(char*) * (size_t)(a_count + 1));
		array[a_count++] = strdup(string + start);
	}

	*array_count = a_count;
	return array;
}

static inline void execute(char c, char p) {

	if (buffer.mode == 0) {

		if (c == 'c' and p == 'h') { undo(); buffer.mode = 1; }
		else if (c == 27 and stdin_is_empty()) buffer.mode = 1;
		else if (c == 27) interpret_escape_code();
		else if (c == 127) delete(1);
		else if (c == '\r') insert('\n', 1);
		else insert(c, 1);

	} else if (buffer.mode == 1) {

		


		/*

			the idea is to rebind the keys in the editor 

		so that you cant accidentily delete the entire file. 

					thats really not going to be allowed. yes you can undo(),   but i dont want to resort to that. ever.



					so the idea is to make           cut()     which uses the anchor    

									to be very hard to press!

										ie, at leastttt a dual char keybind


				anddd



				i want to add a new thing


							which will be callled        cut last


						which uses the  "last" cursor


						which records the lasttt position of the cursor!


							very very very useful. 




		if you move there in one go,   then now we can synethesize stuff 


			super great



						now movement commands are super imporatnt now lol



						i think we need to figure out a way to have more of them



				i want to remove the           ek     keybinding, 



				and also just generally remove alot of the useless ones 



						like   top and bottom, kinda



							we can just use the mode2 for those?    maybe?



								 but yeah, you see now


					


				alsoooo




							i think i kinda want to remove the binding of 



									a



								i want to make that a dead stop actually, i think 



												probably lol



						so yeah


						very useful 





	ehh


		idk though 



						im still thinking about it 










2208217.174341:


		i think i just found a resolution to 

			the fact that i want:

				1. edits in my editor to be quick, and using the anchor isnt really quick 

				2. using the anchor as i have it right now, is bad because 
					you can accidently delete the file because anchor is zero to start with. not good. i hate that. 

				3. i want to use the most recent value that the cursor has. 

					


			yeah, that will be good. 


			i found the way i want to do it. 

			basically, instead of    anchor    setting the current position to the anchor, 


				it just toggles whether the anchor follows the cursor!

					thats going to be the way i do it


						or wait



								WAIT





					NO



						oh my gosh!!!



								we literally can just




						we can just make              THE ANCHOR



									BE 



							THE RECENT





					like, we we dont care about the anchors value, 


							then we 




						okay this is the right way to implement this


						yayyy
								i found it 



					i love this 





							so basically, we make the anchor    be used as the recent 


				MOST OF THE TIME



					but then 



							when ever you press    anchor()


								then now, we are NOT following the cursor    (or i guess, always lagging behind, because thats how recent is supposed to work)


					(usually we lag behind! in "non anchor mode"



						)        	and then when we want to drop our anchor down, 



										and stop lagging behind, 



									we say          anchor()'



									and that changes the state of the system 

										to be NOT changing lal lac   every single command

								but actually just 


			

	
						yeah i know that this is the right way to do this, now,  i think 

						it is just ergonomic,  safe,  and simple.



						very cool  very cool 




					but just implementing it is rather hard... hmmm....


					not sure how to do it 


					

	

	*/






		




		if (true/*STATE__should_recent_anchor*/) { lal = lcl; lac = lcc; }      // where do we do this?
	

	/*
		after every single movement command, i think. yeah. i think it needs to be done like that too.   yikes. 

			i think we just want to do it inside the actual movement commands themselves, actually. yeah. i think so. 

				okay, cool. 






	
*/
		

	
	// all anchor() does now, is just sets the anchor (?... no..?),   and then    sets  STATE_should_recent_anchor to be false.  and then when you do a cut or paste, or anything that uses anchor, it sets it to be 1 again. 
				

	
		if (c == ' ');

		// else if (c == 'l' and p == 'e') prompt_jump_line();         // unbind this?... yeah...
		// else if (c == 'k' and p == 'e') prompt_jump_column();       // unbind this?... hmm...
		// else if (c == 'd' and p == 'h') prompt_open();
		// else if (c == 'f' and p == 'h') create_empty_buffer();


		else if (c == 'l' and p == 'e') {}
		else if (c == 'k' and p == 'e') {}
		else if (c == 'i' and p == 'e') move_bottom();
		else if (c == 'p' and p == 'e') move_top();
		else if (c == 'n' and p == 'e') move_begin();
		else if (c == 'o' and p == 'e') move_end();

		else if (c == 'u' and p == 'e') alternate_decr();
		else if (c == 'r' and p == 'h') alternate_incr();

		else if (c == 'a' and p == 'h') move_to_previous_buffer();
		else if (c == 's' and p == 'h') move_to_next_buffer();

		else if (c == 'm' and p == 'h') copy();
		else if (c == 'c' and p == 'h') paste(); 
		
		else if (c == 't' and p == 'h') anchor();
		else if (c == 'd' and p == 'h') {}
		

		
		
		else if (c == 'a') {}
		else if (c == 'd') delete(1);
		else if (c == 'r') cut();

		else if (c == 't') buffer.mode = 0;
		else if (c == 'm') { buffer.mode = 2; goto command_mode; }

		else if (c == 'c') undo();
		else if (c == 'k') redo();

		else if (c == 'o') move_word_right(); 
		else if (c == 'l') move_word_left(); 

		else if (c == 'p') move_up();
		else if (c == 'u') move_down();
		else if (c == 'i') { move_right(); vdc = vcc; }
		else if (c == 'n') { move_left(); vdc = vcc; }

		else if (c == 's') save();

		else if (c == 'q') {
			if (buffer.saved or confirmed("discard unsaved changes", "discard", "no")) 
				close_active_buffer(); 
		}

		else if (c == 27 and stdin_is_empty()) {}
		else if (c == 27) interpret_escape_code();

	} else if (buffer.mode == 2) {

		command_mode: buffer.mode = 1;

		char string[4096] = {0};
		prompt(" •• ", 82, string, sizeof string);
		
		nat length = (nat) strlen(string);
		char* d = calloc((size_t) length + 1, sizeof(char));
		nat d_length = 0;

		for (nat i = 0; i < length; i++) {
			if ((unsigned char) string[i] < 33) continue;
			d[d_length++] = string[i];
		}

		d[d_length] = 0;
		int command_count = 0;
		char** commands = split(d, '.', &command_count);

		for (int i = 0; i < command_count; i++) {
			const char* command = commands[i];

			     if (equals(command, "donothing")) {}
			else if (equals(command, "datetime")) insertdt();
			else if (equals(command, "run")) prompt_run();
			else if (equals(command, "run")) prompt_run();
			else if (equals(command, "open")) prompt_open();
			else if (equals(command, "rename")) rename_file();
			else if (equals(command, "save")) save();
			else if (equals(command, "duplicate")) {/* duplicate(); */}
			else if (equals(command, "autosave")) autosave();
			else if (equals(command, "in")) { change_directory(); undo_silent(); open_directory(); }
			else if (equals(command, "changedirectory")) change_directory();
			else if (equals(command, "opendirectory")) open_directory();
			else if (equals(command, "selectfile")) file_select();
			else if (equals(command, "openfile")) { file_select(); open_file(user_selection); }
			else if (equals(command, "selection")) sprintf(message, "%s", user_selection);
			else if (equals(command, "where")) sprintf(message, "@ %s", current_path);
			else if (equals(command, "home")) { getcwd(current_path, sizeof current_path); 
								strlcat(current_path, "/", sizeof current_path); }
			else if (equals(command, "clearmessage")) memset(message, 0, sizeof message);
			else if (equals(command, "numbers")) show_line_numbers = not show_line_numbers;
			else if (equals(command, "status")) show_status = not show_status;
			else if (equals(command, "cut")) cut();
			else if (equals(command, "delete")) delete(1);
			else if (equals(command, "anchor")) anchor();
			else if (equals(command, "paste")) paste();
			else if (equals(command, "copy")) copy();
			else if (equals(command, "undo")) undo();
			else if (equals(command, "redo")) redo();
			else if (equals(command, "alternateincr")) alternate_incr();
			else if (equals(command, "alternatedecr")) alternate_decr();
			else if (equals(command, "moveright")) { move_right(); vdc = vcc; }
			else if (equals(command, "moveleft")) { move_left(); vdc = vcc; }
			else if (equals(command, "moveup")) move_up();
			else if (equals(command, "movedown")) move_down();
			else if (equals(command, "movewordright")) move_word_right();
			else if (equals(command, "movewordleft")) move_word_left();
			else if (equals(command, "movebegin")) move_begin();
			else if (equals(command, "moveend")) move_end();
			else if (equals(command, "movetop")) move_top();
			else if (equals(command, "movebottom")) move_bottom();
			else if (equals(command, "jumpline")) prompt_jump_line();
			else if (equals(command, "jumpcolumn")) prompt_jump_column();
			else if (equals(command, "new")) create_empty_buffer();
			else if (equals(command, "nextbuffer")) move_to_next_buffer();
			else if (equals(command, "previousbuffer")) move_to_previous_buffer();			
			else if (equals(command, "mode0")) buffer.mode = 0;
			else if (equals(command, "mode1")) buffer.mode = 1;
			else if (equals(command, "mode2")) buffer.mode = 2;

			else if (equals(command, "goto")) {}
			else if (equals(command, "branch")) {}
			else if (equals(command, "incr")) {}
			else if (equals(command, "setzero")) {}

			else if (equals(command, "commandcount")) sprintf(message, "command count = %d", command_count);

		
			else if (equals(command, "wrapresizetemp")) { wrap_width = 0; recalculate_position(); }
			else if (equals(command, "quit")) {
				if (buffer.saved or confirmed("discard unsaved changes", "discard", "no")) 
					close_active_buffer(); 

			} else {
				sprintf(message, "command not recognized: %s", command);
				break;
			}
		}

	} else buffer.mode = 1;
}

static inline char read_stdin() {
	char c = 0;
	if (fuzz) {
		if (fuzz_input_index >= fuzz_input_count) return 0;
		fuzz_input_index++;
		return (char) fuzz_input[fuzz_input_index - 1];
	}
	read(0, &c, 1);
	return c;
}

static void* autosaver(void* unused) {

	while (1) {
		sleep(autosave_frequency);
		pthread_mutex_lock(&mutex);
		if (not buffer_count) break;
		if (not buffer.autosaved) autosave();
		pthread_mutex_unlock(&mutex);
	}

	return NULL;
}

static inline void editor() {

	struct termios terminal;
	static pthread_t autosave_thread;

	if (not fuzz) {
		terminal = configure_terminal();
		write(1, "\033[?1049h\033[?1000h\033[7l", 20);
		buffer.needs_display_update = 1;
		pthread_mutex_init(&mutex, NULL);
		pthread_mutex_lock(&mutex);
		pthread_create(&autosave_thread, NULL, &autosaver, NULL);

	} 

	char p = 0, c = 0;

    	getcwd(current_path, sizeof current_path);
	strlcat(current_path, "/", sizeof current_path);
loop:
	// if (buffer.needs_display_update) 
	display();

	pthread_mutex_unlock(&mutex);
	c = read_stdin(); 
	pthread_mutex_lock(&mutex);
	if (fuzz and not c) goto done;

	//buffer.needs_display_update = 1;
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
		pthread_mutex_unlock(&mutex);
		pthread_detach(autosave_thread);
		pthread_mutex_destroy(&mutex);
	}
}


#if fuzz && !use_main

int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {
	create_empty_buffer();

	fuzz_input_index = 0; 
	fuzz_input = _input;
	fuzz_input_count = _input_count;

	editor();

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



































/*

	char buffer[4096] = {0};
	printf("input: ");
	fgets(buffer, sizeof buffer, stdin);
	buffer[strlen(buffer) - 1] = 0;


int array_count = 0;
	char* string = buffer;

	nat length = (nat) strlen(string);
	char* d = calloc((size_t) length + 1, sizeof(char));
	nat d_length = 0;

	for (nat i = 0; i < length; i++) {
		if ((unsigned char) string[i] < 33) continue;
		d[d_length++] = string[i];
	}

	d[d_length] = 0;

	char** array = split(d, '.', &array_count);

	printf("(%d)[ ", array_count);
	for (int i = 0; i < array_count; i++) {
		printf("\"%s\" ", array[i]);
	}

	puts("]\ndone!");




	char buffer[4096] = {0};
	printf("input: ");
	fgets(buffer, sizeof buffer, stdin);
	buffer[strlen(buffer) - 1] = 0;

	char* d = string;
	char* s = string;

	do while(isspace(*s)) s++; while(*d++ = *s++);



	int array_count = 0;
	char* string = buffer;
	char** array = split(d, ' ', &array_count);

	printf("(%d)[ ", array_count);
	for (int i = 0; i < array_count; i++) {
		printf("\"%s\" ", array[i]);
	}

	puts("]\ndone!");






	exit(1);













//while (i < length and string[i] == delim) ++i; 
//printf("B start=%d, i=%d\n", start, i);
//if (i < length) i++;
//printf("A start=%d, i=%d\n", start, i);
//printf("Q start=%d, i=%d\n", start, i);
//while (i < length and string[i] == delim) ++i; 
//string[i] = 0;
//	if (i < length) i++;
//	start = i;

//printf("B start=%d, i=%d\n", start, i);

*/




	// -----------------------------------------
	


	// keybingings to change still:

		//else if (c == 'E') show_status = not show_status;            // unbind this.  make this a written out command.
		//else if (c == 'N') show_line_numbers = not show_line_numbers;    // unbind this.  make this a written out command.

		//else if (c == 'F') prompt_open();      		// both bind, and  make this a written out command.
		//else if (c == 'S') rename_file();     				// unbind this. make this a written-out command. 

		//else if (c == '_') memset(message, 0, sizeof message);    	// unbind this. make this a written out command
		//else if (c == '\\') { wrap_width = 0; recalculate_position(); }  // make this part of the set command. 
	
		// im not sure what to do about these....

		//else if (c == '\r') menu_select();         // make this a written out command...
		//else if (c == '\"') menu_change();         // make this a written out command...
		//else if (c == ';')  menu_display();        // make this a written out command...

		//else if (c == '\'') { menu_change(); undo_silent(); menu_display(); }      // make this a written out command...

		// else if (c == '1') {insertdt();}        // make this a written out command...
		









			// todo:  implement a global history for the prompt textbox!!!!
			/*
					very important. 

							also, save it to the config file..? not sure... hmm.. yeah... 

							we should have a directory that we save all of our config files to. 

				*/	





