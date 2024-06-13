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
	autosave_counter = 0;

static char* text = NULL, * clipboard = NULL, * taskboard = NULL;
static struct action* actions = NULL;

static char message[4096] = {0};
static char filename[4096] = {0};
static char autosavename[4096] = {0};

static struct winsize window = {0};
static struct termios terminal = {0};

extern char** environ;

#define max_screen_size  1 << 20

static nat append(const char* string, nat n, char* screen, nat length) {
	if (length >= max_screen_size) return 0;
	memcpy(screen + length, string, n);
	return n;
}

static void print(const char* string) {
	nat l = strlen(message);
	nat n = strlen(string);
	if (n >= sizeof message - l) n = sizeof message - l - 1;
	memcpy(message + l, string, n);
}

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

static void display(void) {
	ioctl(0, TIOCGWINSZ, &window);
	char screen[max_screen_size];
	nat length = append("\033[H", 3, screen, 0);

	static nat origin = 0;
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

	if (*message) {
		length += append("\033[6;6H", 6, screen, length);
		length += append(message, strlen(message), screen, length);
		memset(message, 0, sizeof message);
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

static void write_file(const char* directory, char* name, size_t maxsize) {

	print("write_file: saving file...\n");

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
		print("write_file: creating file with generated name...\n");
	}
	int file = open(name, flags, permission);
	if (file < 0) { perror("save: open file"); puts(name); getchar(); }
	write(file, text, count);

	char string[4096] = {0};
	snprintf(string, sizeof string, "write_file: successfully wrote %llub to file \"%s\"...\n", count, name);
	print(string);
	fsync(file);
	close(file);
}

static void autosave(void) {
	autosave_counter = 0;

	print("autosaving file...\n");
	write_file(autosave_directory, autosavename, sizeof autosavename);
}

static void save(void) {
	print("saving file...");
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

		char string[4096] = {0};
		snprintf(string, sizeof string, 
			"redo: note: choice = %llu, however there are "
			"%llu possible histories to choose.\n", 
			actions[head].choice, child_count
		);
		print(string);

		actions[head].choice = (actions[head].choice + 1) % child_count;
	}
	head = chosen_child;
	const struct action node = actions[head];
	cursor = node.pre; 
	
	if (node.length > 0) insert(node.string, (nat) node.length, 0); else delete((nat) -node.length, 0);
	cursor = node.post;


	char string[4096] = {0};
	snprintf(string, sizeof string, "redo: redid %lldb, head now %llu...\n", node.length, head);
	print(string);

}

static void undo(void) {
	if (not head) return;
	struct action node = actions[head];
	cursor = node.post;
	if (node.length > 0) delete((nat) node.length, 0); else insert(node.string, (nat) -node.length, 0); 
	cursor = node.pre;
	head = node.parent;

	char string[4096] = {0};
	snprintf(string, sizeof string, "undo: undid %lldb, head now %llu...\n", node.length, head);
	print(string);
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

static void cut(void) {
	if (anchor > count or anchor == cursor) return;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	free(clipboard);
	cliplength = cursor - anchor;
	clipboard = strndup(text + anchor, cliplength);
	delete(cliplength, 1);
	anchor = disabled;

	char string[4096] = {0};
	snprintf(string, sizeof string, "cut: removed %llub from file at %llu...\n", cliplength, cursor);
	print(string);
}

static void searchf(void) {
	cut();
	const char* string = clipboard;
	nat length = cliplength;
	nat t = 0;
	loop: if (t == length or cursor >= count) return;
	if (text[cursor] != string[t]) t = 0; else t++; 
	right(); goto loop;
}

static void searchb(void) {
	cut();
	const char* string = clipboard;
	nat length = cliplength;
	nat t = length;
	loop: if (not t or not cursor) return;
	left(); t--; 
	if (text[cursor] != string[t]) t = length;
	goto loop;
}


static inline void copy(void) {
	if (anchor > count or anchor == cursor) return;
	cliplength = anchor < cursor ? cursor - anchor : anchor - cursor;
	free(clipboard);
	clipboard = strndup(text + (anchor < cursor ? anchor : cursor), cliplength);

	FILE* globalclip = popen("pbcopy", "w");
	if (not globalclip) {
		char string[4096] = {0};
		snprintf(string, sizeof string, "copy: error: pbcopy: %s...\n", strerror(errno));
		print(string);
		return;
	}	
	fwrite(clipboard, 1, cliplength, globalclip);
	pclose(globalclip);
	anchor = disabled;

	char string[4096] = {0};
	snprintf(string, sizeof string, "copy: copied %llub at %llu...\n", cliplength, cursor);
	print(string);

}

static void insert_output(const char* input_command) {
	save();
	char command[4096] = {0};
	strlcpy(command, input_command, sizeof command);
	strlcat(command, " 2>&1", sizeof command);

	FILE* f = popen(command, "r");
	if (not f) {
		char string[4096] = {0};
		snprintf(string, sizeof string, "insert output: error: could not execute %s, popen: %s...\n", 
			command, strerror(errno));
		print(string);
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

static void window_resized(int _) {if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	save(); puts(""); exit(0); 
}

static void change_directory(const char* d) {
	if (chdir(d) < 0) {
		char string[4096] = {0};
		snprintf(string, sizeof string, "change directory: error: could not ct into %s: chdir: %s...\n", 
			d, strerror(errno));
		print(string);
		return;
	}
	print("changed directories\n");
}

static void create_process(char** args) {
	pid_t pid = fork();
	if (pid < 0) { 
		char string[4096] = {0};
		snprintf(string, sizeof string, "create procress: error: fork: %s...\n", strerror(errno));
		print(string);
		return;
	}
	if (not pid) {
		if (execve(args[0], args, environ) < 0) { perror("execve"); exit(1); }
	} 
	int status = 0;
	if ((pid = wait(&status)) == -1) { 
		char string[4096] = {0};
		snprintf(string, sizeof string, "create procress: error: wait: %s...\n", strerror(errno));
		print(string);
		return;
	}
	char dt[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
	if (WIFEXITED(status)) 		printf("[%s:(%d) exited with code %d]\n", dt, pid, WEXITSTATUS(status));
	else if (WIFSIGNALED(status)) 	printf("[%s:(%d) was terminated by signal %s]\n", dt, pid, strsignal(WTERMSIG(status)));
	else if (WIFSTOPPED(status)) 	printf("[%s:(%d) was stopped by signal %s]\n", 	dt, pid, strsignal(WSTOPSIG(status)));
	else 				printf("[%s:(%d) terminated for an unknown reason]\n", dt, pid);
	fflush(stdout);
	getchar();
}

static void execute(char* command) {
	save();
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
	for (size_t i = 0; i < n; i++) right();
}

static void jump_line(char* string) {
	const size_t n = (size_t) atoi(string);
	cursor = 0;
	for (size_t i = 0; i < n; i++) down_end();
	up_begin(); 
}

static void paste(void) { insert_output("pbpaste"); }
static void local_paste(void) { insert(clipboard, cliplength, 1); }

// static void paste_ecb(void) { for (nat i = 0; i < tasklength; i++) insert(taskboard[i], 1); }

static void clip_to_ecb(void) {
	char string[4096] = {0};
	snprintf(string, sizeof string, "clip to ecb: copied %llub to task clipboard...\n", cliplength);
	print(string);
	free(taskboard);
	tasklength = cliplength;
	taskboard = strndup(clipboard, cliplength);
}

static void ecb_to_clip(void) {
	char string[4096] = {0};
	snprintf(string, sizeof string, "ecb to clip: copied %llub to main clipboard...\n", tasklength);
	print(string);
	free(clipboard);
	cliplength = tasklength;
	clipboard = strndup(taskboard, tasklength);
}

static void half_page_up(void)   { for (int i = 0; i < (window.ws_row) / 2; i++) up(); } 
static void half_page_down(void) { for (int i = 0; i < (window.ws_row) / 2; i++) down(); }

static char remap(const char c, const nat is_inserting) {

	const char upper_remap_alpha[26] = "AVMHRTGYUNEOLKP:QWSBFCDXJZ"
	const char lower_remap_alpha[26] = "avmhrtgyuneolkp;qwsbfcdxjz"

	if (c == 13 and is_inserting) return 10;

	/*  to use qwerty, uncomment these lines:  */

	//if (c >= 'A' and c <= 'Z') upper_remap_alpha[c - 'A'];
	//if (c >= 'a' and c <= 'z') upper_remap_alpha[c - 'z'];



	return c;
}

int main(int argc, const char** argv) {
	struct sigaction action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);

	if (argc < 2) goto new;
	strlcpy(filename, argv[1], sizeof filename);
	int df = open(filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = open(filename, O_RDONLY);
	if (file < 0) { read_error: perror("load: read open file"); exit(1); }
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
        bool is_inserting = 0;
	char history[5] = {0};
loop:
	display();
	char c = 0;
	read(0, &c, 1);
	c = remap(c, is_inserting);
	if (is_inserting) {
		if ((false)) {}
		else if (
			c == 'n' and 
			history[0] == 'u' and 
			history[1] == 'p' and
			history[2] == 't' and 
			history[3] == 'r' and
			history[4] == 'd'
		) {
			history[0] = 0;
			history[1] = 0;
			history[2] = 0;
			history[3] = 0;
			history[4] = 0;
			delete(5, 1);
			is_inserting = false; 
		}
		else if (c == 27) is_inserting = false;
		else if (c == 127) delete(1,1);
		else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(&c, 1, 1);
		else { 
			char string[4096] = {0};
			snprintf(string, sizeof string, "error: ignoring input byte '%d'...\n", c);
			print(string);
		}
		memmove(history + 1, history, 4);
		history[0] = c;
	} else {
		if (c == 27) {}
		else if (c == ' ') {}
		else if (c == 'a') anchor = anchor == disabled ? cursor : disabled;
		else if (c == 'b') { ecb_to_clip(); goto do_c; }
		else if (c == 'c') copy();
		else if (c == 'd') searchf();
		else if (c == 'e') word_left();
		else if (c == 'f') goto do_c;
		else if (c == 'g') paste();
		else if (c == 'h') half_page_up();
		else if (c == 'i') right();
		else if (c == 'j') clip_to_ecb();
		else if (c == 'k') up_begin();
		else if (c == 'l') down_end();
		else if (c == 'm') half_page_down();
		else if (c == 'n') left();
		else if (c == 'o') word_right();
		else if (c == 'p') up();
		else if (c == 'q') goto done;
		else if (c == 'r') { if (anchor == disabled) delete(1,1); else cut(); }
		else if (c == 's') searchb();
		else if (c == 't') is_inserting = 1;
		else if (c == 'u') down();
		else if (c == 'v') insert_dt();
		else if (c == 'w') local_paste();
		else if (c == 'x') redo();
		else if (c == 'y') save();
		else if (c == 'z') undo();
		else { 
			char string[4096] = {0};
			snprintf(string, sizeof string, "error: unknown command '%d'...\n", c);
			print(string);
		}
	}
	goto loop;
do_c:
	cut();
	char* s = clipboard;
	if (not strcmp(s, "exit")) goto done;
	else if (not strcmp(s, "dt")) 
		insert_dt();
	else if (not strncmp(s, "nop ", 4)) {}
	else if (not strncmp(s, "insert ", 7))
		insert_output(s + 7);
	else if (not strncmp(s, "change ", 7))
		change_directory(s + 7);
	else if (not strncmp(s, "do ", 3)) 
		execute(s + 3);
	else if (not strncmp(s, "index ", 6)) 
		jump_index(s + 6);
	else if (not strncmp(s, "line ", 5))
		jump_line(s + 5);
	else { 
		char string[4096] = {0};
		snprintf(string, sizeof string, "error: unknown command \"%s\"...\n", clipboard);
		print(string);
	}
	goto loop;
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	save(); puts(""); exit(0);
}


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





