#include <stdio.h>  // 202308255.160419:   a rewrite of the simplified editor, to be nonmodal, and even simpler. 
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
typedef size_t nat;
struct action {
	char* text;
	nat* children;
	nat parent, choice, count, length, insert,
	pre_cursor, post_cursor, pre_origin, post_origin, pre_saved, post_saved;
};
static const nat active = 0x01, saved = 0x02, selecting = 0x04;
extern char** environ;
static struct termios terminal;
static struct winsize window;
static char filename[4096] = {0}, directory[4096] = {0};
static char* text = NULL, * clipboard = NULL;
static struct action* actions = NULL;
static nat mode = 0, cursor = 0, origin = 0, anchor = 0, cursor_row = 0, cursor_column = 0, 
	   count = 0, cliplength = 0, action_count = 0, head = 0, previous_cursor = 0;

static void handler(int __attribute__((unused))_) {}
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
	else if (c == 8  /*H*/) {}
	else if (c == 18 /*R*/) {}
	else if (c == 24 /*X*/) {}
	else if (c == 26 /*Z*/) {}
	else if (c == 17 /*Q*/) { if (mode & saved) mode &= ~active; }
	else if (c == 4  /*D*/) insert_now();
	else if (c == 1  /*A*/) { read(0, &c, 1); insert(c, 1); }
	else if (c == 19 /*S*/) alternate();
	else if (c == 27) interpret_arrow_key();
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







































































































/*




perform_directory_read:;
	DIR* thisdir = fdopendir(dir_file);
	struct dirent* entry = readdir(thisdir);

	char* string = NULL;
	size_t length = 0;

	while (entry) {
		char path[4096] = {0};

		strlcpy(path, read_directory, sizeof path);
		strlcat(path, "/", sizeof path);
		strlcat(path, read_filename, sizeof path);
		strlcat(path, "/", sizeof path);
		strlcat(path, entry->d_name, sizeof path);

		const size_t len = strlen(entry->d_name);
		string = realloc(string, length + len + 1);
		strlcpy(string + length, entry->d_name, len + 1);
		length += len;

		struct stat statbuf;
		stat(path, &statbuf);
		if (S_ISDIR(statbuf.st_mode)) {
			string = realloc(string, length + 1);
			string[length++] = '/';
		}

		string = realloc(string, length + 1);
		string[length++] = 10;

		entry = readdir(thisdir);
	}
	closedir(thisdir);
	count = length;
	text = string;
	goto rest_of_stuff;
	
file_read:


*/










/* bugs:   - memory leaks
	 x - fix move up/down functions. get them working perfectly. [actually! no, i'm not going to fix them. i am going to simplify them and just work with the new way they work. NOT visual line based. newline based only. yay. */





//	if (not creating) {
//		printf("save: saved %lub to ", count);
//		printf("%s : %s\n", directory, filename); 
//		getchar(); return;
//	}




//	_unused0 = 0x40,
//	_unused1 = 0x80






/*
static bool identical_files(
	const char* a_d, const char* a_f,
	const char* b_d, const char* b_f
) {
	const int a_dir = open(a_d, O_RDONLY | O_DIRECTORY, 0);
	if (a_dir < 0) { perror("same open directory"); getchar(); return 0;}
	const int a_fd = openat(a_dir, a_f, O_RDONLY, 0);
	if (a_fd < 0) { perror("same openat file"); getchar(); return 0;}

	const int b_dir = open(b_d, O_RDONLY | O_DIRECTORY, 0);
	if (b_dir < 0) { perror("same open directory"); getchar(); return 0; }
	const int b_fd = openat(b_dir, b_f, O_RDONLY, 0);
	if (b_fd < 0) { perror("same openat file"); getchar(); return 0; }

	struct stat a_stat = {0};
	struct stat b_stat = {0};
	fstat(a_fd, &a_stat);
	fstat(b_fd, &b_stat);
	close(b_fd);
	close(b_dir);
	close(a_fd);
	close(a_dir);
	return 	a_stat.st_dev == b_stat.st_dev and 
		a_stat.st_ino == b_stat.st_ino;
}*/


//	if (not md and not identical_files(directory, filename, saved_directory, saved_filename)) {
//		printf("write: error: tried to save the current file a different filename or directory, aborting save.\n");
//		getchar();
//		return;
//	}












// if (not (setting & selecting)) bubbles = cursor;


/* -------- list of commands that the editor can do with keys ----------------



	state = 1;
	{ anchor = cursor; selecting = not selecting; }
	{ mode = 1; c = 0; p1 = 0; } 
	save();
	move_left();
	move_right();
	move_up();
	move_down();
	move_word_left();
	move_word_right();
	backwards();
	forwards();

	move_top();
	move_bottom();
	move_begin();
	move_end();
	paste();
	insert_output("pbpaste");
	copy();
	cut();
	delete(1);
	sendc();
	{ if (saved) mode = 0; }
	




----------------------------------------------------------------------------------















	move_left();
	move_right();
	move_up();
	move_down();

	move_word_left();
	move_word_right();
	move_begin();
	move_end();

	move_top();
	move_bottom();

	backwards();
	forwards();

x	cut();
	paste();

	copy();
	execute("pbpaste");

x	save();

x	sendc();

x	{ anchor = cursor; selecting = not selecting; }

x	{ mode = 1; c = 0; p1 = 0; } 

x	{ if (saved) mode = 0; }

x	state = 1;




















	cliplength = 1; clipboard = strdup("\n");
loop:	display();
	read(0, &c, 1);
	if (mode == 1) {
		if (c == 17) mode = 2;
		else if (c == 27) interpret_arrow_key();
		else if (c == 127) delete(1);
		else if (c == 'd' and p1 == 'q') { delete(1); mode = 2; c = 0; p1 = 0; }
		else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);
	} else if (mode == 2) {
		nat bubbles = anchor;
		if (not selecting) bubbles = cursor;
		if (state == 1) {
			if (c == 'a') {}
			else if (c == 'd') paste();
			else if (c == 'r') save();
			else if (c == 't') sendc();
			else if (c == 's') execute("pbpaste");
			else if (c == 'h') copy();
			else if (c == 'm') {}
			else if (c == 'c') {}

			else if (c == 'n') {}
			else if (c == 'u') {}
			else if (c == 'p') {}
			else if (c == 'i') { if (saved) mode = 0; }
			else if (c == 'e') move_begin();
			else if (c == 'o') move_end();
			else if (c == 'l') move_top();
			else if (c == 'k') move_bottom();

			else if (c == 27) interpret_arrow_key();
			state = 0;
		} else {
			if (c == 'a') { anchor = cursor; selecting = not selecting; }
			else if (c == 'd') {}
			else if (c == 'r') cut();
			else if (c == 't') { mode = 1; c = 0; p1 = 0; } 
			else if (c == 's') move_up();
			else if (c == 'h') move_down();
			else if (c == 'm') {}
			else if (c == 'c') undo();

			else if (c == 'n') backwards();
			else if (c == 'u') move_word_left();
			else if (c == 'p') move_word_right();
			else if (c == 'i') state = 1;
			else if (c == 'e') move_left();
			else if (c == 'o') move_right();
			else if (c == 'l') forwards();
			else if (c == 'k') redo();

			else if (c == 27) interpret_arrow_key();
		}
		if (not selecting) anchor = bubbles;
	} p1 = c; //  p2 = p1; 
	if (mode) goto loop;
	printf("\033[H\033[2J");
	tcsetattr(0, TCSAFLUSH, &terminal);
}








// todo:     - fix memory leaks       o- write manual        x- test test test test test
//           o- write feature list     x- redo readme.md     x- test with unicode                
//           x- fix the display(0) performance bug on move_right(). 
// x     - MAKE THE EDITOR ONLY USE ALPHA KEYS FOR KEYBINDINGSSSSSSSS PLEASEEEEEEEEE MODALLLLLL PLZ



  ----------------- bugs: ---------------------

x	- last line does not display properly     when it is a wrapped line

		x	probably because of a newline in the display code?... idk.




	- fix memory leaks

	- 

*/






















/*
add                  shift + left/right                 and         option + left/right            to insert mode.

				make them          word left and word right,      and begin and end, respectively. 


			that will help a lottttt


	yay






*/












/*

	how to make this more usable:

			add commands for moving the cursor by L/R-word,   up/down,  left/right,    beginline/endline,
									and forwards/backwards-search.




										do not implement top/bottom.



			THEN,  also implement the thing where you toggle whether the anchor is "dropped", or "trailing".


				ie, it is always the last cursor position before the last command. the cursor doesnt just sit there when not being used. nope. 


							then, also make it so that 	dropping anchor 	is seperate from 

										going into insert mode.


									make those seperate keybindings. 









			also, 


				use the keys which are                 a s h t d r         n e o i u p 


						only 


					those 12 keys, only. 



								ANDDDD the    state         ie, deadstop           "n"



											i think 
										maybe not        n          but something 



			that basically doubles our keybindings to like 


									24 ish 



									more like 23 






			but yeah, thats enough, i think.







alsooooo



							make the way to exit insert mode         two characters. 

									pleaseeeeeee please please



							actually even better, make it one character. 
								



						control-A       or something  idk 			or wait control-Q is already exit insert mode 
								ah


										well



								hm


								maybe make it like     qd   or something      or      ;p

										that could work    ;p 

									i like that         


											;p
										qd   or ;p      would be fine 
							honestly 
										but lets pickone. 
									probably qd       i think 
									yeah 
										cool 
						qd  so thats fine 
							instead of drt 				hm
												cool

										i think thats fine lol

				yay





alrighty








so yeah, these changes should bring together the editor quite a bit. i think it will actually be usable after we do these things. 


					forwards/backwadrs search is goodddd      just not for literallyyyyy everything. 

							pretty cool that it can do so much though lol.


									pretty amazing, honestly. 
											but yeah. we need things to be faster, though. 
									so yeah. thats why we are specializing things like this, a little bit. 


											in terms of keybindings. 







soooooo yeahhhhh


	cool beans






			

*/













//		if (c == 'q') { if (b) 	{}		else 	{ b = 1; goto loop; } 	} /* Q */
//		if (c == 'd') { if (b) 	cut();		else 	backwards(); 		} /* D */
//		if (c == 'r') { if (b) 	paste(); 	else 	undo(); 		} /* R */
//		if (c == 'a') { if (b) 	sendc();	else 	anchor = cursor; 	} /* A */
//		if (c == 's') { if (b) 	save();		else 	move_left(); 		} /* S */
//		if (c == 'h') { if (b) 	{}		else 	forwards(); 		} /* H */
//		if (c == 'x') { if (b) 	{} 		else 	move_right(); 		} /* X */
//		if (c == 'z') { if (b)  {}		else 	redo();  		} /* Z */
//

/*

		










































	else if (c == 17) { if (b) 			else 	{ b = 1; goto loop; } 	}  Q 
	else if (c == 4)  { if (b) 	cut();		else 	backwards(); 		}  D 
	else if (c == 18) { if (b) 	paste(); 	else 	undo(); 		}  R 
	else if (c == 1)  { if (b) 	sendc();	else 	anchor = cursor; 	}  A 
	else if (c == 19) { if (b) 	save();		else 	move_left(); 		}  S 
	else if (c == 8)  { if (b) 	copy();		else 	forwards(); 		}  H 
	else if (c == 24) { if (b) 	execute(); 	else 	move_right(); 		}  X 
	else if (c == 26) { if (b)  	{}		else 	redo();  		}  Z 
	else if (c == 127) delete(1);
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);
*/








































/*

 Q	quit() 		deadstop()
 D 	cut()		backwards()
 R 	paste() 	undo()
 A 	sendc()		anchor()
 S 	save()		move_left()
 H 	copy()		forwards()
 X 	execute() 	move_right()
 Z 	load()		redo()

*/

















/** ------------------layer0------------------------------


	control-Q 	:	(deadstop)
		
	control-D	:	backwards()

	control-R	:	undo()

	control-A	:	anchor()

	control-S	:	move_left()

	control-H	:	forwards()

	control-X	:	move_right()

	control-Z	:	redo()



------------------------layer1-------------------------




	control-Q 	:	quit()
		
	control-D	:	cut()

	control-R	:	

	control-A	:	send_command()

	control-S	:	save()

	control-H	:	copy()

	control-X	:	execute()

	control-Z	:	


-------------------------------------------------------






*/























		//puts("rename: unimplemented!"); 
		//getchar(); 

	//if (*filename and not len) goto save;
	//if (not len) { puts("s:no filename given"); goto loop; }







/*
if (anchor < cursor) {

		deleted = text + anchor;
		length = cursor - anchor;

	}
*/









/*

static void paste(void) {
	FILE* file = popen("pbpaste", "r");
	if (not file) { 
		perror("paste popen"); 
		getchar(); 
		return; 
	}
	char* string = NULL;
	nat length = 0;
	int c = 0;
	while ((c = fgetc(file)) != EOF) {
		string = realloc(string, length + 1);
		string[length++] = (char) c;
	}
	pclose(file);
	insert_string(string, length);
}


static void move_left_chunk(void) {
	for (nat i = 0; i < 100; i++) {
		display(0);
		move_left();
	}
}

static void move_right_chunk(void) {
	for (nat i = 0; i < 100; i++) {
		display(0);
		move_right();
	}
}




*/





























//if (c >= 32) { printf("\033[0;44m[%lu]\033[0m", (nat) c); getchar(); }
















/*








		wait i found an error with the execute command, using it to implement copy and paste though!!!


				we actually can't give input of the current selection to the command, 

							because we need to know what the current command is 


							based on what is selected!!


							thats like a critical thing that it needs to do. 


						it cuts the current selection, and executes it as a bash command. thats what it does. 
				so like, how do we give input of another selection then?


					we don't lol.    we need another command for copy lol. 



							i feel like giving input to the command is like      basicallyyyy futile, i think?.. idk.. lol. it seems futile lol.
						so yeah. i think so honestly 


								we should totally have a copy command. 













	 ---------------------- final keybindings: -------------------------------

			lets add a second anchor key!!!! that should help a ton. yay.     a     and    q      i think. 
a	- anchor 
d	- move_left_chunk
h	- move_right_chunk
q	- deadstop (*)
r	- cut
s	- move_left
x	- move_right

qa	- undo
qd	- execute
qh	- copy
qq	- quit
qr	- 
qs	- save
qx	- redo_alternate





------------------------------------------------------------



q	- deadstop0 (*)
r	- redo
z	- undo
a	- anchor
s	- move_left
x	- move_right
d	- move_left_chunk
h	- move_right_chunk
	

qq	- quit
qs	- save

qz	- find selection ("next")
qd	- cut  [anchor-cursor sel]			sets selection
qr	- copy [anchor-cursor sel]			sets selection
qh	- cut-find anchor-cursor sel.			sets selection
qx	- cut-execute anchor-cursor sel.		sets selection
qa	- open [anchor-cursor sel]			sets selection














d s h x r

q a z 



qd qs qh qx qr

qa qz qq




------------------------------------------------------------




*/











/* ---------------------- final keybindings: -------------------------------

a	- move_left
d	- move_left_chunk
h	- move_right_chunk
q	- deadstop (*)
r	- cut
s	- move_right


qa	- redo_alternate          
qd	- save
qh	- anchor
qq	- quit
qr	- execute       (sends selection as input, always, pastes output always. undoable.
qs	- undo

	--------			OLD 











x	else if (c == 'Q') quit();
x	else if (c == 'S') save();
del	else if (c == 'O') open();
x	else if (c == 'K') execute();

x	else if (c == 'A') anchor = cursor;
x	else if (c == 'R') cut();
x	else if (c == 'M') copy();
del	else if (c == 'L') paste();
	
x	else if (c == 'N') move_left();
x	else if (c == 'E') move_right();
x	else if (c == 'U') move_left_chunk();
x	else if (c == 'P') move_right_chunk();
	
	else if (c == 'C') undo();
	else if (c == 'K') redo();
	else if (c == 'I') alternate();
*/













/*














editor primitives:
-----------------------



q	- deadstop (*)
r	- cut
a	- move_left
s	- move_right
d	- move_left_chunk
t	- move_right_chunk


qq	- quit
qd	- save
qr	- execute
qh	- anchor
qs	- undo
qa	- redo_alternate









	


very very tempted to make cut() actually do a copy() as well. that would simplify one further keybind, lol. 
	kindaaa seems like the wrong call though... just because its so wasteful.. like if we just want to copy something. 
				idk...... hm...  we have to cut, it, only to paste it again. like seriously? idk.... 

							yeah, i think we should keep them seperate. it just makes more sense that way. often you want to delete stuff, without copying it to your clipboard. i think. 
								hm.. 


					welppppp we have 1 too many keybindings then for only 6 keys loll. not great. 
				hmmm




					literally nothing else can get eliminated though... thats the problem lol. like thats everything that we have. hm. 


		




welll


	wait


			what if we just unconditionally sent the current selection to the program!!!!

					that we executed!!!



								then, like we could literally have MORE FEATURES

								with less keybindssss




			because then we don't need copy()!!!!

					nice!!!! lets do that. cool beans. 




	so pbcopy and pbpaste, 

		are user senthesized commands

		yay 


								that way its platform independent, 



									andddd has little keybinds. 



		yay. 
			i love this lol. 



cool beans 



	yay. 


lets do that.

lets see if it works








	

----------------------extraneous---------------------------



x	- alternate

	- copy







































Q [17]
D [4]
R [18]

A [1]
S [19]
H [8]



	else if (c == 'Q') { if (saved) goto done; }
	else if (c == 'A') anchor = cursor;
	else if (c == 'S') save();
	else if (c == 'N') move_left();				
	else if (c == 'E') move_right();
	else if (c == 'U') move_left_chunk();
	else if (c == 'P') move_right_chunk();
	else if (c == 'C') undo();
	else if (c == 'K') redo();
	else if (c == 'I') alternate();
	else if (c == 'M') copy();
	else if (c == 'R') {} //  cut();
	else if (c == 'L') paste();
	else if (c == 'K') execute();
*/











/*


  [Q] [D] [R] [W]

   [A] [S] [H] [T]
	
     [Z] [X] [M]              <--- this row is not usable.









  [17] [4] [18] [23]

   [1] [19] [8] [20]

     [-]  [-]  [-]



1   (start of heading)````````````````
4   (end of transmission)```````````````
8   (backspace)	       ```````````````````
17  (device control 1) ``````````````````
18  (device control 2)``````````````````
19  (device control 3)`````````````````
20  (device control 4)`````````````````
23  (end of trans. block)`````````````````









????	
????	

a	moveleft
s	moveright
d	moveleftw
r	moverightw

q	*
d	anchor
r	cut
w	history-mode

q q	quit
q d	save
q r	execute
q w	open

q a	-
q s	copy
q h	paste
q t 	[UNDEFINED]

history mode:
----------------
a	undo
s	redo
d	alternate
q	*





  [ * / quit] [leftw/save]  

    [left/] [right/] [rightw/] 



  [ * / quit] [lw]

   [l] [r] [rw] 





  [17] [4] [18]

   [1] [19] [8]












  [17] [4] [18] [23]

   [1] [19] [8] [20]











2	else if (c == 'Q') quit();
2	else if (c == 'A') anchor();
2	else if (c == 'S') save();
3	else if (c == 'O') open();
1	else if (c == 'N') move_left();				
1	else if (c == 'E') move_right();
1	else if (c == 'U') move_left_chunk();
1	else if (c == 'P') move_right_chunk();
m1	else if (c == 'C') undo();
m1	else if (c == 'K') redo();
m2	else if (c == 'I') alternate();
2	else if (c == 'M') copy();
2	else if (c == 'R') cut();
2	else if (c == 'L') paste();
3	else if (c == 'K') execute();






*/



























/*


//////////printf("%s %s %lu\n", filename, saved ? "s" : "w", count); 






const size_t new = count + a.dlength - a.ilength;
	if (a.dlength > a.ilength) text = realloc(text, new);
	if (a.dlength != a.ilength) memmove(text + *cursor + a.dlength - a.ilength, text + *cursor, count - *cursor);
	memcpy(text + *cursor - a.ilength, a.deleted, a.dlength);
	if (a.dlength < a.ilength) text = realloc(text, new);
	count = new;





const size_t new = count + a.ilength - a.dlength;
	if (a.ilength > a.dlength) text = realloc(text, new);
	if (a.ilength != a.dlength) memmove(text + *cursor + a.ilength - a.dlength, text + *cursor, count - *cursor);
	memcpy(text + *cursor - a.dlength, a.inserted, a.ilength);
	if (a.ilength < a.dlength) text = realloc(text, new);
	count = new; 









		struct action new = {0};
		new.pre = *cursor;
		const size_t dlength = *cursor - cursor[2];
		char* deleted = text ? strndup(text + cursor[2], dlength) : 0;
		const size_t new_count = count + length - dlength;
		if (length > dlength) text = realloc(text, new_count);
		if (length != dlength) memmove(text + *cursor + length - dlength, text + *cursor, count - *cursor);
		if (length) memcpy(text + cursor[2], input, length);
		if (length < dlength) text = realloc(text, new_count);
		count = new_count; if (length or dlength) saved = 0;
		
		*cursor += length - dlength; cursor[2] = *cursor; cursor[1] = *cursor;
		new.post = *cursor; new.parent = head;
		new.inserted = strndup(input, length); new.ilength = length;
		new.deleted = deleted; new.dlength = dlength;
		actions[head].children = realloc(actions[head].children, sizeof(size_t) * (size_t) (actions[head].count + 1));
		actions[head].choice = actions[head].count;
		actions[head].children[actions[head].count++] = action_count;
		head = action_count;
		actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
		actions[action_count++] = new;
		printf("+%lu -%lu \n", length, dlength); insert = 0;
*/




// if (anchor > cursor) { size_t temp = cursor; cursor = anchor; anchor = temp; }










































		//if (not access(rest, F_OK)) { puts("s:file exists"); goto loop; }
		//else strlcpy(filename, rest, sizeof filename);

















// = (column + amount) % window.ws_col;






/*

if (column >= window.ws_col) column = 0;
		if((unsigned char) c >> 6 != 2) column++;
		write(1, &c, 1);






if (text[cursor] == 10) {
			column = newlines[--newline_count];
			printf("\033[A");
			if (column) printf("\033[%huC", column);
			fflush(stdout);

		} else if (text[cursor] == 9) {
			uint8_t amount = tabs[--tab_count];
			column -= amount;
			write(1, "\b\b\b\b\b\b\b\b", amount);

		} else {
			while ((unsigned char) text[cursor] >> 6 == 2) { count--; cursor--; }
			if (not column) {
				column = window.ws_col - 1;
				write(1, "\b", 1);
			} else {
				column--;
				write(1, "\b \b", 3);
			}
		}





	if (len + 1 >= capacity) {
		capacity = 4 * (capacity + 1);
		
	}






process:
	printf("\n\trecieved input(%lu): \n\n\t\t\"", len);
	//fwrite(input, len, 1, stdout);
	for (size_t i = 0; i < len; i++) {
		if (isprint(input[len])) putchar(input[len]);
		else printf("[%d]", input[len]);
	}
	printf("\"\n");
	
	fflush(stdout);
	if (len == 4 and not strncmp(input, "quit", 4)) goto done;
	if (len == 5 and not strncmp(input, "clear", 5)) { printf("\033[H\033[2J"); fflush(stdout); } 
	goto loop;




*/


//for (int i = 0; i < len; i++) {
	//	printf("%c", input[i] == 9 ? '@' : input[i]);
	//}













//printf("\033[%hhuD", amount);






/* 

typed in the editor itself:





okay, so i mean, i am going to try to stress test this code lol. i think we might have actually figured it out. like, its super easy to use and yeah, i just like it alot. i think its really nice actually. yay.

        okay, so i mean, i think we should probably do a tonnnn of testing with it. 

                i think its pretty much perfect though. the only problem is the fact that we cant resize the window like dynamically lol.                           


                but yeah





printf("\033[6n");
	scanf("\033[%lu;%luR", &row, &column);
	fflush(stdout);
	row--; column--;






//printf("[%lu %lu][%lu %lu]\n", window_rows, window_columns, row, column);



//printf("\033[D\033[K\033[D\033[K"); 
		//fflush(stdout);




	if we try to delete a newline....


	then just don't delete anything...?


		...  we avoid so many problems if we just don't allow deleteting lol.

				i feel like we should just not try to delete newlines. 


				idk. kinda feels like an unimportant change. like we don't reallyyyyy need to... 


						i mean 




							okay, technically we could allow for the deleting of the VERY LAST line maybe?... i feel like that would be fine. because thats all we really use really.. 

					but like, even at that point, i feel like deleting the last line is pointless lol. idk. 



			

*/






































/*

				tb that the editor couldnt save   because saving wasnt working properly lol




okay

        i think this is good?

lets try to write this file and see what happens lol.


 i mean,


        it kinda works?

        lol


                        i don't think its going to crash anymore, hopefully lol.


idk. 

it might. 


but probably not lol.



yeah, i really hope this thing doesnt crash lol. 



i mean. 


idk. lol.
i think we'll be okay.
i guess we just have to like
testing things, and make sure that we do everythin properly
lol.
theres not really any subsitute for that. 


                        but yeah, it seems like everything else works like     just fine though!


                                                so i am quite happy about that lol. 


                                                                yay 




                but yeah, lets try to write this now lol hopefully we don't loose it all. gah. 

                                hm
        

*/


//if (b) { memcpy(screen + length, "\033[0;41m", 7); length += 7; } 
//if (i == cursor) { if (b) { memcpy(screen + length, "\033[0m", 4); length += 4; } }  








/*

// okay, so i kinda want to try to use this editor to write a piece of code. 
// it doesnt have to be anything fancy, but yeah, i need to test writing code with this thing. 
// lets see how it will go! yay. [202306305.184617]

#include <stdio.h>
#include <stdlib.h>

int fmian() {
	char buffer[512] = {0};

	fgets(buffer, sizeof buffer, stdin);

	int a = atoi(buffer);
	
	fgets(buffer, sizeof buffer, stdin);
	
	int b = atoi(buffer);

	printf("%d + %d = %d\n", a, b, a + b);

	exit(0);
}




*/







/*

---------- notes about special character sequences with arrow keys: ---------------------



	option + right    :         27    102                       word left and word right.

  
	option + left     :         27    98                        ^




x	option + up       :         27    91     65                 begin and end of line.


x	option + down     :         27    91     66		    ^




x	option + shift + left       :    27  91  68                ...what do we put here?


x	option + shift + right      :   27   91   67               ^




and for completeness: 
----------------------


x	option + shift + up       :    27  91  65           same as without shift. :(


x	option + shift + down      :   27   91   66         same as without shift. :(









						wait





							..never mind. 


					we can't do anything with anything except for the first two. 


					ie,      option left    and option right 





				those the are only ones that are distinct from the regular     arrow key codes. 

					so yeah thats unfornuate lol

			but yeah 
	
						so we need to choose:    begin or end of line,   or word left/word right

									to be available in insert mode. 



								...so yeah. interesting. hm. 


					very interesting. 


		








todo:

	add end of line, 

	add beginning of line 

	add word left 

	add word right 

	

*/



/*

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

*/











/*
	const nat save = cursor_column;
	while (cursor < count) {
		if (text[cursor] == 10 or not cursor_column) break;
		move_right();
	}
	if (text[cursor] == 10) move_right();
	while (cursor < count) {
		if (cursor_column >= save) break;
		if (text[cursor] == 10) break;
		move_right();
	}

*/





/*
202307145.100222:

so i am completely redoing how the    saving/renaming  system works in the editor, to make it alotttt more robust. 

	


	basically 


	
there are two milestones that a file goes through in its life, initially:


	1. creation


	2. saving / editing







CREATION: 
--------------


	here, we want to call                 open()      with 		O_CREAT | O_WRONLY | O_TRUNC | O_EXCL



							O_CREAT     :     create if it doesnt exist. 

	
							O_EXCL      :     make sure you create it. (ie, make sure it doesnt exist!)


							O_TRUNC     :     delete the file to be size 0. (already was!)


							O_WRONLY    :     we only want to write to it. 




SAVING / EDITING:
----------------------


	here, we want to instead call:        open()          with       O_WRONLY | O_TRUNC




			i think thats it actually

			and it will fail if the file doesnt exist. and not create any new files at all. 

		
		so yeah. thats interesting lol. 


cool beans










NOWWWWWWWW       for the fun part 




							i want to introduce the notion of    

				the file's location being different from its name... 


								i think?



					or maybe not actually. 



	but i do want to allow for the user to change the location of the file, somehow. 

		i think the way i am going to try to doing that, 

	is to make a command that pastes the current filename into the buffer.   yeah. then, we can just edit, from there. and then send a rename command. lol. i think that will be great honestly. yayyyyy






okay, lets see if that works i guess






but also



				theres this other thing, called 





		openat()




								and also openat2()





		







wow okay, so this is slightly complicated 






openat              takes a filedescriptor,   for the dir      that you want to locate the file in,





	and so, we need to actually call open 


							for that 





												to get that fd






		so, the calls would be 


								int dir = open("/directory/path/", O_RDONLY | O_DIRECTORY, 0);

								int file = openat(dir, "filename.txt", O_WRONLY | O_TRUNC, 0);


								




		

		thats how we will open the file, for saving        









	and then for creation, we would do:


		int dir = open("/directory/path/", O_RDONLY | O_DIRECTORY, 0);

		int file = openat(dir, "filename.txt", 
					O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 
					S_IRUSR | S_IWUSR  | S_IRGRP | S_IROTH
				);




so yeah 




thats creation. 




			for reading the file, we would do





		
				int dir = open("/directory/path/", O_RDONLY | O_DIRECTORY, 0);

				int file = openat(dir, "filename.txt", O_RDONLY);



	



yayyy



okay thats all pretty simple 
	lets code it up now 











*/



				




/*

old saving code:



static void save_file(void) {
	if (not *filename) {
		puts("save: error: currently unnamed"); 
		getchar(); return;
	}
	FILE* output_file = fopen(filename, "w");
	if (not output_file) { 
		strlcpy(filename, "", sizeof filename);
		perror("save fopen");
		getchar(); return;
	}
	fwrite(text, 1, count, output_file); 
	fclose(output_file); 
	setting |= saved;
}






static void create_file(void) {
	int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { perror("write open directory"); getchar(); return; }
	int file = openat(dir, filename, 
		, 
		
	);
	if (file < 0) { perror("write openat file"); getchar(); return; }

	write(file, text, count);
	close(file);
	mode |= saved;
}






*/	







/*
static void rename_file(const char* new) {
	if (not access(new, F_OK)) { 
		printf("rename: file \"%s\" exists", new); 
		getchar(); return; 
	}
	if (*filename and rename(filename, new)) {
		printf("rename: error renaming to: \"%s\": \n", new);
		perror("rename"); 
		getchar(); 
		return;
	}
	strlcpy(filename, new, sizeof filename);
	printf("rename: named \"%s\"\n", filename); getchar();
}



static void move_path(const char* new) {
	strlcpy(directory, new, sizeof directory);
	printf("move: located at \"%s\"\n", directory); 
	getchar();
}

static void delete_filename(void) {
	strlcpy(filename, "", sizeof filename);
	printf("filename deleted.\n"); 
	getchar();
}

static void delete_filepath(void) {
	strlcpy(directory, "", sizeof directory);
	printf("filepath deleted.\n"); 
	getchar();
}



*/






/*

else if (cliplength > 7 and not strncmp(clipboard, "rename ", 7)) rename_file(clipboard + 7);
else if (cliplength > 5 and not strncmp(clipboard, "open ", 5)) load(clipboard + 5);
	else if (cliplength > 7 and not strncmp(clipboard, "rename ", 7)) rename_file(clipboard + 7);










































x	else if (c == 'a') {} // none" "\n"
"		x	else if (c == 'd') insert_now();" "\n"
"			else if (c == 'r') save_file();" "\n"
"			else if (c == 't') sendc();" "\n"
"			else if (c == 's') paste();" "\n"
"		x	else if (c == 'h') {}" "\n"
"			else if (c == 'm') copy();" "\n"
"			else if (c == 'c') insert_output(\"pbpaste\");" "\n"
"			else if (c == 'n') move_begin();" "\n"
"		x	else if (c == 'u') move_top();" "\n"
"			else if (c == 'p') { print_helpcommands_message(); print_help_message(); }" "\n"
"		x	else if (c == 'i') {} // none" "\n"
"			else if (c == 'e') move_end();" "\n"
"		x	else if (c == 'o') {}" "\n"
"		x	else if (c == 'l') move_bottom();" "\n"
"			else if (c == 'k') alternate();" "\n"
 "\n"
"			else if (c == 27) interpret_arrow_key();" "\n"
"		x	state = 0;" "\n"
"		} else {" "\n"
"			if (c == 'q') { if (mode & saved) mode &= ~active; }" "\n"
"		x	else if (c == 'a') state = 1;" "\n"
"			else if (c == 'd') delete(1);" "\n"
"			else if (c == 'r') cut();" "\n"
"		x	else if (c == 't') { mode |= inserting; c = 0; p1 = 0; } " "\n"
"		x	else if (c == 's') { anchor = cursor; mode ^= selecting; }" "\n"
"			else if (c == 'h') backwards();" "\n"
"			else if (c == 'm') forwards();" "\n"
"			else if (c == 'c') undo();" "\n"
"			else if (c == 'n') move_left();" "\n"
"			else if (c == 'u') move_word_left();" "\n"
"			else if (c == 'p') move_word_right();" "\n"
"		x	else if (c == 'i') state = 1;" "\n"
"			else if (c == 'e') move_up();" "\n"
"			else if (c == 'o') move_right();" "\n"
"			else if (c == 'l') move_down();" "\n"
"			else if (c == 'k') redo();" "\n"









	goes to












"			else if (c == 'r') save_file();" "\n"
"			else if (c == 't') sendc();" "\n"
"			else if (c == 's') paste();" "\n"
"			else if (c == 'm') copy();" "\n"
"			else if (c == 'c') insert_output(\"pbpaste\");" "\n"
"			else if (c == 'n') move_begin();" "\n"
"			else if (c == 'e') move_end();" "\n"
"			else if (c == 'k') alternate();" "\n"


"			else if (c == 'q') { if (mode & saved) mode &= ~active; }" "\n"
"			else if (c == 's') { anchor = cursor; mode ^= selecting; }" "\n"
"			else if (c == 'd') delete(1);" "\n"
"			else if (c == 'r') cut();" "\n"
"			else if (c == 'h') backwards();" "\n"
"			else if (c == 'm') forwards();" "\n"
"			else if (c == 'c') undo();" "\n"
"			else if (c == 'n') move_left();" "\n"
"			else if (c == 'u') move_word_left();" "\n"
"			else if (c == 'p') move_word_right();" "\n"
"			else if (c == 'e') move_up();" "\n"
"			else if (c == 'o') move_right();" "\n"
"			else if (c == 'l') move_down();" "\n"
"			else if (c == 'k') redo();" "\n"
"			else if (c == 27) interpret_arrow_key();" "\n"


"	else if (not strcmp(clipboard, 'discard and quit')) mode &= ~active;" "\n"
"	else if (not strcmp(clipboard, 'visual selections')) mode ^= visual_anchor;" "\n"
"	else if (cliplength > 7 and not strncmp(clipboard, 'insert ', 7)) insert_output(clipboard + 7);" "\n"
"	else if (cliplength > 7 and not strncmp(clipboard, 'change ', 7)) change_directory(clipboard + 7);" "\n"
"	else if (cliplength > 8 and not strncmp(clipboard, 'execute ', 8)) execute(clipboard + 8);" "\n"
"	else if (cliplength > 5 and not strncmp(clipboard, 'line ', 5)) jump_line(clipboard + 5);" "\n"

"	else { printf('unknown command: %%s\\n', clipboard); getchar(); }" "\n"











static inline void move_top(void) { cursor = 0; origin = 0; }
static inline void move_bottom(void) { while (cursor < count) move_right(); }







static void print_help_message(void) {
printf(
"	read(0, &c, 1);" "\n"
"	previous_cursor = cursor;" "\n"
"	if (mode & inserting) {" "\n"
"		if (c == 17) mode &= ~inserting;" "\n"
"		else if (c == 27) interpret_arrow_key();" "\n"
"		else if (c == 127) delete(1);" "\n"
"		else if (c == 'h' and p1 == 'r') { delete(1); mode &= ~inserting; c = 0; p1 = 0; }" "\n"
"		else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);" "\n"
"	} else {" "\n"
"		if (state == 1) {" "\n"
"			if (0) {}" "\n"
"		x	else if (c == 'a') {} // none" "\n"
"		x	else if (c == 'd') insert_now();" "\n"
"			else if (c == 'r') save_file();" "\n"
"			else if (c == 't') sendc();" "\n"
"			else if (c == 's') paste();" "\n"
"		x	else if (c == 'h') {}" "\n"
"			else if (c == 'm') copy();" "\n"
"			else if (c == 'c') insert_output(\"pbpaste\");" "\n"
"			else if (c == 'n') move_begin();" "\n"
"		x	else if (c == 'u') move_top();" "\n"
"			else if (c == 'p') { print_helpcommands_message(); print_help_message(); }" "\n"
"		x	else if (c == 'i') {} // none" "\n"
"			else if (c == 'e') move_end();" "\n"
"		x	else if (c == 'o') {}" "\n"
"		x	else if (c == 'l') move_bottom();" "\n"
"			else if (c == 'k') alternate();" "\n"
 "\n"
"			else if (c == 27) interpret_arrow_key();" "\n"
"		x	state = 0;" "\n"
"		} else {" "\n"
"			if (c == 'q') { if (mode & saved) mode &= ~active; }" "\n"
"		x	else if (c == 'a') state = 1;" "\n"
"			else if (c == 'd') delete(1);" "\n"
"			else if (c == 'r') cut();" "\n"
"		x	else if (c == 't') { mode |= inserting; c = 0; p1 = 0; } " "\n"
"		x	else if (c == 's') { anchor = cursor; mode ^= selecting; }" "\n"
"			else if (c == 'h') backwards();" "\n"
"			else if (c == 'm') forwards();" "\n"
"			else if (c == 'c') undo();" "\n"
"			else if (c == 'n') move_left();" "\n"
"			else if (c == 'u') move_word_left();" "\n"
"			else if (c == 'p') move_word_right();" "\n"
"		x	else if (c == 'i') state = 1;" "\n"
"			else if (c == 'e') move_up();" "\n"
"			else if (c == 'o') move_right();" "\n"
"			else if (c == 'l') move_down();" "\n"
"			else if (c == 'k') redo();" "\n"
 "\n"
"			else if (c == 27) interpret_arrow_key();" "\n"
"		}" "\n"
"		if (not (mode & selecting)) anchor = previous_cursor;" "\n"
 "\n"

);
getchar();

}

static void print_helpcommands_message(void) {
printf(
"x	if (not clipboard) return;" "\n"
"x	if (not strcmp(clipboard, '')) { printf('(empty-test)\\n'); getchar(); }" "\n"
"	else if (not strcmp(clipboard, 'discard and quit')) mode &= ~active;" "\n"
"x	else if (not strcmp(clipboard, 'print state')) { printf('%%lx:%%lu:%%lu:%%lu\\n', mode, count, cursor, anchor); getchar(); }" "\n"
"x	else if (not strcmp(clipboard, 'print read name')) { printf('%%s : %%s\\n', read_directory, read_filename); getchar(); }" "\n"
"x	else if (not strcmp(clipboard, 'print write name')) { printf('%%s : %%s\\n', write_directory, write_filename); getchar(); }" "\n"
"	else if (not strcmp(clipboard, 'visual selections')) mode ^= visual_anchor;" "\n"
"x	else if (not strcmp(clipboard, 'visual split points')) mode ^= visual_splitpoint;" "\n"
"x	else if (not strcmp(clipboard, 'read')) read_file();" "\n"
"x	else if (not strcmp(clipboard, 'save')) save_file();" "\n"
"x	else if (not strcmp(clipboard, 'create')) create_file();" "\n"
"x	else if (not strcmp(clipboard, 'new')) name_uniquely();" "\n"
"x	else if (not strcmp(clipboard, 'dt')) insert_now();" "\n"
"x	else if (not strcmp(clipboard, 'helpmodes')) print_help_message();" "\n"
"x	else if (not strcmp(clipboard, 'helpcommands')) print_helpcommands_message();" "\n"
"x	else if (not strcmp(clipboard, 'help')) { print_helpcommands_message(); print_help_message(); }" "\n"
"x	else if (cliplength > 5 and not strncmp(clipboard, 'name ',  5)) strlcpy(read_filename, clipboard + 5, sizeof read_filename); " "\n"
"x	else if (cliplength > 9 and not strncmp(clipboard, 'location ', 9)) strlcpy(read_directory, clipboard + 9, sizeof read_directory);" "\n"
"	else if (cliplength > 7 and not strncmp(clipboard, 'insert ', 7)) insert_output(clipboard + 7);" "\n"
"	else if (cliplength > 7 and not strncmp(clipboard, 'change ', 7)) change_directory(clipboard + 7);" "\n"
"	else if (cliplength > 8 and not strncmp(clipboard, 'execute ', 8)) execute(clipboard + 8);" "\n"
"	else if (cliplength > 5 and not strncmp(clipboard, 'line ', 5)) jump_line(clipboard + 5);" "\n"
"	else { printf('unknown command: %%s\\n', clipboard); getchar(); }" "\n"
"\n");
getchar();
}


else if (not strcmp(clipboard, "visual split points")) mode ^= visual_splitpoint;
	else if (not strcmp(clipboard, "read")) read_file();
	else if (not strcmp(clipboard, "save")) save_file();
	else if (not strcmp(clipboard, "create")) create_file();
	else if (not strcmp(clipboard, "new")) name_uniquely();
	else if (not strcmp(clipboard, "dt")) insert_now();
	else if (not strcmp(clipboard, "helpmodes")) print_help_message();
	else if (not strcmp(clipboard, "helpcommands")) print_helpcommands_message();
	else if (not strcmp(clipboard, "help")) { print_helpcommands_message(); print_help_message(); }
	else if (cliplength > 5 and not strncmp(clipboard, "name ",  5)) strlcpy(read_filename, clipboard + 5, sizeof read_filename); 
	else if (cliplength > 9 and not strncmp(clipboard, "location ", 9)) strlcpy(read_directory, clipboard + 9, sizeof read_directory);





else if (not strcmp(clipboard, "print state")) { printf("%lx:%lu:%lu:%lu\n", mode, count, cursor, anchor); getchar(); }
	else if (not strcmp(clipboard, "print read name")) { printf("%s : %s\n", read_directory, read_filename); getchar(); }
	else if (not strcmp(clipboard, "print write name")) { printf("%s : %s\n", write_directory, write_filename); getchar(); }
	if (not strcmp(clipboard, "")) { printf("(empty-test)\n"); getchar(); }


else if (not strcmp(clipboard, "visual selections")) mode ^= visual_anchor;

*/




































/*

	things i  want:

		- better key binds:    similar to sublime for selecting text and moving cursor. 


	x	- multiple clipboards..?........ hmmmm  or maybe we just use the global one lol. 
					idk 





	d	- default random/unique filename   for a new file.   rename it using the command-line.   don't reinvent something that works well.
												too risky to do that    for renaming files. 
													or just custom naming files in general.
	d	- don't support opening of files from withint the editor itself.   thats just not neccessary. literally just close that file and open another one. its not that hard lol!!!


		- remove some fancy features:

		x	- being able to change directory of the process?..'
		x	- being able to run a command?... ehh... idk.... maybeeeee.... hm........

						i might leave that one. idk. kinda tricky. 


		leave this one:    being able to paste the output of a command. thats useful. very. welll.... is it though? hm.... idk that it is... 

				we probably need this one for pbpaste,  so i leave it i guess. 


	
	x	- simplify displaying:       try to get rid of the anchor idea...?   ideallyyyyyyy.

		- always have visual split point enabled..  yeah. i think so. theres no reason to disable it. 

		 -      ideallyyyyyyyyyyyyyyy don't have an insert mode.      do everything in insert mode.   using   capital letters for commands.   i thinkkkk

					hm. yeah. lets try it. 

						ORRRR we use control characters. thats another way to do it.   probably capital chars though. 


		remove the top and bottom keybindings. thats just going to be done using a search.   



				also, rework the way that forward and backward will work:       make it even simpler/faster to use. 


								its going to be literally our main way of navigating. 


					
				
				FIX MOVE UP AND MOVE DOWN           PLZ


							WE JUST NEED TO FIX THESEEEE
									PLZ


					


		- make read file done unconditionally at the start of the program,   and called NO WHERE ELSE.   inline the function plz. 

		- SAME WITH CREATE:    inline the code to be at the start of the program, to create the new file (with a random-unique name, of course) if the user didnt supply any arguments to the call.   wait, should we actually not do this, and just have the save function detect if we havent saved it yet?
					WAIT! yes!!!   instead, we will just fall back on the create function, actually.   yeah.  so if write fails,  then we create.   yes.   lets do that.   i like that. 


		- make     cut()   do a     delete(1);       if the range is empty!!!   ???    or maybe not, idk.  actually no, i think we need both. 

		

		- make delete   ("127")      do a cut,   if the current selecting is nonempty. YESS
							ie, more like sublime.   also rebind the SHIFT-arrow keys  to be selecting, instead of move begin and end. i think. idk. hm. 

	







so i think the final command set is:






"			else if (c == 'r') save_file();" "\n"
"			else if (c == 't') sendc();" "\n"
"			else if (c == 's') paste();" "\n"
"			else if (c == 'm') copy();" "\n"
"			else if (c == 'c') insert_output(\"pbpaste\");" "\n"
"			else if (c == 'n') move_begin();" "\n"
"			else if (c == 'e') move_end();" "\n"
"			else if (c == 'k') alternate();" "\n"


"			else if (c == 'q') { if (mode & saved) mode &= ~active; }" "\n"
"			else if (c == 's') { anchor = cursor; mode ^= selecting; }" "\n"
"			else if (c == 'd') delete(1);" "\n"
"			else if (c == 'r') cut();" "\n"
"			else if (c == 'h') backwards();" "\n"
"			else if (c == 'm') forwards();" "\n"
"			else if (c == 'c') undo();" "\n"
"			else if (c == 'n') move_left();" "\n"
"			else if (c == 'u') move_word_left();" "\n"
"			else if (c == 'p') move_word_right();" "\n"
"			else if (c == 'e') move_up();" "\n"
"			else if (c == 'o') move_right();" "\n"
"			else if (c == 'l') move_down();" "\n"
"			else if (c == 'k') redo();" "\n"
"			else if (c == 27) interpret_arrow_key();" "\n"


"	else if (not strcmp(clipboard, 'discard and quit')) mode &= ~active;" "\n"
"	else if (not strcmp(clipboard, 'visual selections')) mode ^= visual_anchor;" "\n"
"	else if (cliplength > 7 and not strncmp(clipboard, 'insert ', 7)) insert_output(clipboard + 7);" "\n"
"	else if (cliplength > 7 and not strncmp(clipboard, 'change ', 7)) change_directory(clipboard + 7);" "\n"
"	else if (cliplength > 8 and not strncmp(clipboard, 'execute ', 8)) execute(clipboard + 8);" "\n"
"	else if (cliplength > 5 and not strncmp(clipboard, 'line ', 5)) jump_line(clipboard + 5);" "\n"

"	else { printf('unknown command: %%s\\n', clipboard); getchar(); }" "\n"




and then we have to rebind all of these,   and delete the   "inserting"   state bit.  but not the selecting bit, i think. 


       // make   visual_anchor     do the thing in display    mode & selecting     display
       //   make visual_selections  to the thing in display   mode & visual_anchor   ie, switch them, and make
       //                           the anchor visualization toggable.
 	




// 2306202.165852:  a screen based simpler editor based on the ed-like one, 
// uses a string ds, and is modal. 



//if ((mode & selecting) and i == cursor) length += (nat) snprintf(screen + length, screen_size - length, "\033[0m");
		//if ((mode & selecting) and i == anchor) length += (nat) snprintf(screen + length, screen_size - length, "\033[0m"); 
		//if ((mode & selecting) and i == begin) length += (nat) snprintf(screen + length, screen_size - length, "\033[7m");
		//if ((mode & selecting) and i == cursor) length += (nat) snprintf(screen + length, screen_size - length, "\033[1m");

*/




