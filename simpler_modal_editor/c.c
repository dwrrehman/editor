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

static nat cursor = 0,  count = 0, anchor = 0, origin = 0,
	cliplength = 0, head = 0, action_count = 0;
static char* text = NULL, * clipboard = NULL;
static struct action* actions = NULL;
static char filename[4096] = {0};
static char status[4096] = {0};
static volatile struct winsize window = {0};
static struct termios terminal = {0};

static char remap(char c) {
	if (c == 13) return 10;
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

static void save(void) {

}

static void window_resized(int _) { if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	save(); exit(0); 
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
	if (file < 0) { 
		read_error: printf("open: %s: %s\n", filename, strerror(errno)); 
		exit(1); 
	}
	struct stat ss; fstat(file, &ss);
	count = (nat) ss.st_size;
	text = malloc(count);
	read(file, text, count);
	close(file);

new: 	cursor = 0; anchor = (nat) -1;
	finish_action((struct action) {0}, NULL, (int64_t) 0);
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 1; 
	terminal_copy.c_cc[VTIME] = 0;
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);
	const bool writable = argc < 3;
	nat mode = 2, saved = 1, target_length = 0, home = 0;
	char history[6] = {0}, target[4096] = {0};

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
			memset(history, 0, sizeof history);
			if (target_length >= 5) target_length -= 5;
			mode = 2; if (c == 27) goto loop; 
		} else if (c == 127) { if (target_length) target_length--; }
		else if (c == 9 or c == 10 or (uint8_t) c >= 32) { 
			if (target_length < sizeof target) target[target_length++] = c; 
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
		else if (c == 'q') {}
		else if (c == 'd' or c == 's') { mode = 1; target_length = 0; home = c == 'd' ? 0 : cursor; }
		else if (c == 'z' and writable) {}
		else if (c == 'x' and writable) {}
		else if (c == 'y' and writable) save();
		else if (c == 't' and writable) mode = 0;
		else if (c == 's' and writable) insert_dt();
		else if (c == 'r' and writable) delete(1, 1);
		else if (c == 'a') anchor = anchor == (nat)-1 ? cursor : (nat) -1;
		else if (c == 'i') { if (cursor < count) cursor++; }
		else if (c == 'n') { if (cursor) cursor--; }
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
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	exit(0);
}




































/*







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


