#include <stdio.h>   // 202402191.234834: a modal text editor written by dwrr. 
#include <stdlib.h>  // new modal editor: rewritten on 202406075.165239
#include <string.h>  // to be more stable, and display text with less bugs. 
#include <iso646.h>  // also redo all the keybinds and change semantics of 
#include <unistd.h>  // many commands.
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <stdbool.h>
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
#include <stdnoreturn.h>

typedef uint64_t nat;

struct action {
	nat parent;
	nat pre;
	nat post;
	nat choice;
	int64_t length;
	char* string;
};

#define disabled   (nat)~0

static const char* autosave_directory = "/Users/dwrr/root/personal/autosaves/";
static const nat autosave_frequency = 200;

static nat 
	moved = 0,          // delete the need for this variable. just use desired, i think..? or nothing?
	cursor = 0,  
	count = 0, 
	anchor = 0, 
	head = 0, 
	action_count = 0, 
	desired = 0, 
	cliplength = 0, 
	tasklength = 0,
	task2length = 0,
	autosave_counter = 0;

static char* text = NULL, * clipboard = NULL, * taskboard = NULL, * task2board = NULL;
static struct action* actions = NULL;

static char status[4096] = {0};
static char filename[4096] = {0};
static char autosavename[4096] = {0};
static volatile struct winsize window = {0};
static struct termios terminal = {0};

extern char** environ;

#define max_screen_size  1 << 20

static nat append(
	const char* string, nat n, 
	char* screen, nat length
) {
	if (length >= max_screen_size) return 0;
	memcpy(screen + length, string, n);
	return n;
}

static void print(const char* string) {
	const nat max_length = sizeof status - 1;
	const nat length = strlen(status);
	const nat remaining = max_length - length;
	const nat l = strlen(string);
	const nat n = l < remaining ? l : remaining;

	memcpy(status + length, string, n);
	status[length + n] = 0;
}

static void number(nat n) { char s[128] = {0}; snprintf(s, sizeof s, "#%llx", n); print(s); }

static nat cursor_in_view(nat origin) {
	nat i = origin, row = 0, column = 0;
	for (; i < count; i++) {
		if (text[i] == 10) {
			if (i == cursor) return true;
		nl:	row++; column = 0;
		} else if (text[i] == 9) {
			nat amount = 8 - column % 8;
			column += amount;
			if (i == cursor) return true;
		} else {
			if (i == cursor) return true;
			if (column >= window.ws_col - 2) goto nl;
			else if ((unsigned char) text[i] >> 6 != 2 and text[i] >= 32) column++;
		}
		if (row >= window.ws_row) break;
	}
	if (i == cursor) return true;
	return false;
}

static nat origin = 0;

static void update_origin(void) {
	if (origin > count) origin = count;

	if (not cursor_in_view(origin)) {
		if (cursor < origin) { 
		while (origin and 
		not cursor_in_view(origin)){
		do origin--; while (origin and 
		text[origin - 1] != 10);
		} }
		else if (cursor > origin) { 
		while (origin < count 
		and not cursor_in_view(origin)) {
		do origin++; while (origin < count and
		(not origin or text[origin - 1] != 10));
		} }
	}
}

static void display(void) {
	char screen[max_screen_size];
	nat length = append("\033[H", 3, screen, 0);

	update_origin();
	nat i = origin, row = 0, column = 0;
	for (; i < count; i++) {
		if (row >= window.ws_row) break;
		if (text[i] == 10) {
			if (i == cursor or i == anchor) length += append("\033[7m \033[0m", 9, screen, length);
		nl:	length += append("\033[K", 3, screen, length);
			if (row < window.ws_row - 1) length += append("\n", 1, screen, length);
			row++; column = 0;

		} else if (text[i] == 9) {
			nat amount = 8 - column % 8;
			column += amount;
			if (i == cursor or i == anchor) { length += append("\033[7m \033[0m", 9, screen, length); amount--; }
			length += append("        ", amount, screen, length);

		} else {
			if (i == cursor or i == anchor) { length += append("\033[7m", 4, screen, length); }
			length += append(text + i, 1, screen, length); 
			if (i == cursor or i == anchor) { length += append("\033[0m", 4, screen, length); }
			if (column >= window.ws_col - 2) goto nl;
			else if ((unsigned char) text[i] >> 6 != 2 and text[i] >= 32) column++;
		}
	}
	if (i == cursor or i == anchor) length += append("\033[7m \033[0m", 9, screen, length);

	while (row < window.ws_row) {
		length += append("\033[K", 3, screen, length);
		if (row < window.ws_row - 1) length += append("\n", 1, screen, length);
		row++;
	}

	if (*status) {
		length += append("\033[H", 3, screen, length);
		length += append(status, strlen(status), screen, length);
		length += append("\033[7m/\033[0m", 9, screen, length);
		memset(status, 0, sizeof status);
	}

	write(1, screen, length);
}

static void left(void) { if (cursor) cursor--; moved = 1; }
static void right(void) { if (cursor < count) cursor++; moved = 1; }

static nat compute_current_visual_cursor_column(void) {
	nat i = cursor, column = 0;
	while (i and text[i - 1] != 10) i--;
	while (i < cursor and text[i] != 10) {
		if (text[i] == 9) { nat amount = 8 - column % 8; column += amount; }
		else if (column >= window.ws_col - 2 - 1) column = 0;
		else if ((unsigned char) text[i] >> 6 != 2 and text[i] >= 32) column++;
		i++;
	}
	return column;
}

static void move_cursor_to_visual_position(nat target) {
	nat column = 0;
	while (cursor < count and text[cursor] != 10) {
		if (column >= target) return;
		if (text[cursor] == 9) { nat amount = 8 - column % 8; column += amount; }
		else if (column >= window.ws_col - 2 - 1) column = 0;
		else if ((unsigned char) text[cursor] >> 6 != 2 and text[cursor] >= 32) column++;
		right();
	}
}

static void up(void) {
	const bool m = moved; 
	const nat column = compute_current_visual_cursor_column();
	while (cursor and text[cursor - 1] != 10) left(); left();
	while (cursor and text[cursor - 1] != 10) left();
	move_cursor_to_visual_position(not m ? desired : column);
	if (m) desired = column;
	moved = 0;
}

static void down(void) {
	const bool m = moved; 
	const nat column = compute_current_visual_cursor_column();
	while (cursor < count and text[cursor] != 10) right(); right();
	move_cursor_to_visual_position(not m ? desired : column);
	if (m) desired = column;
	moved = 0;
}

static void up_begin(void) {
	while (cursor) {
		left();
		if (not cursor or text[cursor - 1] == 10) break;
	}
}

static void down_end(void) {
	while (cursor < count) {
		right();
		if (cursor >= count or text[cursor] == 10) break;
	}
}

static void word_left(void) {
	left();
	while (cursor) {
		if (not (not isalnum(text[cursor]) or isalnum(text[cursor - 1]))) break;
		if (text[cursor - 1] == 10) break;
		left();
	}
}

static void word_right(void) {
	right();
	while (cursor < count) {
		if (not (isalnum(text[cursor]) or not isalnum(text[cursor - 1]))) break;
		if (text[cursor] == 10) break;
		right();
	}
}

static void write_string(const char* directory, 
			char* w_string, nat w_length) {

	char name[4096] = {0};
	srand((unsigned)time(0)); rand();
	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
	snprintf(name, sizeof name, "%s%s_%08x%08x.txt", directory, datetime, rand(), rand());
	int flags = O_WRONLY | O_TRUNC | O_CREAT | O_EXCL;
	mode_t permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	
	int file = open(name, flags, permission);
	if (file < 0) { perror("save: open file"); puts(name); getchar(); }
	write(file, w_string, w_length);

	print("ws"); number(w_length); print(":"); print(name); print("|");
	fsync(file);
	close(file);
}


static void write_file(const char* directory, char* name, size_t maxsize) {

	int flags = O_WRONLY | O_TRUNC;
	mode_t permission = 0;
	if (not *name) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(name, maxsize, "%s%s_%08x%08x.txt", directory, datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		print("g\""); print(name); print("\"|");
	}
	int file = open(name, flags, permission);
	if (file < 0) { perror("save: open file"); puts(name); getchar(); }
	write(file, text, count);

	char string[4096] = {0};
	print("w"); number(count); print("|");
	print(string);
	fsync(file);
	close(file);
}

static void autosave(void) {
	autosave_counter = 0;

	print("as|");
	write_file(autosave_directory, autosavename, sizeof autosavename);
}

static void save(void) {
	print("s|");
	write_file("./", filename, sizeof filename);
}

static void finish_action(struct action node, char* string, int64_t length) {
	node.choice = 0;
	node.parent = head;
	node.post = cursor; 
	node.string = strndup(string, (nat) length);
	node.length = length;
	head = action_count;
	actions = realloc(actions, sizeof(struct action) * (action_count + 1));
	actions[action_count++] = node;
}

static void insert(char* string, nat length, bool should_record) {
	if (should_record) autosave_counter++;
	if (autosave_counter >= autosave_frequency and should_record) autosave();
	struct action node = { .pre = cursor };
	text = realloc(text, count + length);
	memmove(text + cursor + length, text + cursor, count - cursor);
	memcpy(text + cursor, string, length);
	count += length; cursor += length;
	if (should_record) finish_action(node, string, (int64_t) length);
}

static void delete(nat length, bool should_record) {
	if (cursor < length) return;
	if (should_record) autosave_counter++;
	if (autosave_counter >= autosave_frequency and should_record) autosave();
	struct action node = { .pre = cursor };
	cursor -= length; count -= length; 
	char* string = strndup(text + cursor, length);
	memmove(text + cursor, text + cursor + length, count - cursor);
	text = realloc(text, count);
	if (should_record) finish_action(node, string, (int64_t) -length);
}

static void redo(void) {
	nat chosen_child = 0, child_count = 0; 
	for (nat i = 0; i < action_count; i++) {
		if (actions[i].parent != head) continue;
		if (child_count == actions[head].choice) chosen_child = i;
		child_count++;
	}
	if (not child_count) return;
	if (child_count >= 2) {
		print("rd:c"); number(actions[head].choice); 
		print("/"); number(child_count); print("|");
		actions[head].choice = (actions[head].choice + 1) % child_count;
	}
	head = chosen_child;
	const struct action node = actions[head];
	cursor = node.pre; 
	
	if (node.length > 0) insert(node.string, (nat) node.length, 0); else delete((nat) -node.length, 0);
	cursor = node.post;

	print("rd:"); if (node.length < 0) print("-"); else print("+");
	number((nat)(node.length < 0 ? -node.length : node.length)); 
	print(",h"); number(head); print("|");
}

static void undo(void) {
	if (not head) return;
	struct action node = actions[head];
	cursor = node.post;
	if (node.length > 0) delete((nat) node.length, 0); else insert(node.string, (nat) -node.length, 0); 
	cursor = node.pre;
	head = node.parent;

	print("ud:"); if (node.length < 0) print("-"); else print("+");
	number((nat) (node.length < 0 ? -node.length : node.length)); 
	print(",h"); number(head); print("|");
}

static void insert_dt(void) {
	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
	printf("insert_dt: inserted datetime \"%s\" at cursor %llu...\n", datetime, cursor);
	insert(datetime, strlen(datetime), 1);
}

static void delete_selection(void) {
	if (anchor > count or anchor == cursor) return;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	const nat len = cursor - anchor;
	delete(len, 1); anchor = disabled;
	print("cut:r"); number(len);
	print(",c"); number(cursor); print("|");
}

static void search_forwards(const char* string, nat length) {
	nat t = 0;
	loop: if (t == length or cursor >= count) return;
	if (text[cursor] != string[t]) t = 0; else t++; 
	right(); goto loop;
}

static void search_backwards(const char* string, nat length) {
	nat t = length;
	loop: if (not t or not cursor) return;
	left(); t--; 
	if (text[cursor] != string[t]) t = length;
	goto loop;
}

static inline void copy_local(void) {
	if (anchor > count or anchor == cursor) return;
	cliplength = anchor < cursor ? cursor - anchor : anchor - cursor;
	free(clipboard);
	clipboard = strndup(text + (anchor < cursor ? anchor : cursor), cliplength);
	anchor = disabled;
	print("cl:l"); number(cliplength);
	print(",c"); number(cursor); print("|");
}

static inline void copy_global(void) {
	if (anchor > count or anchor == cursor) return;
	const nat length = anchor < cursor ? cursor - anchor : anchor - cursor;
	FILE* globalclip = popen("pbcopy", "w");
	if (not globalclip) {
		print("cpy:*:\"");
		print(strerror(errno));
		print("\"|"); return;
	}
	fwrite(text + (anchor < cursor ? anchor : cursor), 1, length, globalclip);
	pclose(globalclip);
	anchor = disabled;
	print("cg:l"); number(length);
	print(",c"); number(cursor); print("|");
}

static void insert_output(const char* input_command) {
	save();
	char command[4096] = {0};
	//strlcpy(command, input_command, sizeof command);
	//strlcat(command, " 2>&1", sizeof command);
	snprintf(command, sizeof command, "%s 2>&1", input_command);

	FILE* f = popen(command, "r");
	if (not f) {
		print("iop:*:popen\""); print(strerror(errno)); print("\"|");
		return;
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

static void paste_global(void) { insert_output("pbpaste"); }
static void paste(void) { insert(clipboard, cliplength, 1); }
static void window_resized(int _) {if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	save(); puts(""); exit(0); 
}

static void change_directory(const char* d) {
	if (chdir(d) < 0) {
		print("crp:err\""); print(d); print("\"fork\""); 
		print(strerror(errno)); print("\"|");
		return;
	}
	print("changed directories\n");
}

static void create_process(char** args) {
	pid_t pid = fork();
	if (pid < 0) { 
		print("crp:*:fork\""); print(strerror(errno)); print("\"|");
		return;
	}
	if (not pid) {
		if (execve(args[0], args, environ) < 0) { perror("execve"); exit(1); }
	} 
	int s = 0;
	if ((pid = wait(&s)) == -1) { 
		print("crp:*:wait\""); print(strerror(errno)); print("\"|");
		return;
	}
	char dt[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
	if (WIFEXITED(s)) 	printf("[%s:(%d) exited with code %d]\n", dt, pid, WEXITSTATUS(s));
	else if (WIFSIGNALED(s))printf("[%s:(%d) was terminated by signal %s]\n", dt, pid, strsignal(WTERMSIG(s)));
	else if (WIFSTOPPED(s)) printf("[%s:(%d) was stopped by signal %s]\n", 	dt, pid, strsignal(WSTOPSIG(s)));
	else 			printf("[%s:(%d) terminated for an unknown reason]\n", dt, pid);
	fflush(stdout);
	getchar();
}

static void execute(char* command) {
	if (not strlen(command)) return;
	save();
	const char delimiter = command[0];
	const char* string = command + 1;
	const size_t length = strlen(command + 1);

	char** arguments = NULL;
	size_t argument_count = 0;

	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (string[index] != delimiter) {
			if (not argument_length) start = index;
			argument_length++;

		} else if (string[index] == delimiter) {
		push:	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
			arguments[argument_count++] = strndup(string + start, argument_length);
			start = index;
			argument_length = 0; 
		}
	}
	if (argument_length) goto push;

	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
	arguments[argument_count] = NULL;

	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);

	for (nat i = 0; i < (nat) (window.ws_row * 2); i++) puts("");
	printf("\033[H"); fflush(stdout);

	create_process(arguments);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);
	free(arguments);
}

static void jump_index(char* string) {
	const size_t n = (size_t) atoi(string);
	cursor = 0;
	for (size_t i = 0; i < n; i++) right();
}

static void jump_line(char* string) {
	const size_t n = (size_t) atoi(string);
	cursor = 0;
	for (size_t i = 0; i < n; i++) down_end();
	up_begin(); 
}

static void string_to_task(char* s) {
	print("ctt:c"); number(cliplength); print("\"|");
	free(taskboard);
	tasklength = strlen(s);
	taskboard = strdup(s);
}

static void task_to_clip(void) {
	print("ttc:t"); number(tasklength); print("\"|");
	free(clipboard);
	cliplength = tasklength;
	clipboard = strndup(taskboard, tasklength);
}

static void string_to_task2(char* s) {
	print("ctt2:c"); number(cliplength); print("\"|");
	free(task2board);
	task2length = strlen(s);
	task2board = strdup(s);
}

static void task2_to_clip(void) {
	print("t2tc:t"); number(task2length); print("\"|");
	free(clipboard);
	cliplength = task2length;
	clipboard = strndup(task2board, task2length);
}

static void half_page_up(void)   { for (int i = 0; i < (window.ws_row) / 2; i++) up(); } 
static void half_page_down(void) { for (int i = 0; i < (window.ws_row) / 2; i++) down(); }

static char remap(const char c) {

	if (c == 13) return 10;

	/*  to use qwerty, uncomment these lines:  */
	// const char upper_remap_alpha[26] = "AVMHRTGYUNEOLKP:QWSBFCDXJZ";
	// const char lower_remap_alpha[26] = "avmhrtgyuneolkp;qwsbfcdxjz";
	// if (c >= 'A' and c <= 'Z') upper_remap_alpha[c - 'A'];
	// if (c >= 'a' and c <= 'z') lower_remap_alpha[c - 'a'];
	// if (c == ';') return 'i';
	// if (c == 'I') return 'I';

	return c;
}

static void center(void) {
	const nat saved = cursor;
	half_page_down(); 
	update_origin();
	half_page_up();
	half_page_up(); 
	update_origin();
	half_page_down();
	cursor = saved;
}

static void insert_char(void) {
	char c = 0;
	read(0, &c, 1);
	if (c == 'a') insert_dt();
	else if (c == 'r') { c = 10; insert(&c, 1, 1); }
	else if (c == 'h') { c = 32;  insert(&c, 1, 1); }
	else if (c == 'm') { c = 9;  insert(&c, 1, 1); }
	else if (c == 't') { read(0, &c, 1); insert(&c, 1, 1); }
	else if (c == 'u') { read(0, &c, 1); c = (char) toupper(c); insert(&c, 1, 1); }
	else if (c == 'n') {
		read(0, &c, 1);
		if (c == 'a') { c = '0'; insert(&c, 1, 1); }
		if (c == 'd') { c = '1'; insert(&c, 1, 1); }
		if (c == 'r') { c = '2'; insert(&c, 1, 1); }
		if (c == 't') { c = '3'; insert(&c, 1, 1); }
		if (c == 'm') { c = '4'; insert(&c, 1, 1); }
		if (c == 'l') { c = '5'; insert(&c, 1, 1); }
		if (c == 'n') { c = '6'; insert(&c, 1, 1); }
		if (c == 'u') { c = '7'; insert(&c, 1, 1); }
		if (c == 'p') { c = '8'; insert(&c, 1, 1); }
		if (c == 'i') { c = '9'; insert(&c, 1, 1); }
	}
	
	else if (c == 'e') {
		read(0, &c, 1);

		if (c == 'a') { c = '\'';insert(&c, 1, 1); }
		if (c == 'd') { c = '('; insert(&c, 1, 1); }
		if (c == 'r') { c = ')'; insert(&c, 1, 1); }
		if (c == 't') { c = '.'; insert(&c, 1, 1); }
		if (c == 'n') { c = ','; insert(&c, 1, 1); }
		if (c == 'u') { c = '['; insert(&c, 1, 1); }
		if (c == 'p') { c = ']'; insert(&c, 1, 1); }
		if (c == 'i') { c = '"'; insert(&c, 1, 1); }

		if (c == 's') { c = '<'; insert(&c, 1, 1); }
		if (c == 'h') { c = '>'; insert(&c, 1, 1); }
		if (c == 'e') { c = '{'; insert(&c, 1, 1); }
		if (c == 'o') { c = '}'; insert(&c, 1, 1); }

		if (c == 'm') { c = '-'; insert(&c, 1, 1); }
		if (c == 'l') { c = '_'; insert(&c, 1, 1); }
		if (c == 'c') { c = '~'; insert(&c, 1, 1); }
		if (c == 'k') { c = '`'; insert(&c, 1, 1); }

	} else if (c == 'o') {
		read(0, &c, 1);
		if (c == 'a') { c = '+'; insert(&c, 1, 1); }
		if (c == 'd') { c = '?'; insert(&c, 1, 1); }
		if (c == 'r') { c = '!'; insert(&c, 1, 1); }
		if (c == 't') { c = '*'; insert(&c, 1, 1); }
		if (c == 'n') { c = '/'; insert(&c, 1, 1); }
		if (c == 'u') { c = '%'; insert(&c, 1, 1); }
		if (c == 'p') { c = '^'; insert(&c, 1, 1); }
		if (c == 'i') { c = '='; insert(&c, 1, 1); }

		if (c == 's') { c = '|'; insert(&c, 1, 1); }
		if (c == 'h') { c = '&'; insert(&c, 1, 1); }
		if (c == 'e') { c = '@'; insert(&c, 1, 1); }
		if (c == 'o') { c = '\\';insert(&c, 1, 1); }

		if (c == 'm') { c = ':'; insert(&c, 1, 1); }
		if (c == 'l') { c = ';'; insert(&c, 1, 1); }
		if (c == 'c') { c = '#'; insert(&c, 1, 1); }
		if (c == 'k') { c = '$'; insert(&c, 1, 1); }
	}
	else {
		print("*:"); number((nat)c); print("|");
	}
}

int main(int argc, const char** argv) {
	struct sigaction action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);

	if (argc == 0) exit(1);
	else if (argc == 1) goto new;
	else if (argc == 2) {
		snprintf(filename, sizeof filename, "%s", argv[1]);

	} else if (argc == 3 and argv[1][0] == '+') {
		snprintf(filename, sizeof filename, "%s", argv[2]);

	} else {
		printf("received these %u arguments:\n", argc);
		for (int i = 0; i < argc; i++) {
			printf("   arg #%u = \"%s\"\n", i, argv[i]);
		}
		puts("end of argv.");
		return puts("usage: ./editor [file]");
	}
	
	int df = open(filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = open(filename, O_RDONLY);
	if (file < 0) { read_error: printf("could not open \"%s\"\n", filename); perror("editor: open"); exit(1); }
	struct stat ss; fstat(file, &ss);
	count = (nat) ss.st_size;
	text = malloc(count);
	read(file, text, count);
	close(file);

new: 	cursor = 0; anchor = disabled;
	finish_action((struct action) {0}, NULL, (int64_t) 0);
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 1; 
	terminal_copy.c_cc[VTIME] = 0;
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);

	enum mode {edit_mode, insert_mode, search_mode};
	nat mode = 0;

	char history[5] = {0};
	char S[1024] = {0};
	nat ss_len = 0;

loop:	ioctl(0, TIOCGWINSZ, &window);
	display();
	char c = 0;
	const ssize_t n = read(0, &c, 1);
	if (n < 0) { perror("read"); fflush(stderr); }
	c = remap(c);
	if (mode == insert_mode) {
		if (
			c == 'n' and 
			history[0] == 'u' and 
			history[1] == 'p' and
			history[2] == 't' and 
			history[3] == 'r' and
			history[4] == 'd'
		) {
			memset(history, 0, 5);
			delete(5, 1);
			mode = edit_mode;
		}
		else if (c == 27) mode = edit_mode;
		else if (c == 127) delete(1,1);
		else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(&c, 1, 1);
		else { print("*:"); number((nat)c); print("|"); }
		memmove(history + 1, history, sizeof history - 1);
		history[0] = c;

	} else if (mode == search_mode) {


		/*   problem / feature / bug:


			we need to assign  cursor   to be start    where start is where we were at the point of doing d   (or ie triggering the search)    we always start looking at start,   EXCEPT when we hit the end, in which we wrap around.    we need to make search_forwards return whether we found it, and then if we didnt find it using our { cursor = start; search_forwards()-->false }  then we must try using 

				{ cursor = start; search_backwards() --> ?? }      and if that failsssss then we must put ourselves back where we started.   at start.

							thats the correct semantics.   oh, and pressing enter should not move us, then. so yeah, tab and enter just do an additional searchf or searchb      without setting cursor to start. they max out at the rails,   0 and textlength. so yeah. 


				drtpun must then   do      this whole sequence of logic as well,      to restore position, after saying drtpun. so yeah. 



			that should make search perfect then. 

		*/

		if (
			c == 'n' and 
			history[0] == 'u' and 
			history[1] == 'p' and
			history[2] == 't' and 
			history[3] == 'r' and
			history[4] == 'd'
		) {
			if (ss_len >= 5) ss_len -= 5;
			cursor = 0; search_forwards(S, ss_len); center();
			memset(history, 0, 5);
			memset(S, 0, sizeof S); 
			ss_len = 0;
			mode = edit_mode;
			goto loop; 
		}
		else if (c == 27) { memset(S, 0, sizeof S); ss_len = 0; mode = edit_mode; goto loop; }
		else if (c == 127) { if (ss_len) ss_len--; }
		else if (c == 9) { search_backwards(S, ss_len); center(); } 
		else if (c == 10) { search_forwards(S, ss_len); center(); } 
		else if ((unsigned char) c >= 32) { if (ss_len < sizeof S) S[ss_len++] = c; }
		else goto loop;
		memset(status, 0, sizeof status);
		status[0] = '.';
		memcpy(status + 1, S, ss_len);
		memmove(history + 1, history, sizeof history - 1);
		history[0] = c;
		if (c != 9 and c != 10) { cursor = 0; search_forwards(S, ss_len); center(); }
	} else {
		if (c == 27 or c == ' ') {}
		else if (c == 127) { if (anchor == disabled) delete(1,1); else delete_selection(); }
		else if (c == 'a') anchor = anchor == disabled ? cursor : disabled;
		else if (c == 'b') { task_to_clip(); goto do_c; }
		else if (c == 'c') copy_local();
		else if (c == 'd') mode = search_mode;
		else if (c == 'e') word_left();
		else if (c == 'f') copy_global();
		else if (c == 'g') paste_global(); 
		else if (c == 'h') half_page_up();
		else if (c == 'i') right();
		else if (c == 'j') { snprintf(S, sizeof S - 1, "%s", clipboard); ss_len = cliplength; mode = search_mode; }
		else if (c == 'k') up_begin();
		else if (c == 'l') down_end();
		else if (c == 'm') half_page_down();
		else if (c == 'n') left();
		else if (c == 'o') word_right();
		else if (c == 'p') up();
		else if (c == 'q') goto do_c;
		else if (c == 'r') delete(1, 1);
		else if (c == 's') insert_char();
		else if (c == 't') mode = insert_mode;
		else if (c == 'u') down();
		else if (c == 'v') { task2_to_clip(); goto do_c; }
		else if (c == 'w') paste();
		else if (c == 'x') redo();
		else if (c == 'y') save();
		else if (c == 'z') undo();
		else { print("*:"); number((nat)c); print("|"); }
	}
	goto loop;
do_c:;	char* s = clipboard;
	if (not s) {}
	else if (not strcmp(s, "nop")) {}
	else if (not strcmp(s, "exit")) goto done;
	else if (not strcmp(s, "quit")) goto done;
	else if (not strcmp(s, "dt")) insert_dt();
	else if (not strcmp(s, "delete")) { if (anchor == disabled) delete(1,1); else delete_selection(); }
	else if (not strcmp(s, "tab")) { c = 9; insert(&c, 1, 1); }
	else if (not strcmp(s, "newline")) { c = 10; insert(&c, 1, 1); }
	else if (not strcmp(s, "search")) { snprintf(S, sizeof S - 1, "%s", taskboard); ss_len = tasklength; mode = search_mode; }
	else if (not strcmp(s, "forwards")) search_forwards(taskboard, tasklength);
	else if (not strcmp(s, "backwards")) search_backwards(taskboard, tasklength);
	else if (not strncmp(s, "write ", 6)) write_string("./", clipboard + 6, cliplength - 6);
	else if (not strncmp(s, "insert ", 7)) insert_output(s + 7);
	else if (not strncmp(s, "copyb ", 6)) { string_to_task(s + 6); }
	else if (not strncmp(s, "copya ", 6)) { string_to_task2(s + 6); }
	else if (not strncmp(s, "change ", 7)) change_directory(s + 7);
	else if (not strncmp(s, "index ", 6)) jump_index(s + 6);
	else if (not strncmp(s, "line ", 5)) jump_line(s + 5);
	else if (not strncmp(s, "do ", 3)) execute(s + 3);
	else { print("*:cmd\""); print(clipboard); print("\"|"); }
	goto loop;
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	save(); puts(""); exit(0);
}












































//free(clipboard);
//cliplength = cursor - anchor;
//clipboard = strndup(text + anchor, cliplength);


// static void paste_ecb(void) { for (nat i = 0; i < tasklength; i++) insert(taskboard[i], 1); }


// paste_global(); paste();
// copy_global(); copy();
// clip_to_ecb();

// searchb();
// searchf();






// else if (not strncmp(clipboard, "copy ", 5)) set_ecb(clipboard + 5);
// else if (not strcmp(clipboard, "paste")) paste_ecb();

























// if (selecting) cut(); else if (cursor) delete(1);     <--- these two lines are not usable now. 

// if (selecting and cursor != anchor) cut(); 





	// make this instead be given a string, and length.    as opposed to using the clipboard always, 
	// we will instead use the current anchor-cursor selection, with the assumption that the user will:
	
	//       <anchor> <enter-insert-mode> type some text <ESC> <find-forwards-command>

	// using that sequence, which in reality will look like        a t some text ESC f           or alternatively:   atsome textdrtpunf
	// and what this function will do, is actually perform a vanilla cut,   using cut(),     to get rid of the text that was typed, 
	// and thennn it will use the clipboard contents to do this.  
	// however, if the user is not even in anchoring mode,    
	// then we will just use the clipboard with no cut performed on the document. yay! 



//       alias pbcopy="xclip -selection c"
//       alias pbpaste="xclip -selection clipboard -o" 









// 
// static void page_up(void)   { for (int i = 0; i < window.ws_row - 3; i++) up(); } 
// static void page_down(void) { for (int i = 0; i < window.ws_row - 3; i++) down(); } 













/*
static void interpret_arrow_key(void) {
	char c = 0; 
	read(0, &c, 1);
	     if (c == 'u') { clear_anchor(); up_begin(); }
	else if (c == 'd') { clear_anchor(); down_end(); }
	else if (c == 'l') { clear_anchor(); word_left(); }
	else if (c == 'r') { clear_anchor(); word_right(); }
	else if (c == 'f') { clear_anchor(); searchf(); }
	else if (c == 'b') { clear_anchor(); searchb(); }
	else if (c == 't') { clear_anchor(); page_up(); }
	else if (c == 'e') { clear_anchor(); page_down(); }
	else if (c == 's') {
		read(0, &c, 1); 
		     if (c == 'u') { set_anchor(); up(); }
		else if (c == 'd') { set_anchor(); down(); }
		else if (c == 'r') { set_anchor(); right(); }
		else if (c == 'l') { set_anchor(); left(); }
		else if (c == 'b') { set_anchor(); up_begin(); }
		else if (c == 'e') { set_anchor(); down_end(); }
		else if (c == 'w') { set_anchor(); word_right(); }
		else if (c == 'm') { set_anchor(); word_left(); }
	} else if (c == '[') {
		read(0, &c, 1); 
		if (c == 'A') { clear_anchor(); up(); }
		else if (c == 'B') { clear_anchor(); down(); }
		else if (c == 'C') { clear_anchor(); right(); }
		else if (c == 'D') { clear_anchor(); left(); }
		else { printf("error: found escape seq: ESC [ #%d\n", c); getchar(); }
	} else { printf("error found escape seq: ESC #%d\n", c); getchar(); }
}


	} else if (c == '[') {
		read(0, &c, 1); 
		if (c == 'A') { clear_anchor(); up(); }
		else if (c == 'B') { clear_anchor(); down(); }
		else if (c == 'C') { clear_anchor(); right(); }
		else if (c == 'y') { clear_anchor(); left(); }
		else { printf("error: found escape seq: ESC [ #%d\n", c); getchar(); }
	} else { printf("error found escape seq: ESC #%d\n", c); getchar(); }
}







//static nat recompute_origin(void) { // set origin to the value such that the cursor is in the center of the screen....
//	return 0; // ..... uhhh....
//}




// static void clear_anchor(void) { if (not selecting) return; anchor = (nat) ~0; selecting = 0; }







static void paste_ecb(void) { for (nat i = 0; i < cliplength1; i++) insert(clipboard1[i], 1); }
static void set_ecb(char* string) {
	free(clipboard1); 
	cliplength1 = strlen(string);
	clipboard1 = strdup(string);
}
static void ecb_to_clip(void) {
	free(clipboard); cliplength = cliplength1;
	clipboard = strndup(clipboard1, cliplength1);
}
static void insert_dt(void) {
	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
	insert_string(datetime);
}



{ ecb_to_clip(); goto do_c; }




// static void insert_string(const char* string) { for (nat i = 0; i < strlen(string); i++) insert(string[i], 1); }






static void set_anchor(void) { anchor = cursor; }



*/





