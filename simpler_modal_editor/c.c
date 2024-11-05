#include <stdio.h>   // written 202410266.140132 by dwrr
#include <stdlib.h>  // 1202411041.210238 an editor that
#include <string.h>  // supposed to be a simpler version of
#include <iso646.h>  // the modal editor that we wrote.
#include <unistd.h>  
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
#define max_screen_size  1 << 20

struct action {
	nat parent;
	nat pre;
	nat post;
	nat choice;
	int64_t length;
	char* string;
};

static const bool use_qwerty_layout = false;
static nat cursor = 0,  count = 0, anchor = 0, origin = 0,
	cliplength = 0, head = 0, action_count = 0, writable = 0;
static char* text = NULL, * clipboard = NULL;
static struct action* actions = NULL;
static char filename[4096] = {0};
static char last_modified[32] = {0};
static char status[4096] = {0};
static volatile struct winsize window = {0};
static struct termios terminal = {0};

static char remap(const char c) {
	if (c == 13) return 10;	
	if (use_qwerty_layout) {
		const char upper_remap_alpha[26] = "AVMHRTGYUNEOLKP:QWSBFCDXJZ";
		const char lower_remap_alpha[26] = "avmhrtgyuneolkp;qwsbfcdxjz";
		if (c >= 'A' and c <= 'Z') return upper_remap_alpha[c - 'A'];
		if (c >= 'a' and c <= 'z') return lower_remap_alpha[c - 'a'];
		if (c == ';') return 'i';
		if (c == ':') return 'I';
	}
	return c;
}

static void append(
	const char* string, nat length, 
	char* screen, nat* screen_length) {
	if (*screen_length + length >= max_screen_size) return;
	memcpy(screen + *screen_length, string, length);
	*screen_length += length;
}

static nat cursor_in_view(nat given_origin) {
	nat i = given_origin, column = 0, row = 0;
	for (; i < count; i++) {
		const char c = text[i];
		if (i == cursor) return true;
		if (c == 10) {
			print_newline: row++; column = 0;
			if (row >= window.ws_row - 1) break;
		} else if (c == 9) {
			nat amount = 8 - column % 8;
			column += amount;
		} else if ((unsigned char) c >= 32) { column++; } else { column += 4; }
		if (column >= window.ws_col - 2) goto print_newline;
	} 
	if (i == cursor) return true; return false;
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

static void display(void) {
	char screen[max_screen_size];
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
			if (row >= window.ws_row - 1) {
				append("\033[0m", 4, screen, &length);
				break;
			}
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
		if (column >= window.ws_col - 2) goto print_newline;
		if (i == cursor or i == anchor) append("\033[0m", 4, screen, &length);
	}
	if (i == cursor or i == anchor) append("\033[7m \033[0m", 9, screen, &length);
	while (row < window.ws_row) {
		append("\033[K", 3, screen, &length);
		if (row < window.ws_row - 1) append("\n", 1, screen, &length);
		row++;
	}

	if (*status) {
		append("\033[H", 3, screen, &length);
		append(status, strlen(status), screen, &length);
		append("\033[7m \033[0m", 9, screen, &length);
		memset(status, 0, sizeof status);
	}
	write(1, screen, length);
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
	struct action node = { .pre = cursor };
	text = realloc(text, count + length);
	memmove(text + cursor + length, text + cursor, count - cursor);
	memcpy(text + cursor, string, length);
	count += length; cursor += length;
	if (should_record) finish_action(node, string, (int64_t) length);
}

static void delete(nat length, bool should_record) {
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

static void save(void) {
	const mode_t permissions = 
		S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;
	printf("saving: %s: %llub: testing timestamps...\n", 
		filename, count
	);
	fflush(stdout);

	char new_last_modified[32] = {0};
	struct stat attr;
	stat(filename, &attr);
	strftime(new_last_modified, 32, "1%Y%m%d%u.%H%M%S", 
		localtime(&attr.st_mtime)
	);
	printf("save: current last modified time: %s\n", new_last_modified);
	fflush(stdout);
	printf("save: existing last modfied time: %s\n", last_modified);
	fflush(stdout);

	if (not strcmp(new_last_modified, last_modified)) {

		puts("matched timestamps: saving...");
		fflush(stdout);

		printf("saving: %s: %llub\n", filename, count);
		fflush(stdout);

		int output_file = open(filename, O_WRONLY | O_TRUNC, permissions);
		if (output_file < 0) { 
			printf("error: save: %s\n", strerror(errno));
			fflush(stdout);
			goto emergency_save;
		}

		write(output_file, text, count);  // TODO: check these. 
		close(output_file);

		struct stat attrn;
		stat(filename, &attrn);
		strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", 
			localtime(&attrn.st_mtime)
		);

	} else {
		puts("timestamp mismatch: refusing to save to prior path");
		fflush(stdout);
		puts("force saving file contents to a new file...");
		fflush(stdout);

		char name[4096] = {0};
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(name, sizeof name, "%s_%08x%08x_forcesave.txt", 
			datetime, rand(), rand());
		int flags = O_WRONLY | O_CREAT | O_EXCL;

		printf("saving: %s: %llub\n", name, count);
		fflush(stdout);
		int output_file = open(name, flags, permissions);
		if (output_file < 0) { 
			printf("error: save: %s\n", strerror(errno));
			fflush(stdout);
			goto emergency_save;
		} 
		write(output_file, text, count); // TODO: check these. 
		close(output_file);

		printf("please reload the file, to see the external changes.\n");
		fflush(stdout);
		puts("warning: not reloading."); 
		fflush(stdout);
	}
	return;

emergency_save:
	puts("printing stored document so far: ");
	fflush(stdout);
	fwrite(text, 1, count, stdout);
	fflush(stdout);
	fwrite(text, 1, count, stdout);
	fflush(stdout);
	puts("emergency save: force saving to the home directory.");
	fflush(stdout);
	puts("force saving file contents to a new file at /Users/dwrr/...");
	fflush(stdout);

	char name[4096] = {0};
	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
	snprintf(name, sizeof name, 
		"/Users/dwrr/%s_%08x%08x_emergency_save.txt", 
		datetime, rand(), rand()
	);
	int flags = O_WRONLY | O_CREAT | O_EXCL;

	printf("saving: %s: %llub\n", name, count);
	fflush(stdout);

	int output_file = open(name, flags, permissions);
	if (output_file < 0) { 
		printf("error: save: %s\n", strerror(errno));
		fflush(stdout);
		puts("failed emergency save. try again? ");
		char c[3] = {0};
		read(0, &c, 2);
		if (c[0] == 'y') { 
			puts("trying to emergency save again..."); 
			fflush(stdout);
			goto emergency_save; 
		} else 
		puts("warning: not saving again, exiting editor."); 
		fflush(stdout);
		abort();
	} 
	write(output_file, text, count); // TODO: check these. 
	close(output_file);
}

static void delete_selection(void) {
	if (anchor > count or anchor == cursor) return;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	const nat len = cursor - anchor;
	delete(len, 1);
	anchor = (nat) -1;
}

static char* get_selection(void) {
	if (anchor > count) return NULL;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	char* selection = strndup(text + anchor, cursor - anchor);
	return selection;
}

static void local_copy(void) {
	if (anchor > count or anchor == cursor) return;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	cliplength = cursor - anchor;
	free(clipboard);
	clipboard = strndup(text + anchor, cliplength);
}

static void insert_char(void) {
	char c = 0;
	read(0, &c, 1); c = remap(c);
	if (c == 'a') insert_dt();
	else if (c == 'd') { read(0, &c, 1); c = remap(c);
		if (c == 'e') { read(0, &c, 1); c = remap(c);
			if (c == 'l') delete_selection();
		}
	}
	else if (c == 'r') { c = 10; insert(&c, 1, 1); }
	else if (c == 'h') { c = 32; insert(&c, 1, 1); }
	else if (c == 'm') { c = 9;  insert(&c, 1, 1); }
	else if (c == 't') { read(0, &c, 1);c = remap(c); insert(&c, 1, 1); }
	else if (c == 'u') { read(0, &c, 1);c = remap(c); c = (char) toupper(c); insert(&c, 1, 1); }
	else if (c == 'n') {
		read(0, &c, 1);c = remap(c);
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
		read(0, &c, 1);c = remap(c);
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
		read(0, &c, 1);c = remap(c);
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
}

static void window_resized(int _) { if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	if (writable) save(); exit(0); 
}

int main(int argc, const char** argv) {
	struct sigaction action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);

	if (argc == 1) goto new;
	else if (argc == 2 or argc == 3) strlcpy(filename, argv[1], sizeof filename);
	else exit(puts("usage: ./editor [file]"));

	int df = open(filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = open(filename, O_RDONLY);
	if (file < 0) read_error: exit(printf("open: %s: %s\n", filename, strerror(errno)));
	struct stat ss; fstat(file, &ss);
	count = (nat) ss.st_size;
	text = malloc(count);
	read(file, text, count); close(file);
new: 	cursor = 0; anchor = (nat) -1;
	finish_action((struct action) {0}, NULL, (int64_t) 0);
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 1; 
	terminal_copy.c_cc[VTIME] = 0;
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);
	writable = argc < 3;
	nat mode = 2, target_length = 0, home = 0;
	char history[6] = {0}, target[4096] = {0};
	struct stat attr_;
	stat(filename, &attr_);
	strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attr_.st_mtime));

loop:	ioctl(0, TIOCGWINSZ, &window);
	display();
	char c = 0;
	ssize_t n = read(0, &c, 1); 
	c = remap(c);
	if (n < 0) { perror("read"); fflush(stderr); }

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
			mode = 2;  memset(history, 0, sizeof history); 
			if (c == 'n') { if (target_length >= 5) target_length -= 5; } else goto loop;
		} else if (c == 127) { if (target_length) target_length--; }
		else if (c == 9 or c == 10 or (uint8_t) c >= 32) { 
			if (target_length < sizeof target - 1) target[target_length++] = c; 
		} 
		memmove(history + 1, history, sizeof history - 1);
		*history = c;
		cursor = home;
		for (nat t = 0; cursor < count; cursor++) {
			if (text[cursor] != target[t]) { t = 0; continue; }
			t++; if (t == target_length) goto found;
		}
		cursor = home; found:; 
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
		else if (c == 'c') local_copy();
		else if (c == 'a') anchor = anchor == (nat)-1 ? cursor : (nat) -1;
		else if (c == 'i') { if (cursor < count) cursor++; }
		else if (c == 'n') { if (cursor) cursor--; }
		else if (c == 'd' or c == 'k') { 
			mode = 1; 
			target_length = 0; 
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
					if (cursor < count and text[cursor] == 10) break;
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
do_command:
	if (not writable) goto done;
	char* s = get_selection();
	if (not s) goto loop;
	else if (not strncmp(s, "do ", 3)) {}
	else if (not strncmp(s, "del ", 4)) delete_selection();
	else if (not strcmp(s, "insert hello")) insert("hello world", 11, 1);
	else if (not strcmp(s, "dt")) insert_dt();
	else if (not strcmp(s, "exit")) goto done;
	free(s);
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	if (writable) save(); exit(0);
}







































/*
	char name[4096] = {0};
	strlcpy(name, filename, sizeof name);

	nat saved = 0;
loop:
	if (not *name) {
		
	}
	


	if (not saved) goto loop;



*/



/*



		print("rd:c"); number(actions[head].choice); 
		print("/"); number(child_count); print("|");



	print("rd:"); if (node.length < 0) print("-"); else print("+");
	number((nat)(node.length < 0 ? -node.length : node.length)); 
	print(",h"); number(head); print("|");





	//print("ud:"); if (node.length < 0) print("-"); else print("+");
	//number((nat) (node.length < 0 ? -node.length : node.length)); 
	//print(",h"); number(head); print("|");



	left();
	while (cursor) {
		if (not (not isalnum(text[cursor]) or isalnum(text[cursor - 1]))) break;
		if (text[cursor - 1] == 10) break;
		left();
	}
}

	right();
	while (cursor < count) {
		if (not (isalnum(text[cursor]) or not isalnum(text[cursor - 1]))) break;
		if (text[cursor] == 10) break;
		right();
	}







	//printf("length = %llu, screen = <<<%.*s>>>\n", 
	//	length, (int) length, screen);

	for (nat ii = 0; ii < length; ii++) {
		printf("[c=%d]", screen[ii]);
	}
	puts("");
	fflush(stdout);


		
		else if (c == 't') mode = insert_mode;

		else if (c == 'd') mode = search_mode;

		else if (c == 'r') delete(1, 1);


		else if (c == 'a') anchor = anchor == disabled ? cursor : disabled;
		else if (c == 'w') paste();
		else if (c == 'c') copy_local();

		else if (c == 'q') goto do_c;
		else if (c == 'y') save();
		
		else if (c == 'i') right();
		else if (c == 'n') left();
		else if (c == 'p') up();
		else if (c == 'u') down();

		else if (c == 'h') half_page_up();
		else if (c == 'm') half_page_down();

		







else if (c == 'b') { task_to_clip(); goto do_c; }
else if (c == 'v') { task2_to_clip(); goto do_c; }

else if (c == 'x') redo();
else if (c == 'z') undo();

else if (c == 'g') paste_global(); 
else if (c == 'f') copy_global();


else if (c == 'e') word_left();
else if (c == 'o') word_right();


else if (c == 's') insert_char();


else if (c == 127) { if (anchor == disabled) delete(1,1); else delete_selection(); }






	static const char* help_string = 
	"q	quit, must be saved first.\n"                              q
	"z	this help string.\n"					  ...
	"s	save the current file contents.\n"                         y
	"<sp>	exit insert mode.\n"                                       drtpun
	"<sp>	move up one line.\n"                                      ...
	"<nl>	move down one line.\n"                                    ...
	"a	anchor at cursor.\n"                                       a
	"p	display cursor/file information.\n"                       ...
	"t	go into insert mode.\n"                                    t
	"u<N>	cursor += N.\n"                                           i u m
	"n<N>	cursor -= N.\n"                                           n p h
	"c<N>	cursor = N.\n"                                             d
	"d[N]	display page of text starting from cursor.\n"             ...
	"w[N]	display page of text starting from anchor.\n"             ...
	"m<S>	search forwards from cursor for string S.\n"               d
	"h<S>	search backwards from cursor for string S.\n"             ...
	"o	copy current selection to clipboard.\n"                    c
	"k	inserts current datetime at cursor and advances.\n"       ...
	"e<S>	inserts S at cursor and advances.\n"                      ...
	"i	inserts clipboard at cursor, and advances.\n"              w
	"r	removes current selection. requires confirming.\n";        r 









*/











/*	nat column = 0, row = 0;
	for (nat i = origin; i < count; i++) {

		const char c = text[i]; 

		if (i == cursor or i == anchor) printf("\033[7m");
		if (c == 10) putchar(' ');

		if (c == 9) {
			const nat amount = 8 - column % 8;
			for (nat _ = 0; _ < amount; _++) putchar(32);
			column += amount;

		} else if (c == 10) {
		print_newline: 
			puts("");
			column = 0; row++;
			if (row >= max_row_count) { 
				at_bottom = false; 
				printf("\033[0m"); 
				break; 
			}

		} else if (c >= 32) {
			putchar(c);
			column++;
		} else {
			printf("\033[7mCTL%2hhu\033[0m", c);
			column += 5;
		}
		if (column >= wrap_width) goto print_newline;

		if (i == this->cursor or i == this->anchor) printf("\033[0m");
	}

	bool at_bottom = true;
{

	nat column = 0;
	nat row = 0;
	for (nat i = this->origin; i < this->length; i++) {

		const char c = this->output[i]; 

		if (i == this->cursor or i == this->anchor) printf("\033[7m");
		if (c == 10) putchar(' ');

		if (c == 9) {
			const nat amount = 8 - column % 8;
			for (nat _ = 0; _ < amount; _++) putchar(32);
			column += amount;

		} else if (c == 10) {
		print_newline: 
			puts("");
			column = 0; row++;
			if (row >= max_row_count) { at_bottom = false; printf("\033[0m"); break; }

		} else if (c >= 32) {
			putchar(c);
			column++;
		} else {
			printf("\033[7mCTL%2hhu\033[0m", c);
			column += 5;
		}
		if (column >= wrap_width) goto print_newline;

		if (i == this->cursor or i == this->anchor) printf("\033[0m");
	}
	
}
	if (not at_bottom) printf("\033[7m[scrolling back]\033[0m");
	else printf("\033[7m(head)\033[0m");

	fflush(stdout);




*/


