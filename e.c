#include <iso646.h>    // Daniel Warren Riaz Rehman's minimalist terminal-based text editor.
#include <termios.h>   //
#include <stdio.h>     //     designed with reliability, minimalism, 
#include <stdlib.h>    //     simplicity, and ergonimics in mind.
#include <string.h>    //
#include <unistd.h>    //          written on 2101177.005105
#include <sys/ioctl.h> //         finished on 2101181.131817
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <errno.h>

// current implementing:     copy-paste,  undo-tree, 
// --------------------------------------------------------


//        multiple buffers   :   completed.



//	  copy-paste         :   incomplete.

//	  undo-tree          :   incomplete.


// --------------------------------------------------------


struct line { char* data; int count, capacity; };


struct file_data {
	struct line* lines;
	int 
		saved, mode, 
		count, capacity, 
		scroll_counter, line_number_width, needs_display_update, 
		lcl, lcc, 	vcl, vcc,  	vol, voc, 	vsl, vsc, 	vdc,     lal,  lac,
		line_number_color, status_bar_color, alert_prompt_color, 
		info_prompt_color, default_prompt_color, 
		wrap_width, tab_width, 
		scroll_speed, show_status, show_line_numbers, 
		use_txt_extension_when_absent, padding0;
	char message[4096];
	char filename[4096];
};

// struct action {
// 	struct file_data data;
// };

// #define no_action 0
// #define insert_action 1
// #define backspace_action 2
// #define paste_text_action 3
// #define delete_text_action 4
// #define set_anchor_action 5




// preferences and configurations:
static int alert_prompt_color = 196;
static int info_prompt_color = 45;
static int default_prompt_color = 214;
static int line_number_color = 236;
static int status_bar_color = 245;

static int wrap_width = 120;
static int tab_width = 8;
static int scroll_speed = 4;

static int show_status = 1;
static int show_line_numbers = 1;
static int use_txt_extension_when_absent = 1;

static char left_exit[2] = {'d','f'}, right_exit[2] = {'k','j'};


// application global data:
static int window_rows = 0;
static int window_columns = 0;
static char* screen = NULL;

static char* clipboard = NULL;
static int clipboard_length = 0;


// textbox data:
static char* tb_data = NULL;
static int tb_count = 0;
static int tb_capacity = 0;
static int tb_prompt_length = 0;
static int tb_c = 0;
static int tb_vc = 0;
static int tb_vs = 0;
static int tb_vo = 0;


// current tab data:
static char message[4096] = {0};
static char filename[4096] = {0};
static int saved = 0, mode = 0;
static struct line* lines = NULL;
static int count = 0, capacity = 0;
static int scroll_counter = 0;
static int line_number_width = 0;
static int needs_display_update = 0;
static int lcl = 0, lcc = 0, vcl = 0, vcc = 0, vol = 0, 
           voc = 0, vsl = 0, vsc = 0, vdc = 0, lal = 0, lac = 0;


// all tab data:
static struct file_data* buffers = NULL;
static int buffer_count = 0;
static int active_buffer = 0;



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
		if (vcc > window_columns - 1 - line_number_width) { 
			vsc = window_columns - 1 - line_number_width; 
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
	int line_target = vcl - 1;
	while (vcc and vcl > line_target) move_left(0); 
	do move_left(0); while (vcc > vdc and vcl == line_target);
	if (vcc > window_columns - 1 - line_number_width) { vsc = window_columns - 1 - line_number_width; voc = vcc - vsc; } 
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

static inline void move_down_10() {
	for (int i = 0; i < 10; i++) move_down();
}

static inline void move_up_10() {
	for (int i = 0; i < 10; i++) move_up();
}

static inline void insert(char c) {
	struct line* this = lines + lcl;
	if (c == 10) {
		int rest = this->count - lcc;
		this->count = lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, this->data + lcc, (size_t) rest);
		if (count + 1 > capacity) 
			lines = realloc(lines, sizeof(struct line) * (size_t)(capacity = 8 * (capacity + 1)));
		memmove(lines + lcl + 2, lines + lcl + 1, 
			sizeof(struct line) * (size_t)(count - (lcl + 1)));
		lines[lcl + 1] = new;
		count++;
	} else {
		if (this->count + 1 > this->capacity) 
			this->data = realloc(this->data, (size_t)(this->capacity = 8 * (this->capacity + 1)));
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

		if (new->count + this->count > new->capacity)
			new->data = realloc(new->data, (size_t)(new->capacity = 8 * (new->capacity + this->count)));
        
		if (this->count) memcpy(new->data + new->count, this->data, (size_t) this->count);

		new->count += this->count;
		memmove(lines + lcl + 1, lines + lcl + 2, 
			sizeof(struct line) * (size_t)(count - (lcl + 2)));
		count--;
	} else {
		int save = lcc;
		move_left(1);
		memmove(this->data + lcc, this->data + save, (size_t)(this->count - save));
		this->count -= save - lcc;
	}
	saved = 0;
}

static inline void adjust_window_size() {
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);

	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 20; window.ws_col = 40; }

	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col;
		screen = realloc(screen, sizeof(char) * (size_t) (window_rows * window_columns * 4));
	}

	if (not wrap_width) wrap_width = window_columns - 1;
}

static inline void get_datetime(char buffer[16]) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm* tm_info = localtime(&tv.tv_sec);
	strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

// static inline void get_full_datetime(char buffer[32]) {
// 	struct timeval tv;
// 	gettimeofday(&tv, NULL);
// 	struct tm* time = localtime(&tv.tv_sec);
// 	strftime(buffer, 31, "CE%Y%m%d%u.%H%M%S", time);
// }

static inline void display() {
	
	int length = 9; 
	memcpy(screen, "\033[?25l\033[H", 9);

	int line = 0, col = 0;
	int vl = 0, vc = 0;
	int sl = 0, sc = 0;

	double f = floor(log10((double) count)) + 1;
	int line_number_digits = (int)f;
	line_number_width = show_line_numbers * (line_number_digits + 2);

	do {
		if (line >= count) goto next_visual_line;

		if (show_line_numbers and vl >= vol and vl < vol + window_rows - show_status) {
			if (not col) length += sprintf(screen + length, "\033[38;5;%dm%*d\033[0m  ", line_number_color, line_number_digits, line + 1);
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
		length += sprintf(screen + length, "\033[7m\033[38;5;%dm", status_bar_color);

		char datetime[16] = {0};
		get_datetime(datetime);
		int status_length = sprintf(screen + length, " %s %d %d %d %d %d %s %c  %s",
			datetime, 
			mode, 
			active_buffer, buffer_count,
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
    
	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", vsl + 1, vsc + 1 + line_number_width);
	write(1, screen, (size_t) length);
}

static inline void textbox_move_left() {
	if (not tb_c) return;
	do tb_c--; while (tb_c and zero_width(tb_data[tb_c]));
	tb_vc--; 
	if (tb_vs) tb_vs--; else if (tb_vo) tb_vo--;
}

static inline void textbox_move_right() {
	if (tb_c >= tb_count) return;
	tb_vc++; 
	if (tb_vs < window_columns - 1 - tb_prompt_length) tb_vs++; else tb_vo++;
	do tb_c++; while (tb_c < tb_count and zero_width(tb_data[tb_c]));
}

static inline void textbox_insert(char c) {
	if (tb_count + 1 > tb_capacity) 
		tb_data = realloc(tb_data, (size_t)(tb_capacity = 8 * (tb_capacity + 1)));
	memmove(tb_data + tb_c + 1, tb_data + tb_c, (size_t) (tb_count - tb_c));
	tb_data[tb_c] = c;
	tb_count++;
	if (zero_width(c)) tb_c++; else textbox_move_right();
}

static inline void textbox_delete() {
	if (not tb_c) return;
	int save = tb_c;
	textbox_move_left();
	memmove(tb_data + tb_c, tb_data + save, (size_t)(tb_count - save));
	tb_count -= save - tb_c;
}

static inline void textbox_display(const char* prompt, int prompt_color) {
	int length = sprintf(screen, "\033[?25l\033[%d;1H\033[38;5;%dm%s\033[m", window_rows, prompt_color, prompt);
	int col = 0, vc = 0, sc = tb_prompt_length;
	while (sc < window_columns and col < tb_count) {
		char k = tb_data[col];
		if (vc >= tb_vo and vc < tb_vo + window_columns - 1 - tb_prompt_length and (sc or visual(k))) {
			screen[length++] = k;
			if (visual(k)) { sc++; }
		}
		if (visual(k)) vc++; 
		col++;
	} 

	for (int i = sc; i < window_columns; i++) 
		screen[length++] = ' ';

	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", window_rows, tb_vs + 1 + tb_prompt_length);
	write(1, screen, (size_t) length);
}

static inline void prompt(const char* prompt_message, int color, char* out, int out_size) {
	tb_prompt_length = (int) strlen(prompt_message);
	do {
		adjust_window_size();
		textbox_display(prompt_message, color);
		char c = 0;
		read(0, &c, 1);
		if (c == '\r') break;
		if (c == '\t') { /*tab complete*/ }
		else if (c == 27) {
			read(0, &c, 1);
			if (c == '[') {
				read(0, &c, 1);
				if (c == 'A') {}
				else if (c == 'B') {}
				else if (c == 'C') textbox_move_right();
				else if (c == 'D') textbox_move_left();
			} else if (c == 27) { tb_count = 0; break; }
		} else if (c == 127) textbox_delete();
		else textbox_insert(c);
	} while (1);
	if (tb_count > out_size) tb_count = out_size;
	memcpy(out, tb_data, (size_t) tb_count);
	memset(out + tb_count, 0, (size_t) out_size - (size_t) tb_count);
	out[out_size - 1] = 0;
	free(tb_data); tb_data = NULL;
	tb_count = 0; tb_capacity = 0;
	tb_c = 0; tb_vo = 0; tb_vc = 0; tb_vs = 0;
}

static inline void print_above_textbox(char* write_message, int color) {
	int length = sprintf(screen, "\033[%d;1H\033[38;5;%dm%s\033[m", window_rows - 1, color, write_message);
	write(1, screen, (size_t) length);
}

static inline int confirmed(const char* question) {
	char prompt_message[4096] = {0};
	sprintf(prompt_message, "%s? (yes/no): ", question);

	while (1) {
		char response[10] = {0};
		prompt(prompt_message, alert_prompt_color, response, sizeof response);
		
		if (not strncmp(response, "yes", 4)) return 1;
		else if (not strncmp(response, "no", 3)) return 0;
		else print_above_textbox("please type \"yes\" or \"no\".", 214L);
	}
}

static inline void store_current_data_to_buffer() {
	if (not buffer_count) return;

	const int b = active_buffer;
	
	buffers[b].saved = saved;
	buffers[b].mode = mode;

	buffers[b].scroll_counter = scroll_counter;
	buffers[b].line_number_width = line_number_width;
	buffers[b].needs_display_update = needs_display_update;

	buffers[b].capacity = capacity;
	buffers[b].count = count;
	buffers[b].lines = lines;

	buffers[b].lcl = lcl; 
	buffers[b].lcc = lcc; 
	buffers[b].vcl = vcl;
	buffers[b].vcc = vcc; 
	buffers[b].vol = vol; 
	buffers[b].voc = voc; 
	buffers[b].vsl = vsl; 
	buffers[b].vsc = vsc; 
	buffers[b].vdc = vdc;
	buffers[b].lal = lal;
	buffers[b].lac = lac;

	// buffers[b].alert_prompt_color = alert_prompt_color;
	// buffers[b].info_prompt_color = info_prompt_color;
	// buffers[b].default_prompt_color = default_prompt_color;
	// buffers[b].line_number_color = line_number_color;
	// buffers[b].status_bar_color = status_bar_color;

	buffers[b].wrap_width = wrap_width;
	buffers[b].tab_width = tab_width;
	buffers[b].scroll_speed = scroll_speed;
	buffers[b].show_status = show_status;
	buffers[b].show_line_numbers = show_line_numbers;
	buffers[b].use_txt_extension_when_absent = use_txt_extension_when_absent;

	memcpy(buffers[b].message, message, sizeof message);
	memcpy(buffers[b].filename, filename, sizeof filename);
}

static inline void load_buffers_data_into_registers() {
	if (not buffer_count) return;

	struct file_data this = buffers[active_buffer];
	
	saved = this.saved;
	mode = this.mode;

	scroll_counter = this.scroll_counter;
	line_number_width = this.line_number_width;
	needs_display_update = this.needs_display_update;

	capacity = this.capacity;
	count = this.count;
	lines = this.lines;

	lcl = this.lcl; 
	lcc = this.lcc; 
	vcl = this.vcl;
	vcc = this.vcc; 
	vol = this.vol; 
	voc = this.voc; 
	vsl = this.vsl; 
	vsc = this.vsc; 
	vdc = this.vdc;

	lal = this.lal;
	lac = this.lac;

	// alert_prompt_color = this.alert_prompt_color;
	// info_prompt_color = this.info_prompt_color;
	// default_prompt_color = this.default_prompt_color;
	// line_number_color = this.line_number_color;
	// status_bar_color = this.status_bar_color;

	wrap_width = this.wrap_width;
	tab_width = this.tab_width;
	scroll_speed = this.scroll_speed;
	show_status = this.show_status;
	show_line_numbers = this.show_line_numbers;
	use_txt_extension_when_absent = this.use_txt_extension_when_absent;

	memcpy(message, this.message, sizeof message);
	memcpy(filename, this.filename, sizeof filename);
}

static inline void initialize_current_data_registers() {
	
	saved = 1;
	mode = 0;
	scroll_counter = 0;
	line_number_width = 0;
	needs_display_update = 1;
	capacity = 1;
	count = 1;
	lines = calloc((size_t) (capacity), sizeof(struct line));

	lcl = 0; lcc = 0; vcl = 0; vcc = 0; vol = 0; 
	voc = 0; vsl = 0; vsc = 0; vdc = 0; lal = 0; lac = 0;

	alert_prompt_color = 196;
	info_prompt_color = 45;
	default_prompt_color = 214;
	line_number_color = 236;
	status_bar_color = 245;

	wrap_width = 120;
	tab_width = 8;
	scroll_speed = 4;

	show_status = 1;
	show_line_numbers = 1;
	use_txt_extension_when_absent = 1;

	memset(message, 0, sizeof message);
	memset(filename, 0, sizeof filename);
}

static inline void create_empty_buffer() {
	store_current_data_to_buffer();
	buffers = realloc(buffers, sizeof(struct file_data) * (size_t)(buffer_count + 1));
	buffers[buffer_count] = (struct file_data) {0};
	initialize_current_data_registers();
	active_buffer = buffer_count;
	buffer_count++;
	store_current_data_to_buffer();
}

static inline void close_active_buffer() {
	buffer_count--;
	memmove(buffers + active_buffer, buffers + active_buffer + 1, 
		sizeof(struct file_data) * (size_t)(buffer_count - active_buffer));
	if (active_buffer >= buffer_count) active_buffer = buffer_count - 1;
	buffers = realloc(buffers, sizeof(struct file_data) * (size_t)(buffer_count));
	load_buffers_data_into_registers();
}

static inline void move_to_next_buffer() {
	store_current_data_to_buffer(); 
	if (active_buffer) active_buffer--; 
	load_buffers_data_into_registers();
}

static inline void move_to_previous_buffer() {
	store_current_data_to_buffer(); 
	if (active_buffer < buffer_count - 1) active_buffer++; 
	load_buffers_data_into_registers();
}

static inline void open_file(const char* given_filename) {
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
	char* buffer = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(buffer, sizeof(char), length, file);
	create_empty_buffer();
	for (size_t i = 0; i < length; i++) insert(buffer[i]);
	free(buffer); fclose(file);
	saved = 1; mode = 1; move_top();
	strcpy(filename, given_filename);
	store_current_data_to_buffer();
	sprintf(message, "read %lub", length);
}

static inline void prompt_open() {
	char new_filename[4096] = {0};
	prompt("open: ", default_prompt_color, new_filename, sizeof new_filename);
	if (not strlen(new_filename)) { sprintf(message, "aborted open"); return; }
	open_file(new_filename);
}

static inline void save() {

	if (not strlen(filename)) {
		prompt("save as: ", default_prompt_color, filename, sizeof filename);
		if (not strlen(filename)) { sprintf(message, "aborted save"); return; }
		if (not strrchr(filename, '.') and use_txt_extension_when_absent) strcat(filename, ".txt");
		if (file_exists(filename) and not confirmed("file already exists, overwrite")) {
			strcpy(filename, "");
			sprintf(message, "aborted save");
			return;
		}
	}

	FILE* file = fopen(filename, "w+");
	if (not file) {
		sprintf(message, "error: %s", strerror(errno));
		strcpy(filename, "");
		return;

	} else {
		unsigned long long bytes = 0;
		for (int i = 0; i < count; i++) {
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

		} else {
			fclose(file);
			sprintf(message, "wrote %lldb;%dl", bytes, count);
			saved = 1;
		}
	}
}

static inline void rename_file() {
	char new[4096] = {0};
	prompt("rename to: ", default_prompt_color, new, sizeof new);
	if (not strlen(new)) { sprintf(message, "aborted rename"); return; }

	if (file_exists(new) and not confirmed("file already exists, overwrite")) {
		sprintf(message, "aborted rename"); return;
	}

	if (rename(filename, new)) sprintf(message, "error: %s", strerror(errno));
	else {
		strncpy(filename, new, sizeof new);
		sprintf(message, "renamed to \"%s\"", filename);
	}
}

static inline void interpret_escape_code() {
	char c = 0;
	read(0, &c, 1);
	if (c == 27) mode = 1;
	else if (c == '[') {
		read(0, &c, 1);
		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') move_right(1);
		else if (c == 'D') move_left(1);
		else if (c == 32) { 
			for (int i = 0; i < 4; i++) {
				read(0, &c, 1);
			}
		}
		else if (c == 77) {
			read(0, &c, 1);
			if (c == 97) {
				read(0, &c, 1); read(0, &c, 1);
				scroll_counter++;
				if (scroll_counter == scroll_speed) {
					move_down();
					scroll_counter = 0;
				} else needs_display_update = 0;
			} else if (c == 96) {
				read(0, &c, 1); read(0, &c, 1);
				scroll_counter++;
				if (scroll_counter == scroll_speed) {
					move_up();
					scroll_counter = 0;
				} else needs_display_update = 0;
			}
		}
	} 
}

// static inline void undo() {

// if (not head->parent) return;

// reverse_action();

// if (file->head->parent->count == 1) {
// sprintf(file->message, "undoing %s", action_name(file->head->type));

// } else {
// sprintf(file->message, "selected #%d from %d histories: undoing %s",
//         file->head->parent->choice, file->head->parent->count,
//         action_name(file->head->type));
// }

// file->head = file->head->parent;
// }

// static inline void redo() {
        
//     if (not file->head->count) return;
    
//     file->head = file->head->children[file->head->choice];
    
//     if (file->head->parent->count == 1) {
//         sprintf(file->message, "redoing %s", action_name(file->head->type));
        
//     } else {
//         sprintf(file->message, "selected #%d from %d histories: redoing %s",
//                 file->head->parent->choice, file->head->parent->count,
//                 action_name(file->head->type));
//     }
    
//     replay_action();
// }

// static inline void alternate_up() {
    
//     if (file->head->parent and file->head->parent->choice + 1 < file->head->parent->count) {
//         undo();
//         file->head->choice++;
//         redo();
//     }
// }

// static inline void alternate_down() {
//     if (file->head->parent and file->head->parent->choice) {
//         undo();
//         file->head->choice--;
//         redo();
//     }
// }

static inline void jump_line() {
	char string_number[128] = {0};
	prompt("line: ", default_prompt_color, string_number, sizeof string_number);
	int line = atoi(string_number);

	sprintf(message, "UNIMPLEMENTED FUNCTION");
}

static inline void jump_column() {
	char string_number[128] = {0};
	prompt("column: ", default_prompt_color, string_number, sizeof string_number);
	int column = atoi(string_number);

	sprintf(message, "UNIMPLEMENTED FUNCTION");
}

static inline void show_buffer_list() {
	char list[4096] = {0};
	int length = 0;
	
	for (int i = 0; i < buffer_count; i++) {
		length += sprintf(list + length, "[%d:%s] ", i, buffers[i].filename);
	}
	print_above_textbox(list, info_prompt_color);
	needs_display_update = 0;
}

static inline int get_numeric_option_value(const char* option) {
	char string_number[128] = {0};
	prompt(option, default_prompt_color, string_number, sizeof string_number);
	int value = atoi(string_number);
	sprintf(message, "%s set to %d", option, value);
	return value;
}

static inline int is_exit_sequence(char c, char p) {
	return (c == left_exit[1] and p == left_exit[0]) or
	       (c == right_exit[1] and p == right_exit[0]);
}


static inline void paste() {
	// if (not use_system_clipboard) return;
	// char buffer[128] = {0};
	FILE* file = popen("pbpaste", "r");
	int c;
	while ((c = fgetc(file)) != EOF) insert((char)c);
	pclose(file);
}

static inline void copy() {



	if (lal < lcl) goto anchor_first;
	if (lcl < lal) goto cursor_first;
	if (lac < lcc) goto anchor_first;
	if (lcc < lac) goto cursor_first;
	return;


anchor_first:

	int line = lal, column = lac;



loop:
	if (line == lcl and column >= lcc) goto done;
	if (line == lcl and column < lcc) fwrite(lines[line].data, 1, lcc - lac, file);

	}
	


done:
	

	

	// if (not use_system_clipboard) return;
	// char buffer[128] = "hello there from space.";
	// size_t buffer_length = (size_t) strlen(buffer);

	FILE* file = popen("pbcopy", "w");

	fwrite(buffer, 1, buffer_length, file);

	pclose(file);
}

static inline void anchor() {
	lal = lcl; 
	lac = lcc; 
	sprintf(message, "set anchor %d %d", lal, lac);
}


int main(const int argc, const char** argv) {

	// paste();

	// printf("\n\ncopying things to clipboard!!\n\n");
	
	// copy();

	// exit(1);

	if (argc == 1) create_empty_buffer();
	else for (int i = 1; i < argc; i++) open_file(argv[i]);

	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h", 16);
	char p = 0, c = 0;

	adjust_window_size();
	needs_display_update = 1;
loop:	
	if (needs_display_update) {
		adjust_window_size();
		display();
	}
	read(0, &c, 1);
	needs_display_update = 1;

	if (mode == 0) {
		if (is_exit_sequence(c, p)) { /*undo();*/delete(); mode = 1; }
		else if (c == 127) delete();
		else if (c == 13) insert(10);
		else if (c == 27) interpret_escape_code();
		else insert(c);

	} else if (mode == 1) {

		if (c == 'q') { if (saved) close_active_buffer(); }
		else if (c == 'Q') { if (saved or confirmed("discard unsaved changes")) close_active_buffer(); }
		
		else if (c == 'f') mode = 0;
		else if (c == 'a') mode = 1;
		else if (c == 'e') mode = 2;
		else if (c == 't') mode = 3;

		else if (c == 'w') save();
		else if (c == 's') anchor();
		else if (c == 'v') paste();
		else if (c == 'c') copy();

		// else if (c == 'd') cut();

		else if (c == 'r') {}
		else if (c == 'u') {}

		else if (c == 'j') move_left(1);
		else if (c == ';') move_right(1);
		else if (c == 'o') move_up();
		else if (c == 'i') move_down();

		else if (c == 'k') move_begin();
		else if (c == 'l') move_end();

		else if (c == 'J') move_word_left();
		else if (c == ':') move_word_right();
		else if (c == 'I') move_down_10();
		else if (c == 'O') move_up_10();

		else if (c == 'K') move_top();
		else if (c == 'L') move_bottom();

		else if (c == 'n') jump_line();
		else if (c == 'm') jump_column();

		else if (c == '_') memset(message, 0, sizeof message);

		else if (c == 27) interpret_escape_code();

	} else if (mode == 2) {
		
		if (c == 'q') { if (saved) close_active_buffer(); }
		else if (c == 'Q') { if (saved or confirmed("discard unsaved changes")) close_active_buffer(); }

		else if (c == 'f') mode = 0;
		else if (c == 'a') mode = 1;
		else if (c == 'e') mode = 2;
		else if (c == 't') mode = 3;

		else if (c == 'w') save();
		else if (c == 'W') rename_file();

		// else if (c == 'f') execute_command();

		else if (c == 'E') use_txt_extension_when_absent = not use_txt_extension_when_absent;
		else if (c == 's') show_status = not show_status;
		else if (c == 'd') show_line_numbers = not show_line_numbers;

		else if (c == 'j') move_to_next_buffer();
            	else if (c == ';') move_to_previous_buffer();

		else if (c == 'o') prompt_open();
            	else if (c == 'i') create_empty_buffer();
		else if (c == 'l') show_buffer_list();

		// else if (c == 'u') undo();
		// else if (c == 'r') redo();
		// else if (c == 'U') alternate_up();
		// else if (c == 'R') alternate_down();
		
		else if (c == 27) interpret_escape_code();
		
	} else if (mode == 3) {

		     if (c == 'f') mode = 0;
		else if (c == 'a') mode = 1;
		else if (c == 'e') mode = 2;
		else if (c == 't') mode = 3;

		else if (c == 'w') {			
			print_above_textbox("(0 sets to window width)", info_prompt_color);
			wrap_width = get_numeric_option_value("wrap width: "); move_top();
		} else if (c == 't') { tab_width = get_numeric_option_value("tab width: "); move_top(); } 
		else if (c == '1') default_prompt_color = get_numeric_option_value("default prompt color: ");
		else if (c == '2') alert_prompt_color = get_numeric_option_value("alert prompt color: ");
		else if (c == '3') line_number_color = get_numeric_option_value("line number color: ");
		else if (c == '4') status_bar_color = get_numeric_option_value("status bar color: ");
		else if (c == '5') info_prompt_color = get_numeric_option_value("info prompt color: ");

		else if (c == '[') {
			char string[128] = {0};
			print_above_textbox("(empty string disables exit sequence)", info_prompt_color);
			prompt("left exit sequence (2 characters): ", default_prompt_color, string, sizeof string);
			if (strlen(string) == 2) memcpy(left_exit, string, 2); else memset(left_exit, 0, 2);
			sprintf(message, "left exit sequence set to %d %d", left_exit[0], left_exit[1]);

		} else if (c == ']') {
			char string[128] = {0};
			print_above_textbox("(empty string disables exit sequence)", info_prompt_color);
			prompt("right exit sequence (2 characters): ", default_prompt_color, string, sizeof string);
			if (strlen(string) == 2) memcpy(right_exit, string, 2); else memset(right_exit, 0, 2);
			sprintf(message, "right exit sequence set to %d %d", right_exit[0], right_exit[1]);
		}
	}
	p = c;
	if (buffer_count) goto loop;
	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);
	free(buffers); //todo: free lines in each buffer, and lines reg.
	// free(system_clipboard);
}

