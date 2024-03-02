#include <stdio.h>   // 202402191.234834: this version of the editor is trying to
#include <stdlib.h>  // make as simplest and robustful of minimalist screen based editor
#include <string.h>  // as possible, to make it not ever crash.
#include <iso646.h>  // it will probably also have an automatic saving and autosaving  system.
#include <unistd.h>
#include <fcntl.h>
#include <termios.h> // finished main implementation on 202403015.223257.
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
#include <signal.h>       //       alias pbpaste="xclip -selection clipboard -o" 
#include <stdnoreturn.h>  //       alias pbcopy="xclip -selection c"
typedef uint64_t nat;
struct action {
	nat parent, pre, post;
	uint32_t choice;
	bool inserting;
	char c;
	uint16_t _;
};
static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";
static const nat autosave_frequency = 100;
static bool moved = 0, selecting = 0;
static nat cursor = 0, count = 0, anchor = 0, origin = 0, finish = 0, head = 0, action_count = 0,
       desired = 0, cliplength = 0, screen_size = 0, autosave_counter = 0;
static char* text = NULL, * clipboard = NULL, * screen = NULL;
static struct action* actions = NULL;
static char filename[4096] = {0};
static char autosavename[4096] = {0};
static struct winsize window = {0};
static struct termios terminal = {0};
extern char** environ;

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

static void searchf(void) {
	nat t = 0;
	loop: if (t == cliplength or cursor >= count) return;
	if (text[cursor] != clipboard[t]) t = 0; else t++; 
	right(); goto loop;
}

static void searchb(void) {
	nat t = cliplength;
	loop: if (not t or not cursor) return;
	left(); t--; 
	if (text[cursor] != clipboard[t]) t = cliplength;
	goto loop;
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
	if (++autosave_counter >= autosave_frequency and should_record) save();
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
	write(1, "\033[?1049h", 8);
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

static void set_anchor(void) { if (selecting) return; anchor = cursor;  selecting = 1; }
static void clear_anchor(void) { if (not selecting) return; anchor = (nat) ~0; selecting = 0; }
static void paste(void) { if (selecting) cut(); insert_output("pbpaste"); }
static void local_paste(void) { for (nat i = 0; i < cliplength; i++) insert(clipboard[i], 1); }

static void interpret_arrow_key(void) {
	char c = 0; 
	read(0, &c, 1);
	     if (c == 'u') { clear_anchor(); up_begin(); }
	else if (c == 'd') { clear_anchor(); down_end(); }
	else if (c == 'l') { clear_anchor(); word_left(); }
	else if (c == 'r') { clear_anchor(); word_right(); }
	else if (c == 'f') { clear_anchor(); searchf(); }
	else if (c == 'b') { clear_anchor(); searchb(); }
	else if (c == 't') { clear_anchor(); for (int i = 0; i < window.ws_row - 1; i++) up(); }
	else if (c == 'e') { clear_anchor(); for (int i = 0; i < window.ws_row - 1; i++) down(); }
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
	terminal_copy.c_cc[VMIN] = 1; terminal_copy.c_cc[VTIME] = 0;
	terminal_copy.c_iflag &= ~((size_t) IXON);
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSAFLUSH, &terminal_copy);
	write(1, "\033[?1049h\033[?25l", 14);
loop:;	char c = 0;
	display(1);
	read(0, &c, 1);
	     if (c == 17) goto do_c;	// Q
	else if (c == 19) save();	// S
	else if (c == 18) redo(); 	// R
	else if (c == 4)  undo(); 	// D
	else if (c == 8)  copy(0); 	// H
	else if (c == 24) copy(1); 	// X
	else if (c == 1)  paste();	// A
	else if (c == 20) local_paste();// T
	else if (c == 27) interpret_arrow_key();
	else if (c == 127) { if (selecting) cut(); else if (cursor) delete(1); }
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) { if (selecting) cut(); insert(c, 1); }
	else { printf("error: ignoring input byte '%d'\n", c); fflush(stdout); getchar(); } 
	goto loop;
do_c:	if (not cliplength) goto loop;
	else if (not strcmp(clipboard, "exit")) goto done;
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






































/*
202403026.014938:
	just found a crashing bug, where the zero charcter was continually sent, while fullscreen, 
	and then when i resized the window with control-+   the editor crashed, segfaulting on 
		the screen*   buffer access. in the write call, in display. 

	so yeah. this still has a lot of bugtesting to go, i think. crap. 




*/







// --------------------------------------------------------------------------------------------------------------------------------------




//printf("screen_size = %llu, length = %llu\n", screen_size, length);
	//fflush(stdout); getchar();





//	struct sigaction default0 = {.sa_handler = SIG_DFL}, 
//			 action0 = {.sa_handler = window_resized},
//			 action1 = {.sa_handler = interrupted};
//	sigaction(SIGWINCH, &default0, NULL);
//	sigaction(SIGINT,   &default0, NULL);
//
//	sigaction(SIGWINCH, &action0, NULL);
//	sigaction(SIGINT,   &action1, NULL);



/*




		//printf("[parent has forked child with pid of %d]\n", (int) pid);
		//time_t t = 0;
    		//time(&t);
    		//printf("[parent is starting wait at %s]\n", ctime(&t));
		//time(&t);









//int status = 0;
		//do waitpid(pid, &status, WUNTRACED);
		//while (!WIFEXITED(status) && !WIFSIGNALED(status));




  if ((pid = fork()) < 0)
    perror("fork() error");

  else if (pid == 0) {
    time(&t);
    printf("child (pid %d) started at %s", (int) getpid(), ctime(&t));
    sleep(5);
    time(&t);
    printf("child exiting at %s", ctime(&t));
    exit(42);
  }


  else {
  
    time(&t);
    printf("parent is starting wait at %s", ctime(&t));
    if ((pid = wait(&status)) == -1)
         perror("wait() error");
    else {
	      time(&t);
	      printf("parent is done waiting at %s", ctime(&t));
	      printf("the pid of the process that ended was %d\n", (int) pid);
	      if (WIFEXITED(status))
	        printf("child exited with status of %d\n", WEXITSTATUS(status));
	      else if (WIFSIGNALED(status))
	        printf("child was terminated by signal %d\n",
	               WTERMSIG(status));
	      else if (WIFSTOPPED(status))
	        printf("child was stopped by signal %d\n", WSTOPSIG(status));
	      else puts("reason unknown for child termination");
    }
  }
}



*/




//last_child = i;

// printf("found the chosen child!\n");

//printf("child_count=%llu : last_child=%llu : "
			//	"node#%llu is the child of head=%llu.\n", 
			//	child_count, last_child, i, head
			//);
			//fflush(stdout); getchar();

	//	//printf("redo: error: could not find any children of head=%llu.\n", head);
	//	//fflush(stdout); getchar();
	//	return;
	//}

/*	
//	if (child_count == 1) {
//
//		//printf("single: last_child=node#%llu is the sole child of head=%llu.\n", last_child, head);
//		//fflush(stdout); getchar();
//
//		head = last_child;
//	}
*/
//printf("error: found %llu children of head=%llu!!! please pick one.\n", 
		//	child_count, head
		//);
		//fflush(stdout); getchar();




/*

// used for debugging the undo tree.

static void paste_undotree(void) {

	puts("undo tree is still a work in progress");
	fflush(stdout); getchar();

	char* string = NULL;
	size_t length = 0;
	char line[2048] = {0};
	for (nat i = 0; i < action_count; i++) {

		const size_t len = (size_t) snprintf(line, sizeof line, "node[%llu]:{^%llu,p@%llu,@p%llu,%s['%d']}\n", 
			i, 
			actions[i].parent,
			actions[i].pre,
			actions[i].post,
			actions[i].inserting ? "insert" : "delete",
			actions[i].c
		);

		string = realloc(string, length + len);
		memcpy(string + length, line, len);
		length += len;
	}
	const size_t len = (size_t) snprintf(line, sizeof line, "count=%llu,head=%llu\n", action_count, head);
	string = realloc(string, length + len);
	memcpy(string + length, line, len);
	length += len;

	for (nat i = 0; i < length; i++) insert(string[i], 0);
	free(string);
}





*/


// head = actions[head].children[actions[head].choice];

/*

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
}
*/






//	else if (c == 20) 	 	// T





/*
	//printf("\033[2J\033[H");
	//printf("searching for path string...\n");
	size_t env_index = 0;	
	for (; environ[env_index]; env_index++) {
		if (not strncmp(environ[env_index], "PATH=", 5)) {
			//printf("found path! \"%s\"...\n", environ[env_index]);
			break;
		}
	}

	char* path_string = strdup(environ[env_index] + 5);
	
	//printf("path strings: { \n");
	size_t pre_index = 0; 

	bool found = false;

	while (1) {
		if (not path_string[pre_index]) break;	
		//printf("\t\"");
		char dir[4096] = {0};
		size_t dir_count = 0;
		for (; path_string[pre_index] != ':'; pre_index++) {
			if (not path_string[pre_index]) break; 			
			//putchar(path_string[pre_index]);
			dir[dir_count++] = path_string[pre_index];
		}
	
		dir[dir_count++] = '/';
		for (int ib = 0; arguments[0][ib]; ib++) {
			dir[dir_count++] = arguments[0][ib];
		}
		if (not access(dir, F_OK) and not found) {
			free(arguments[0]);
			arguments[0] = strndup(dir, dir_count);
			//printf(" [\033[32m√\033[0m] " );
			found = true;		
		}

		//printf("\"\n");
		if (not path_string[pre_index]) break;
		pre_index++;
	}

	free(path_string);

	//printf("} \n");

	//printf("\nargv(argc=%lu) {\n", argument_count);
	//for (size_t a = 0; a < argument_count; a++) printf("\targv[%lu]: \"%s\"\n", a, arguments[a]);
	//printf("} \n");
*/



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// todo: IMPORTANT]:   make this function overwrite the previously existing file, if we did an autosave in this editing session.  ie, keep a global  autosave_filename,   which we set here, if empty, and we use, if nonempty. also, don't require that its {creat+exclusive}-access if its nonempty, and wasnt created here. cuz it won't work lol 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	
	
/*
	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
	char autosave_filename[4096] = {0};
	snprintf(autosave_filename, sizeof autosave_filename, "%s%s_%08x%08x.txt", autosave_directory, datetime, rand(), rand());

	int autosave_file = open(autosave_filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (autosave_file < 0) { perror("autosave: open autosave_file"); puts(autosave_filename); getchar(); }
	
	int file = open(filename, O_RDWR, 0);
	if (file < 0) { perror("autosave: open file"); puts(filename); getchar(); }

	if (fcopyfile(file, autosave_file, NULL, COPYFILE_ALL)) {
		perror("autosave: fcopyfile COPYFILE_ALL");
		getchar();
	}

	close(autosave_file);
	close(file);
*/







//	else if (c == 1) 	anchor = cursor;// A     // delete this one? ..... yeah.... 
//	else if (c == 19) 	searchb();	// S     // make this control-f 
//	else if (c == 23) 	searchf();	// W     // make this control-r
//	else if (c == 5) 	up();		// E     // delete these 4 ones.
//	else if (c == 12) 	down();		// L
//	else if (c == 4) 	left(); 	// D
//	else if (c == 18) 	right(); 	// R





//printf("\033[7meditor: wrote %llu bytes to file \"%s\"\033[0m\n", count, filename);
	//fflush(stdout);




/*
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

	           // note:   no need to do this. 

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
*/








/*


        i think the todo for the editor to replace sublime will be:


       x         - implment up/down and word movement                  // not strictly neccessary... technicallyyyyyyyyyy
       x         - bind all arrow key + modifiers to movement            // maybe we add move up and move down and arrow keys though?... hmmm.. idk...
        
                - make the autosave system

                - implement copy/paste fully

                - implement a simple undo-tree system!

                - implement the automatic saving system.   yay 





// printf("[[[i=%llu]]]", i); fflush(stdout); getchar();

	// printf("[[[x=%llu]]]", x); fflush(stdout); getchar();




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

*/






//
//   q d r w  
//    a s h t  
//       x     
//


//    how to use this editor:
//
//        CONTROL-Q      quit the editor.
//
//        CONTROL-D      move left
//
//        CONTROL-R      move right
//
//        CONTROL-A      set anchor
//
//        CONTROL-S

// temporary:
//
//        CONTROL-N      save file      (temporary, this will be automatic soon.)
//
// obvious:
//
//        (character)    insert one char
//
//        (backspace)    delete one char
//











/*











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
	clipboard = realloc(clipboard, cliplength + 1);
	if (anchor < cursor) at_string(anchor, cliplength, clipboard);
	else at_string(cursor, cliplength, clipboard);
	clipboard[cliplength] = 0;
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












//	down impl

	// step 0. move to the beginning of this line.
	// step 1. compute the current line length, in terminal characters. call it x.
	// step 2. move to the next line's beginning.
	// step 3. move x characters over to the right.

//	up impl

	// step 0. move to the beginning of this line.
	// step 1. compute the current line length, in terminal characters. call it x.
	// step 2. move to the previous line's beginning.
	// step 3. move x characters over to the right.




	
// if (moved) { desired = x; moved = false; }


// static nat desired = 0;






	we will only support insert(1), delete(1), left(1), and right(1) only. 
	everything else is built around those. 

	no "visually-complex" characters will be used, no tabs, no unicode.
	and canonical mode screen updating is done in an interesting way maybe

*/







//	     if (c == 1/*A*/) sendc();
//	else if (c == 2/*B*/) {} 
//	// C
//	else if (c == 4/*D*/) sendc();
//	else if (c == 5/*E*/) {}
//	else if (c == 6/*F*/) {}
//	else if (c == 7/*G*/) {}
//	else if (c == 8/*H*/) copy();
//	// I
//	// J
//	else if (c == 11/*K*/) {}
//	else if (c == 12/*L*/) {}
//	// M
//	else if (c == 14/*N*/) {}
//	// O
//	else if (c == 16/*P*/) {/* redo(); */}
//	else if (c == 17/*Q*/) {}
//	else if (c == 18/*R*/) paste();
//	else if (c == 19/*S*/) {}
//	else if (c == 20/*T*/) {}
//	else if (c == 21/*U*/) undo();
//	// V
//	else if (c == 23/*W*/) {}
//	else if (c == 24/*X*/) { copy(); cut(); }
//	// Y
//      // Z
//
//   C, I, J, M, O, Q, S, V, Y, Z   are all unavailable.
//




/*





  //char* tofind = strndup(text + anchor, cursor - anchor);

//write(1, text, count);


if (cursor) { delete(); write(1, "\b\b\b   \b\b\b", 9); } else write(1, "\b\b  \b\b", 6);










	ioctl(0, TIOCGWINSZ, &window);
	const nat new_size = 9 + 32 + window.ws_row * (window.ws_col + 5) * 4;
	if (new_size != screen_size) { screen = realloc(screen, new_size); screen_size = new_size; display_mode = 0; }

	memcpy(screen, "\033[?25l\033[H", 9);
	nat length = 9;
	nat row = 0, column = 0;
	off_t i = origin;










	else if (c == 1)  {
		printf("\033[7m%llu\033[0m\n", cursor);
		anchor = cursor; 	//A  -   anchor()
	}

	else if (c == 4)  { 			//D  -   find()

		if (anchor > cursor) { puts("\033[7merror\033[0m"); goto loop; }

		char* tofind = strndup(text + anchor, cursor - anchor);
		const nat tofind_count = cursor - anchor;
		for (nat i = 0; i < tofind_count; i++) delete();

		bool looped = false;

		again:;

		nat t = 0;
		for (nat i = anchor; i < count; i++) {
			if (t == tofind_count) {
				printf("\033[7m%llu:%llu\033[0m\n", i - tofind_count, i);
				selection_count = tofind_count;
				selection_begin = i; 

				cursor = i; anchor = i;

				const nat begin = (int64_t)i - (int64_t)tofind_count - (int64_t)view_size < (int64_t)0 ? 0 : i - view_size;
				const nat end = i + view_size >= count ? count : i + view_size;
				
				write(1, text + begin, (i - tofind_count) - begin);
				//write(1, "\033[7m", 4);
				write(1, text + (i - tofind_count), tofind_count);

				////write(1, "/\033[0m", 5);
				//write(1, text + i, end - i);

				goto loop;
			}

			if (text[i] == tofind[t]) t++; else t = 0;
		}
		anchor = 0;
		if (looped) puts("\033[7m?\033[0m"); else { looped = true; goto again; }
		free(tofind);
	}

	else if (c == 18) { 			//R  -   replace()

		if (anchor > cursor) { puts("\033[7merror\033[0m"); goto loop; }

		char* toreplace = strndup(text + anchor, cursor - anchor);
		const nat toreplace_count = cursor - anchor;
		for (nat i = 0; i < toreplace_count; i++) delete();

		cursor = selection_begin;
		for (nat i = 0; i < selection_count; i++) delete();    // delete the current selection from a find call.
		
		// insert a series of characters equal to the current string. 

		for (nat i = 0; i < toreplace_count; i++) {
			insert(toreplace[i]);
		}

		printf("\033[7m%llu: -%llu +%llu\033[0m\n", selection_begin, selection_count, toreplace_count);
		selection_count = 0;
		free(toreplace);
	}


*/






// add backwards searching,   control-h   i think 










// todo:

//	goto done;  // quit editor

//	write(1, text + origin, display_count);
	







/*

	the basic way of text editing, is going to be to 


		insert a bunch of text, just by typing. 

			you can't backspace on a line though. which is kind of rough. but we'll figure that out later. 
					thats a small problem honestly 


	

		setting the current cursor position, to be the anchor.   ie, anchoring 



		getting the text in between the anchor and cursor, and saving it in a string,    also called the current selection. 

		getting the text the current selection, and storing it in a seperate string, 


		doing a find and replace, which searches for the first occurence of that first string, and replaces it with the second one. 
		this can be repeated with the same strings, without having to set them again and again. 

			note, that the text around the selection is actually printed out, (a bunch of characters before, and after) when we do this operation.



		i think i want to make it so that when you set the first string, the text is printed. ie, searching for strings is also the way you navigate the cursor, and visualize the document. pretty neat. 




so yeah, the typical usage would be 




	<anchor> type some text into the document <cut_and_obtain_selection_for_string1> 


								which also deletes what you had just typed too. anchor is unchanged. 
								but cursor = anchor. 

then 


	type some text to replace that string <cut_and_obtain_selection_for_string2>
							which would delete the string that was found in the document matching string1, 
							and will insert string2 in its place. so yeah. pretty simple. 


note, when you actually type the keyboard shortcut for <cut_and_obtain_selection_for_string1>
the display would have printed out a ton of text surrounding the thing you are searching for.
yeah, i think i want the <find_command> to be inreplace of <cut_and_obtain_selection_for_string1> 
it does the same thing, but it also updates the screen right when you do it, i think. for immediate feedback. that would work. 


	





note 

i think i want the string-search to have modulo behavior...     but its also like more effort to implement it like that lololol 
	hmm 
	idk 
				ill think about it 

						i defintiely don't want two different keybinds for forwards and backwards, i feel like thats just kinda wasteful 

					we only have so many keybinds that we can actually use with this thing lol 

						i want to keep it to like                AT MAX         5 key binds. 


								thats like if we do things really messy,      we'll be at 5 keybinds 


										ideallyyyyy 3 


								or 2 



									but probably 3 






	so yeah 



so we need at least one 



	for the anchor... and for string finding, and for replaceing... hmmm...



so here are the commands so far:  


	(in addition to ESC for quitting the editor, backspace for deleting characters, and other characters for inserting text, lol)









	control-A		set anchor 




	control-D		find-string




	control-R		replace-string 

















*/






























		                // note:  we don't update the display each key press. 
				// for the most part, this is handled by the terminal's ECHO mode. 
				// however, for scrolling the file, and navigation, we update the screen ourselves, 
				// after the user presses the associated key! very efficient! yay.  impervious to display bugs lol. 



















//	     if (c == 1/*A*/) sendc();
//	else if (c == 2/*B*/) {} 
//	// C
//	else if (c == 4/*D*/) sendc();
//	else if (c == 5/*E*/) {}
//	else if (c == 6/*F*/) {}
//	else if (c == 7/*G*/) {}
//	else if (c == 8/*H*/) copy();
//	// I
//	// J
//	else if (c == 11/*K*/) {}
//	else if (c == 12/*L*/) {}
//	// M
//	else if (c == 14/*N*/) {}
//	// O
//	else if (c == 16/*P*/) {/* redo(); */}
//	// Q
//	else if (c == 18/*R*/) paste();
//	// S
//	else if (c == 20/*T*/) {}
//	else if (c == 21/*U*/) undo();
//	// V
//	else if (c == 23/*W*/) {}
//	else if (c == 24/*X*/) { copy(); cut(); }
//	// Y
//	// Z
//	
//	//     d r
//	//   a   h
	//      x     

	//     u p 
	//
	//   


/*
	{ copy(); cut(); } // X
	copy(); // H
	paste(); // R

	sendc(); // D
	sendc(); // A

	undo(); // U
	redo(); // P	

*/



//	else if (c == 27) interpret_arrow_key();
//	else if (c == 127) delete_cut();
//
//	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert_replace(&c, 1);
//


/*



struct action {
	off_t parent;
	off_t pre;
	off_t post;
	off_t length;
};



// char filename[4096] = {0};

	//int flags = O_RDONLY;
	//mode_t permission = 0;

		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;




static void save() {            // simplify / collapse this function.

	int flags = O_WRONLY;
	mode_t permission = 0;

	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	} else strlcpy(filename, argv[1], sizeof filename);

	int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { perror("read open directory"); exit(1); }
	int df = openat(dir, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = openat(dir, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }

	write(file, text, count);
	close(file);
	close(dir);

}

// have a create_new() function,   a  load_existing()     and save()     function.    then do a save() on quit.




*/












/////////////////////    /////////////////////     /////////////////////    /////////////////////     ///////////////////// 

// how do we display the text?  does backspace work properly?   how do we display where we are in the document?....
// do we use a page size like the pager?     lets implement find-forwards next. thats important.
//   what keybind?  

/////////////////////    /////////////////////     /////////////////////    /////////////////////     ///////////////////// 




/*











if (c == 1) {
		if (not length) {
			char* page = malloc(page_size);			
			ssize_t n = read(file, page, page_size);
			if (n < 0) write(1, "read error\n", 11);
			else { 
				lseek(file, -n, SEEK_CUR);
				write(1, page, (size_t) n); 
				write(1, "\033[7m/\033[0m", 9);
			} free(page); goto loop;
		}
		string[--length] = 0;
		size_t i = 0;
		search:; char c = 0;
		if (not read(file, &c, 1)) { write(1, "?\n", 2); selection = 0; goto clear; } 
		if (c == string[i]) i++; else i = 0; 
		if (i != length) goto search;
		selection = length;
		printf("c%lld s%llu\n", lseek(file, 0, SEEK_CUR), selection); 
		clear: memset(string, 0, sizeof string); 
		length = 0; goto loop;

	} 















	okay, so i think we kinda have a problem lol. 

	so with the previous ed like editor,   we could use the current input buffer as the mechanism for getting a string that we want to search for. 

		not so with this version of the text editor. 

				all characters that we type that are text, are immediately inserted, actually.  so yeahhhh


						and the only mechanism we have of deleting text, is the backspace key, i think. 


		wellll, actuall, 


						i still think we are going to provide a way of doing cutting actaully        of the current selection, only, though. 



			and actually, i feel like i want to make the search        delete it too?   no 



		bad idea 


									i don't know how to implement the search bar, basically. thats the problem. 

						its not going to be a seperate search bar-     its going to be within the document itself, 



				its just, then that means, in order to do that, 





						we need to have some way of cutting the text that the user literally typed into the document just to be the text was searched for 


							so like 


	the document looks like this 




						hello there from space
						
						hello



the user typed hello in order to search for the "hello" string, 

	

					and we have to delete it some how, 


						and         we have to have marked the spot in the text where the selection begins. thats important too!

								so yeah. i think we need at least three things-



									1. a cut command, which deletes the current selection   (start + length)


									2. a search command, which finds a given string, starting from start, of length  cursor - start

									3.  a set-start command, which drops an anchor at the current cursor position.  ie, 
												start = cursor;



	this way the typical usage would be 



				<set-start>  hello 




	OH WAIT 

			we can just use a clipboard. 


					OH wow thats actually so good. lets just do that. 


			 a clipboard     


						thats the solution. 



								ie, when you type some text, 



							


		wait


	okay

	so 

	heres the sequence


							<set-start> hello <cut> <search>



		thats the sequence 



						when you cut it ends up on your clipboard, 



							and search just picks up on the clipboard. 




								




			so yeah, literally identical to the current editor, kinda



						except, 


			wait, what if when you backspaced, it ended up on the thingy 



					so we didnt even need to have a seperate cut command!?!?!


						like, when you backspaced, character by character, 



						it always put it into the current buffer, 

				i guess it has to do it backwards LOL       but thats fine



								but yeah, 










		i think regardless       we definitely need to have a        <set-start>       key binding 



							probably control-A        if i had to guess.  idk. 




						


			and then, maybe control-T for searching. that would work. 




			



		wait, so why do we even need to do control-A?



							ohhh, i guess to clear the buffer, actually!




								yeah, we are going to actaully buffer,    at all times, what the user deleted. i think. 


									and that backwards-clipboard is used       is used for doing searching. 




			so the usage is, 



											control-A    (which clears the clipboard)

		wait, no, we should just make it so that control-A starts   or enables the recording of data to the clipboard. we shouldnt do it always. so yeah. 



				okay, that couldddd work


									 








not quite happy with this... hm... 

	i feel like there HAS to be a simpler solution. 



like, literally the rest of this editor is so incredibly simple, and perfect so far 



















// write(1, "\033[7m/\033[0m", 9);



















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








#include <stdio.h>     // revised editor, based on less, more, and ed. 
#include <stdlib.h>    // only uses three commands, find-select, replace-selection, and set-numeric
#include <unistd.h>    // written by dwrr on 202312177.223938
#include <string.h>
#include <iso646.h>
#include <fcntl.h>
#include <termios.h>

int main(int argc, const char** argv) {
	typedef uint64_t nat;
	if (argc > 2) exit(puts("usage ./editor <file>"));

	int file = open(argv[1], O_RDWR, 0);
	if (file < 0) { 
		perror("read open file"); 
		exit(1); 
	}

	struct termios original = {0};
	tcgetattr(0, &original);
	struct termios terminal = original; 
	terminal.c_cc[VKILL]   = _POSIX_VDISABLE;
	terminal.c_cc[VWERASE] = _POSIX_VDISABLE;
	tcsetattr(0, TCSAFLUSH, &terminal);

	nat page_size = 1024, length = 0, selection = 0;
	char string[4096] = {0};
		
	loop:; char line[4096] = {0};
	read(0, line, sizeof line);

	if (not strcmp(line, "n\n")) {
		if (not length) { 
			printf("c%lld s%llu p%llu\n", lseek(file, 0, SEEK_CUR), selection, page_size); 
			goto loop; 
		}

		string[--length] = 0;
		char c = *string;
		const nat n = strtoull(string + 1, NULL, 10); 

		if (c == 'q') goto done;
		else if (c == 'o') { for (int i = 0; i < 100; i++) puts(""); puts("\033[1;1H"); }
		else if (c == 'p') page_size = n;
		else if (c == 's') selection = n;
		else if (c == 'l') lseek(file, (off_t) n, SEEK_SET);
		else if (c == '+') lseek(file, (off_t) n, SEEK_CUR);
		else if (c == '-') lseek(file, (off_t)-n, SEEK_CUR);
		else puts("?");
		goto clear;
	
	} else if (not strcmp(line, "u\n")) {

		// todo: make a backup of the file, after every single edit. 

		if (length) string[--length] = 0;
		const off_t cursor = lseek(file, (off_t) -selection, SEEK_CUR);
		const off_t count = lseek(file, 0, SEEK_END);
		const nat rest = (nat) (count - cursor - (off_t) selection);
		char* buffer = malloc(length + rest);
		memcpy(buffer, string, length);
		lseek(file, cursor + (off_t) selection, SEEK_SET);
		read(file, buffer + length, rest);
		lseek(file, cursor, SEEK_SET);
		write(file, buffer, length + rest);
		lseek(file, cursor, SEEK_SET);
		ftruncate(file, count + (off_t) length - (off_t) selection);
		fsync(file); free(buffer);
		printf("+%llu -%llu\n", length, selection);
		goto clear;
 	
	} else if (not strcmp(line, "t\n")) {
		if (not length) {
			char* page = malloc(page_size);			
			ssize_t n = read(file, page, page_size);
			if (n < 0) write(1, "read error\n", 11);
			else { 
				lseek(file, -n, SEEK_CUR);
				write(1, page, (size_t) n); 
				write(1, "\033[7m/\033[0m", 9);
			} free(page); goto loop;
		}
		string[--length] = 0;
		size_t i = 0;
		search:; char c = 0;
		if (not read(file, &c, 1)) { write(1, "?\n", 2); selection = 0; goto clear; } 
		if (c == string[i]) i++; else i = 0; 
		if (i != length) goto search;
		selection = length;
		printf("c%lld s%llu\n", lseek(file, 0, SEEK_CUR), selection); 
		clear: memset(string, 0, sizeof string); 
		length = 0; goto loop;
	} else length = strlcat(string, line, sizeof string);
	goto loop; done: close(file);
}







*/


