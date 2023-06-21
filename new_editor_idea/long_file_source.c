//  Daniel Warren Riaz Rehman's minimalist 
//        terminal-based text editor 
//
//     Designed with reliability, minimalism, 
//     simplicity, and ergonimics in mind.
//
//          currently unnamed
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
//           edited on 2303061.005150
//
/*
	bugs:     crashing bug:   open_file,    performance bug: long lines.
	

	yay. i am just jotting down a note in this file, to see the performance of the editor
	when editing a very long text document. so far, its actually going pretty well. 
	i mean, even though its exetremely long, the thing is actually not consuming that much 
	resources, like only 0.2 cpu %... which is pretty impressive. i really overestimated
	the effect that havin a very long file actually does on the system... like,
	and doing an O(n) operation on every single keypress. 

			i think the reason why it was really terrible before, is that 
			before i wasnt actually even printing out the text in the most efficient
			way, so it was like an n squared algorithm, or something like that lol.

			like, super duper bad. now, its super fast, and it seems to be paying off
			like, there are like no concerns, basically, that i have. pretty amazing. 
		i am going to try writing this file now, and seeing if we can actually do 
			that too lol


i  think it will be fine though. 













    
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

//#define reading_crash  		0
//#define running_crash  		0
#define fuzz 			0
#define use_main    		1
#define memory_safe 		1

typedef ssize_t nat; 

static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";
static const nat autosave_frequency = 8;     // in seconds

enum action_type {
	no_action, insert_action,
	delete_action, paste_action,
	cut_action, anchor_action,
};
struct line { char* data; nat count, capacity; };
struct logical_state {
	nat     saved, autosaved, selecting, wrap_width, tab_width,
		lcl, lcc, 	vcl, vcc,    vol, voc,   
		vsl, vsc, 	vdc,         lal, lac   ;
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

static struct line* lines;
static struct action* actions;

static nat     saved, mode, autosaved, action_count, head, count, capacity, 
	selecting,  wrap_width, tab_width, line_number_width,
	lcl, lcc, 	vcl, vcc,  	vol, voc,   
	vsl, vsc, 	vdc,    	lal, lac,   
	show_line_numbers, fixed_wrap, use_txt_extension_when_absent
;
static char filename[4096], location[4096], message[4096];
static char* user_response = NULL;
static nat user_response_length = 0;

static inline bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2;  }
static inline bool visual(char c) { return not zero_width(c); }
static inline bool file_exists(const char* f) { return access(f, F_OK) != -1; }
//static inline bool equals(const char* s1, const char* s2) { return not strcmp(s1, s2); }

static inline char read_stdin(void) {
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

static inline void get_datetime(char datetime[16]) {
	struct timeval t;
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 15, "%y%m%d%u.%H%M%S", tm);
}

static inline bool stdin_is_empty(void) {
	if (fuzz) return fuzz_input_index >= fuzz_input_count;

	fd_set f;
	FD_ZERO(&f);
	FD_SET(STDIN_FILENO, &f);
	struct timeval timeout = {0};
	return select(1, &f, NULL, NULL, &timeout) != 1;
}

static inline struct termios configure_terminal(void) {
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

static inline nat compute_vcc(void) {
	nat v = 0;
	for (nat c = 0; c < lcc; c++) {
		char k = lines[lcl].data[c];
		if (k == '\t') { 
			if (v + tab_width - v % tab_width < wrap_width or not wrap_width) 
				do v++;
				while (v % tab_width);  
			else v = 0;
		} else if (visual(k)) {
			if (v < wrap_width or not wrap_width) v++; else v = 0;
		}
	}
	return v;
}

static inline void move_left(void) {
	
	if (not lcc) {
		if (not lcl) return;
		lcl--;
		lcc = lines[lcl].count;
 line_up: 	vcl--;
		if (vsl) vsl--;
		else if (vol) vol--;
		vcc = compute_vcc();
		if (vcc >= window_columns - line_number_width) { 
			vsc = window_columns - line_number_width;  voc = vcc - vsc; 
		} else { vsc = vcc; voc = 0; }
	} else {
		do lcc--; while (lcc and zero_width(lines[lcl].data[lcc]));
		if (lines[lcl].data[lcc] == '\t') {
			const nat diff = tab_width - compute_vcc() % tab_width;
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

static inline void move_right(void) {

	if (lcl >= count) return;
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; lcc = 0; 
line_down:	vcl++; vcc = 0; voc = 0; vsc = 0;
		if (vsl + 1 < window_rows) vsl++; 
		else vol++;
	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
			if (vcc + tab_width - vcc % tab_width >= wrap_width and wrap_width) goto line_down;
			do {
				vcc++; 
				if (vsc + 1 < window_columns - line_number_width) vsc++;
				else voc++;
			} while (vcc % tab_width);
			
		} else {
			do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
			if (vcc >= wrap_width and wrap_width) goto line_down;
			vcc++; 
			if (vsc + 1 < window_columns - line_number_width) vsc++; 
			else voc++;
		}
	}
}

static inline void move_up(void) {
	if (not vcl) {
		lcl = 0; lcc = 0; vcl = 0; vcc = 0;
		vol = 0; voc = 0; vsl = 0; vsc = 0;
		return;
	}
	nat line_target = vcl - 1;
	while (vcc and vcl > line_target) move_left(); 
	do move_left(); while (vcc > vdc and vcl == line_target);
	if (vcc >= window_columns - line_number_width) { 
		vsc = window_columns - line_number_width; voc = vcc - vsc; 
	} 
	else { vsc = vcc; voc = 0; }
}

static inline void move_down(void) {
	nat line_target = vcl + 1;
	while (vcl < line_target) { 
		if (lcl == count - 1 and lcc == lines[lcl].count) return;
		move_right();
	}
	while (vcc < vdc and lcc < lines[lcl].count) {
		if (lines[lcl].data[lcc] == '\t' and vcc + (tab_width - (vcc % tab_width)) > vdc) return;
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

//static inline void jump_to(nat line, nat column) { jump_line(line); jump_column(column); }
static inline void move_begin(void) { while (vcc) move_left(); vdc = vcc; }

static inline void move_end(void) {
	while (lcc < lines[lcl].count and (vcc < wrap_width or not wrap_width)) move_right(); 
	vdc = vcc;
}

static inline void move_top(void) {
	lcl = 0; lcc = 0;
	vcl = 0; vcc = 0;
	vol = 0; voc = 0;
	vsl = 0; vsc = 0;
	vdc = 0;
}

static inline void move_bottom(void) {
	while (lcl < count - 1 or lcc < lines[lcl].count) move_down(); 
	vdc = vcc;
}

static inline void move_word_left(void) {
	do move_left();
	while (not(
		(not lcl and not lcc) or (
			(lcc < lines[lcl].count and isalnum(lines[lcl].data[lcc]))  and 
			(not lcc or not isalnum(lines[lcl].data[lcc - 1]))
		)
	));
	vdc = vcc;
}

static inline void move_word_right(void) {
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

	p->saved = saved;
	p->autosaved = autosaved;
	p->selecting = selecting;
	p->wrap_width = wrap_width;
	p->tab_width = tab_width;
	p->lcl = lcl;  p->lcc = lcc; 
	p->vcl = vcl;  p->vcc = vcc;
  	p->vol = vol;  p->voc = voc;
	p->vsl = vsl;  p->vsc = vsc; 
	p->vdc = vdc;  p->lal = lal;
	p->lac = lac;
}

static inline void require_logical_state(struct logical_state* pcond_in) {  
	struct logical_state* p = pcond_in;

	saved = p->saved;
	autosaved = p->autosaved;
	selecting = p->selecting;
	wrap_width = p->wrap_width;
	tab_width = p->tab_width;
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

	struct line* here = lines + lcl;
	if (c == '\n') {
		nat rest = here->count - lcc;
		here->count = lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, here->data + lcc, (size_t) rest);

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
			if (here->count + 1 >= here->capacity) 
				here->data = realloc(here->data, (size_t)(here->capacity = 8 * (here->capacity + 1)));
		} else {
			here->data = realloc(here->data, (size_t)(here->count + 1));
		}

		memmove(here->data + lcc + 1, here->data + lcc, (size_t) (here->count - lcc));
		here->data[lcc] = c;
		here->count++;
	}

	if (zero_width(c)) lcc++; 
	else { move_right(); vdc = vcc; }

	saved = 0;
	autosaved = 0;
	if (not should_record) return;

	if (zero_width(c)) {
		actions[head].text = realloc(actions[head].text, (size_t) actions[head].length + 1);
		actions[head].text[actions[head].length++] = c;
		record_logical_state(&(actions[head].post));
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
	struct line* here = lines + lcl;

	if (not lcc) {
		if (not lcl) return;
		move_left(); vdc = vcc;
		struct line* new = lines + lcl;

		if (not memory_safe) {
			if (new->count + here->count >= new->capacity)
				new->data = realloc(new->data, (size_t)(new->capacity = 8 * (new->capacity + here->count)));
		} else {
			new->data = realloc(new->data, (size_t)(new->count + here->count));
		}

		if (here->count) memcpy(new->data + new->count, here->data, (size_t) here->count);
		free(here->data);

		new->count += here->count;
		memmove(lines + lcl + 1, lines + lcl + 2, 
			sizeof(struct line) * (size_t)(count - (lcl + 2)));
		count--;

		if (memory_safe) lines = realloc(lines, sizeof(struct line) * (size_t)count);

		if (should_record) {
			deleted_length = 1;
			deleted_string = malloc(1);
			deleted_string[0] = '\n';
		}

	} else {
		nat save = lcc;
		move_left(); vdc = vcc;
		
		if (should_record) {
			deleted_length = save - lcc;
			deleted_string = malloc((size_t) deleted_length);
			memcpy(deleted_string, here->data + lcc, (size_t) deleted_length);
		}

		memmove(here->data + lcc, here->data + save, (size_t)(here->count - save));
		here->count -= save - lcc;

		if (memory_safe) here->data = realloc(here->data, (size_t)(here->count));
	}

	saved = 0;
	autosaved = 0;
	if (not should_record) return;

	record_logical_state(&new_action.post);
	new_action.type = delete_action;
	new_action.text = deleted_string;
	new_action.length = deleted_length;
	create_action(new_action);
}

static inline void initialize_buffer(void) {
	move_top();

	wrap_width = 80;     
	tab_width = 8; 
	capacity = 1;
	count = 1;
	action_count = 1;
	lines = calloc(1, sizeof(struct line));
	actions = calloc(1, sizeof(struct action));

	fixed_wrap = 1;
	show_line_numbers = 1; 
	saved = 1;
	autosaved = 1;
	use_txt_extension_when_absent = 1;
}


static inline void adjust_window_size(void) {
	static struct winsize window = {0}; 
	ioctl(1, TIOCGWINSZ, &window);

	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 27; window.ws_col = 70; }

	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen = realloc(screen, (size_t) (window_rows * window_columns * 4));	
	}
	//if (_.fixed_wrap and _.wrap_width != window_columns - line_number_width - 1) {
	//		 _.wrap_width = window_columns - line_number_width - 1; recalculate_position(); }

	window.ws_row = 27; window.ws_col = 50;
}


static inline void display(void) {
	adjust_window_size();
	
	nat length = 9;
	memcpy(screen, "\033[?25l\033[H", 9);

	nat sl = 0, sc = 0, vl = vol, vc = voc;
	struct logical_state state = {0};
	record_logical_state(&state);
	while (1) { 
		if (vcl <= 0 and vcc <= 0) break;
		if (vcl <= state.vol and vcc <= state.voc) break;
		move_left();
	}

	const double f = floor(log10((double) count)) + 1;
	const int line_number_digits = (int)f;
	line_number_width = show_line_numbers * (line_number_digits + 2);

 	nat line = lcl, col = lcc; 
	require_logical_state(&state); 

	do {
		if (line >= count) goto next_visual_line;

		if (show_line_numbers and vl >= vol and vl < vol + window_rows) {
			if (not col or (not sc and not sl)) 
				length += sprintf(screen + length, 
					"\033[38;5;%ldm%*ld\033[0m  ", 
					236L + (line == lcl ? 5 : 0), line_number_digits, line
				);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}
		do {
			if (col >= lines[line].count) goto next_logical_line;  
			
			char k = lines[line].data[col++];
			if (k == '\t') {

				if (vc + (tab_width - vc % tab_width) >= wrap_width and wrap_width) goto next_visual_line;
				do { 
					if (	vc >= voc and vc < voc + window_columns - line_number_width
					and 	vl >= vol and vl < vol + window_rows
					) {
						screen[length++] = ' ';
						sc++;
					}
					vc++;
				} while (vc % tab_width);

			} else {
				if (	vc >= voc and vc < voc + window_columns - line_number_width
				and 	vl >= vol and vl < vol + window_rows
				and 	(sc or visual(k))
				) { 
					screen[length++] = k;
					if (visual(k)) sc++;	
				}
				if (visual(k)) {
					if (vc >= wrap_width and wrap_width) goto next_visual_line; 
					vc++; 
				} 
			}

		} while (sc < window_columns - line_number_width or col < lines[line].count);

	next_logical_line:
		line++; col = 0;

	next_visual_line:
		if (vl >= vol and vl < vol + window_rows) {
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
	} while (sl < window_rows);

	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", vsl + 1, vsc + 1 + line_number_width);

	if (not fuzz) write(1, screen, (size_t) length);
}



static inline void open_file(const char* given_filename) {
	if (fuzz) return;
	if (not strlen(given_filename)) return;

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

	strlcpy(location, given_filename, sizeof filename); // todo:   seperate out these two things!!!
	sprintf(message, "read %lub", length);
}

static inline void emergency_save_to_file(void) { // todo: remove this. 
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

static inline void autosave(void) {    // todo: make this not use threads anymore. 
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
	
	for (nat i = 0; i < count; i++) {
		fwrite(lines[i].data, sizeof(char), (size_t) lines[i].count, file);
		if (i < count - 1) {
			fputc('\n', file);
		}
	}

	if (ferror(file)) {
		printf("emergency error: %s\n", strerror(errno));
		fclose(file);
		return;
	}

	fclose(file);
	autosaved = 1;
}

static void handle_signal_interrupt(int code) {   // have the program ignore interrupts. 
	if (fuzz) exit(1);

	printf(	"interrupt: caught signal SIGINT(%d), "
		"emergency saving...\n\r", 
		code);

	srand((unsigned)time(NULL));
	emergency_save_to_file();

	printf("press '.' to continue running process\n\r");
	int c = getchar(); 
	if (c != '.') exit(1);	
}


static inline void save(void) {  // abort the save if the user has not specified a file. to save an unnamed file, theyll use the create command.
	if (fuzz) return;

	if (not strlen(filename)) {

	prompt_filename:
		////// we need to get a string from the user some how, here.  the user needs to give the filename for the current file, somehow. 
		
		if (not strlen(filename)) { sprintf(message, "aborted save"); return; }
		if (not strrchr(filename, '.') and use_txt_extension_when_absent) strcat(filename, ".txt");
		if (file_exists(filename)) {
			strcpy(filename, ""); goto prompt_filename;
		}
	}

	FILE* file = fopen(filename, "w+");
	if (not file) {
		sprintf(message, "error: %s", strerror(errno));
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
		fclose(file);
		return;
	}

	fclose(file);
	sprintf(message, "wrote %lldb;%ldl", bytes, count);
	saved = 1;
}
/*
static inline void rename_file(void) {        // buggy!!!! fix me. 
	if (fuzz) return;

	char new[4096] = {0};
	prompt_filename:
	//prompt("rename to: ", new, sizeof new);
	if (not strlen(new)) { sprintf(message, "aborted rename"); return; }

	if (file_exists(new) ///and not confirmed("file already exists, overwrite", "overwrite", "no")) {
		strcpy(new, ""); goto prompt_filename;
	}

	if (rename(filename, new)) sprintf(message, "error: %s", strerror(errno));
	else {
		strncpy(filename, new, sizeof new);
		sprintf(message, "renamed to \"%s\"", filename);
	}
}
*/
static inline void interpret_escape_code(void) {         // get scrolling and mouse support working. please. its critical feature.
	static nat scroll_counter = 0;
	char c = read_stdin();
	if (c == '[') {
		c = read_stdin();
		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') { move_right(); vdc = vcc; }
		else if (c == 'D') { move_left(); vdc = vcc; }
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


static inline void prompt_jump_line(void) {                // instead, require the user to issue the "line XXX" command.
	char string_number[128] = {0};
	//prompt("line: ", string_number, sizeof string_number);
	nat line = atoi(string_number);
	if (line >= count) line = count - 1;
	jump_line(line);
	sprintf(message, "jumped to %ld %ld", lcl, lcc);
}

static inline void prompt_jump_column(void) {            // instead reqiure the user to issue the "column XXXX" command.
	char string_number[128] = {0};
	//prompt("column: ", string_number, sizeof string_number);
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

		if (line >= count or column > lines[line].count) {
			*out_length = 0;
			return NULL;
		}

		if (not memory_safe) {
			if (length + lines[line].count - column + 1 >= s_capacity) 
				string = realloc(string, (size_t) 
					(s_capacity = 8 * (s_capacity + length + lines[line].count - column + 1)));
		} else string = realloc(string, (size_t) (length + lines[line].count - column));

		if (lines[line].count - column) 
			memcpy(string + length, lines[line].data + column, (size_t)(lines[line].count - column));

		length += lines[line].count - column;

		if (not memory_safe) {
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

static inline void paste(void) {

 if (not fuzz) {

	FILE* file = popen("pbpaste", "r");
	if (not file) { sprintf(message, "error: paste: popen(): %s", strerror(errno)); return; }
	struct action new = {0};
	record_logical_state(&new.pre);
	char* string = malloc(256);
	nat s_capacity = 256, length = 0, c = 0;
	lac = lcc; lal = lcl;

	while ((c = fgetc(file)) != EOF) {

		if (not memory_safe) {
			if (length + 1 >= s_capacity) 
				string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + 1)));
		} else string = realloc(string, (size_t) (length + 1));

		string[length++] = (char) c;
		insert((char)c, 0);
	}
	pclose(file);
	sprintf(message, "pasted %ldb", length);
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
 	lac = lcc; lal = lcl;
 	string = realloc(string, (size_t) (length + 1));
 	string[length++] = (char) 'A';
 	insert((char)'A', 0);
 	sprintf(message, "pasted %ldb", length);
 	record_logical_state(&new.post);
 	new.type = paste_action;
 	new.text = string;
 	new.length = length;
 	create_action(new);
 }
}

static inline void cut_selection(void) {
	if (lal < lcl) goto anchor_first;
	if (lcl < lal) goto cursor_first;
	if (lac < lcc) goto anchor_first;
	if (lcc < lac) goto cursor_first;
	return;
cursor_first: ;
	nat line = lcl, column = lcc;
	while (lcl < lal or lcc < lac) move_right();
	lal = line; lac = column;
anchor_first:
	while (lal < lcl or lac < lcc) delete(0);
}

static inline void cut(bool should_execute) { 
	if (lal >= count or lac > lines[lal].count) return;
	struct action new = {0};
	record_logical_state(&new.pre);
	nat deleted_length = 0;
	char* deleted_string = get_selection(&deleted_length);
	cut_selection();
	sprintf(message, "deleted %ldb", deleted_length);
	record_logical_state(&new.post);
	new.type = cut_action;
	new.text = deleted_string;
	new.length = deleted_length;
	create_action(new);

	if (not deleted_length) insert_string(message, (nat) strlen(message));
	else if (should_execute) {
		// execute the command "deleted_string:deleted_length". 
	}
}

static inline void copy(void) {
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

static inline void replay_action(struct action a) { // inline me
	require_logical_state(&a.pre);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action or a.type == paste_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_action) cut_selection();
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {    // inline me
	require_logical_state(&a.post);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_action) {
		while (lcc > a.pre.lcc or lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} require_logical_state(&a.pre);
}

static inline void undo(void) {
	if (not head) return;
	reverse_action(actions[head]);
	head = actions[head].parent;
}

static inline void redo(void) {                   ////   how do we make an undo/redo tell you iff there is a tree node in the undo tree?...
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	replay_action(actions[head]);
}

static inline void alternate_incr(void) {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void alternate_decr(void) {
	if (actions[head].choice) actions[head].choice--;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void execute(char c, char p) {

#define a   if (not selecting) { lal = al; lac = ac; } 

	if (mode == 1) {
		if (c == 'c' and p == 'h') { undo(); mode = 2; }
		else if (c == 27 and stdin_is_empty()) mode = 2;
		else if (c == 27) interpret_escape_code();
		else if (c == 127) delete(1);
		else if (c == '\r') insert('\n', 1);
		else insert(c, 1);

	} else if (mode == 2) {
		const nat al = lcl, ac = lcc;

		if (c == ' ') {} 

		else if (c == 'l' and p == 'e') {}   // 
		else if (c == 'k' and p == 'e') {}
		else if (c == 'i' and p == 'e') { move_bottom(); a }
		else if (c == 'p' and p == 'e') { move_top(); a }
		else if (c == 'n' and p == 'e') { move_begin(); a }
		else if (c == 'o' and p == 'e') { move_end(); a }

		else if (c == 'u' and p == 'e') alternate_decr();
		else if (c == 'r' and p == 'h') alternate_incr();

		else if (c == 'm' and p == 'h') copy();
		else if (c == 'c' and p == 'h') paste(); 
		else if (c == 't' and p == 'h') {}             //submit response!
		else if (c == 's' and p == 'h') {}
		else if (c == 'a' and p == 'h') {}      // swap anchors?...
		else if (c == 'd' and p == 'h') {}

		else if (c == 'a') { selecting = not selecting; lal = al; lac = ac; }		
		else if (c == 's') save();
		else if (c == 'd') delete(1);
		else if (c == 'r') cut(0);
		else if (c == 't') mode = 1;
		else if (c == 'c') undo();
		else if (c == 'm') cut(1);

		else if (c == 'k') redo();
		else if (c == 'o') { move_word_right(); a }
		else if (c == 'l') { move_word_left(); a }
		else if (c == 'p') { move_up(); a }
		else if (c == 'u') { move_down(); a }
		else if (c == 'i') { move_right(); vdc = vcc; a }
		else if (c == 'n') { move_left(); vdc = vcc; a }
		
		else if (c == '#') fixed_wrap = not fixed_wrap;   // rebind this one.
 
		else if (c == 'q') { if (saved) mode = 0; }
		else if (c == 27 and stdin_is_empty()) {}
		else if (c == 27) interpret_escape_code();


	} else mode = 2;
}


static inline void editor(void) {
	struct termios terminal;
	if (not fuzz) {
		terminal = configure_terminal();
		write(1, "\033[?1049h\033[?1000h\033[7l", 20);
	} 
	char p = 0, c = 0;
loop:	display();
	c = read_stdin(); 
	if (fuzz and not c) goto done;
	execute(c, p);
	p = c;
	if (mode) goto loop;
done:	free(screen); screen = NULL;
	window_rows = 0; window_columns = 0;
	if (not fuzz) {
		write(1, "\033[?1049l\033[?1000l\033[7h", 20);	
		tcsetattr(0, TCSAFLUSH, &terminal);
	}
}

#if fuzz && !use_main

int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {
	create_empty_buffer();
	fuzz_input_index = 0; 
	fuzz_input = input;
	fuzz_input_count = size;
	editor();
	return 0;
}

#else

int main(const int argc, const char** argv) {
	initialize_buffer();
	if (argc == 2) open_file(argv[1]);
	signal(SIGINT, handle_signal_interrupt);
	editor();
}

#endif




















































// ---------------------------------------------------------------------------------------------------1747 before changes

//static pthread_t autosave_thread;

//pthread_mutex_init(&mutex, NULL);
		//pthread_mutex_lock(&mutex);
		//pthread_create(&autosave_thread, NULL, &autosaver, NULL);


//pthread_mutex_unlock(&mutex);
		//pthread_detach(autosave_thread);
		//pthread_mutex_destroy(&mutex);




/*
static void autosaver(void) {
	while (1) {
		//sleep(autosave_frequency);
		//pthread_mutex_lock(&mutex);
		//if (not mode) break;

		if (not autosaved) autosave();

		//pthread_mutex_unlock(&mutex);
	} 
}
*/

















/*
	sprintf(message, "redoing %ld ", actions[head].type);
	if (actions[head].count != 1) 
		sprintf(message + strlen(message), "%ld %ld", actions[head].choice, actions[head].count);
*/

/*
	sprintf(message, "undoing %ld ", actions[head].type);
	if (actions[head].count != 1) 
		sprintf(message + strlen(message), "%ld %ld", actions[head].choice, actions[head].count);
*/





/*
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
};

char str[] = {0};

if (running_crash) {
	fuzz_input_index = 0; 
	fuzz_input = (const uint8_t*) str;
	fuzz_input_count = sizeof str;
}


*/





//working_index = active_index;
	//while (buffer_count) 

	//close_active_buffer();

	// destroy(0);


	//free(buffers); buffers = NULL;





/*
static inline void open_directory(void) {
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

static inline void change_directory(void) {

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

static inline void file_select(void) {
	char* line = strndup(lines[lcl].data, (size_t) lines[lcl].count);
	strlcpy(selected_file, cwd, sizeof selected_file);
	strlcat(selected_file, line, sizeof selected_file);
	free(line);
	sprintf(message, "selected: %s", selected_file);
}
*/






/*
static inline void prompt_open(void) {
	char new_filename[4096] = {0};
	//prompt("open: ", new_filename, sizeof new_filename);
	if (not strlen(new_filename)) { sprintf(message, "aborted open"); return; }
	open_file(new_filename);
}
*/






/*
static inline nat display_proper(
	nat length, nat* total, 
	int line_number_digits
) {
	

	nat sl = 0, sc = 0, vl = vol, vc = voc;
	struct logical_state state = {0};
	record_logical_state(&state);
	while (1) { 
		if (vcl <= 0 and vcc <= 0) break;
		if (vcl <= state.vol and vcc <= state.voc) break;
		move_left();
	}


	const double f = floor(log10((double) count)) + 1;
	const int line_number_digits = (int)f;
	line_number_width = show_line_numbers * (line_number_digits + 2);




 	nat line = lcl, col = lcc; 
	require_logical_state(&state); 

	do {
		if (line >= count) goto next_visual_line;

		if (show_line_numbers and vl >= vol and vl < vol + swl) {
			if (not col or (not sc and not sl)) 
				length += sprintf(screen + length, 
					"\033[38;5;%ldm%*ld\033[0m  ", 
					236L + (line == lcl ? 5 : 0), line_number_digits, line
				);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}
		do {
			if (col >= lines[line].count) goto next_logical_line;  
			
			char k = lines[line].data[col++];
			if (k == '\t') {

				if (vc + (tab_width - vc % tab_width) >= wrap_width and wrap_width) goto next_visual_line;
				do { 
					if (	vc >= voc and vc < voc + swc
					and 	vl >= vol and vl < vol + swl
					) {
						screen[length++] = ' ';
						sc++;
					}
					vc++;
				} while (vc % tab_width);

			} else {
				if (	vc >= voc and vc < voc + swc
				and 	vl >= vol and vl < vol + swl
				and 	(sc or visual(k))
				) { 
					screen[length++] = k;
					if (visual(k)) sc++;	
				}
				if (visual(k)) {
					if (vc >= wrap_width and wrap_width) goto next_visual_line; 
					vc++; 
				} 
			}

		} while (sc < swc or col < lines[line].count);

	next_logical_line:
		line++; col = 0;

	next_visual_line:
		if (vl >= vol and vl < vol + swl) {
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
	} while (sl < swl);

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

	nat save_col = lcc, save_line = lcl;
	move_top();
	move_end();
	while (lcc) delete(0);
	insert_string(status, status_length);
	jump_to(save_line, save_col);
}

//nat save_wi = working_index;
	//working_index = active_index;

	//_.sbl = 0;
	//_.sbc = ;
	//_.sel = ;
	//_.sec = ;
	//_.swl = window_rows;
	//_.swc = window_columns - line_number_width;

	//nat cursor_line = 0, cursor_col = 0;


*/







/*command_mode: mode = 2;

		char string[4096] = {0};
		// prompt(" • ", string, sizeof string);
		
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
			else if (equals(command, "rename")) rename_file();  
			else if (equals(command, "save")) save();
			else if (equals(command, "duplicate")) { duplicate(); }
			else if (equals(command, "autosave")) autosave();
			else if (equals(command, "clearmessage")) memset(message, 0, sizeof message);
			else if (equals(command, "numbers")) show_line_numbers = not show_line_numbers;
			else if (equals(command, "cut")) cut();
			else if (equals(command, "delete")) delete(1);
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
			else if (equals(command, "quit")) mode = 0;
			else if (equals(command, "mode0")) mode = 0;
			else if (equals(command, "mode1")) mode = 1;
			else if (equals(command, "mode2")) mode = 2;
			else if (equals(command, "goto")) {}
			else if (equals(command, "branch")) {}
			else if (equals(command, "incr")) {}
			else if (equals(command, "setzero")) {}
			else if (equals(command, "toggle_selecting")) selecting = not selecting;
			else if (equals(command, "toggle_fixed_wrap")) fixed_wrap = not fixed_wrap;
			else if (equals(command, "disable_wrap")) { fixed_wrap = 0; wrap_width = 0; recalculate_position(); }
			else if (equals(command, "enable_wrap")) { fixed_wrap = 0; wrap_width = 80; recalculate_position(); }
			else {
				sprintf(message, "command not recognized: %s", command);
				break;
			}
		}
	
		for (nat i = 0; i < command_count; i++) free(commands[i]);
		free(commands);
		free(d);
	*/









/*
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
}*/





/*
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
*/


/*
static inline void insertdt(void) {
	char datetime[16] = {0};
	get_datetime(datetime);
	insert_string(datetime, 14);
}
*/

/*
static inline void recalculate_position(void) {
	nat save_lcl = lcl, save_lcc = lcc;
	move_top();
	jump_to(save_lcl, save_lcc);
}*/




/*
static inline void undo_silent(void) {
	if (not head) return;
	reverse_action(actions[head]);
	head = actions[head].parent;
}
*/







	//if (save_wi == active_index) {
	//	cursor_line = _.sbl + _.vsl + 1;
	//	cursor_col = _.sbc + _.vsc + 1;
	//}

	// working_index = buffer_count;

	// f = floor(log10((double) _.count)) + 1;
	// line_number_digits = (int)f;
	// line_number_width = show_line_numbers * (line_number_digits + 2);
	
	//_.sbl = window_rows - _sn_rows;
	//_.sbc = line_number_width;
	//_.sel = window_rows;
	//_.sec = window_columns;
	//_.swl = _.sel - _.sbl;
	//_.swc = _.sec - _.sbc;

	//  if (save_wi != buffer_count)
	//	add_status(active_index); 

	// length += sprintf(screen + length, "\033[%ld;%ldH",  window_rows - _sn_rows + 1L,  1L ); 
	// length = display_proper(length, &total, line_number_digits);

	//if (save_wi == buffer_count and active_index != buffer_count) {
	//	cursor_line = _.sbl + _.vsl + 1;
	//	cursor_col = _.sbc + _.vsc + 1;
	//}


	

	//working_index = save_wi;


/*
static inline void print_above_textbox(char* message) {
	const nat save_index = working_index;
	working_index = buffer_count;
	insert('\n', 1);
	insert_string(message, (nat) strlen(message));
	working_index = save_index;
}
*/

/*
static inline void prompt(const char* prompt_message, char* out, nat out_size)  {
	const nat save_index = working_index;
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

	move_top();     // ????????..... hmmmmmmmmmmmmmmm

	working_index = save_index;
}*/

/*
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

static inline void create_sn_buffer(void) {
	buffer_count = 0; active_index = 0; working_index = 0;
	buffers = calloc(1, sizeof(struct buffer));
	initialize_buffer();
}

*/
//static inline void create_empty_buffer(void) {
	//buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count + 2));
	//buffers[buffer_count + 1] = buffers[buffer_count];
	//working_index = active_index = buffer_count;
	//buffer_count++;
	
//}
/*


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
*/

/*
static inline void close_active_buffer(void) {
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


static inline void move_to_previous_buffer(void) {
	if (active_index) active_index--; else active_index = buffer_count;
	working_index = active_index;
}

static inline void move_to_next_buffer(void) {
	if (active_index < buffer_count) active_index++; else active_index = 0;
	working_index = active_index;
}
*/












/*
static inline void run_shell_command(const char* command) {
	if (fuzz) return;

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
	}
	pclose(f);
	lal = lcl; lac = lcc;
	sprintf(message, "output %ldb", length);
	insert_string(output, length);
}

static inline void prompt_run(void) {
	char command[4096] = {0};
	//prompt("run(2>&1): ", command, sizeof command);
	if (not strlen(command)) { sprintf(message, "aborted run"); return; }
	sprintf(message, "running: %s", command);
	run_shell_command(command);
}
*/







// we should be using     (c == keys[34] and (p == keys[35] or keys[35] == 27))





/*
static inline bool is_directory(const char *path) {
	struct stat s;
	if (stat(path, &s)) return false;
	return S_ISDIR(s.st_mode);
}
*/



// todo: make these part of a config struct..?  and   also  config file / parameters.


