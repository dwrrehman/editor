#include <stdio.h>   // written 202410266.140132 by dwrr
#include <stdlib.h>  // 1202411041.210238 an editor that
#include <string.h>  // supposed to be a simpler version of
#include <iso646.h>  // the modal editor that we wrote.
#include <unistd.h>  // finished on 1202411052.131234
#include <stdbool.h>
#include <stdnoreturn.h>
#include <fcntl.h>
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
extern char** environ;

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
			if (row >= window.ws_row) break;
		} else if (c == 9) {
			nat amount = 8 - column % 8;
			column += amount;
		} else if ((unsigned char) c >= 32) { column++; } else { column += 4; }
		if (column >= window.ws_col - 2) goto print_newline;
	} 
	if (i == cursor and i == count) return true; return false;
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
	else if (c == 'n') insert("0123456789", 10, 1);
	else if (c == 'p') insert("`~!@#$%^&*(){}[]<>|\\+=_-:;?/.,'\"", 32, 1);
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
	snprintf(message, sizeof message, 
		"error: %s: %s\n", 
		call, strerror(errno)
	);
	insert(message, strlen(message), 1);
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

static char** parse_arguments(const char delimiter, const char* string, const size_t length) {
	char** arguments = NULL;
	size_t argument_count = 0;
	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (string[index] != delimiter) {
			if (not argument_length) start = index;
			argument_length++;
		} else {
		push:	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
			arguments[argument_count++] = strndup(string + start, argument_length);
			start = index;
			argument_length = 0; 
		}
	}
	if (argument_length) goto push;
	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
	arguments[argument_count] = NULL;
	return arguments;
}

static char* path_lookup(const char* executable_name) {
	const nat executable_length = (nat) strlen(executable_name);
	if (not executable_length) return NULL;
	const char* path = getenv("PATH");
	const nat length = (nat) strlen(path);
	size_t start = 0, prefix_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (path[index] != ':') {
			if (not prefix_length) start = index;
			prefix_length++;
		} else {
		push:;	char* file = calloc(prefix_length + 1 + executable_length + 1, 1);
			memcpy(file, path + start, prefix_length);
			file[prefix_length] = '/';
			memcpy(file + prefix_length + 1, executable_name, executable_length);
			if (not access(file, X_OK)) return file; else free(file);
			start = index; prefix_length = 0; 
		}
	}
	if (prefix_length) goto push;
	return NULL;
}

static int job_fdo[2] = {0};
static int job_rfd[2] = {0};
static int job_fdm[2] = {0};
static int job_status = 0;
static pid_t pid = 0;

static void read_output(void) {
	if (not job_status) return;
	char buffer[65536] = {0};
	if (poll(&(struct pollfd){ .fd = job_fdo[0], .events = POLLIN }, 1, 0) == 1) {
		ssize_t nbytes = read(job_fdo[0], buffer, sizeof buffer);
		if (nbytes < 0) insert_error("read"); else insert(buffer, (nat) nbytes, 1);
	}
	if (poll(&(struct pollfd){ .fd = job_fdm[0], .events = POLLIN }, 1, 0) == 1) {
		ssize_t nbytes = read(job_fdm[0], buffer, sizeof buffer);
		if (nbytes < 0) insert_error("read"); else insert(buffer, (nat) nbytes, 1);
	} 
}

static void finish_job(void) {
	if (not job_status) return;
	int s = 0;
	int r = waitpid(pid, &s, WNOHANG);
	if (r == -1) insert_error("wait");
	else if (r == 0) return;
	char dt[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
	char message[4096] = {0};
	if (WIFEXITED(s)) 
		snprintf(message, sizeof message, 
			"[%s:(%d) exited with code %d]\n", 
			dt, pid, WEXITSTATUS(s)
		);
	else if (WIFSIGNALED(s)) 
		snprintf(message, sizeof message, 
			"[%s:(%d) was terminated by signal %s]\n", 
			dt, pid, strsignal(WTERMSIG(s))
		);
	else if (WIFSTOPPED(s)) 
		snprintf(message, sizeof message, 
			"[%s:(%d) was stopped by signal %s]\n", 	
			dt, pid, strsignal(WSTOPSIG(s))
		);
	else snprintf(message, sizeof message, 
		"[%s:(%d) terminated for an unknown reason]\n", 
		dt, pid
	);
	insert(message, strlen(message), 1);
	job_status = 0;
}

static void start_job(const char* input) {
	if (job_status) { insert("\njob running\n", 13, 1); return; } 
	if (writable) save();
	const nat length = (nat) strlen(input);
	if (not length) return;

	char** arguments = parse_arguments(*input, input + 1, length - 1);
	if (arguments[0][0] != '.' and arguments[0][0] != '/') {
		char* path = path_lookup(arguments[0]);
		if (not path) { errno = ENOENT; insert_error("pathlookup"); return; }
		free(arguments[0]);
		arguments[0] = path;
	}
	pipe(job_fdo);
	pipe(job_rfd);
	pipe(job_fdm);
	pid = fork();
	if (pid < 0) { insert_error("fork"); } 
	else if (not pid) {
		close(job_fdo[0]);  dup2(job_fdo[1], 1);
		close(job_fdo[1]);
		close(job_fdm[0]);  dup2(job_fdm[1], 2);
		close(job_fdm[1]);
		close(job_rfd[1]);  dup2(job_rfd[0], 0);
		close(job_rfd[0]);
		execve(arguments[0], arguments, environ);
		insert_error("execve");
		exit(1);
	} else {
		close(job_fdo[1]);
		close(job_fdm[1]);
		close(job_rfd[0]);
		job_status = 1;
		char dt[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
		char message[4096] = {0};
		snprintf(message, sizeof message, "[%s: pid %d running]\n", dt, pid);
		insert(message, strlen(message), 1);
	}
}

static void open_file(const char* argument) {
	if (writable) save();
	if (not strlen(argument)) { 
		free(text); text = NULL; 
		count = 0; goto new; 
	} 
	int df = open(argument, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }

	int file = open(argument, O_RDONLY);
	if (file < 0) {
		read_error: insert_error("open");
		return;
	}
	struct stat ss; fstat(file, &ss);
	count = (nat) ss.st_size;
	free(text); text = malloc(count);
	read(file, text, count); 
	close(file);
new: 	cursor = 0; origin = 0;
	anchor = (nat) -1; writable = 1;
	strlcpy(filename, argument, sizeof filename);
	for (nat i = 0; i < action_count; i++) free(actions[i].string);
	free(actions); actions = NULL;
	head = 0; action_count = 0;
	finish_action((struct action) {0}, NULL, (int64_t) 0);
	struct stat attr_;
	stat(filename, &attr_);
	strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attr_.st_mtime));
}

static void window_resized(int _) { if(_){} ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	if (writable) save(); exit(0); 
}

int main(int argc, const char** argv) {
	signal(SIGPIPE, SIG_IGN);
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
	read_output();  finish_job();  display();
	char c = 0;
	ssize_t n = read(0, &c, 1); 
	c = remap(c);
	if (n <= 0) { 
		perror("read"); 
		fflush(stderr);
		usleep(100000);
	}
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
			t++; if (t == target_length) { cursor++; goto found; }
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
		else if (c == 'g' and writable) insert_output("pbpaste");
		else if (c == 'c') local_copy();
		else if (c == 'f') copy_global();
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
	char* s = clipboard;
	const nat len = cliplength;
	if (not s) goto loop;
	if (not strcmp(s, "exit")) goto done;
	else if (not strncmp(s, "open ", 5)) open_file(s + 5); 
	else if (not strcmp(s, "read")) read_output();
	else if (not strcmp(s, "wait")) finish_job();
	else if (not strncmp(s, "do", 2)) start_job(s + 2);
	else if (not strncmp(s, "cd ", 3)) { if (chdir(s + 3) < 0) insert_error("chdir"); } 
	else if (not strncmp(s, "signal ", 7)) { if (kill(pid, atoi(s + 3)) < 0) insert_error("kill"); }
	else if (not strcmp(s, "close")) { if (not job_status or close(job_rfd[1]) < 0) insert_error("close"); }
	else if (not strncmp(s, "write ", 6)) { if (not job_status or write(job_rfd[1], s + 6, len - 6) <= 0) insert_error("write"); }
	else insert("\ncommand not found\n", 19, 1);
	goto loop;
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	if (writable) save(); exit(0);
}


























/*static char* get_selection(void) {
	if (anchor > count) return NULL;
	if (anchor > cursor) { nat t = anchor; anchor = cursor; cursor = t; }
	char* selection = strndup(text + anchor, cursor - anchor);
	return selection;
}
*/







/*
hellot = 0;
warning: include location '/usr/local/include' is unsafe for cross-compilation [-Wpoison-system-directories]
1 warning generated.
[1202411111.194337:(86830) exited with code 0]

do ./build
warning: include location '/usr/local/include' is unsafe for cross-compilation [-Wpoison-system-directories]
c.c:20:1: warning: type specifier missing, defaults to 'int' [-Wimplicit-int]
hellot = 0;
^
c.c:20:1: warning: no previous extern declaration for non-static variable 'hellot' [-Wmissing-variable-declarations]
note: declare 'static' if the variable is not intended to be used outside of this translation unit
c.c:22:1: error: expected identifier or '('
do ./build
^
fatal error: too many errors emitted, stopping now [-ferror-limit=]
3 warnings and 2 errors generated.
[1202411111.194301:(86792) exited with code 1]











     NAME            Default Action          Description
     SIGHUP          terminate process       terminal line hangup
     SIGINT          terminate process       interrupt program
     SIGQUIT         create core image       quit program
     SIGILL          create core image       illegal instruction
     SIGTRAP         create core image       trace trap
     SIGABRT         create core image       abort(3) call (formerly SIGIOT)
     SIGEMT          create core image       emulate instruction executed
     SIGFPE          create core image       floating-point exception
     SIGKILL         terminate process       kill program
     SIGBUS          create core image       bus error
     SIGSEGV         create core image       segmentation violation
     SIGSYS          create core image       non-existent system call invoked
     SIGPIPE         terminate process       write on a pipe with no reader
     SIGALRM         terminate process       real-time timer expired
     SIGTERM         terminate process       software termination signal
     SIGURG          discard signal          urgent condition present on socket
     SIGSTOP         stop process            stop (cannot be caught or ignored)
     SIGTSTP         stop process            stop signal generated from keyboard
     SIGCONT         discard signal          continue after stop
     SIGCHLD         discard signal          child status has changed
     SIGTTIN         stop process            background read attempted from control terminal
     SIGTTOU         stop process            background write attempted to control terminal
     SIGIO           discard signal          I/O is possible on a descriptor (see fcntl(2))
     SIGXCPU         terminate process       cpu time limit exceeded (see setrlimit(2))
     SIGXFSZ         terminate process       file size limit exceeded (see setrlimit(2))
     SIGVTALRM       terminate process       virtual time alarm (see setitimer(2))
     SIGPROF         terminate process       profiling timer alarm (see setitimer(2))
     SIGWINCH        discard signal          Window size change
     SIGINFO         discard signal          status request from keyboard
     SIGUSR1         terminate process       User defined signal 1
     SIGUSR2         terminate process       User defined signal 2

       SIGHUP           1           1       1       1
       SIGINT           2           2       2       2
       SIGQUIT          3           3       3       3
       SIGILL           4           4       4       4
       SIGTRAP          5           5       5       5
       SIGABRT          6           6       6       6
       SIGIOT           6           6       6       6
       SIGBUS           7          10      10      10
       SIGEMT           -           7       7      -
       SIGFPE           8           8       8       8
       SIGKILL          9           9       9       9
       SIGUSR1         10          30      16      16
       SIGSEGV         11          11      11      11
       SIGUSR2         12          31      17      17
       SIGPIPE         13          13      13      13
       SIGALRM         14          14      14      14
       SIGTERM         15          15      15      15
       SIGSTKFLT       16          -       -        7
       SIGCHLD         17          20      18      18
       SIGCLD           -          -       18      -
       SIGCONT         18          19      25      26
       SIGSTOP         19          17      23      24
       SIGTSTP         20          18      24      25
       SIGTTIN         21          21      26      27
       SIGTTOU         22          22      27      28
       SIGURG          23          16      21      29
       SIGXCPU         24          24      30      12
       SIGXFSZ         25          25      31      30
       SIGVTALRM       26          26      28      20
       SIGPROF         27          27      29      21
       SIGWINCH        28          28      20      23
       SIGIO           29          23      22      22
       SIGPOLL                                            Same as SIGIO
       SIGPWR          30         29/-     19      19
       SIGINFO          -         29/-     -       -
       SIGLOST          -         -/29     -       -
       SIGSYS          31          12      12      31
       SIGUNUSED       31          -       -       31










*/





