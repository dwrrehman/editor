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
// 	     edited on 2209036.110503
//	   debugged on 2209121.211839
// 	   

/*
	------------- bugs todo list ----------------
			


		- rework in_prompt variable.      make part of the buffer struct. 


		- find any other ways we can improve the runtime performance of the editor. 



		- do some performance testing, with large files!
		- - (disable autosaving!)




		- allow the user to disable autosaving.

		- allow the mode=2 commands to modify the anchor as well as the keybindings.

		-  get the editor down to only 1500 lines. i think its possible. 

		- remove unneccessary commands. 

		- make the set-param command as well. 
		




	------------- feature todo list ----------------


easy	**	- add the ww=0 ww_disable code everywhere.
					x < ww     to     x < ww or not ww

hard	***	- implment word wrapping. 

hard	***	- add CORRECT scrolling code. 

easy	*	- add mouse support!

easy	***	- split out location and filename when saving. 

easy	**	- implement programming lang interpter

		- implement a copy/paste history!?!  or mulitple copy/paste registers!


 ------------------ testing: ------------------------------

		- fuzz test for bugs  alot more 

		- test for visual bugs alot more 

		- make sure that all features are working as intended.




	x	- test anchoring system. make sure it works. 


	--------------- done --------------------------



***	x	- fix the huge performance bug in the editor..
			...performance is actually terrible right now. 


		x	- - decrease the numberof context switches. they are very expensive.
		x	- - make the status bar implemented more efficiently. 


		x	- - make the scratch buffer implemented more efficiently. 



		x	- display the *n buffer.
		x	- get the textbox working using *n.
		x	- get the status bar working, using *n.

			- add selecting/anchor/recent logic. (only using "lal/lac").




		x	- redo how to submit a response in the textbox. allow for new lines, somehow... i think.

		x	- make sn_rows part of the buffer struct. 


		x	-   anchor is becoming invalid after a cut. 




	*/

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

#define reading_crash  		0
#define running_crash  		0
#define fuzz 			0
#define use_main    		1
#define memory_safe 		1

typedef ssize_t nat;

// todo: make these part of a config struct..?  and   also  config file / parameters.
static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";
static const nat autosave_frequency = 8;     // in seconds

enum action_type {
	no_action, insert_action,
	delete_action, paste_action,
	cut_action, anchor_action,
};

struct line { char* data; nat count, capacity; };

struct buffer {
	struct line* lines;
	struct action* actions;

	nat     saved, mode, autosaved, action_count, head, count, capacity, 
		selecting,  wrap_width, tab_width, sn_rows,

		lcl, lcc, 	vcl, vcc,  	vol, voc,    sbl, sbc,    sel, sec,
		vsl, vsc, 	vdc,    	lal, lac,    swl, swc,

		show_line_numbers, ww_fix, use_txt_extension_when_absent
	;

	char filename[4096], location[4096], message[4096], cwd[4096], selected_file[4096];
};

struct logical_state {
	nat     saved, autosaved, selecting, wrap_width, tab_width,
		lcl, lcc, 	vcl, vcc,    vol, voc,    sbl, sbc,  sel, sec,
		vsl, vsc, 	vdc,         lal, lac,    swl, swc
	;
};

struct action { 
	nat* children; 
	char* text;
	nat parent, type, choice, count, length;
	struct logical_state pre;
	struct logical_state post;
};

static pthread_mutex_t mutex;
static nat window_rows = 0, window_columns = 0;
static char* screen = NULL;
static size_t fuzz_input_index = 0, fuzz_input_count = 0;
static const uint8_t* fuzz_input = NULL; 
static nat buffer_count = 0, active_index = 0, working_index = 0;
static struct buffer* buffers = NULL;
static bool in_prompt = false;      // rework this...    is should be a buffer member?.... 

#define _ buffers[working_index]
static inline bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2;  }
static inline bool visual(char c) { return not zero_width(c); }
static inline bool file_exists(const char* f) { return access(f, F_OK) != -1; }
static inline bool equals(const char* s1, const char* s2) { return not strcmp(s1, s2); }

static inline char read_stdin() {
	char c = 0;
	if (fuzz) {
		if (fuzz_input_index >= fuzz_input_count) return 0;
		fuzz_input_index++;
		return (char) fuzz_input[fuzz_input_index - 1];
	}
	pthread_mutex_unlock(&mutex);
	read(0, &c, 1);
	pthread_mutex_lock(&mutex);
	return c;
}

static inline bool is_directory(const char *path) {
	struct stat s;
	if (stat(path, &s)) return false;
	return S_ISDIR(s.st_mode);
}

static inline void get_datetime(char datetime[16]) {
	struct timeval t;
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 15, "%y%m%d%u.%H%M%S", tm);
}

static inline bool stdin_is_empty() {
	if (fuzz) return fuzz_input_index >= fuzz_input_count;

	fd_set f;
	FD_ZERO(&f);
	FD_SET(STDIN_FILENO, &f);
	struct timeval timeout = {0};
	return select(1, &f, NULL, NULL, &timeout) != 1;
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

static inline nat compute_vcc() {
	nat v = 0;
	for (nat c = 0; c < _.lcc; c++) {
		char k = _.lines[_.lcl].data[c];
		if (k == '\t') { 
			if (v + _.tab_width - v % _.tab_width <= _.wrap_width) 
				do v++;
				while (v % _.tab_width);  
			else v = 0;
		} else if (visual(k)) {
			if (v < _.wrap_width) v++; else v = 0;
		}
	}
	return v;
}

static inline void move_left() {
	
	if (not _.lcc) {
		if (not _.lcl) return;
		_.lcl--;
		_.lcc = _.lines[_.lcl].count;
 line_up: 	_.vcl--;
		if (_.vsl) _.vsl--;
		else if (_.vol) _.vol--;
		_.vcc = compute_vcc();
		if (_.vcc >= _.swc) { 
			_.vsc = _.swc;  _.voc = _.vcc - _.vsc; 
		} else { _.vsc = _.vcc; _.voc = 0; }
	} else {
		do _.lcc--; while (_.lcc and zero_width(_.lines[_.lcl].data[_.lcc]));
		if (_.lines[_.lcl].data[_.lcc] == '\t') {
			const nat diff = _.tab_width - compute_vcc() % _.tab_width;
			if (_.vcc < diff) goto line_up;
			_.vcc -= diff;
			if (_.vsc >= diff) _.vsc -= diff;
			else if (_.voc >= diff - _.vsc) { _.voc -= diff - _.vsc; _.vsc = 0; }
		} else {
			if (not _.vcc) goto line_up;
			_.vcc--;
			if (_.vsc) _.vsc--; else if (_.voc) _.voc--;
		}
	}
}

static inline void move_right() {

	if (_.lcl >= _.count) return;
	if (_.lcc >= _.lines[_.lcl].count) {
		if (_.lcl + 1 >= _.count) return;
		_.lcl++; _.lcc = 0; 
line_down:	_.vcl++; _.vcc = 0; _.voc = 0; _.vsc = 0;
		if (_.vsl + 1 < _.swl) _.vsl++; 
		else _.vol++;
	} else {
		if (_.lines[_.lcl].data[_.lcc] == '\t') {
			do _.lcc++; 
			while (_.lcc < _.lines[_.lcl].count and zero_width(_.lines[_.lcl].data[_.lcc]));
			if (_.vcc + _.tab_width - _.vcc % _.tab_width > _.wrap_width) goto line_down;
			do {
				_.vcc++; 
				if (_.vsc + 1 < _.swc) _.vsc++;
				else _.voc++;
			} while (_.vcc % _.tab_width);
			
		} else {
			do _.lcc++; 
			while (_.lcc < _.lines[_.lcl].count and zero_width(_.lines[_.lcl].data[_.lcc]));
			if (_.vcc >= _.wrap_width) goto line_down;
			_.vcc++; 
			if (_.vsc + 1 < _.swc) _.vsc++; 
			else _.voc++;
		}
	}
}

static inline void move_up() {
	if (not _.vcl) {
		_.lcl = 0; _.lcc = 0; _.vcl = 0; _.vcc = 0;
		_.vol = 0; _.voc = 0; _.vsl = 0; _.vsc = 0;
		return;
	}
	nat line_target = _.vcl - 1;
	while (_.vcc and _.vcl > line_target) move_left(); 
	do move_left(); while (_.vcc > _.vdc and _.vcl == line_target);
	if (_.vcc >= _.swc) { 
		_.vsc = _.swc; _.voc = _.vcc - _.vsc; 
	} 
	else { _.vsc = _.vcc; _.voc = 0; }
}

static inline void move_down() {
	nat line_target = _.vcl + 1;
	while (_.vcl < line_target) { 
		if (_.lcl == _.count - 1 and _.lcc == _.lines[_.lcl].count) return;
		move_right();
	}
	while (_.vcc < _.vdc and _.lcc < _.lines[_.lcl].count) {
		if (_.lines[_.lcl].data[_.lcc] == '\t' and _.vcc + (_.tab_width - (_.vcc % _.tab_width)) > _.vdc) return;
		move_right();
	}
}

static inline void jump_line(nat line) {
	while (_.lcl < line and _.lcl < _.count) move_down();
	while (_.lcl > line and _.lcl) move_up();
}

static inline void jump_column(nat column) {
	while (_.lcc < column and _.lcc < _.lines[_.lcl].count) move_right();
	while (_.lcc > column and _.lcc) move_left();
	_.vdc = _.vcc;
}

static inline void jump_to(nat line, nat column) { jump_line(line); jump_column(column); }
static inline void move_begin() { while (_.vcc) move_left(); _.vdc = _.vcc; }

static inline void move_end() {
	while (_.lcc < _.lines[_.lcl].count and _.vcc < _.wrap_width) move_right(); 
	_.vdc = _.vcc;
}

static inline void move_top() {
	_.lcl = 0; _.lcc = 0;
	_.vcl = 0; _.vcc = 0;
	_.vol = 0; _.voc = 0;
	_.vsl = 0; _.vsc = 0;
	_.vdc = 0;
}

static inline void move_bottom() {
	while (_.lcl < _.count - 1 or _.lcc < _.lines[_.lcl].count) move_down(); 
	_.vdc = _.vcc;
}

static inline void move_word_left() {
	do move_left();
	while (not(
		(not _.lcl and not _.lcc) or (
			(_.lcc < _.lines[_.lcl].count and isalnum(_.lines[_.lcl].data[_.lcc]))  and 
			(not _.lcc or not isalnum(_.lines[_.lcl].data[_.lcc - 1]))
		)
	));
	_.vdc = _.vcc;
}

static inline void move_word_right() {
	do move_right();
	while (not(
		(_.lcl >= _.count - 1 and _.lcc >= _.lines[_.lcl].count) or (
			(_.lcc >= _.lines[_.lcl].count or not isalnum(_.lines[_.lcl].data[_.lcc]))  and 
			(_.lcc and isalnum(_.lines[_.lcl].data[_.lcc - 1]))
		)
	));
	_.vdc = _.vcc;
}

static inline void record_logical_state(struct logical_state* pcond_out) {
	struct logical_state* p = pcond_out; 

	p->saved = _.saved;
	p->autosaved = _.autosaved;
	p->selecting = _.selecting;
	p->wrap_width = _.wrap_width;
	p->tab_width = _.tab_width;
	p->lcl = _.lcl;  p->lcc = _.lcc; 	
	p->vcl = _.vcl;  p->vcc = _.vcc;
  	p->vol = _.vol;  p->voc = _.voc;
	p->vsl = _.vsl;  p->vsc = _.vsc; 
	p->vdc = _.vdc;  p->lal = _.lal;
	p->lac = _.lac;
}

static inline void require_logical_state(struct logical_state* pcond_in) {  
	struct logical_state* p = pcond_in;

	_.saved = p->saved;
	_.autosaved = p->autosaved;
	_.selecting = p->selecting;
	_.wrap_width = p->wrap_width;
	_.tab_width = p->tab_width;
	_.lcl = p->lcl;  _.lcc = p->lcc;
	_.vcl = p->vcl;  _.vcc = p->vcc;
  	_.vol = p->vol;  _.voc = p->voc;
	_.vsl = p->vsl;  _.vsc = p->vsc; 
	_.vdc = p->vdc;  _.lal = p->lal;
	_.lac = p->lac;
}

static inline void create_action(struct action new) {
	new.parent = _.head;
	_.actions[_.head].children = realloc(_.actions[_.head].children, sizeof(nat) * (size_t) (_.actions[_.head].count + 1));
	_.actions[_.head].choice = _.actions[_.head].count;
	_.actions[_.head].children[_.actions[_.head].count++] = _.action_count;
	
	_.actions = realloc(_.actions, sizeof(struct action) * (size_t)(_.action_count + 1));
	_.head = _.action_count;
	_.actions[_.action_count++] = new;
}

static inline void insert(char c, bool should_record) { 

	if (should_record and zero_width(c) 
		and not (
			_.actions[_.head].type == insert_action and 
			_.actions[_.head].text[0] != '\n' and
			_.actions[_.head].post.lcl == _.lcl and 
			_.actions[_.head].post.lcc == _.lcc and 
			_.actions[_.head].count == 0
		)) return; 

	struct action new_action = {0};
	if (should_record and visual(c)) record_logical_state(&new_action.pre);

	struct line* here = _.lines + _.lcl;
	if (c == '\n') {
		nat rest = here->count - _.lcc;
		here->count = _.lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, here->data + _.lcc, (size_t) rest);

		if (not memory_safe) {
			if (_.count + 1 >= _.capacity) 
				_.lines = realloc(_.lines, sizeof(struct line) * (size_t)(_.capacity = 8 * (_.capacity + 1)));
		} else {
			_.lines = realloc(_.lines, sizeof(struct line) * (size_t)(_.count + 1));
		}

		memmove(_.lines + _.lcl + 2, _.lines + _.lcl + 1, sizeof(struct line) * (size_t)(_.count - (_.lcl + 1)));
		_.lines[_.lcl + 1] = new;
		_.count++;

	} else {
		if (not memory_safe) {
			if (here->count + 1 >= here->capacity) 
				here->data = realloc(here->data, (size_t)(here->capacity = 8 * (here->capacity + 1)));
		} else {
			here->data = realloc(here->data, (size_t)(here->count + 1));
		}

		memmove(here->data + _.lcc + 1, here->data + _.lcc, (size_t) (here->count - _.lcc));
		here->data[_.lcc] = c;
		here->count++;
	}

	if (zero_width(c)) _.lcc++; 
	else { move_right(); _.vdc = _.vcc; }

	_.saved = false;
	_.autosaved = false;
	if (not should_record) return;

	if (zero_width(c)) {
		_.actions[_.head].text = realloc(_.actions[_.head].text, (size_t) _.actions[_.head].length + 1);
		_.actions[_.head].text[_.actions[_.head].length++] = c;
		record_logical_state(&(_.actions[_.head].post));
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
	struct line* here = _.lines + _.lcl;

	if (not _.lcc) {
		if (not _.lcl) return;
		move_left(); _.vdc = _.vcc;
		struct line* new = _.lines + _.lcl;

		if (not memory_safe) {
			if (new->count + here->count >= new->capacity)
				new->data = realloc(new->data, (size_t)(new->capacity = 8 * (new->capacity + here->count)));
		} else {
			new->data = realloc(new->data, (size_t)(new->count + here->count));
		}

		if (here->count) memcpy(new->data + new->count, here->data, (size_t) here->count);
		free(here->data);

		new->count += here->count;
		memmove(_.lines + _.lcl + 1, _.lines + _.lcl + 2, 
			sizeof(struct line) * (size_t)(_.count - (_.lcl + 2)));
		_.count--;

		if (memory_safe) _.lines = realloc(_.lines, sizeof(struct line) * (size_t)_.count);

		if (should_record) {
			deleted_length = 1;
			deleted_string = malloc(1);
			deleted_string[0] = '\n';
		}

	} else {
		nat save = _.lcc;
		move_left(); _.vdc = _.vcc;
		
		if (should_record) {
			deleted_length = save - _.lcc;
			deleted_string = malloc((size_t) deleted_length);
			memcpy(deleted_string, here->data + _.lcc, (size_t) deleted_length);
		}

		memmove(here->data + _.lcc, here->data + save, (size_t)(here->count - save));
		here->count -= save - _.lcc;

		if (memory_safe) here->data = realloc(here->data, (size_t)(here->count));
	}

	_.saved = false;
	_.autosaved = false;
	if (not should_record) return;

	record_logical_state(&new_action.post);
	new_action.type = delete_action;
	new_action.text = deleted_string;
	new_action.length = deleted_length;
	create_action(new_action);
}

static void insert_string(const char* string, nat length) {
	struct action new = {0};
	record_logical_state(&new.pre);

	for (nat i = 0; i < length; i++) 
		insert((char) string[i], 0);
	
	record_logical_state(&new.post);
	new.type = paste_action;
	new.text = strndup(string, (size_t) length);
	new.length = length;
	create_action(new);
}

static inline void insertdt() {
	char datetime[16] = {0};
	get_datetime(datetime);
	insert_string(datetime, 14);
}

static inline void initialize_buffer() {
	_ = (struct buffer) {0};

	_.wrap_width = 80;     
	_.tab_width = 8; 
	_.capacity = 1;
	_.count = 1;
	_.action_count = 1;
	_.lines = calloc(1, sizeof(struct line));
	_.actions = calloc(1, sizeof(struct action));

	_.sn_rows = 2;
	_.ww_fix = 0;
	_.show_line_numbers = 1; 
	_.saved = true;
	_.autosaved = true;
	_.use_txt_extension_when_absent = 1;

	_.sel = window_rows - _.sn_rows;
	_.sec = window_columns;
	_.swl = _.sel - _.sbl;
	_.swc = _.sec - _.sbc;
}

static inline void adjust_window_size() {
	static struct winsize window = {0}; 
	ioctl(1, TIOCGWINSZ, &window);

	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 27; window.ws_col = 70; }

	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen = realloc(screen, (size_t) (window_rows * window_columns * 4));	
	}

	//		 todo:

	// const nat w = (window_columns - 1) - (this.line_number_width);
	// if (this.ww_fix and wrap_width != w) wrap_width = w;

	_.wrap_width = 80;   // temp;
	// split_point = ;     // temp... 
}


static inline nat display_proper(
	nat length, nat* total, 
	int line_number_digits
) {
	nat sl = 0, sc = 0, vl = _.vol, vc = _.voc;
	struct logical_state state = {0};
	record_logical_state(&state);
	while (1) { 
		if (_.vcl <= 0 and _.vcc <= 0) break;
		if (_.vcl <= state.vol and _.vcc <= state.voc) break;
		move_left();
	}

 	nat line = _.lcl, col = _.lcc; 
	require_logical_state(&state); 

	do {
		if (line >= _.count) goto next_visual_line;

		if (_.show_line_numbers and vl >= _.vol and vl < _.vol + _.swl) {
			if (not col or (not sc and not sl)) 
				length += sprintf(screen + length, 
					"\033[38;5;%ldm%*ld\033[0m  ", 
					236L + (line == _.lcl ? 5 : 0), line_number_digits, line
				);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}
		do {
			if (col >= _.lines[line].count) goto next_logical_line;  
			
			char k = _.lines[line].data[col++];
			if (k == '\t') {

				if (vc + (_.tab_width - vc % _.tab_width) > _.wrap_width) goto next_visual_line;
				do { 
					if (	vc >= _.voc and vc < _.voc + _.swc
					and 	vl >= _.vol and vl < _.vol + _.swl
					) {
						screen[length++] = ' ';
						sc++;
					}
					vc++;
				} while (vc % _.tab_width);

			} else {
				if (	vc >= _.voc and vc < _.voc + _.swc
				and 	vl >= _.vol and vl < _.vol + _.swl
				and 	(sc or visual(k))
				) { 
					screen[length++] = k;
					if (visual(k)) sc++;	
				}
				if (visual(k)) {
					if (vc >= _.wrap_width) goto next_visual_line; 
					vc++; 
				} 
			}

		} while (sc < _.swc or col < _.lines[line].count);

	next_logical_line:
		line++; col = 0;

	next_visual_line:
		if (vl >= _.vol and vl < _.vol + _.swl) {
			screen[length++] = '\033';
			screen[length++] = '[';	
			screen[length++] = 'K';
			if (*total < window_rows - 1) {
				screen[length++] = '\r';
				screen[length++] = '\n';
			}
			sl++; sc = 0; (*total)++;
		}
		vl++; vc = 0; 
	} while (sl < _.swl);

	return length;
}

static inline void add_status(nat b) {

	char status[8448] = {0};
	nat status_length = sprintf(status, " [wi=%ld m=%ld] [s=%ld ai=%ld bc=%ld] a[%ld %ld] c[%ld %ld] %s %c%c %s",
		(nat) working_index, buffers[b].mode, 
		buffers[b].selecting, active_index, buffer_count, 
		buffers[b].lal, buffers[b].lac, buffers[b].lcl, buffers[b].lcc, buffers[b].filename,
		buffers[b].saved ? 's' : 'e', buffers[b].autosaved ? ' ' : '*',
		buffers[b].message
	);

	nat save_col = _.lcc, save_line = _.lcl;
	move_top();
	move_end();
	while (_.lcc) delete(0);
	insert_string(status, status_length);
	jump_to(save_line, save_col);
}

static inline void display() {
	adjust_window_size();
	nat total = 0, cursor_line = 0, cursor_col = 0;
	nat length = 6;
	memcpy(screen, "\033[?25l", 6);

	nat save_wi = working_index;
	working_index = active_index;

	double f = floor(log10((double) _.count)) + 1;
	int line_number_digits = (int)f;
	nat line_number_width = _.show_line_numbers * (line_number_digits + 2);

	nat _sn_rows = _.sn_rows;
	_.sbl = 0;
	_.sbc = line_number_width;
	_.sel = window_rows - _sn_rows;
	_.sec = window_columns;
	_.swl = _.sel - _.sbl;
	_.swc = _.sec - _.sbc;

	length += sprintf(screen + length, "\033[%ld;%ldH", 1L, 1L); 
	length = display_proper(length, &total, line_number_digits);

	if (save_wi == active_index) {
		cursor_line = _.sbl + _.vsl + 1;
		cursor_col = _.sbc + _.vsc + 1;
	}

	working_index = buffer_count;

	f = floor(log10((double) _.count)) + 1;
	line_number_digits = (int)f;
	line_number_width = _.show_line_numbers * (line_number_digits + 2);
	
	_.sbl = window_rows - _sn_rows;
	_.sbc = line_number_width;
	_.sel = window_rows;
	_.sec = window_columns;
	_.swl = _.sel - _.sbl;
	_.swc = _.sec - _.sbc;

	add_status(save_wi);

	length += sprintf(screen + length, "\033[%ld;%ldH",  window_rows - _sn_rows + 1L,  1L ); 
	length = display_proper(length, &total, line_number_digits);

	if (save_wi == buffer_count and active_index != buffer_count) {
		cursor_line = _.sbl + _.vsl + 1;
		cursor_col = _.sbc + _.vsc + 1;
	}

	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", cursor_line, cursor_col);
	if (not fuzz) write(1, screen, (size_t) length);

	working_index = save_wi;
}

static inline void print_above_textbox(char* message) {
	working_index = buffer_count;
	insert('\n', 1);
	insert_string(message, (nat) strlen(message));
	working_index = active_index;
}

static inline void execute(char, char p);

static inline void prompt(const char* prompt_message, char* out, nat out_size)  {
	working_index = buffer_count;
	move_bottom(); _.mode = 0;
	insert('\n', 0);
	insert_string(prompt_message, (nat) strlen(prompt_message));
	insert('\n', 0);
	in_prompt = true;
	char c = 0, p = 0;

loop:	display(); 
	c = read_stdin(); 
	if (fuzz and not c) goto done;
	execute(c, p);
	p = c;
	if (in_prompt) goto loop;
done:;  const char* string = _.lines[_.lcl].data;
	nat string_length = _.lines[_.lcl].count;

	if (string_length > out_size) string_length = out_size;
	memcpy(out, string, (size_t) string_length);
	memset(out + string_length, 0, (size_t) out_size - (size_t) string_length);
	out[out_size - 1] = 0;
	working_index = active_index;
}

static inline bool confirmed(const char* question, const char* yes_action, const char* no_action) {
	
	char prompt_message[4096] = {0}, invalid_response[4096] = {0};
	sprintf(prompt_message, "%s? (%s/%s): ", question, yes_action, no_action);
	sprintf(invalid_response, "please type \"%s\" or \"%s\".", yes_action, no_action);
	
	while (1) {
		char response[4096] = {0};
		prompt(prompt_message, response, sizeof response);

		if (equals(response, yes_action)) return true;
		else if (equals(response, no_action)) return false;
		else if (equals(response, "")) return false;
		else print_above_textbox(invalid_response);

		if (fuzz) return true;
	}
}

static inline void create_sn_buffer() {
	buffer_count = 0;  active_index = 0;
	buffers = calloc(1, sizeof(struct buffer));
	initialize_buffer();
}

static inline void create_empty_buffer() {
	buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count + 2));
	buffers[buffer_count + 1] = buffers[buffer_count];
	working_index = active_index = buffer_count;
	buffer_count++;
	initialize_buffer();
}

static inline void destroy(nat b) {
	for (nat line = 0; line < buffers[b].count; line++) 
		free(buffers[b].lines[line].data);
	free(buffers[b].lines);
	for (nat a = 0; a < buffers[b].action_count; a++) {
		free(buffers[b].actions[a].text);
		free(buffers[b].actions[a].children);
	}
	free(buffers[b].actions);
}

static inline void close_active_buffer() {
	if (not buffer_count) return;

	destroy(active_index);
	memmove(buffers + active_index, buffers + active_index + 1, 
		sizeof(struct buffer) * (size_t)(buffer_count - active_index));
	buffer_count--;
	buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count + 1));

	if (active_index >= buffer_count and buffer_count) active_index = buffer_count - 1; 
	else if (not buffer_count) active_index = 0;	
	working_index = active_index;
}

static inline void move_to_next_buffer() {
	if (active_index) active_index--; else active_index = buffer_count;
	working_index = active_index;
}

static inline void move_to_previous_buffer() {
	if (active_index < buffer_count) active_index++; else active_index = 0;
	working_index = active_index;
}

static inline void open_file(const char* given_filename) {
	if (fuzz) return;

	if (not strlen(given_filename)) return;
	
	FILE* file = fopen(given_filename, "r");
	if (not file) {
		if (buffer_count) {
			sprintf(_.message, "error: fopen: %s", strerror(errno));
			return;
		}
		perror("fopen"); exit(1);
	}

	fseek(file, 0, SEEK_END);        
        size_t length = (size_t) ftell(file);
	char* text = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(text, sizeof(char), length, file);
	fclose(file);

	create_empty_buffer();
	for (size_t i = 0; i < length; i++) insert(text[i], 0);
	free(text); 
	_.saved = true; 
	_.autosaved = true; 
	_.mode = 1; 
	move_top();

	strcpy(_.filename, given_filename);
	strcpy(_.location, given_filename); // todo:   seperate out these two things!!!
	sprintf(_.message, "read %lub", length);
}

static inline void emergency_save_to_file() {
	if (fuzz) return;

	char dt[16] = {0};
	get_datetime(dt);
	char id[32] = {0};
	sprintf(id, "%08x%08x", rand(), rand());
	char local_filename[4096] = {0};
	sprintf(local_filename, "EMERGENCY_FILE_SAVE__%s__%s__.txt", dt, id);

	FILE* file = fopen(local_filename, "w+");
	if (not file) {
		printf("emergency error: %s\n", strerror(errno));
		return;
	}
	
	unsigned long long bytes = 0;
	for (nat i = 0; i < _.count; i++) {
		bytes += fwrite(_.lines[i].data, sizeof(char), (size_t) _.lines[i].count, file);
		if (i < _.count - 1) {
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
	printf("interrupt: emergency wrote %lldb;%ldl to %s\n\r", bytes, _.count, local_filename);
}

static inline void emergency_save_all_buffers() {
	nat save_wi = working_index;
	for (int i = 0; i < buffer_count + 1; i++) {
		working_index = i;
		emergency_save_to_file();
		sleep(1);
	}
	working_index = save_wi;
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
	for (nat i = 0; i < _.count; i++) {
		bytes += fwrite(_.lines[i].data, sizeof(char), (size_t) _.lines[i].count, file);
		if (i < _.count - 1) {
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
	_.autosaved = true;
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

	if (not strlen(_.filename)) {

	prompt_filename:

		prompt("save as: ", _.filename, sizeof _.filename);
		if (not strlen(_.filename)) { sprintf(_.message, "aborted save"); return; }
		if (not strrchr(_.filename, '.') and _.use_txt_extension_when_absent) strcat(_.filename, ".txt");
		if (file_exists(_.filename) and not confirmed("file already exists, overwrite", "overwrite", "no")) {
			strcpy(_.filename, ""); goto prompt_filename;
		}

	}

	FILE* file = fopen(_.filename, "w+");
	if (not file) {
		sprintf(_.message, "error: %s", strerror(errno));
		return;
	}
	
	unsigned long long bytes = 0;
	for (nat i = 0; i < _.count; i++) {
		bytes += fwrite(_.lines[i].data, sizeof(char), (size_t) _.lines[i].count, file);
		if (i < _.count - 1) {
			fputc('\n', file);
			bytes++;
		}
	}

	if (ferror(file)) {
		sprintf(_.message, "error: %s", strerror(errno));
		fclose(file);
		return;
	}

	fclose(file);
	sprintf(_.message, "wrote %lldb;%ldl", bytes, _.count);
	_.saved = true;
}

static inline void rename_file() {
	if (fuzz) return;

	char new[4096] = {0};
	prompt_filename:
	prompt("rename to: ", new, sizeof new);
	if (not strlen(new)) { sprintf(_.message, "aborted rename"); return; }

	if (file_exists(new) and not confirmed("file already exists, overwrite", "overwrite", "no")) {
		strcpy(new, ""); goto prompt_filename;
	}

	if (rename(_.filename, new)) sprintf(_.message, "error: %s", strerror(errno));
	else {
		strncpy(_.filename, new, sizeof new);
		sprintf(_.message, "renamed to \"%s\"", _.filename);
	}
}

static inline void interpret_escape_code() {

	static nat scroll_counter = 0;
	char c = read_stdin();
	
	if (c == '[') {

		c = read_stdin();

		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') { move_right(); _.vdc = _.vcc; }
		else if (c == 'D') { move_left(); _.vdc = _.vcc; }

		//  TODO:   i need to completely redo the way that we scroll the screen, using mouse/trackpad scrolling. 

		else if (c == 'M') {

			read_stdin();

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
	prompt("open: ", new_filename, sizeof new_filename);
	if (not strlen(new_filename)) { sprintf(_.message, "aborted open"); return; }
	open_file(new_filename);
}

static inline void prompt_jump_line() {
	char string_number[128] = {0};
	prompt("line: ", string_number, sizeof string_number);
	nat line = atoi(string_number);
	if (line >= _.count) line = _.count - 1;
	jump_line(line);
	sprintf(_.message, "jumped to %ld %ld", _.lcl, _.lcc);
}

static inline void prompt_jump_column() {
	char string_number[128] = {0};
	prompt("column: ", string_number, sizeof string_number);
	nat column = atoi(string_number);
	if (column > _.lines[_.lcl].count) column = _.lines[_.lcl].count;
	jump_column(column);
	sprintf(_.message, "jumped to %ld %ld", _.lcl, _.lcc);
}

static char* get_sel(nat* out_length, nat first_line, nat first_column, nat last_line, nat last_column) {
	
	char* string = malloc(256);
	nat length = 0;
	nat s_capacity = 256;

	nat line = first_line, column = first_column;

	while (line < last_line) {

		if (line >= _.count or column > _.lines[line].count) {
			*out_length = 0;
			return NULL;
		}

		if (not memory_safe) {
			if (length + _.lines[line].count - column + 1 >= s_capacity) 
				string = realloc(string, (size_t) 
					(s_capacity = 8 * (s_capacity + length + _.lines[line].count - column + 1)));
		} else string = realloc(string, (size_t) (length + _.lines[line].count - column));

		if (_.lines[line].count - column) 
			memcpy(string + length, _.lines[line].data + column, (size_t)(_.lines[line].count - column));

		length += _.lines[line].count - column;

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
	} else string = realloc(string, (size_t) (length + last_column - column));

	if (last_column - column) memcpy(string + length, _.lines[line].data + column, (size_t)(last_column - column));
	length += last_column - column;
	*out_length = length;
	return string;
}

static inline char* get_selection(nat* out) {
	if (_.lal < _.lcl) return get_sel(out, _.lal, _.lac, _.lcl, _.lcc);
	if (_.lcl < _.lal) return get_sel(out, _.lcl, _.lcc, _.lal, _.lac);
	if (_.lac < _.lcc) return get_sel(out, _.lal, _.lac, _.lcl, _.lcc);
	if (_.lcc < _.lac) return get_sel(out, _.lcl, _.lcc, _.lal, _.lac);
	*out = 0;
	return NULL;
}

static inline void paste() {

 if (not fuzz) {

	FILE* file = popen("pbpaste", "r");
	if (not file) { sprintf(_.message, "error: paste: popen(): %s", strerror(errno)); return; }
	struct action new = {0};
	record_logical_state(&new.pre);
	char* string = malloc(256);
	nat s_capacity = 256, length = 0, c = 0;
	_.lac = _.lcc; _.lal = _.lcl;

	while ((c = fgetc(file)) != EOF) {

		if (not memory_safe) {
			if (length + 1 >= s_capacity) 
				string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + 1)));
		} else string = realloc(string, (size_t) (length + 1));

		string[length++] = (char) c;
		insert((char)c, 0);
	}
	pclose(file);
	sprintf(_.message, "pasted %ldb", length);
	record_logical_state(&new.post);
	new.type = paste_action;
	new.text = string;
	new.length = length;
	create_action(new);


 } else {
 	struct action new = {0};
 	record_logical_state(&new.pre);
 	char* string = malloc(256);
 	nat length = 0;
 	_.lac = _.lcc; _.lal = _.lcl;
 	string = realloc(string, (size_t) (length + 1));
 	string[length++] = (char) 'A';
 	insert((char)'A', 0);
 	sprintf(_.message, "pasted %ldb", length);
 	record_logical_state(&new.post);
 	new.type = paste_action;
 	new.text = string;
 	new.length = length;
 	create_action(new);
 }
}

static inline void cut_selection() {
	if (_.lal < _.lcl) goto anchor_first;
	if (_.lcl < _.lal) goto cursor_first;
	if (_.lac < _.lcc) goto anchor_first;
	if (_.lcc < _.lac) goto cursor_first;
	return;
cursor_first:;
	nat line = _.lcl, column = _.lcc;
	while (_.lcl < _.lal or _.lcc < _.lac) move_right();
	_.lal = line; _.lac = column;
anchor_first:
	while (_.lal < _.lcl or _.lac < _.lcc) delete(0);
}

static inline void cut() { 
	if (_.lal >= _.count or _.lac > _.lines[_.lal].count) return;
	struct action new = {0};
	record_logical_state(&new.pre);
	nat deleted_length = 0;
	char* deleted_string = get_selection(&deleted_length);
	cut_selection();
	sprintf(_.message, "deleted %ldb", deleted_length);
	record_logical_state(&new.post);
	new.type = cut_action;
	new.text = deleted_string;
	new.length = deleted_length;
	create_action(new);
}

static inline void copy() {
	if (fuzz) return;

	FILE* file = popen("pbcopy", "w");
	if (not file) { sprintf(_.message, "error: copy: popen(): %s", strerror(errno)); return; }

	nat length = 0;
	char* selection = get_selection(&length);
	fwrite(selection, 1, (size_t)length, file);
	pclose(file);
	free(selection);
	sprintf(_.message, "copied %ldb", length);
}

static inline void run_shell_command(const char* command) {
	if (fuzz) return;

	FILE* f = popen(command, "r");
	if (not f) {
		sprintf(_.message, "error: could not run command \"%s\": %s\n", command, strerror(errno));
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
	}
	pclose(f);
	_.lal = _.lcl; _.lac = _.lcc;
	sprintf(_.message, "output %ldb", length);
	insert_string(output, length);
}

static inline void prompt_run() {
	char command[4096] = {0};
	prompt("run(2>&1): ", command, sizeof command);
	if (not strlen(command)) { sprintf(_.message, "aborted run"); return; }
	sprintf(_.message, "running: %s", command);
	run_shell_command(command);
}

static inline void replay_action(struct action a) {
	require_logical_state(&a.pre);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action or a.type == paste_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_action) cut_selection();
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {
	require_logical_state(&a.post);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_action) {
		while (_.lcc > a.pre.lcc or _.lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} require_logical_state(&a.pre);
}

static inline void undo() {
	if (not _.head) return;
	reverse_action(_.actions[_.head]);

	sprintf(_.message, "undoing %ld ", _.actions[_.head].type);
	if (_.actions[_.head].count != 1) 
		sprintf(_.message + strlen(_.message), "%ld %ld", _.actions[_.head].choice, _.actions[_.head].count);

	_.head = _.actions[_.head].parent;
}

static inline void undo_silent() {
	if (not _.head) return;
	reverse_action(_.actions[_.head]);
	_.head = _.actions[_.head].parent;
}

static inline void redo() {
	if (not _.actions[_.head].count) return;
	_.head = _.actions[_.head].children[_.actions[_.head].choice];
	sprintf(_.message, "redoing %ld ", _.actions[_.head].type);
	if (_.actions[_.head].count != 1) 
		sprintf(_.message + strlen(_.message), "%ld %ld", _.actions[_.head].choice, _.actions[_.head].count);
	replay_action(_.actions[_.head]);
}

static inline void alternate_incr() {
	if (_.actions[_.head].choice + 1 < _.actions[_.head].count) _.actions[_.head].choice++;
	sprintf(_.message, "switched %ld %ld", _.actions[_.head].choice, _.actions[_.head].count);
}

static inline void alternate_decr() {
	if (_.actions[_.head].choice) _.actions[_.head].choice--;
	sprintf(_.message, "switched %ld %ld", _.actions[_.head].choice, _.actions[_.head].count);
}

static inline void recalculate_position() {
	nat save_lcl = _.lcl, save_lcc = _.lcc;
	move_top();
	jump_to(save_lcl, save_lcc);
}

static inline void open_directory() {
	if (fuzz) return;
	
	DIR* directory = opendir(_.cwd);
	if (not directory) { 
		sprintf(_.message, "couldnt open cwd=%s, reason=%s", _.cwd, strerror(errno));	
		return;
	}

	struct dirent *e = NULL;

	nat length = (nat)strlen(_.cwd) + 2;
	char* menu = calloc((size_t) length + 1, sizeof(char));
	sprintf(menu, ":%s\n", _.cwd);
	
	while ((e = readdir(directory))) {

		char path[4096] = {0};
		strlcpy(path, _.cwd, sizeof path);
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

	nat tlal = _.lcl; nat tlac = _.lcc;
	insert_string(menu, length);
	jump_to(tlal, tlac);
}

static inline void change_directory() {

	char* selection = strndup(_.lines[_.lcl].data, (size_t) _.lines[_.lcl].count);

	if (equals(selection, "../") ) {
		if (not equals(_.cwd, "/")) {
			_.cwd[strlen(_.cwd) - 1] = 0;
			*(1+strrchr(_.cwd, '/')) = 0;
		} else {
			sprintf(_.message, "error: at root /");
		}
		
	} else if (equals(selection, "./") ) {
		// do nothing.

	} else {
		char path[4096] = {0};
		strlcpy(path, _.cwd, sizeof path);
		strlcat(path, selection, sizeof path);

		if (is_directory(path)) {
			strlcat(_.cwd, selection, sizeof _.cwd);
		} else {
			sprintf(_.message, "error: not a directory");
		}
	}
	free(selection);
}

static inline void file_select() {
	char* line = strndup(_.lines[_.lcl].data, (size_t) _.lines[_.lcl].count);
	strlcpy(_.selected_file, _.cwd, sizeof _.selected_file);
	strlcat(_.selected_file, line, sizeof _.selected_file);
	free(line);
	sprintf(_.message, "selected: %s", _.selected_file);
}

static inline char** split(char* string, char delim, nat* array_count) {

	nat a_count = 0;
	char** array = NULL;
	nat start = 0, i = 0;
	const nat length = (nat)strlen(string);

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
#define a   if (not _.selecting) { _.lal = al; _.lac = ac; } 
	if (_.mode == 0) {
		if (in_prompt and c == '\r') { in_prompt = false; return; }
		if (c == 'c' and p == 'h') { undo(); _.mode = 1; }
		else if (c == 27 and stdin_is_empty()) _.mode = 1;
		else if (c == 27) interpret_escape_code();
		else if (c == 127) delete(1);
		else if (c == '\r') insert('\n', 1);
		else insert(c, 1);

	} else if (_.mode == 1) {

		const nat  al = _.lcl,  ac = _.lcc;
	
		if (c == ' ') {}
		else if (c == 'l' and p == 'e') prompt_jump_line();         // unbind this?... yeah...
		else if (c == 'k' and p == 'e') prompt_jump_column();       // unbind this?... hmm...
		else if (c == 'd' and p == 'h') prompt_open();              // unbind this.?
		else if (c == 'f' and p == 'h') create_empty_buffer();       // unbind this.?
		else if (c == 'l' and p == 'e') {}
		else if (c == 'k' and p == 'e') in_prompt = not in_prompt;
		else if (c == 'i' and p == 'e') { move_bottom(); a }
		else if (c == 'p' and p == 'e') { move_top(); a }
		else if (c == 'n' and p == 'e') { move_begin(); a }
		else if (c == 'o' and p == 'e') { move_end(); a }
		else if (c == 'u' and p == 'e') alternate_decr();
		else if (c == 'r' and p == 'h') alternate_incr();
		else if (c == 'a' and p == 'h') move_to_previous_buffer();
		else if (c == 's' and p == 'h') move_to_next_buffer();
		else if (c == 'm' and p == 'h') copy();
		else if (c == 'c' and p == 'h') paste(); 	
		else if (c == 'd' and p == 'h') {}
		else if (c == 'a') _.selecting = not _.selecting;
		else if (c == 'W') _.ww_fix = not _.ww_fix;
		else if (c == 'd') delete(1);
		else if (c == 'r') cut();
		else if (c == 't') _.mode = 0;
		else if (c == 'm') { _.mode = 2; goto command_mode; }
		else if (c == 'c') undo();
		else if (c == 'k') redo();
		else if (c == 'o') { move_word_right(); a}
		else if (c == 'l') { move_word_left(); a}
		else if (c == 'p') { move_up(); a}
		else if (c == 'u') { move_down(); a}
		else if (c == 'i') { move_right(); _.vdc = _.vcc; a}
		else if (c == 'n') { move_left(); _.vdc = _.vcc; a}
		else if (c == '-') { if (_.sn_rows < window_rows) _.sn_rows++; } // unbind this. make it a set command.
		else if (c == '=') { if (_.sn_rows) _.sn_rows--; }               // unbind this. make it a set command.
		else if (c == '0') {
			if (working_index == active_index) working_index = buffer_count;
			else working_index = active_index;
		}
		else if (c == 's') save();
		else if (c == 'q') {
			if (_.saved or confirmed("discard unsaved changes", "discard", "no")) close_active_buffer(); 
		}
		else if (c == 27 and stdin_is_empty()) {}
		else if (c == 27) interpret_escape_code();

	} else if (_.mode == 2) {
		command_mode: _.mode = 1;

		char string[4096] = {0};
		prompt(" •• ", string, sizeof string);
		
		nat length = (nat) strlen(string);
		char* d = calloc((size_t) length + 1, sizeof(char));
		nat d_length = 0;
		
		for (nat i = 0; i < length; i++) {
			if ((unsigned char) string[i] < 33) continue;
			d[d_length++] = string[i];
		}

		d[d_length] = 0;
		nat command_count = 0;
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
			else if (equals(command, "openfile")) { file_select(); open_file(_.selected_file); }
			else if (equals(command, "selection")) sprintf(_.message, "%s", _.selected_file);
			else if (equals(command, "where")) sprintf(_.message, "@ %s", _.cwd);
			else if (equals(command, "home")) { getcwd(_.cwd, sizeof _.cwd); strlcat(_.cwd, "/", sizeof _.cwd); }
			else if (equals(command, "clearmessage")) memset(_.message, 0, sizeof _.message);
			else if (equals(command, "numbers")) _.show_line_numbers = not _.show_line_numbers;
			else if (equals(command, "snincr")) { if (_.sn_rows < window_rows) _.sn_rows++; }
			else if (equals(command, "sndecr")) { if (_.sn_rows) _.sn_rows--; } 
			else if (equals(command, "cut")) cut();
			else if (equals(command, "delete")) delete(1);
			else if (equals(command, "paste")) paste();
			else if (equals(command, "copy")) copy();
			else if (equals(command, "undo")) undo();
			else if (equals(command, "redo")) redo();
			else if (equals(command, "alternateincr")) alternate_incr();
			else if (equals(command, "alternatedecr")) alternate_decr();
			else if (equals(command, "moveright")) { move_right(); _.vdc = _.vcc; }
			else if (equals(command, "moveleft")) { move_left(); _.vdc = _.vcc; }
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
			else if (equals(command, "mode0")) _.mode = 0;
			else if (equals(command, "mode1")) _.mode = 1;
			else if (equals(command, "mode2")) _.mode = 2;

			else if (equals(command, "goto")) {}
			else if (equals(command, "branch")) {}
			else if (equals(command, "incr")) {}
			else if (equals(command, "setzero")) {}

			else if (equals(command, "wrapresizetemp")) { _.wrap_width = 0; recalculate_position(); }
			else if (equals(command, "quit")) {
				if (_.saved or confirmed("discard unsaved changes", "discard", "no")) 
					close_active_buffer(); 

			} else {
				sprintf(_.message, "command not recognized: %s", command);
				break;
			}
		}
	
		for (nat i = 0; i < command_count; i++) free(commands[i]);
		free(commands);
		free(d);

	} else _.mode = 1;
}

static void* autosaver(void* unused) {
	while (1) {
		sleep(autosave_frequency);
		pthread_mutex_lock(&mutex);
		if (not buffer_count) break;
		if (not _.autosaved) autosave();
		pthread_mutex_unlock(&mutex);
	}
	return unused;
}

static inline void editor() {

if (reading_crash) {
	const char* crashname = "slow-unit-b038966e9f5ed3ef1df6e108df29fe92e9fcd748";
	// 
	FILE* file = fopen(crashname, "r");
	fseek(file, 0, SEEK_END);
	size_t crash_length = (size_t) ftell(file);
	char* crash = malloc(sizeof(char) * crash_length);
	fseek(file, 0, SEEK_SET);
	fread(crash, sizeof(char), crash_length, file);
	fclose(file);
	fuzz_input = (const uint8_t*) crash;
	fuzz_input_count = crash_length;
	printf("\n\n\nchar str[] = {");
	for (size_t i = 0; i < fuzz_input_count; i++) {
		if (not (i % 4)) printf("\n\t\t");
		printf("0x%hhx, ", fuzz_input[i]);
	}
	printf("\n};\n\n\n");
	exit(1);
}
/*	
char str[] = {
		0xea, 0x89, 0x3d, 0x23, 
		0x60, 0xc9, 0x1c, 0x3c, 
		0x1c, 0x1c, 0x1c, 0x8, 
		0x1, 0xa, 0x88, 0x3b, 
		0xff, 0xff, 0xff, 0x9, 
		0x9, 0x9, 0x9, 0x25, 
		0x9, 0x9, 0x9, 0x9, 
		0x9, 0x48, 0x9, 0x9, 
		0x9, 0x9, 0x9, 0x9, 
		0x9, 0x9, 0x9, 0x9, 
};*/

char str[] = {0};

if (running_crash) {
	fuzz_input_index = 0; 
	fuzz_input = (const uint8_t*) str;
	fuzz_input_count = sizeof str;
}
	struct termios terminal;
	static pthread_t autosave_thread;

	if (not fuzz) {
		terminal = configure_terminal();
		write(1, "\033[?1049h\033[?1000h\033[7l", 20);
		pthread_mutex_init(&mutex, NULL);
		pthread_mutex_lock(&mutex);
		pthread_create(&autosave_thread, NULL, &autosaver, NULL);
	} 

	char p = 0, c = 0;
    	getcwd(_.cwd, sizeof _.cwd);
	strlcat(_.cwd, "/", sizeof _.cwd);

loop:	display();
	c = read_stdin(); 
	if (fuzz and not c) goto done;
	execute(c, p);
	p = c;
	if (buffer_count) goto loop;
done:	
	working_index = active_index;
	while (buffer_count) close_active_buffer();
	destroy(0);
	free(buffers); buffers = NULL;
	free(screen);  screen = NULL;
	buffer_count = 0;
	active_index = 0;	
	window_rows = 0;
	window_columns = 0;
	in_prompt = false;

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
	create_sn_buffer();
	create_empty_buffer();
	fuzz_input_index = 0; 
	fuzz_input = input;
	fuzz_input_count = size;
	editor();
	return 0;
}

#else

int main(const int argc, const char** argv) {
	create_sn_buffer();
	adjust_window_size();
	if (argc <= 1) create_empty_buffer();
	else for (int i = 1; i < argc; i++) open_file(argv[i]);
	signal(SIGINT, handle_signal_interrupt);
	editor();
}

#endif

// ---------------------------------------------------------------------------------------------------


