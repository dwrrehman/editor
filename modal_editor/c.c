#include <stdio.h>   // started 1202410266.140132 by dwrr
#include <stdlib.h>  // 1202411041.210238 an editor that
#include <string.h>  // supposed to be a simpler version of
#include <iso646.h>  // the modal editor that we wrote.
#include <unistd.h>  // written more 1202411052.131234
#include <fcntl.h>   // finished 1202411111.220521
#include <stdnoreturn.h>// updated 1202412301.150909
#include <errno.h>
#include <ctype.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <sys/wait.h> 
typedef uint64_t nat;
struct action {
	nat parent;
	nat pre;
	nat post;
	nat choice;
	int64_t length;
	char* string;
};
static const char use_qwerty_layout = 0;
static nat cursor = 0,  count = 0, anchor = 0, origin = 0, mode = 2,
	cliplength = 0, head = 0, action_count = 0, writable = 0;
static char* text = NULL, * clipboard = NULL;
static struct action* actions = NULL;
static char filename[4096] = {0}, last_modified[32] = {0}, status[4096] = {0};
static volatile struct winsize window = {0};
static struct termios terminal = {0};
extern char** environ;

static char remap(const char c) {
	if (c == 13) return 10;	else if (c == 8) return 127;
	if (use_qwerty_layout and mode == 2) {
		const char upper_remap_alpha[26] = "AVMHRTGYUNEOLKP:QWSBFCDXJZ";
		const char lower_remap_alpha[26] = "avmhrtgyuneolkp;qwsbfcdxjz";
		if (c >= 'A' and c <= 'Z') return upper_remap_alpha[c - 'A'];
		if (c >= 'a' and c <= 'z') return lower_remap_alpha[c - 'a'];
		if (c == ';') return 'i';
		if (c == ':') return 'I';
	} 
	return c;
}

static void append(const char* string, nat length, char* screen, nat* screen_length) {
	if (*screen_length + length >= 1 << 20) return;
	memcpy(screen + *screen_length, string, length);
	*screen_length += length;
}

static nat cursor_in_view(nat given_origin) {
	nat i = given_origin, column = 0, row = 0;
	for (; i < count; i++) {
		const char c = text[i];
		if (i == cursor) return 1;
		if (c == 10) {
			print_newline: row++; column = 0;
			if (row >= window.ws_row) break;
		} else if (c == 9) {
			nat amount = 8 - column % 8;
			column += amount;
		} else if ((unsigned char) c >= 32) { column++; } else { column += 4; }
		if (column >= window.ws_col - 2) goto print_newline;
	} 
	if (i == cursor and i == count) return 1; return 0;
}

static void update_origin(void) {
	if (origin > count) origin = count;
	if (not cursor_in_view(origin)) {
		if (cursor < origin) { 
			while (origin and not cursor_in_view(origin)) {
				do origin--; 
				while (origin and text[origin - 1] != 10);
			} 
		} else if (cursor > origin) { 
			while (origin < count and not cursor_in_view(origin)) {
				do origin++; 
				while (origin < count and text[origin - 1] != 10);
			} 
		}
	}
}

static void display(nat should_write) {
	char screen[1 << 20];
	nat length = 0, column = 0, row = 0;
	update_origin();
	append("\033[H", 3, screen, &length);
	nat i = origin;
	for (; i < count; i++) {
		const char c = text[i];
		if (i == cursor or i == anchor) append("\033[7m", 4, screen, &length);
		if (c == 10) {
			if (i == cursor or i == anchor) append(" \033[0m", 5, screen, &length);
		print_newline:
			append("\033[K", 3, screen, &length);
			if (row < window.ws_row - 1) append("\n", 1, screen, &length);
			row++; column = 0;
			if (row >= window.ws_row) break;
		} else if (c == 9) {
			nat amount = 8 - column % 8;
			column += amount;
			if (i == cursor or i == anchor) { 
				append(" \033[0m", 5, screen, &length); 
				amount--; 
			}
			append("        ", amount, screen, &length);
		} else if ((unsigned char) c >= 32) {
			append(text + i, 1, screen, &length); 
			column++;
		} else {
			const char control_code = c + 'A';
			append("\033[7m^", 5, screen, &length); 
			append(&control_code, 1, screen, &length);
			append("\033[0m", 4, screen, &length); 
			column += 4;
		}
		if (i == cursor or i == anchor) append("\033[0m", 4, screen, &length);
		if (column >= window.ws_col - 2) goto print_newline;
	}
	if (i == count and (i == cursor or i == anchor)) append("\033[7m \033[0m", 9, screen, &length);
	while (row < window.ws_row) {
		append("\033[K", 3, screen, &length);
		if (row < window.ws_row - 1) append("\n", 1, screen, &length);
		row++;
	}
	if (*status) {
		append("\033[H", 3, screen, &length);
		append(status, strlen(status), screen, &length);
		append("\033[7m \033[0m", 9, screen, &length);
		if (should_write) memset(status, 0, sizeof status);
	}
	if (should_write) write(1, screen, length);
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

static void insert(char* string, nat length, char should_record) {
	struct action node = { .pre = cursor };
	text = realloc(text, count + length);
	memmove(text + cursor + length, text + cursor, count - cursor);
	memcpy(text + cursor, string, length);
	count += length; cursor += length;
	if (should_record) finish_action(node, string, (int64_t) length);
}

static void delete(nat length, char should_record) {
	if (cursor < length) return;
	struct action node = { .pre = cursor };
	cursor -= length; count -= length; 
	char* string = strndup(text + cursor, length);
	memmove(text + cursor, text + cursor + length, count - cursor);
	text = realloc(text, count);
	if (should_record) finish_action(node, string, (int64_t) -length);
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

static void redo(void) {
	nat chosen_child = 0, child_count = 0; 
	for (nat i = 0; i < action_count; i++) {
		if (actions[i].parent != head) continue;
		if (child_count == actions[head].choice) chosen_child = i;
		child_count++;
	}
	if (not child_count) return;
	if (child_count >= 2) 
		actions[head].choice = (actions[head].choice + 1) % child_count;
	head = chosen_child;
	const struct action node = actions[head];
	cursor = node.pre; 
	if (node.length > 0) insert(node.string, (nat) node.length, 0); 
	else delete((nat) -node.length, 0);
	cursor = node.post;
}

static void undo(void) {
	if (not head) return;
	struct action node = actions[head];
	cursor = node.post;
	if (node.length > 0) delete((nat) node.length, 0); 
	else insert(node.string, (nat) -node.length, 0); 
	cursor = node.pre;
	head = node.parent;
}

static void emergency_save(const char* call) {
	printf("save error: %s: %s\n", call, strerror(errno)); fflush(stdout); sleep(2);
	puts("printing stored document: "); fflush(stdout); sleep(1);
	fwrite(text, 1, count, stdout); fflush(stdout); sleep(1);
	fwrite(text, 1, count, stdout); fflush(stdout); sleep(1);
	puts("emergency save: printed out all contents"); fflush(stdout); sleep(1);
	printf("save error: %s: %s\n", call, strerror(errno)); fflush(stdout); sleep(2);
	return; 
}

static void save(void) {
	const mode_t permissions = S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;
	int flags = 0;
	char matches = 0;
	if (not *filename) {
		char name[4096] = {0}, datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(name, sizeof name, "%s_%08x%08x.txt", datetime, rand(), rand());
		strlcpy(filename, name, sizeof filename);
		flags = O_WRONLY | O_CREAT | O_EXCL;
		matches = 1; 
	} else {
		char empirically_last_modified[32] = {0};
		struct stat attr;
		stat(filename, &attr);
		strftime(empirically_last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attr.st_mtime));
		if (not strcmp(empirically_last_modified, last_modified)) {
			flags = O_WRONLY | O_TRUNC;
			matches = 1; 
		}
	}
	if (matches) {
		int output_file = open(filename, flags, permissions);
		if (output_file < 0) { emergency_save("open"); goto recover; }
		if (write(output_file, text, count) < 0){ emergency_save("write"); goto recover; }
		if (close(output_file) < 0) { emergency_save("close"); goto recover; }
		struct stat attrn;
		stat(filename, &attrn);
		strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attrn.st_mtime));
		return;
	}
	recover: write(1, "\033[?25h", 6); tcsetattr(0, TCSANOW, &terminal);
	printf("--- emergency save ---\nplease give a valid filename.\n(enter: <path>\\n)\n");
	loop: fflush(stdout); char input[4096] = {0};
	printf(":: "); fflush(stdout);
	const ssize_t n = read(0, input, sizeof input);
	if (n <= 0) {
		emergency_save("read");
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u_%H%M%S", tm);
		snprintf(input, sizeof input, "/Users/dwrr/%s_%08x%08x_emergency.txt", datetime, rand(), rand());
	} else input[n - 1] = 0;
	int output_file = open(input, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, permissions);
	if (output_file < 0) { emergency_save("open"); goto loop; }
	if (write(output_file, text, count) < 0) { emergency_save("write"); goto loop; }
	if (close(output_file) < 0) { emergency_save("close"); goto loop; }
	printf("successfully emergency saved, exiting editor..."); fflush(stdout); 
	exit(0);
}

static void delete_selection(void) {
	if (anchor > count or anchor == cursor) return;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	const nat len = cursor - anchor;
	delete(len, 1);
	anchor = (nat) -1;
}

static void local_copy(void) {
	if (anchor > count or anchor == cursor) return;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	cliplength = cursor - anchor;
	free(clipboard);
	clipboard = strndup(text + anchor, cliplength);
}

static void insert_char(void) {
	char c = 0; read(0, &c, 1); c = remap(c);
	if (c == 'a') insert_dt();
	else if (c == 'g') insert("0123456789", 10, 1);
	else if (c == 'b') insert("`~!@#$%^&*(){}[]<>|\\+=_-:;?/.,'\"", 32, 1);
	else if (c == 'd') { read(0, &c, 1); c = remap(c);
		if (c == 'e') { read(0, &c, 1); c = remap(c);
			if (c == 'l') delete_selection();
		}
	} else if (c == 'r') { c = 10; insert(&c, 1, 1); }
	else if (c == 'h') { c = 32; insert(&c, 1, 1); }
	else if (c == 'm') { c = 9;  insert(&c, 1, 1); }
	else if (c == 't') { read(0, &c, 1); c = remap(c); insert(&c, 1, 1); }
	else if (c == 'u' and cursor < count) {
		c = (char) toupper(text[cursor]);
		cursor++; delete(1, 1); insert(&c, 1, 1); cursor--;
	} else if (c == 'l' and cursor < count) {
		c = (char) tolower(text[cursor]);
		cursor++; delete(1, 1); insert(&c, 1, 1); cursor--;
	} else if (c == 'e' and cursor < count and text[cursor] < 126) { 
		c = text[cursor] + 1; 
		cursor++; delete(1, 1); insert(&c, 1, 1); cursor--;
	} else if (c == 'o' and cursor < count and text[cursor] > 32) { 
		c = text[cursor] - 1;
		cursor++; delete(1, 1); insert(&c, 1, 1); cursor--;
	}
}

static void insert_error(const char* call) {
	char message[4096] = {0};
	snprintf(message, sizeof message, "error: %s: %s\n", call, strerror(errno));
	if (text) insert(message, strlen(message), 1); else printf("%s", message);
}

static inline void copy_global(void) {
	if (anchor > count or anchor == cursor) return;
	const nat length = anchor < cursor ? cursor - anchor : anchor - cursor;
	FILE* globalclip = popen("pbcopy", "w");
	if (not globalclip) { insert_error("popen:pbcopy"); return; }
	fwrite(text + (anchor < cursor ? anchor : cursor), 1, length, globalclip);
	pclose(globalclip);
}

static void insert_output(const char* input_command) {
	if (writable) save();
	char command[4096] = {0};
	strlcpy(command, input_command, sizeof command);
	strlcat(command, " 2>&1", sizeof command);
	FILE* f = popen(command, "r");
	if (not f) { insert_error("popen"); return; }
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

static char open_file(const char* argument) {
	if (writable) save();
	if (not argument or not strlen(argument)) { 
		if (text) free(text); 
		text = NULL; count = 0; 
		goto new; 
	} 
	int df = open(argument, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = open(argument, O_RDONLY);
	if (file < 0) { read_error: insert_error("open"); return 1; }
	struct stat ss; fstat(file, &ss);
	count = (nat) ss.st_size;
	if (text) free(text); 
	text = malloc(count);
	read(file, text, count); 
	close(file);
new: 	cursor = 0; origin = 0;
	anchor = (nat) -1; writable = 1;
	if (argument) strlcpy(filename, argument, sizeof filename); 
	else memset(filename, 0, sizeof filename);
	for (nat i = 0; i < action_count; i++) free(actions[i].string);
	if (actions) free(actions); actions = NULL;
	head = 0; action_count = 0;
	finish_action((struct action) {0}, NULL, 0LL);
	struct stat attr_;
	stat(filename, &attr_);
	strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attr_.st_mtime));
	return 0;
}

static void window_resized(int _) { if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	if (writable) save(); exit(0); 
}

static void jump_numeric(char* s) {
	if (not strlen(s)) return;
	const nat n = (nat) atoi(s), b = s[strlen(s) - 1] == 'l';
	cursor = 0;
	for (nat i = 0; i < n; i++) {
		if (not b) { if (cursor < count) cursor++; continue; }
		while (cursor < count) {
			cursor++;
			if (cursor < count and text[cursor - 1] == 10) break;
		}
	}
}

static void center_cursor(void) {
	const nat save = cursor;
	for (nat i = 0; i < window.ws_row >> 1; i++) 
		while (cursor) { 
			cursor--; 
			if (not cursor or text[cursor - 1] == 10) break;
		}
	display(0);
	cursor = save;	
	for (nat i = 0; i < window.ws_row >> 1; i++) 
		while (cursor < count) { 
			cursor++; 
			if (cursor < count and text[cursor - 1] == 10) break;
		}
	display(0);
	cursor = save;
}

int main(int argc, const char** argv) {
	if (argc > 3) exit(puts("usage: ./editor [file]")); else if (open_file(argv[1])) exit(1);
	srand((unsigned) time(0));
	struct sigaction action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 1; 
	terminal_copy.c_cc[VTIME] = 0;
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);
	writable = argc < 3;
 	nat target_length = 0, home = 0, should_center = 0, search_index = 0;
	char history[6] = {0}, target[4096] = {0};
loop:	ioctl(0, TIOCGWINSZ, &window); 
	if (should_center) center_cursor(); display(1);
	char c = 0; ssize_t n = read(0, &c, 1); c = remap(c);
	if (n <= 0) { perror("read"); fflush(stderr); usleep(10000); }
	if (mode == 0) {
		if (c == 27 or (c == 'n' and not memcmp(history, "uptrd", 5))) {
			memset(history, 0, sizeof history);
			if (c == 'n') delete(5, 1); 
			mode = 2; goto loop;
		} else if (c == 127) delete(1, 1);
		else if (c == 9 or c == 10 or (uint8_t) c >= 32) insert(&c, 1, 1);
		memmove(history + 1, history, sizeof history - 1);
		*history = c;
	} else if (mode == 1) {
		if (c == 27 or (c == 'n' and not memcmp(history, "uptrd", 5))) {
			mode = 2;  
			memset(history, 0, sizeof history); 
			if (c == 'n') { if (target_length >= 5) target_length -= 5; } 
			else goto loop;
		} else if (c == 127) { if (target_length) target_length--; }
		else if (c == 9 and search_index) search_index--;
		else if (c == 10) search_index++;
		else if ((uint8_t) c >= 32) { 
			if (target_length < sizeof target - 1) target[target_length++] = c; 
		} 
		memmove(history + 1, history, sizeof history - 1);
		*history = c;
		cursor = home;
		nat found_at = 0;
		for (nat t = 0, search_count = 0; cursor < count; cursor++) {
			if (text[cursor] != target[t]) { t = 0; continue; } t++; 
			if (t == target_length) {
				if (not search_count) found_at = cursor + 1;
				if (search_count == search_index) { cursor++; goto found; } 
				else { search_count++; t = 0; } 
			}
		}
		cursor = not found_at ? home : found_at; 
		if (found_at) search_index = 0; 
		found: center_cursor();
		memset(status, 0, sizeof status);
		memcpy(status, target, target_length);
	} else {
		if (c == 'Q') goto done;
		else if (c == 'q') goto do_command;
		else if (c == 'z' and writable) undo();
		else if (c == 'x' and writable) redo();
		else if (c == 'y' and writable) save();
		else if (c == 't' and writable) mode = 0;
		else if (c == 's' and writable) insert_char();
		else if (c == 'r' and writable) delete(1, 1);
		else if (c == 'w' and writable) insert(clipboard, cliplength, 1);
		else if (c == 'g' and writable) insert_output("pbpaste");
		else if (c == 'c') local_copy();
		else if (c == 'f') copy_global();
		else if (c == 'v') should_center = not should_center;
		else if (c == 'a') anchor = anchor == (nat)-1 ? cursor : (nat) -1;
		else if (c == 'i') { if (cursor < count) cursor++; }
		else if (c == 'n') { if (cursor) cursor--; }
		else if (c == 'd' or c == 'l' or c == 'k') {
			mode = 1; target_length = 0; search_index = 0;
			home = c == 'd' ? 0 : cursor;
		}
		else if (c == 'p' or c == 'h') {
			nat times = 1;
			if (c == 'h') times = window.ws_row >> 1;
			for (nat i = 0; i < times; i++) 
				while (cursor) { 
					cursor--; 
					if (not cursor or text[cursor - 1] == 10) break;
				}
		} else if (c == 'u' or c == 'm') {
			nat times = 1;
			if (c == 'm') times = window.ws_row >> 1;
			for (nat i = 0; i < times; i++) 
				while (cursor < count) { 
					cursor++; 
					if (cursor < count and text[cursor - 1] == 10) break;
				}
		} else if (c == 'e') {
			while (cursor) { 
				cursor--; 
				if (not cursor or text[cursor - 1] == 10) break;
				if (isalnum(text[cursor]) and 
				not isalnum(text[cursor - 1])) break;
			}
		} else if (c == 'o') {
			while (cursor < count) { 
				cursor++; 
				if (cursor >= count or text[cursor] == 10) break;
				if (not isalnum(text[cursor]) and 
				isalnum(text[cursor - 1])) break;
			}
		}
	}
	goto loop; 
do_command: if (not writable) goto done;
	if (not clipboard) goto loop;
	else if (not strcmp(clipboard, "exit")) goto done;
	else if (not strncmp(clipboard, "edit ", 5)) open_file(clipboard + 5);
	else if (not strncmp(clipboard, "goto ", 5)) jump_numeric(clipboard + 5);
	else if (not strncmp(clipboard, "insert ", 7)) insert_output(clipboard + 7);
	goto loop;
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	if (writable) save(); exit(0);
}













