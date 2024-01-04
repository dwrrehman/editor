// editor source code. written on 202311201.014937 by dwrr

#include <stdio.h>  // 202309074.165637:   
#include <stdlib.h> //  another rewrite to make the editor simpler, and non-volatile- to never require saving. 
#include <string.h> 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdint.h>
#include <signal.h>
#include <copyfile.h>

typedef uint64_t nat;
static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";
static const nat autosave_frequency = 12; // every 12 edits, fcopyfile is called. 

static const nat active = 0x01, inserting = 0x02, selecting = 0x04;
extern char** environ;
static struct winsize window = {0};
static char filename[4096] = {0}, directory[4096] = {0};
static int file = -1, directory_fd = -1;
static off_t cursor = 0, origin = 0, count = 0, anchor = 0;
static nat mode = 0;
static nat cursor_row = 0, cursor_column = 0, desired_column = 0;
static char* screen = NULL;
static nat screen_size = 0;
static char* clipboard = NULL;
static nat cliplength = 0;
static nat display_mode = 0;
static int history = -1;
static off_t head = 0;
static nat autosave_counter = 0;

struct action {
	off_t parent;
	off_t pre;
	off_t post;
	off_t length;
};

static void get_count(void) {
	struct stat s;
	fstat(file, &s);
	count = s.st_size;
}

static char next(void) {
	char c = 0;
	ssize_t n = read(file, &c, 1);
	if (n == 0) return 0;
	if (n != 1) { perror("next read"); exit(1); }
	return c;
}

static char at(off_t a) {
	lseek(file, a, SEEK_SET);
	return next();
}

static void at_string(off_t a, nat byte_count, char* destination) {
	lseek(file, a, SEEK_SET);
	ssize_t n = read(file, destination, byte_count);
	if (n == 0) return;
	if (n < 0) { perror("next read"); exit(1); }
}

static void autosave(void) {

	char autosave_filename[4096] = {0};

	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);

	snprintf(autosave_filename, sizeof autosave_filename, "%s_%08x%08x.txt", datetime, rand(), rand());
	int flags = O_RDWR | O_CREAT | O_EXCL;
	mode_t permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	const int autosave_dir = open(autosave_directory, O_RDONLY | O_DIRECTORY, 0);
	if (autosave_dir < 0) { perror("read open autosave_directory"); exit(1); }
	int df = openat(autosave_dir, autosave_filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int autosave_file = openat(autosave_dir, autosave_filename, flags, permission);
	if (autosave_file < 0) { read_error: perror("read openat autosave_filename"); exit(1); }

	close(file);
	close(directory_fd);
	
	flags = O_RDWR;
	permission = 0;

	directory_fd = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (directory_fd < 0) { perror("read open directory"); exit(1); }
	df = openat(directory_fd, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error2; }
	file = openat(directory_fd, filename, flags, permission);
	if (file < 0) { read_error2: perror("read openat file"); exit(1); }

	if (fcopyfile(file, autosave_file, NULL, COPYFILE_ALL)) {
		perror("fcopyfile autosave COPYFILE_ALL");
		getchar();
	}

	close(autosave_file);
	close(autosave_dir);

	autosave_counter = 0; 
}


static void display(void) {

	ioctl(0, TIOCGWINSZ, &window);
	const nat new_size = 9 + 32 + window.ws_row * (window.ws_col + 5) * 4;
	if (new_size != screen_size) { screen = realloc(screen, new_size); screen_size = new_size; display_mode = 0; }

	memcpy(screen, "\033[?25l\033[H", 9);
	nat length = 9;
	nat row = 0, column = 0;
	off_t i = origin;
	lseek(file, origin, SEEK_SET);

	const off_t begin = anchor < cursor ? anchor : cursor;
	const off_t end = anchor < cursor ? cursor : anchor;

	bool selection_started = 0, found_end = false, found_cursor = false;

	if ((mode & selecting) and anchor < origin) { selection_started = 1; memcpy(screen + length, "\033[7m", 4); length += 4; }

	while (1) {
		if ((mode & selecting) and i == begin) { selection_started = 1; memcpy(screen + length, "\033[7m", 4); length += 4; }
		if ((mode & selecting) and i == end)   { selection_started = 0; memcpy(screen + length, "\033[0m", 4); length += 4; }
		if (i == cursor) { found_cursor = true; cursor_row = row; cursor_column = column; }
		if (row >= window.ws_row) break;
		char k = next();
		if (not k) { found_end = true; break; }
		i++;

		if (k == 10) {
			column = 0; row++;
			memcpy(screen + length, "\033[K", 3); 
			length += 3; 
			if (row < window.ws_row) screen[length++] = 10;
		} else if (k == 9) {
			const uint8_t amount = 8 - column % 8;
			column += amount;
			memcpy(screen + length, "        ", amount);
			length += amount;
		} else {
			if (column >= window.ws_col - 1) { column = 0; row++; }
			else if ((unsigned char) k >> 6 != 2 and k >= 32) column++;
			screen[length++] = k;
		}
	}

	if ((mode & selecting) and selection_started)   { selection_started = 0; memcpy(screen + length, "\033[0m", 4); length += 4; }

	if (i == cursor) { found_cursor = true; cursor_row = row; cursor_column = column; }

	while (row < window.ws_row) {
		row++;
		memcpy(screen + length, "\033[K", 3);
		length += 3; 
		if (row < window.ws_row) screen[length++] = 10;
	} 

	if (	cursor == count and found_end and
		cursor_row == window.ws_row - 1 and 
		not cursor_column
	)
		length += (nat) snprintf(
			screen + length, 
			screen_size - length, 
			"\033[%d;1H\033[K", 
			window.ws_row
		);

	if (not found_cursor) {
		printf("\033[31mcursor not in the view while displaying the screen. [cursor=%llu, origin=%llu, count=%llu], abort?\033[0m", 
			cursor, origin, count);
		fflush(stdout);
		getchar();
	}
	length += (nat) snprintf(
		screen + length, 
		screen_size - length, 
		"\033[K\033[%llu;%lluH\033[?25h", 
		cursor_row + 1, 
		cursor_column + 1
	);

	write(1, screen, length);
}

static void window_resize_handler(int unused) { display(); }
static void interrupt_handler(int unused) { exit(1); }

static void move_left_raw(void) {    
	get_count();
	if (not cursor) return;
	if (origin < cursor) goto decr;
	origin--;
loop: 	if (not origin) goto decr;
	origin--;
	char c = at(origin);
	if (not c) return;
	if (c != 10) goto loop;
	origin++;
decr: 	cursor--;
}

static void move_right_raw(void) {
	get_count();
	if (cursor >= count) return;
	char c = at(cursor);
	if (not c) return;
	cursor++;
	if (c == 10) { cursor_column = 0; cursor_row++; }
	else if (c == 9) cursor_column += 8 - cursor_column % 8;
	else if (cursor_column >= window.ws_col - 1) { cursor_column = 0; cursor_row++; }
	else if ((unsigned char) c >> 6 != 2) cursor_column++; 
	if (cursor_row < window.ws_row) return;
	nat column = 0;
loop: 	if (origin >= count) return;
	c = at(origin);
	if (not c) return;
	origin++;
	if (c == 10) goto done;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col - 1) goto done;
	else if ((unsigned char) c >> 6 != 2) column++;
	goto loop; 
done: 	cursor_row--;
}

static void move_left(void) {
	move_left_raw();
	desired_column = cursor_column;
}

static void move_right(void) {
	move_right_raw();
	desired_column = cursor_column;
}

static void move_up_begin(void) {
	while (cursor) {
		move_left();
		char c = at(cursor - 1);
		if (not c) return;
		if (cursor and c == 10) break;
	}
}

static void move_down_end(void) {
	while (cursor < count) {
		move_right();
		char c = at(cursor);
		if (not c) return;
		if (cursor < count and c == 10) break;
	}
}

static void move_word_left(void) {
	move_left();
	while (cursor) {
		char behind = at(cursor - 1);
		if (not behind) return;
		char here = next();
		if (not here) return;
		if (not (not isalnum(here) or isalnum(behind))) break;
		if (behind == 10) break;
		move_left();
	}
}

static void move_word_right(void) {
	move_right();
	while (cursor < count) {
		char behind = at(cursor - 1);
		if (not behind) return;
		char here = next();
		if (not here) return;
		if (not (isalnum(here) or not isalnum(behind))) break;
		if (here == 10) break;
		move_right();
	}
}

static void move_up(void) {
	while (cursor) {
		move_left_raw();
		if (at(cursor - 1) == 10) break;
	}
}

static void move_down(void) {
	while (cursor < count) {
		move_right_raw();
		if (not cursor_column) break;
	}

	while (cursor < count) {
		if (cursor_column >= desired_column) break;
		if (at(cursor) == 10) break;
		move_right_raw(); 
	}
}

static void forwards(void) {
	nat t = 0;
	loop: if (t == cliplength or cursor >= count) return;
	if (at(cursor) != clipboard[t]) t = 0; else t++;
	move_right(); 
	goto loop;
}

static void backwards(void) {
	nat t = cliplength;
	loop: if (not t or not cursor) return;
	move_left(); t--; 
	if (at(cursor) != clipboard[t]) t = cliplength;
	goto loop;
}

static void move_top(void) { cursor = 0; origin = 0; cursor_column = 0; cursor_row = 0; }
static void move_bottom(void) { while (cursor < count) move_right(); }

static void finish_action(struct action node, const char* string, nat length) {
	node.post = cursor;
	head = lseek(history, 0, SEEK_END);
	write(history, &node, sizeof node);
	write(history, string, length);
	lseek(history, 0, SEEK_SET);
	write(history, &head, sizeof head);
	fsync(history);
}

static void insert(char* string, nat length, bool should_record) {
	if (++autosave_counter >= autosave_frequency) autosave();
	lseek(history, 0, SEEK_SET);
	read(history, &head, sizeof head);
	struct action node = { .parent = head, .pre = cursor, .length = (off_t) length };
	get_count();
	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size + length); 
	memcpy(rest, string, length);
	lseek(file, cursor, SEEK_SET);
	read(file, rest + length, size);
	lseek(file, cursor, SEEK_SET);
	write(file, rest, size + length);
	count += length;
	fsync(file);
	free(rest);
	for (nat i = 0; i < length; i++) move_right();
	if (should_record) finish_action(node, string, length);
}

static void delete(nat length, bool should_record) {
	if (++autosave_counter >= autosave_frequency) autosave();
	if (cursor < (off_t) length) return;
	lseek(history, 0, SEEK_SET);
	read(history, &head, sizeof head);
	struct action node = { .parent = head, .pre = cursor, .length = - (off_t) length };
	get_count();
	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size);
	char* string = malloc(length);
	lseek(file, cursor - (off_t) length, SEEK_SET);
	read(file, string, length);
	lseek(file, cursor, SEEK_SET); //TODO: optimize this away!
	read(file, rest, size);
	lseek(file, cursor - (off_t) length, SEEK_SET);
	write(file, rest, size);
	count -= length;
	ftruncate(file, count);
	fsync(file);
	free(rest);
	for (nat i = 0; i < length; i++) move_left();
	if (should_record) finish_action(node, string, length);
	free(string);
}

static void cut(void) {
	if (not (mode & selecting)) return;
	if (anchor > cursor) {
		const off_t temp = anchor;
		anchor = cursor;
		cursor = temp;
	}
	const off_t length = cursor - anchor;
	get_count();
	cliplength = (size_t) length;
	clipboard = realloc(clipboard, cliplength);
	at_string(anchor, cliplength, clipboard);
	delete((nat) length, 1);
	anchor = cursor;
	mode &= ~selecting;
}

static void delete_cut(void) {
	if (mode & selecting) cut(); else delete(1, 1);
}

static bool is(const char* string, char c, char* past) {
	nat length = strlen(string);
	if (c != string[length - 1]) return 0;
	for (nat h = 0, i = 1; i < length; i++, h++) {
		if (past[h] != string[length - 1 - i]) return 0;
	}
	return 1;
}

static void undo(void) {
	lseek(history, 0, SEEK_SET);
	read(history, &head, sizeof head);
	if (not head) return;
	struct action node = {0};
	lseek(history, head, SEEK_SET);
	read(history, &node, sizeof node);
	nat len = (nat) (node.length < 0 ? -node.length : node.length);
	char* string = malloc(len);
	read(history, string, len);
	cursor = node.post;
	if (node.length < 0) insert(string, len, 0); else delete(len, 0);
	cursor = node.pre; 
	anchor = node.pre;
	head = node.parent; 
	lseek(history, 0, SEEK_SET);
	write(history, &head, sizeof head);
}

static void redo(void) {

	/*
	if (not actions[head].count) return;

	head = actions[head].children[actions[head].choice];

	const struct action a = actions[head];

	cursor = a.pre_cursor; 

	if (a.insert) 
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else 
		for (nat i = 0; i < a.length; i++) delete(0);

	cursor = a.post_cursor;
	anchor = cursor;

	//if (a.count > 1) { 
	//	printf("\033[0;44m[%lu:%lu]\033[0m", a.count, actions[head].choice); 
	//	getchar(); 
	//}

	*/
}

static inline void copy(void) {
	if (not (mode & selecting)) return;
	get_count();
	FILE* globalclip = popen("pbcopy", "w");
	if (not globalclip) {
		perror("copy popen pbcopy");
		getchar(); return;
	}

	if (anchor > count) anchor = count;
	cliplength = (size_t) (anchor < cursor ? cursor - anchor : anchor - cursor);
	clipboard = realloc(clipboard, cliplength);
	if (anchor < cursor) at_string(anchor, cliplength, clipboard);
	else at_string(cursor, cliplength, clipboard);
	fwrite(clipboard, 1, cliplength, globalclip);
	pclose(globalclip);
}

static void insert_output(const char* input_command) {

	char command[4096] = {0};
	strlcpy(command, input_command, sizeof command);
	strlcat(command, " 2>&1", sizeof command);

	FILE* f = popen(command, "r");
	if (not f) {
		printf("error: could not execute \"%s\"\n", command);
		perror("insert_output popen");
		getchar(); return;
	}
	char* string = NULL;
	size_t length = 0;
	char line[2048] = {0};
	while (fgets(line, sizeof line, f)) {
		size_t l = strlen(line);
		string = realloc(string, length + l);
		memcpy(string + length, line, l);
		length += l;
	}
	pclose(f);
	insert(string, length, 1);
	free(string);
}

static void jump_index(void) {
	char* string = strndup(clipboard, cliplength);	
	const size_t n = (size_t) atoi(string);
	for (size_t i = 0; i < n; i++) move_right();
	free(string);
}

static void jump_line(void) {
	char* string = strndup(clipboard, cliplength);	
	const size_t n = (size_t) atoi(string);
	move_top(); 
	for (size_t i = 0; i < n; i++) move_down_end();
	move_up_begin(); 
	free(string);
}

static void set_anchor(void) {
	if (mode & selecting) return;
	anchor = cursor; 
	mode |= selecting;
}


static void clear_anchor(void) {
	if (not (mode & selecting)) return;
	anchor = cursor; 
	mode &= ~selecting;
}

static void interpret_arrow_key(void) {
	char c = 0; read(0, &c, 1);

	if (c == 'q') mode &= ~active;
	else if (c == 'u') { clear_anchor(); move_up_begin(); }
	else if (c == 'd') { clear_anchor(); move_down_end(); }
	else if (c == 'l') { clear_anchor(); move_word_left(); }
	else if (c == 'r') { clear_anchor(); move_word_right(); }

	else if (c == 'f') { clear_anchor(); forwards(); }
	else if (c == 'b') { clear_anchor(); backwards(); }
	else if (c == 't') { clear_anchor(); for (int i = 0; i < window.ws_row; i++) move_up(); }
	else if (c == 'e') { clear_anchor(); for (int i = 0; i < window.ws_row; i++) move_down(); }

	else if (c == 's') {
		read(0, &c, 1); 
		     if (c == 'u') { set_anchor(); move_up(); }
		else if (c == 'd') { set_anchor(); move_down(); }
		else if (c == 'r') { set_anchor(); move_right(); }
		else if (c == 'l') { set_anchor(); move_left(); }

		else if (c == 'b') { set_anchor(); move_up_begin(); }
		else if (c == 'e') { set_anchor(); move_down_end(); }
		else if (c == 'w') { set_anchor(); move_word_right(); }
		else if (c == 'm') { set_anchor(); move_word_left(); }
	}

	else if (c == '[') {
		read(0, &c, 1); 
		if (c == 'A') { clear_anchor(); move_up(); }
		else if (c == 'B') { clear_anchor(); move_down(); }
		else if (c == 'C') { clear_anchor(); move_right(); }
		else if (c == 'D') { clear_anchor(); move_left(); }
		else { printf("error: found escape seq: ESC [ #%d\n", c); getchar(); }
	} else { printf("error found escape seq: ESC #%d\n", c); getchar(); }
}

static void paste(void) {
	if (mode & selecting) cut();
	insert_output("pbpaste");
}

static void insert_replace(char* s, nat l) {
	if (mode & selecting) cut();
	insert(s, l, 1);
}

int main(int argc, const char** argv) {

	srand((unsigned)time(0)); rand();
	getcwd(directory, sizeof directory);

	struct sigaction action = {.sa_handler = window_resize_handler}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupt_handler}; 
	sigaction(SIGINT, &action2, NULL);

	int flags = O_RDWR;
	mode_t permission = 0;

	if (argc < 2) {
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	} else strlcpy(filename, argv[1], sizeof filename);

	directory_fd = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (directory_fd < 0) { perror("read open directory"); exit(1); }
	int df = openat(directory_fd, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	file = openat(directory_fd, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }

	const char* history_directory = "/Users/dwrr/Documents/personal/histories/";  // "./"
	char history_filename[4096] = {0};

	flags = O_RDWR;
	permission = 0;

	if (argc < 3) {
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(history_filename, sizeof history_filename, "%s_%08x%08x.history", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	} else if (argc == 3) strlcpy(history_filename, argv[2], sizeof history_filename);
	else exit(puts("usage error"));

	const int hdir = open(history_directory, O_RDONLY | O_DIRECTORY, 0);
	if (hdir < 0) { perror("read open history directory"); exit(1); }
	df = openat(hdir, history_filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto hread_error; }
	history = openat(hdir, history_filename, flags, permission);
	if (history < 0) { hread_error: perror("read openat history file"); exit(1); }

	if (argc < 3) 
		write(history, &head, sizeof head); 
	else 	 read(history, &head, sizeof head);

	struct termios terminal;
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSAFLUSH, &terminal_copy);
	write(1, "\033[?1049h", 8);

	get_count();
	char c = 0;
	mode = active;

loop:	display();
	c = 0;
	read(0, &c, 1);
	if (c == 1) paste();
	else if (c == 8) copy();
	else if (c == 20) { copy(); cut(); }
	else if (c == 24) undo();
	else if (c == 27) interpret_arrow_key();
	else if (c == 127) delete_cut();
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert_replace(&c, 1);
	if (mode & active) goto loop;
	close(file);
	close(directory_fd);
	close(history);
	write(1, "\033[?1049l", 8);
	tcsetattr(0, TCSAFLUSH, &terminal);
}




	////else if (c == XXX) { anchor = cursor; mode |= selecting; }

	//else if (c == ) move_top();
	//else if (c == ) move_bottom();

	//else if (c == XXX) redo();
	//else if (c == XXX) jump_line();




/*
202311245.115046:
	BUG:
		if the line is exactly the wrap width, there is a bug where the cursor desyncs. fix this.

		unicode: we currently don't handle it good at all, when deleting characters. its really bad actually. some unicode charcters are completely undeletable. its bad. fix this. 

	tood:

		- support unicode

	x	- make the editor nonmodal.

*/











































//if (c == '.') mode &= ~inserting;
	//else if (c == 's') { anchor = cursor; mode ^= selecting; }
	//else if (c == 'r') cut();




	//char past[10] = {0};
	//const char* exit_sequence = "rtun";




	//nat i = 10; 
	//while (i-- > 1) past[i] = past[i - 1];
	//*past = c;




	//if (mode & inserting) {
	//else if (is(exit_sequence, c, past)) { delete(strlen(exit_sequence) - 1, 1); mode &= ~inserting; } 
	//} else {
	//else if (c == ' ') mode |= inserting;
	//else if (c == ' ') move_down_end();
	//else if (c == ' ') move_up_begin();
	//else if (c == 'n') move_left();
	//else if (c == 'e') move_up();
	//else if (c == 'o') move_right();
	//else if (c == 'u') move_word_left();
	//else if (c == 'p') move_word_right();
	//else if (c == 'l') move_down();
	//else if (c == XXX) jump_index();






































//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////






/*static void alternate(void) { 

	if (actions[head].choice + 1 < actions[head].count) 
		actions[head].choice++; 
	else 
		actions[head].choice = 0; 
}*/


//if (actions[head].count > 1) { 
	//	printf("\033[0;44m[%lu:%lu]\033[0m", actions[head].count, actions[head].choice); 
	//	getchar(); 
	//}

	/*printf("head = %lld, node {.parent = %llx, .post = %llx, .pre = %llx, .length = %llx }\n", 
		head, 	node.parent, node.post, node.pre, node.length
	); 
	fflush(stdout);
	getchar();*/







	//  argc == 1     ./editor          
	//  argc == 2     ./editor file.txt
	//  argc == 3     ./editor file.txt history.txt



















		//printf("c%lu: comparing %d and %d\n", h, past[h], string[length - 1 - i]);
		//getchar();


	//printf("c0: comparing %d and %d\n", c, string[length - 1]);
	//getchar();





///TODO: make the editor use a seqence of inputs, 
///    a up to 5 deep history of what we pressed!! useful alot. 
///    put all commands into insert. this will be a nonmodal editor. 
///    we will figure out the ergonomics of holding down a key later. 
///     in actuality we shouldnt even be doing that, so yeah. it works. 








/*
	nat column = 0;
	off_t p = origin, o = origin;
loop1: 	if (origin >= cursor) goto done;
	c = at(origin);
	if (not c) return;
	origin++;
	if (c == 10) goto r;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col - 1) { r: column = 0; p = o; o = origin; } 
	else if ((unsigned char) c >> 6 != 2) column++;
	goto loop1;
done:	cursor_column = column;
	o = p; 
*/



// this code has a bug:  its not computing cursor_column and cursor_row correctly, which is neccessary information for move_up to work properly. we need to compute that properly using an n^2 algorithm over the line... yikes.... not happening. hmm. 










/*


absolutely required edit mode commands:


done:
x	- inserting text    (done)  just typing
x	- deleting text     (done)  pressing the delete key

todo:
x	- backwards search     <--------  these ones are the most important. 
x	- forwards search      <--------  these ones are the most important. 

x	- cut selection  <--------- these ones are of second importance.
x	- anchor drop    <--------- these ones are of second importance.

	- copy selection     <------- make this have its own sub-key sequence.
	- paste selection    <------- make this have its own sub-key sequence.








deleted:

	













	while (cursor) {
		if (at(cursor) == 10) break;
		move_left_raw();
	}

	// move_right_raw();

*/
























/*
		how do we exit insert mode?!?!

			we should build in all of these commands into insert mode, by  using multi-char sequences to do movements.  and other things too.  multi char sequences are the best. we should use them.   yes.         tihnk     typing           asht   in insert mode    to move right!!!


			okay cool, lets do that 



		
		also, every single function     move_left     delete(),  etc


					should do its own displaying. it knows how it changed the text. it knows what text needs to be updated on the screen it knows these things. we should make a    per function    custom display  code   done after each command,    in the command itself.  

					so it does the tihng,     and updates the display in the correct minimal way. 

						so yeah. pretty cool. thats going to be the way we do things, i think. nice. 


	

	
	
	yay




*/
















	// else if (c == 27) interpret_arrow_key();































/*










static void interpret_arrow_key(void) { 
	char c = 0; read(0, &c, 1);
	if (false) {}
	else if (c == 'b') {} // move_word_left();
	else if (c == 'f') {} // move_word_right();
	else if (c == 'd') {} // move_down_end();
	else if (c == 'u') {} // move_up_begin();
	else if (c == '[') {
		read(0, &c, 1); 
		if (false) {}
		else if (c == 'A') {} // move_up(); 
		else if (c == 'B') {} // move_down();
		else if (c == 'C') {} // move_right(); 
		else if (c == 'D') {} // move_left();
		else if (c == 49) {
			read(0, &c, 1); read(0, &c, 1); read(0, &c, 1); 
			if (false) {}
			else if (c == 'D') {} // { anchor = cursor; mode ^= selecting; }
			else if (c == 'C') {} // {  }
		}
	}
}





static void alternate(void) { if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0; }
static void undo(void) {
	if (not head) return;
	struct action a = actions[head];
	cursor = a.post_cursor; origin = a.post_origin; mode = (mode & ~saved) | a.post_saved; 
	if (not a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);	
	cursor = a.pre_cursor; origin = a.pre_origin; mode = (mode & ~saved) | a.pre_saved; anchor = cursor;
	head = a.parent; a = actions[head];
	if (a.count > 1) { 
		printf("\033[0;44m[%lu:%lu]\033[0m", a.count, a.choice); 
		getchar(); 
	}
}

static void redo(void) {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	const struct action a = actions[head];
	cursor = a.pre_cursor; origin = a.pre_origin; mode = (mode & ~saved) | a.pre_saved; 
	if (a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);
	cursor = a.post_cursor; origin = a.post_origin; mode = (mode & ~saved) | a.post_saved; anchor = cursor;
	if (a.count > 1) { 
		printf("\033[0;44m[%lu:%lu]\033[0m", a.count, actions[head].choice); 
		getchar(); 
	}
}
*/






/*

	flags = O_RDWR;
	permission = 0;
	srand((unsigned)time(0)); rand();
	char datetime[32] = {0};
	now(datetime);
	snprintf(filename, sizeof filename, "%x%x_%s.txt", rand(), rand(), datetime);
	flags |= O_CREAT | O_EXCL;
	permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	if (argc == 2) strlcpy(filename, argv[1], sizeof filename);
	else exit(puts("usage error"));
	const int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { perror("read open directory"); exit(1); }
	int df = openat(dir, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	file = openat(dir, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }
*/


/*
static void forwards(void) {
	nat t = 0;
	loop: if (t == cliplength or cursor >= count) return;
	if (text[cursor] != clipboard[t]) t = 0; else t++;
	move_right(); 
	goto loop;
}

static void backwards(void) {
	nat t = cliplength;
	loop: if (not t or not cursor) return;
	move_left(); t--; 
	if (text[cursor] != clipboard[t]) t = cliplength;
	goto loop;
}





static void create_action(struct action new) {
	new.post_cursor = cursor; 
	new.post_origin = origin; 
	new.post_saved = mode & saved;
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(size_t) * (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	head = action_count;
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	actions[action_count++] = new;
}

static void delete(bool should_record) {
	if (not cursor) return;
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
	const char c = text[cursor - 1];
	memmove(text + cursor - 1, text + cursor, count - cursor);
	count--;
	text = realloc(text, count); 
	mode &= ~saved; move_left();
	if (not should_record) return;
	new.insert = 0;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);
}

static void insert(char c, bool should_record) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
	text = realloc(text, count + 1);
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c;
	count++; mode &= ~saved;
	move_right();
	if (not should_record) return;
	new.insert = 1;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);
}




*/























// awesome reference for escape sequences:
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html





/*



if (not fuzz) {
		write(1, "\033[?1049h\033[?1000h\033[7l", 20);
	} 


	if (not fuzz) {
		write(1, "\033[?1049l\033[?1000l\033[7h", 20);
	}






 // todo: make this move up visual lines properly, not skipping the origin to the next logical line. 



	todos:

	x	- make move_right and move_left move the origin as well   up and down   visual lines.   
				 thats the secret building block that everything builds off of. 


		- make move_right and move_left??? go up and down visual lines.

		- make move_up??? and move_down use visual lines as well. 


		- have very good support for visual movement on softwrapped lines. 

	x	- have very good support for displaying softwrapped lines. 





		- make smooth scrolling a thing!!!! use terminal sequences to print just what you need to print in order to see the next line scroll!!!


					- on second thought:      we should acttually just discard the mouse/trackpad entirely. 

									no support for anything related to the mouse, 

									this includes smooth scrolling?.. hm...... hm...






static void move_begin(void) { 
	while (cursor) {
		char k = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &k, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (k == 10) break;
		move_left();  
	}
}

static void move_end(void) {
	while (cursor < count) {
		char k = 0;
		lseek(file, cursor, SEEK_SET);
		ssize_t n = read(file, &k, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (k == 10) break;
		move_right(); 
	}
}










// if (cursor_column >= desired_column) break; // and cursor_row >= desired_row// 


// if (origin_s == origin or save == cursor_row)





// const	 nat origin_s = origin;	
	// const nat save = cursor_row;
	// const nat desired_row = cursor_row < window.ws_row - 1 ? cursor_row + 1 : cursor_row;
	//if (cursor_row < window.ws_row - 1 and cursor_row >= desired_row) break;










static void move_left(void) {
	if (not cursor) return;
	if (origin < cursor) goto decr;

	
	



decr: 	cursor--;
}



static void move_left(void) {
	if (not cursor) return;

	if (origin < cursor) goto decr;
	origin--;
e: 	if (not origin) goto decr;
	origin--;
	if (text[origin] != 10) goto e;
	origin++;

decr: 	cursor--;
}


static void move_right(void) {
	if (cursor >= count) return;
	char c = text[cursor++];
	if (c == 10) { cursor_column = 0; cursor_row++; }
	else if (c == 9) cursor_column += 8 - cursor_column % 8;
	else if (cursor_column >= window.ws_col - 1) { cursor_column = 0; cursor_row++; }
	else if ((unsigned char) c >> 6 != 2) cursor_column++; 
	if (cursor_row < window.ws_row) return;
	nat column = 0;
	l: if (origin >= count) return;
	c = text[origin++];
	if (c == 10) goto done;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col - 1) goto done;
	else if ((unsigned char) c >> 6 != 2) column++;
	goto l; done: cursor_row--;
}
*/






























/*





static void move_begin(void) { 
	while (cursor) {
		char k = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &k, 1);
		lseek(file, cursor, SEEK_SET);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (k == 10) break;
		move_left();  
	}
}

static void move_end(void) {
	while (cursor < count and text[cursor] != 10) move_right(); 
}

static void move_word_left(void) {
	move_left();
	while (cursor) {

		char here = 0, behind = 0;
		
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &behind, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
			n = read(file, &here, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		lseek(file, cursor, SEEK_SET);

		if(not (
		not isalnum(here) or 
		isalnum(behind) 
		)) break;
		move_left();
	}
}

static void move_word_right(void) {
	move_right();
	while (cursor < count) {

		char here = 0, behind = 0;
		
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &behind, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
			n = read(file, &here, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		lseek(file, cursor, SEEK_SET);

		if(not (
		isalnum(here) or 
		not isalnum(behind) 
		)) break;
		move_right();
	}
}

static void move_word_right(void) {
	//do move_right();
	//while (cursor < count and (isalnum(text[cursor]) or not isalnum(text[cursor - 1])));
}


static void move_word_left(void) {
	move_left();
	while (cursor and (not isalnum(text[cursor]) or isalnum(text[cursor - 1])));
}
static void move_word_right(void) {
	//do move_right(); 
	//while (cursor < count and (isalnum(text[cursor]) or not isalnum(text[cursor - 1])));
}


static void move_up(void) {}

static void move_down(void) {}






// lseek(file, cursor, SEEK_SET);
	// lseek(file, cursor, SEEK_SET);
	// lseek(file, -1, SEEK_CUR);






	if (origin < cursor or not origin) goto decr;
	origin--;
	e: if (not origin) goto decr;
	origin--;
	if (text[origin] != 10) goto e;
	origin++;
	decr: if (cursor) cursor--;



	if (not should_record) return;
	new.insert = 0;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);







	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};

	const char c = text[cursor - 1];

	memmove(text + cursor - 1, text + cursor, count - cursor);

	count--;

	text = realloc(text, count);




static void move_right(void) {          // todo: make this function NOT move down visual lines. only logical lines. its simpler. 
	if (cursor >= count) return;
	char c = text[cursor++];
	if (c == 10) { cursor_column = 0; cursor_row++; }
	else if (c == 9) cursor_column += 8 - cursor_column % 8;
	else if (cursor_column >= window.ws_col - 1) { cursor_column = 0; cursor_row++; }
	else if ((unsigned char) c >> 6 != 2) cursor_column++; 
	if (cursor_row < window.ws_row) return;
	nat column = 0;

	l: if (origin >= count) return;
	c = text[origin++];
	if (c == 10) goto done;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col - 1) goto done;
	else if ((unsigned char) c >> 6 != 2) column++;
	goto l; done: cursor_row--;
}


	// struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};

	//text = realloc(text, count + 1);
	//memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c;
	count++; 

if (not should_record) return;
	new.insert = 1;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);


*/






/*




fn delete  =    27 91 51 126

shift fn delete  =    27 91 51 59 50 126








	read()
	write()
	open()
	openat()
	close()
	lseek()
	exit()


	If whence is SEEK_SET, the offset is set to offset bytes.
	If whence is SEEK_CUR, the offset is set to its current location plus offset bytes.
	If whence is SEEK_END, the offset is set to the size of the file plus offset bytes.
*/





/*


//      keys:           a s h t   n e o i                 d r     u p 




	else if (c == 8)  {} // H
	else if (c == 20) {} // T

	else if (c == 18) {} // R
	
	
	else if (c == 14) {} // N
	else if (c == 5)  {} // E
	else if (c == 15) {} // O

	else if (c == 21) {} // U
	else if (c == 16) {} // P







	else if (c == 'M') forwards();
	else if (c == 'X') redo();
	else if (c == 'Z') undo();
	else if (c == 'N') move_left();
	else if (c == 'U') move_word_left();
	else if (c == 'P') move_word_right();
	else if (c == 'K') move_begin();
	else if (c == 'I') move_end();
	else if (c == 'E') move_up();
	else if (c == 'O') move_right();
	else if (c == 'L') move_down();







	else if (c == 'R') cut();
	else if (c == 'A') sendc();

	else if (c == 'C') copy();
	else if (c == 'V') insert_output("pbpaste");






	else if (c == 'X') redo();
	else if (c == 'Z') undo();
	else if (c == 'A') alternate();






 else if (c == 'T') { anchor = cursor; mode ^= selecting; }
 if (not (mode & selecting)) anchor = previous_cursor;



	else if (c == 'U') move_word_left();
	else if (c == 'P') move_word_right();
	else if (c == 'K') move_begin();
	else if (c == 'I') move_end();
	else if (c == 'E') move_up();


	else if (c == 'H') backwards();
	else if (c == 'M') forwards();










static void save_file(void) {
	const int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { 
		perror("write open directory"); 
		printf("directory=%s ", directory); 
		getchar(); return; 
	}
	int flags = O_WRONLY | O_TRUNC;  mode_t m = 0;
try_open:;
	const int file = openat(dir, filename, flags, m);
	if (file < 0) {
		if (m) {
			perror("create openat file");
			printf("filename=%s ", filename);
			getchar(); return;
		}
		perror("write openat file");
		printf("filename=%s\n", filename);
		flags = O_CREAT | O_WRONLY | O_TRUNC | O_EXCL;
		m     = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		goto try_open;
	}
	write(file, text, count);
	close(file); close(dir);
	mode |= saved;
	if (m) {
		printf("write: created %lub to ", count);
		printf("%s : %s\n", directory, filename); 
		getchar(); return;
	}
	printf("\033[1mwrite: saved %lub to ", count);
	printf("%s : %s \033[0m\n", directory, filename);
	fflush(stdout); usleep(300000);
}





*/































/*

static void display(void) {
	ioctl(0, TIOCGWINSZ, &window);
	const nat screen_size = window.ws_row * window.ws_col * 4;
	char* screen = calloc(screen_size, 1);
	memcpy(screen, "\033[?25l\033[H", 9);
	nat length = 9, row = 0, column = 0, i = origin;
	const nat begin = anchor < cursor ? anchor : cursor;
	const nat end = anchor < cursor ? cursor : anchor;
	if ((mode & selecting) and anchor < origin) length += (nat) snprintf(screen + length, screen_size - length, "\033[7m");
	for (; i < count; i++) {
		if (i == cursor) { cursor_row = row; cursor_column = column; }
		if ((mode & selecting) and i == begin) length += (nat) snprintf(screen + length, screen_size - length, "\033[7m"); 
		if ((mode & selecting) and i == end) length += (nat) snprintf(screen + length, screen_size - length, "\033[0m"); 
		if (row >= window.ws_row) break;
		char k = text[i];
		if (k == 10) {
			column = 0; row++;
			memcpy(screen + length, "\033[K", 3); 
			length += 3; 
			if (row < window.ws_row) screen[length++] = 10;
		} else if (k == 9) {
			const uint8_t amount = 8 - column % 8;
			column += amount;
			memcpy(screen + length, "        ", amount);
			length += amount;
		} else {
			if (column >= window.ws_col - 1) { column = 0; row++; }
			else if ((unsigned char) k >> 6 != 2 and k >= 32) column++;
			screen[length++] = k;
		}
	}
	if (i == cursor) { cursor_row = row; cursor_column = column; }
	if ((mode & selecting) and i == begin) length += (nat) snprintf(screen + length, screen_size - length, "\033[7m"); 
	if ((mode & selecting) and i == end) length += (nat) snprintf(screen + length, screen_size - length, "\033[0m"); 
	while (row < window.ws_row) {
		row++;
		memcpy(screen + length, "\033[K", 3);
		length += 3; 
		if (row < window.ws_row) screen[length++] = 10;
	}
	length += (nat) snprintf(screen + length, screen_size - length, "\033[K\033[%lu;%luH\033[?25h", cursor_row + 1, cursor_column + 1);
	write(1, screen, length);
	free(screen);
}

static void move_left(void) {
	if (origin < cursor or not origin) goto decr;
	origin--;
	e: if (not origin) goto decr;
	origin--;
	if (text[origin] != 10) goto e;
	origin++;
	decr: if (cursor) cursor--;
}

static void move_right(void) {
	if (cursor >= count) return;
	char c = text[cursor++];
	if (c == 10) { cursor_column = 0; cursor_row++; }
	else if (c == 9) cursor_column += 8 - cursor_column % 8;
	else if (cursor_column >= window.ws_col - 1) { cursor_column = 0; cursor_row++; }
	else if ((unsigned char) c >> 6 != 2) cursor_column++; 
	if (cursor_row < window.ws_row) return;
	nat column = 0;
	l: if (origin >= count) return;
	c = text[origin++];
	if (c == 10) goto done;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col - 1) goto done;
	else if ((unsigned char) c >> 6 != 2) column++;
	goto l; done: cursor_row--;
}

static void move_up(void) {
	while (cursor) {
		move_left();
		if (cursor and text[cursor - 1] == 10) break;
	}
}
static void move_down(void) {
	const nat save = cursor_row;
	while (cursor < count) {
		move_right();
		
		if (cursor < count and text[cursor] == 10) break;
	}
	while (cursor < count) {
		move_right();
		if (cursor < count and cursor_row >= save) break;
		if (cursor < count and text[cursor] == 10) break;
	}
}
static inline void move_begin(void) { while (cursor and text[cursor - 1] != 10) move_left();  }
static inline void move_end(void) { while (cursor < count and text[cursor] != 10) move_right();  }
static inline void move_word_left(void) {
	do move_left();
	while (cursor and (not isalnum(text[cursor]) or isalnum(text[cursor - 1])));
}
static inline void move_word_right(void) {
	do move_right(); 
	while (cursor < count and (isalnum(text[cursor]) or not isalnum(text[cursor - 1])));
}
static void forwards(void) {
	nat t = 0;
	loop: if (t == cliplength or cursor >= count) return;
	if (text[cursor] != clipboard[t]) t = 0; else t++;
	move_right(); 
	goto loop;
}
static void backwards(void) {
	nat t = cliplength;
	loop: if (not t or not cursor) return;
	move_left(); t--; 
	if (text[cursor] != clipboard[t]) t = cliplength;
	goto loop;
}

static void create_action(struct action new) {
	new.post_cursor = cursor; 
	new.post_origin = origin; 
	new.post_saved = mode & saved;
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(size_t) * (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	head = action_count;
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	actions[action_count++] = new;
}

static void delete(bool should_record) {
	if (not cursor) return;
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
	const char c = text[cursor - 1];
	memmove(text + cursor - 1, text + cursor, count - cursor);
	count--;
	text = realloc(text, count); 
	mode &= ~saved; move_left();
	if (not should_record) return;
	new.insert = 0;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);
}

static void insert(char c, bool should_record) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
	text = realloc(text, count + 1);
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c;
	count++; mode &= ~saved;
	move_right();
	if (not should_record) return;
	new.insert = 1;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);
}

static void insert_string(char* string, nat length) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
	for (nat i = 0; i < length; i++) insert(string[i], 0); 
	new.insert = 1;
	new.text = strndup(string, length);
	new.length = length;
	create_action(new);
}

static void paste(void) { insert_string(clipboard, cliplength); }
static void cut(void) {
	if (anchor > count) anchor = count;
	if (anchor > cursor) { nat temp = anchor; anchor = cursor; cursor = temp; }
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
	cliplength = cursor - anchor;
	clipboard = strndup(text + anchor, cliplength);
	for (nat i = 0; i < cliplength; i++) delete(0);
	anchor = cursor; previous_cursor = anchor; mode &= ~selecting;
	new.insert = 0;
	new.text = strdup(clipboard);
	new.length = cliplength;
	create_action(new);
}

static void configure_terminal(void) {
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON | IEXTEN | ISIG);
	copy.c_iflag &= ~((size_t) BRKINT | IXON);
	tcsetattr(0, TCSAFLUSH, &copy);
}

static inline void copy(void) {
	FILE* file = popen("pbcopy", "w");
	if (not file) {
		perror("copy popen");
		getchar(); return;
	}
	if (anchor > count) anchor = count;
	if (anchor < cursor) fwrite(text + anchor, 1, cursor - anchor, file);
	else fwrite(text + cursor, 1, anchor - cursor, file);
	pclose(file);
}

static void insert_output(const char* input_command) {
	char command[4096] = {0};
	strlcpy(command, input_command, sizeof command);
	strlcat(command, " 2>&1", sizeof command);
	printf("executing: %s\n", command);
	FILE* f = popen(command, "r");
	if (not f) {
		printf("error: could not execute \"%s\"\n", command);
		perror("insert_output popen");
		getchar(); return;
	}
	char* string = NULL;
	nat length = 0;
	char line[2048] = {0};
	while (fgets(line, sizeof line, f)) {
		size_t l = strlen(line);
		string = realloc(string, length + l);
		memcpy(string + length, line, l);
		length += l;
	}
	pclose(f);
	insert_string(string, length);
	free(string);
}

static void save_file(void) {
	const int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { 
		perror("write open directory"); 
		printf("directory=%s ", directory); 
		getchar(); return; 
	}
	int flags = O_WRONLY | O_TRUNC;  mode_t m = 0;
try_open:;
	const int file = openat(dir, filename, flags, m);
	if (file < 0) {
		if (m) {
			perror("create openat file");
			printf("filename=%s ", filename);
			getchar(); return;
		}
		perror("write openat file");
		printf("filename=%s\n", filename);
		flags = O_CREAT | O_WRONLY | O_TRUNC | O_EXCL;
		m     = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		goto try_open;
	}
	write(file, text, count);
	close(file); close(dir);
	mode |= saved;
	if (m) {
		printf("write: created %lub to ", count);
		printf("%s : %s\n", directory, filename); 
		getchar(); return;
	}
	printf("\033[1mwrite: saved %lub to ", count);
	printf("%s : %s \033[0m\n", directory, filename);
	fflush(stdout); usleep(300000);
}

static inline void now(char datetime[32]) {
	struct timeval t;
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
}
static void insert_now(void) { char dt[32] = {0}; now(dt); insert_string(dt, 17); }
static void alternate(void) { if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0; }
static void undo(void) {
	if (not head) return;
	struct action a = actions[head];
	cursor = a.post_cursor; origin = a.post_origin; mode = (mode & ~saved) | a.post_saved; 
	if (not a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);	
	cursor = a.pre_cursor; origin = a.pre_origin; mode = (mode & ~saved) | a.pre_saved; anchor = cursor;
	head = a.parent; a = actions[head];
	if (a.count > 1) { 
		printf("\033[0;44m[%lu:%lu]\033[0m", a.count, a.choice); 
		getchar(); 
	}
}

static void redo(void) {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	const struct action a = actions[head];
	cursor = a.pre_cursor; origin = a.pre_origin; mode = (mode & ~saved) | a.pre_saved; 
	if (a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);
	cursor = a.post_cursor; origin = a.post_origin; mode = (mode & ~saved) | a.post_saved; anchor = cursor;
	if (a.count > 1) { 
		printf("\033[0;44m[%lu:%lu]\033[0m", a.count, actions[head].choice); 
		getchar(); 
	}
}

static void interpret_arrow_key(void) {
	char c = 0; read(0, &c, 1);
	if (c == 98) move_word_left();
	else if (c == 102) move_word_right();
	else if (c == '[') {
		read(0, &c, 1); 
		if (c == 'A') move_up(); 
		else if (c == 'B') move_down();
		else if (c == 'C') move_right();
		else if (c == 'D') move_left();
		else if (c == 49) {
			read(0, &c, 1); read(0, &c, 1); read(0, &c, 1); 
			if (c == 68) move_begin();
			else if (c == 67) move_end();
		}
		else { printf("2: found escape seq: %d\n", c); getchar(); }
	} 
	else { printf("3: found escape seq: %d\n", c); getchar(); }
}

static void jump_line(const char* line_string) {
	const nat target = (nat) atoi(line_string);
	cursor = 0; origin = 0; cursor_row = 0; cursor_column = 0;
	nat line = 0;
	while (cursor < count) {
		if (line == target) break;
		if (text[cursor] == 10) line++;
		move_right();
	}
}

static void change_directory(const char* d) {
	if (chdir(d) < 0) {
		perror("change directory chdir");
		printf("directory=%s\n", d);
		getchar(); return;
	}
	printf("changed to %s\n", d);
	getchar();
}

static void create_process(char** args) {
	pid_t pid = fork();
	if (pid < 0) { perror("fork"); getchar(); return; }
	if (not pid) {
		if (execve(args[0], args, environ) < 0) { perror("execve"); exit(1); }
	} else {
		int status = 0;
		do waitpid(pid, &status, WUNTRACED);
		while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
}

static void execute(char* command) {
	printf("executing \"%s\"...\n", command);
	const char* string = command;
	const size_t length = strlen(command);
	char** arguments = NULL;
	size_t argument_count = 0;
	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (string[index] != 10) {
			if (not argument_length) start = index;
			argument_length++; continue;
		} else if (not argument_length) continue;
	process_word:
		arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
		arguments[argument_count++] = strndup(string + start, argument_length);
		argument_length = 0;
	}
	if (argument_length) goto process_word;
	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
	arguments[argument_count] = NULL;
	printf("argv(argc=%lu):\n", argument_count);
	for (size_t i = 0; i < argument_count; i++) {
		printf("\targument#%lu: \"%s\"\n", i, arguments[i]);
	}
	printf("[end of args]\n");
	tcsetattr(0, TCSAFLUSH, &terminal);
	create_process(arguments);
	configure_terminal();
	printf("[finished]\n"); getchar();
	free(arguments);
}

static void sendc(void) {
	if (not clipboard) return;
	else if (not strcmp(clipboard, "discard and quit")) mode &= ~active;
	else if (cliplength > 7 and not strncmp(clipboard, "insert ", 7)) insert_output(clipboard + 7);
	else if (cliplength > 7 and not strncmp(clipboard, "change ", 7)) change_directory(clipboard + 7);
	else if (cliplength > 3 and not strncmp(clipboard, "do ", 3)) execute(clipboard + 3);
	else if (cliplength > 5 and not strncmp(clipboard, "line ", 5)) jump_line(clipboard + 5);
	else { printf("unknown command: %s\n", clipboard); getchar(); }
}

int main(int argc, const char** argv) {
	struct sigaction action = {.sa_handler = handler}; 
	sigaction(SIGINT, &action, NULL);
	configure_terminal();
	actions = calloc(1, sizeof(struct action));
	action_count = 1;
	mode = active | saved; 
	char cwd[4096] = {0};
	getcwd(cwd, sizeof cwd);
	strlcpy(directory, cwd, sizeof directory);
	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		now(datetime);
		snprintf(filename, sizeof filename, "%x%x_%s.txt", rand(), rand(), datetime);
		goto loop;
	}
	strlcpy(filename, argv[1], sizeof filename);
	const int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { 
		perror("read open directory"); 
		printf("directory=%s ", directory);  exit(1);
	}
	int df = openat(dir, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	const int file = openat(dir, filename, O_RDONLY, 0);
	if (file < 0) { 
		read_error: perror("read openat file"); 
		printf("filename=%s ", filename);  exit(1);
	}
	count = (size_t) lseek(file, 0, SEEK_END);
	text = malloc(count);
	lseek(file, 0, SEEK_SET);
	read(file, text, count);
	close(file); close(dir);
	anchor = 0; cursor = 0; origin = 0; cursor_row = 0; cursor_column = 0;
	clipboard = NULL; cliplength = 0;
	actions = calloc(1, sizeof(struct action));
	head = 0; action_count = 1;
	char c = 0;
loop:	display();
	read(0, &c, 1);
	previous_cursor = cursor;
	if (c == 127 or c == 'D') delete(1);
	else if (c == 8  H) {}
	else if (c == 18 R) {}
	else if (c == 24 X) {}
	else if (c == 26 Z) {}
	else if (c == 17 Q) { if (mode & saved) mode &= ~active; }
	else if (c == 4  D) insert_now();
	else if (c == 1  A) { read(0, &c, 1); insert(c, 1); }
	else if (c == 19 S) alternate();
	else if (c == 27)  interpret_arrow_key();
	else if (c == 'Q') { if (mode & saved) mode &= ~active; }
	else if (c == 'T') { anchor = cursor; mode ^= selecting; }
	else if (c == 'R') cut();
	else if (c == 'C') copy();
	else if (c == 'W') paste();
	else if (c == 'V') insert_output("pbpaste");
	else if (c == 'A') sendc();
	else if (c == 'S') save_file();
	else if (c == 'H') backwards();
	else if (c == 'M') forwards();
	else if (c == 'X') redo();
	else if (c == 'Z') undo();
	else if (c == 'N') move_left();
	else if (c == 'U') move_word_left();
	else if (c == 'P') move_word_right();
	else if (c == 'K') move_begin();
	else if (c == 'I') move_end();
	else if (c == 'E') move_up();
	else if (c == 'O') move_right();
	else if (c == 'L') move_down();
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);

	if (not (mode & selecting)) anchor = previous_cursor;
	if (mode & active) goto loop;
	printf("\033[H\033[2J");
	tcsetattr(0, TCSAFLUSH, &terminal);
}











static void insert(char c) {

	struct stat s;
	fstat(file, &s);
	count = s.st_size;
	size_t size = (size_t) (count - cursor);
	char* r = malloc(size + 1);
	r[0] = c;

	lseek(file, (off_t) cursor, SEEK_SET);
	read(file, r + 1, size);
	lseek(file, (off_t) cursor, SEEK_SET);
	write(file, r, size + 1);

	free(r);

	move_right();

	fsync(file);





}










char c = 0;
		lseek(file, cursor, SEEK_SET);
		ssize_t n = read(file, &c, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (c == 10) break;













































#include <stdio.h>  // 202310017.201349:   
#include <stdlib.h> //  a combination of the ed-like simpler editor, with the nonvolatile data structure! 
#include <string.h> 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
int main(int argc, const char** argv) {
	typedef size_t nat;
	const char active = 0x01, inserting = 0x02;
	int file = -1, history = -1;
	int mode = 0;
	off_t cursor = 0, count = 0, anchor = 0, m = 0;
	const char* help_string = "q z c m d<str> t<str> h<str> r<str> a<num> s<num> ";
	char filename[4096] = {0}, directory[4096] = {0};
	getcwd(directory, sizeof directory);
	int flags = O_RDWR;
	mode_t permission = 0;
	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t; gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%x%x_%s.txt", rand(), rand(), datetime);
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	} else if (argc == 2) strlcpy(filename, argv[1], sizeof filename);
	else exit(puts("usage error"));
	const int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { perror("read open directory"); exit(1); }
	int df = openat(dir, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	file = openat(dir, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }

	struct termios original = {0};
	tcgetattr(0, &original);
	struct termios terminal = original; 
	terminal.c_cc[VKILL]   = _POSIX_VDISABLE;
	terminal.c_cc[VWERASE] = _POSIX_VDISABLE;
	terminal.c_cc[VQUIT]   = _POSIX_VDISABLE;
	tcsetattr(0, TCSAFLUSH, &terminal);

	struct stat s;
	fstat(file, &s);
	count = s.st_size;
	anchor = 0;
	cursor = count;
	char buffer[64] = {0};
	mode = active;
	goto loop;

sw:	mode ^= inserting;
	write(1, "\n", 1);
loop:;	ssize_t n = read(0, buffer, sizeof buffer - 1); // printf("read: \"%.*s\" : %ld\n", (int) n, buffer, n);
	if (n < 0) { perror("read"); exit(errno); } 
	else if (n == 0) goto sw;

	if (mode & inserting) {
		if (n == 4 and not memcmp(buffer, "drt\n", 4)) goto sw;
	insert: fstat(file, &s);
		count = s.st_size;
		const nat length = (nat) n;
		const size_t size = (size_t) (count - cursor);
		char* rest = malloc(size + length); 
		memcpy(rest, buffer, length);
		lseek(file, cursor, SEEK_SET);
		read(file, rest + length, size);
		lseek(file, cursor, SEEK_SET);
		write(file, rest, size + length);
		count += length;
		cursor += length;
		fsync(file);
		free(rest);
		goto loop;
	}
	const char c = *buffer;
	     if (c == 'q') mode = 0;
	else if (c == 'z') puts(help_string); 
	else if (c == 'c') write(1, "\033[H\033[2J", 7);
	else if (c == 'm') printf("%d %llu %llu-%llu %lld\n", mode, count, cursor, anchor, cursor - anchor);
	else if (c == 'a') anchor = cursor + atoi(buffer + 1); 
	else if (c == 't') {
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		if (n == 1) goto sw; 
		memmove(buffer, buffer + 1, (size_t) --n);
		goto insert;
	} else if (c == 's') { // delete <num>
		fstat(file, &s);
		count = s.st_size;
		if (anchor > count) anchor = count;
		if (cursor > count) cursor = count;
		if (anchor > cursor) { off_t temp = anchor; anchor = cursor; cursor = temp; }
		const off_t length = cursor - anchor;
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		if (length != atoi(buffer + 1)) goto error;
		const nat size = (nat)(count - cursor);
		char* rest = malloc(size);
		lseek(file, cursor, SEEK_SET);
		read(file, rest, size);
		lseek(file, cursor - length, SEEK_SET);
		write(file, rest, size);
		count -= length;
		ftruncate(file, count);
		fsync(file);
		free(rest);
		cursor = anchor;


				///                  fund problem       we can't do anything with newlines. we need to fix this. newlines shouldnt cause so much of an issue. we need to fix this. 
	} else if (c == 'd') {    // display <number>         TODO: delete the  first ternary expresion, only print a-c selection. 
		nat begin = 0, end = 0;
		fstat(file, &s);
		count = s.st_size;
		if (anchor > count) anchor = count;
		if (cursor > count) cursor = count;
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		if (n > 1) m = atoi(buffer + 1);
		begin = m ? (cursor - m >= 0    ? (nat) (cursor - m) : (nat) 0)     : (anchor < cursor ? (nat) anchor : (nat) cursor);
		end   = m ? (cursor + m < count ? (nat) (cursor + m) : (nat) count) : (anchor < cursor ? (nat) cursor : (nat) anchor);
		char* screen = malloc(end - begin);
		lseek(file, (off_t) begin, SEEK_SET);
		read(file, screen, end - begin);
		write(1, screen, end - begin);

	} else if (*buffer == 'r') {    // forwards <string>
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		const char* clipboard = buffer + 1;
		const nat cliplength = (nat) n - 1;
		nat t = 0;
		sloop: if (t == cliplength or cursor >= count) goto loop;
		char cc = 0;
		lseek(file, cursor, SEEK_SET);
		n = read(file, &cc, 1);
		if (n == 0) goto loop;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (cc != clipboard[t]) t = 0; else t++;
		cursor++; goto sloop;

	} else if (*buffer == 'h') {    //  backwards <string>
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		const char* clipboard = buffer + 1;
		const nat cliplength = (nat) n - 1;
		nat t = cliplength;
		bloop: if (not t or not cursor) goto loop;
		cursor--; t--; 		
		char cc = 0;
		lseek(file, cursor, SEEK_SET);
		n = read(file, &cc, 1);
		if (n == 0) goto loop;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (cc != clipboard[t]) t = cliplength;
		goto bloop;

	} else error: write(1, "?\n", 2);

	if (mode) goto loop;
	close(file);
	close(dir);
	tcsetattr(0, TCSAFLUSH, &original);
}












 























































// static struct winsize window = {0};
// , anchor = 0;
// origin = 0,
// static int cursor_row = 0, cursor_column = 0, desired_column = 0;



static void forwards(void) {
	nat t = 0;
	loop: if (t == cliplength or cursor >= count) return;
	if (text[cursor] != clipboard[t]) t = 0; else t++;
	move_right(); 
	goto loop;
}

static void backwards(void) {
	nat t = cliplength;
	loop: if (not t or not cursor) return;
	move_left(); t--; 
	if (text[cursor] != clipboard[t]) t = cliplength;
	goto loop;
}



static void get_count(void) {
	
}

static void insert(char* string, nat length) {
	struct stat s;
	fstat(file, &s);
	count = s.st_size;

	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size + length); 
	memcpy(rest, string, length);
	lseek(file, cursor, SEEK_SET);
	read(file, rest + length, size);
	lseek(file, cursor, SEEK_SET);
	write(file, rest, size + length);
	count += length;
	fsync(file);
	free(rest);
	for (nat i = 0; i < length; i++) move_right();
}

static void delete(off_t length) {
	if (cursor < length) return;

	struct stat s;
	fstat(file, &s);
	count = s.st_size;

	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size);
	lseek(file, cursor, SEEK_SET);
	read(file, rest, size);
	lseek(file, cursor - length, SEEK_SET);
	write(file, rest, size);
	count -= length;
	ftruncate(file, count);
	fsync(file);
	free(rest);
	for (nat i = 0; i < length; i++) move_left();
}



	} else if (n < (ssize_t) sizeof buffer) {
		printf("[small line]\n");
	} else if (n == (ssize_t) sizeof buffer) {
		printf("[normal buffer read]\n");
	}













































if the lien    is exactly the rap with         then we will get a cursor   d sink

		static void create_action(struct action new) {
			new.post_cursor = cursor; 
			new.post_origin = origin; 
		//	new.post_saved = mode & saved;
			new.parent = head;
			actions[head].children = realloc(actions[head].children, sizeof(size_t) * (actions[head].count + 1));
			actions[head].choice = actions[head].count;
			actions[head].children[actions[head].count++] = action_count;
			head = action_count;
			actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
			actions[action_count++] = new;
		}
 
		static void delete(bool should_record) {
			if (not cursor) return;
			struct action new = {
				.pre_cursor = cursor, 
				.pre_origin = origin, 
		//		.pre_saved = mode & saved
			};
			const char c = text[cursor - 1];
			memmove(text + cursor - 1, text + cursor, count - cursor);
			count--;
			text = realloc(text, count); 
			mode &= ~saved; move_left();
			if (not should_record) return;
			new.insert = 0;
			new.length = 1;
			new.text = malloc(1);
			new.text[0] = c;
			create_action(new);
		}

		static void insert(char c, bool should_record) {
			struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};
			text = realloc(text, count + 1);
			memmove(text + cursor + 1, text + cursor, count - cursor);
			text[cursor] = c;
			count++; mode &= ~saved;
			move_right();
			if (not should_record) return;
			new.insert = 1;
			new.length = 1;
			new.text = malloc(1);
			new.text[0] = c;
			create_action(new);
		}



static void alternate(void) { if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0; }
static void undo(void) {
	if (not head) return;
	struct action a = actions[head];
	cursor = a.post_cursor; origin = a.post_origin; mode = (mode & ~saved) | a.post_saved; 
	if (not a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);	
	cursor = a.pre_cursor; origin = a.pre_origin; mode = (mode & ~saved) | a.pre_saved; anchor = cursor;
	head = a.parent; a = actions[head];
	if (a.count > 1) { 
		printf("\033[0;44m[%lu:%lu]\033[0m", a.count, a.choice); 
		getchar(); 
	}
}

static void redo(void) {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	const struct action a = actions[head];
	cursor = a.pre_cursor; origin = a.pre_origin; mode = (mode & ~saved) | a.pre_saved; 
	if (a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);
	cursor = a.post_cursor; origin = a.post_origin; mode = (mode & ~saved) | a.post_saved; anchor = cursor;
	if (a.count > 1) { 
		printf("\033[0;44m[%lu:%lu]\033[0m", a.count, actions[head].choice); 
		getchar(); 
	}
}




typedef size_t nat;


						struct action {
							char* text;
						//	nat* children;
							nat parent, 
							choice, 
						//	count, 
							length, 
						//	insert,
							pre_cursor, post_cursor, 
						//	pre_origin, post_origin, 
						//	pre_saved, post_saved;
						};


						struct action {
							char* text;
																//	nat* children;
							nat parent, 
				not	necc	--->	choice, 
																//	count, 
							length, 
																//	insert,
							pre_cursor, post_cursor, 
															//	pre_origin, post_origin, 
															//	pre_saved, post_saved;
						};






						struct action {
							nat parent, pre_cursor, post_cursor, length;          4 * 4bytes (u32)  =  16 bytes + string.
							char* text;
						};




				each of these will be uint32_t's, except for   length which is a   int32_t   i think  ie signed 
				so yeah. yay


						



							oh, also head      which is the first 4 bytes of the file, 
							is found at the beginning,  so all offsets   .parents       will be larger than 4. 

									so yeah 


								also .parents are file offsets of other node's .parent







		struct action_header {
			u32 parent;
			u32 pre;
			u32 post;
			u32 length;
		};


	we will simply write a header, and then write the string .text  directly afterwards. 

			using   

				add new node in tree:

				===================================

		

						struct action_header node = {
							.parent = read_head(),
							.pre = cursor,
							.length = text_length,
						};


							// do the operation();
			

						node.post = cursor;
						move_to_end_of_file
						pos = get_current_file_pos;
						write(history, node, sizeof node);
						write(history, text, text_length);
						head = get_current_file_pos;
						write_head(head);
		










static const nat active = 0x01, saved = 0x02, selecting = 0x04;


extern char** environ;

static struct termios terminal;

static struct winsize window;



static char filename[4096] = {0}, directory[4096] = {0};

static char* text = NULL, * clipboard = NULL;

static struct action* actions = NULL;

static nat mode = 0, 
	
	cursor = 0, origin = 0, anchor = 0, 

	cursor_row = 0, cursor_column = 0, 

	   count = 0, 

	cliplength = 0, 


	action_count = 0, head = 0, 


previous_cursor = 0;




		TODO: implement this:
	---------------------------------------------------------------




			- implementing the undo tree     create a .history file in the /histories/ dir,
								 if none was supplied on command line.


			- more efficient display logic:   multiple types of display updates:


			x	0 means full refresh,	

		 	x	1 means partial refresh after cursor, 

			x	2 means just update cursor position, 

			x	3 means scroll screen downwards, 

			x	4 means scroll screen upwards. 


	done:

		x	- move up       on visual lines

		x	- make it nonmodal, by using a input history system! i think.. 


	
			
	




------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------





*/






