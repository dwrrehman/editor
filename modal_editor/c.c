#include <stdio.h>   // 202402191.234834: a modal text editor written by dwrr. 
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
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

struct action {
	nat parent, pre, post;
	uint32_t choice;
	bool inserting;
	char c;
	uint16_t _padding;
};

static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";
static const nat autosave_frequency = 100; // (nat) -1; // -1 disables autosaving
static bool moved = 0, selecting = 0;
static nat cursor = 0, count = 0, anchor = 0, 
	origin = 0, finish = 0, head = 0, 
	action_count = 0, desired = 0, cliplength = 0, 
	screen_size = 0, autosave_counter = 0;
static char* text = NULL, * clipboard = NULL, * screen = NULL;
static struct action* actions = NULL;
static char filename[4096] = {0};
static char autosavename[4096] = {0};
static struct winsize window = {0};
static struct termios terminal = {0};
extern char** environ;

static char* clipboard1 = NULL;
static nat cliplength1 = 0;

static void display(bool should_write) {
	ioctl(0, TIOCGWINSZ, &window);
	const nat new_size = 128 + window.ws_row * (window.ws_col + 8) * 8;
	if (new_size != screen_size) { screen = realloc(screen, new_size); screen_size = new_size; }
	memcpy(screen, "\033[H", 3);
	nat length = 3;
	nat i = origin, row = 0, column = 0;
	finish = (nat) ~0;
	for (; i < count; i++) {
		if (row >= window.ws_row) { finish = i; break; }
		if (text[i] == 10) {
			if (i == cursor or i == anchor) { memcpy(screen + length, "\033[7m \033[0m", 9); length += 9; }
		nl:	memcpy(screen + length, "\033[K", 3); length += 3; 
			if (row < window.ws_row - 1) screen[length++] = 10;
			row++; column = 0;

		} else if (text[i] == 9) {
			nat amount = 8 - column % 8;
			column += amount;
			if (i == cursor or i == anchor) { memcpy(screen + length, "\033[7m \033[0m", 9); length += 9; amount--; }
			memcpy(screen + length, "        ", amount); length += amount;

		} else {
			if (i == cursor or i == anchor) { memcpy(screen + length, "\033[7m", 4); length += 4; }
			screen[length++] = text[i];
			if (i == cursor or i == anchor) { memcpy(screen + length, "\033[0m", 4); length += 4; }
			if (column >= window.ws_col - 2) goto nl;
			else if ((unsigned char) text[i] >> 6 != 2 and text[i] >= 32) column++;
		}
	}
	if (i == cursor or i == anchor) { memcpy(screen + length, "\033[7m \033[0m", 9); length += 9; }
	while (row < window.ws_row) {
		memcpy(screen + length, "\033[K", 3);
		length += 3; 
		if (row < window.ws_row - 1) screen[length++] = 10;
		row++;
	} 

	if (should_write) write(1, screen, length);
}

static void left(void) { 
	if (cursor) cursor--; 
	if (cursor < origin) {
		if (origin) origin--;
		if (origin) origin--;
		while (origin and text[origin] != 10) origin--;
		if (origin and origin < count) origin++;
		display(0);
	} moved = 1;
}

static void right(void) { 
	if (cursor < count) cursor++;
	if (cursor >= finish) {
		while (origin < count and text[origin] != 10) origin++;
		if (origin < count) origin++;
		display(0);
	} moved = 1;
}

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
	}
	int file = open(name, flags, permission);
	if (file < 0) { perror("save: open file"); puts(name); getchar(); }
	write(file, text, count);
	close(file);
}

static void save(void) {    
	write_file("./", filename, sizeof filename); 
	if (autosave_counter < autosave_frequency) return;
	write_file(autosave_directory, autosavename, sizeof autosavename); 
	autosave_counter = 0;
}

static void finish_action(struct action node, char c) {
	node.post = cursor; node.c = c;
	head = action_count;
	actions = realloc(actions, sizeof(struct action) * (action_count + 1));
	actions[action_count++] = node;
}

static void insert(char c, bool should_record) {
	if (should_record) autosave_counter++;
	if (autosave_counter >= autosave_frequency and should_record) save();
	struct action node = { .parent = head, .pre = cursor, .inserting = 1, .choice = 0 };
	text = realloc(text, count + 1);
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c; count++; right();
	if (should_record) finish_action(node, c);
}

static char delete(bool should_record) {
	struct action node = { .parent = head, .pre = cursor, .inserting = 0, .choice = 0 };
	left(); count--; char c = text[cursor];
	memmove(text + cursor, text + cursor + 1, count - cursor);
	text = realloc(text, count);
	if (should_record) finish_action(node, c);
	return c;
}

static void set_anchor(void) { if (selecting) return; anchor = cursor;  selecting = 1; }
static void clear_anchor(void) { if (not selecting) return; anchor = (nat) ~0; selecting = 0; }

static void cut(void) {
	if (not selecting or anchor == (nat) ~0) return;
	if (anchor > count) anchor = count;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	free(clipboard);
	clipboard = strndup(text + anchor, cursor - anchor);
	cliplength = cursor - anchor;
	for (nat i = 0; i < cliplength and cursor; i++) delete(1);
	anchor = (nat) ~0;
	selecting = 0;
}

static void searchf(void) {
	cut(); 
	nat t = 0;
	loop: if (t == cliplength or cursor >= count) return;
	if (text[cursor] != clipboard[t]) t = 0; else t++; 
	right(); goto loop;
}

static void searchb(void) {
	cut(); 
	nat t = cliplength;
	loop: if (not t or not cursor) return;
	left(); t--; 
	if (text[cursor] != clipboard[t]) t = cliplength;
	goto loop;
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
		printf("\n\033[7m[      %u  :  %llu      ]\033[0m\n", 
			actions[head].choice, child_count
		); getchar(); 
		actions[head].choice = (actions[head].choice + 1) % child_count;
	}
	head = chosen_child;
	const struct action node = actions[head];
	cursor = node.pre; 
	if (node.inserting) insert(node.c, 0); else delete(0);
	cursor = node.post;
}

static void undo(void) {
	if (not head) return;
	struct action node = actions[head];
	cursor = node.post;
	if (node.inserting) delete(0); else insert(node.c, 0); 
	cursor = node.pre;
	head = node.parent;
}

static inline void copy(bool should_delete) {
	if (not selecting or anchor == (nat) ~0) return;
	if (anchor > count) anchor = count;
	if (anchor == cursor) { clear_anchor(); return; }

	cliplength = anchor < cursor ? cursor - anchor : anchor - cursor;
	clipboard = strndup(text + (anchor < cursor ? anchor : cursor), cliplength);
	FILE* globalclip = popen("pbcopy", "w");
	if (not globalclip) {
		perror("copy popen pbcopy");
		getchar(); return;
	}	
	fwrite(clipboard, 1, cliplength, globalclip);
	pclose(globalclip);
	if (should_delete) cut();
	clear_anchor();
}

static void insert_output(const char* input_command) {
	save();
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
	for (nat i = 0; i < length; i++) insert(string[i], 1);
	free(string);
}

static void window_resized(int _) {if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h\033[?1049l", 14);
	tcsetattr(0, TCSAFLUSH, &terminal);
	save(); exit(0); 
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
	} 
	int status = 0;
	if ((pid = wait(&status)) == -1) { perror("wait"); getchar(); return; }
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
	write(1, "\033[?25h\033[?1049l", 14);
	tcsetattr(0, TCSAFLUSH, &terminal);
	create_process(arguments);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_iflag &= ~((size_t) IXON);
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSAFLUSH, &terminal_copy);
	write(1, "\033[?1049h\033[?25l", 14);
	free(arguments);
}

static void jump_index(char* string) {
	const size_t n = (size_t) atoi(string);
	for (size_t i = 0; i < n; i++) right();
}

static void jump_line(char* string) {
	const size_t n = (size_t) atoi(string);
	cursor = 0; origin = 0;
	for (size_t i = 0; i < n; i++) down_end();
	up_begin(); 
}

static void insert_string(const char* string) { for (nat i = 0; i < strlen(string); i++) insert(string[i], 1); }
static void paste(void) { if (selecting) cut(); insert_output("pbpaste"); }
static void local_paste(void) { for (nat i = 0; i < cliplength; i++) insert(clipboard[i], 1); }
static void half_page_up(void)   { for (int i = 0; i < (window.ws_row) / 2; i++) up(); } 
static void half_page_down(void) { for (int i = 0; i < (window.ws_row) / 2; i++) down(); }
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
	struct stat s; fstat(file, &s);
	count = (nat) s.st_size;
	text = malloc(count);
	read(file, text, count);
	close(file);
new: 	origin = 0; cursor = 0; anchor = (nat) ~0;
	finish_action((struct action){.parent = (nat) ~0}, 0);
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 1; 
	terminal_copy.c_cc[VTIME] = 0;  //vmin=1,vtime=0   
	terminal_copy.c_iflag &= ~((size_t) IXON);
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSAFLUSH, &terminal_copy);
	write(1, "\033[?1049h\033[?25l", 14);
        bool is_inserting = 0, literal = 0;
	char history[5] = {0};
loop:
	display(1);
	char c = 0;
	read(0, &c, 1);
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
			memset(history, 0, 5);
			delete(1); delete(1); delete(1); delete(1); delete(1); 
			is_inserting = false; 
		}
		else if (c == 27) is_inserting = false;
		else if (c == 127) { if (cursor) delete(1); }
		else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);
		else { 
			printf("error: ignoring input byte '%d'", c); 
			fflush(stdout); 
			getchar();
		}
		memmove(history + 1, history, 4);
		history[0] = c;
	} else {
		if (literal) { insert(c, 1); literal = 0; goto loop; }
		c = (char) tolower(c);
		if ((false)) {}
		else if (c == 27) {}
		else if (c == 9) insert(9, 1);
		else if (c == 10) insert(10, 1);
		else if (c == 32) insert(32, 1);
		else if (c == 't') is_inserting = true;
		else if (c == 'a') { if (not selecting) set_anchor(); else clear_anchor(); }
		else if (c == 'r') { if (selecting) cut(); else if (cursor) delete(1); }
		else if (c == 'n') left();
		else if (c == 'u') down();
		else if (c == 'p') up();
		else if (c == 'i') right();
		else if (c == 'e') word_left();
		else if (c == 'o') word_right();
		else if (c == 'h') half_page_up();
		else if (c == 'm') half_page_down();
		else if (c == 'k') up_begin();
		else if (c == 'l') down_end();
		else if (c == 'y') searchf();
		else if (c == 'g') searchb();
		else if (c == 'f') goto do_c;
		else if (c == 'q') goto done;
		else if (c == 's') save();
		else if (c == 'x') redo();
		else if (c == 'z') undo();
		else if (c == 'c') copy(0);
		else if (c == 'd') copy(1);
		else if (c == 'v') paste();
		else if (c == 'w') local_paste();
		else if (c == 'b') { ecb_to_clip(); goto do_c; }
		else if (c == 'j') literal = 1;
		else { printf("error: unknown command '%d'", c); fflush(stdout); }
	}
	goto loop;
do_c:	if (not cliplength) goto loop;
	else if (not strcmp(clipboard, "exit")) goto done;
	else if (not strcmp(clipboard, "dt")) insert_dt();
	else if (not strncmp(clipboard, "nop ", 4)) {}
	else if (not strncmp(clipboard, "copy ", 5)) set_ecb(clipboard + 5);
	else if (not strcmp(clipboard, "paste")) paste_ecb();
	else if (not strncmp(clipboard, "insert ", 7)) insert_output(clipboard + 7);
	else if (not strncmp(clipboard, "change ", 7)) change_directory(clipboard + 7);
	else if (not strncmp(clipboard, "do ", 3)) execute(clipboard + 3);
	else if (not strncmp(clipboard, "index ", 6)) jump_index(clipboard + 6);
	else if (not strncmp(clipboard, "line ", 5)) jump_line(clipboard + 5);	
	else { printf("unknown command: %s\n", clipboard); getchar(); }
	goto loop;
done:	write(1, "\033[?25h\033[?1049l", 14);
	tcsetattr(0, TCSAFLUSH, &terminal);
	save(); exit(0);
}




























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





*/





