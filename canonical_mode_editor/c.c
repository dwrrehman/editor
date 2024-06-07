#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <iso646.h>
#include <stdio.h>   
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

extern char** environ;
typedef uint64_t nat;
static void interrupted(int _) { if(_){} }

struct action {
	nat parent;
	nat pre;
	nat post;
	nat choice;
	int64_t length;
	char* string;
};

static nat cursor = 0, anchor = 0, count = 0, head = 0, action_count = 0;
static struct action* actions = NULL;
static char* text = NULL;

static nat page_size = 1024;


static char filename[4096] = {0};

extern char** environ;

static void finish_action(struct action node, char* string, nat length) {
	node.choice = 0;
	node.parent = head;
	node.post = cursor; 
	node.string = string;
	node.length = (int64_t) length;
	head = action_count;
	actions = realloc(actions, sizeof(struct action) * (action_count + 1));
	actions[action_count++] = node;
}

/*

if (should_record) autosave_counter++;
if (autosave_counter >= autosave_frequency and should_record) save();

*/

static void insert(char* string, nat length, bool should_record) {
	struct action node = { .pre = cursor };
	text = realloc(text, count + length);
	memmove(text + cursor + length, text + cursor, count - cursor);
	memcpy(text + cursor, string, length);
	count += length; 
	cursor += length;
	if (should_record) finish_action(node, string, length);
}

static void delete(nat length, bool should_record) {
	if (cursor < length) return;
	struct action node = { .pre = cursor };
	cursor -= length;
	count -= length; 
	char* string = strndup(text + cursor, length);
	memmove(text + cursor, text + cursor + length, count - cursor);
	text = realloc(text, count);
	if (should_record) finish_action(node, string, length);
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
		printf("note: choice = %llu, however there are "
			"%llu possible histories to choose.\n", 
			actions[head].choice, child_count
		); 
		actions[head].choice = (actions[head].choice + 1) % child_count;
	}
	head = chosen_child;
	const struct action node = actions[head];
	cursor = node.pre; 
	if (node.length > 0) insert(node.string, (nat) node.length, 0); else delete((nat) -node.length, 0);
	cursor = node.post;
}

static void undo(void) {
	if (not head) return;
	struct action node = actions[head];
	cursor = node.post;
	if (node.length > 0) delete((nat) node.length, 0); else insert(node.string, (nat) -node.length, 0); 
	cursor = node.pre;
	head = node.parent;
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

static void change_directory(const char* d) {
	if (chdir(d) < 0) {
		perror("change directory chdir");
		printf("directory=%s\n", d);
		getchar(); return;
	}
	printf("changed current directory to be to %s\n", d);
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
	
	printf("[continue]");
	fflush(stdout);
	getchar();
}

static void execute(char* command) {

	printf("execute: executing command %s...\n", command);

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

	create_process(arguments);

	free(arguments);
}


static void searchf(const char* string, nat length) {
	printf("searching forwards for \"%.*s\" (length %llu) within document...\n", (int) length, string, length);
	printf("count = %llu, starting from cursor %llu...\n", count, cursor);

	nat t = 0;
	loop: if (t == length or cursor >= count) return;
	if (text[cursor] != string[t]) t = 0; else t++; 
	cursor++; goto loop;
}

static void searchb(const char* string, nat length) {
	printf("searching backwards for \"%.*s\" (length %llu) within document...\n", (int) length, string, length);
	printf("count = %llu, starting from cursor %llu...\n", count, cursor);

	nat t = length;
	loop: if (not t or not cursor) return;
	cursor--; t--; 
	if (text[cursor] != string[t]) t = length;
	goto loop;
}



static void write_file(const char* directory, char* name) {

	/*


			TODO:

				detect if the file was changed by some other program, by recording the last write_file datetime stamp that we did, and see if the "last-written" attr of the file is the same as that stamp. if not, we know someone else modified it, and we SHOULD NOT write out our changes to this file, rather  to a new file, that we should prompt the user for a filename, maybe. if they choose a file that already exists, then just fall back on our random filename instead. so yeah.     don't overwrite changes made by another program!!!!!

				implement this please.



	*/

	puts("write_file: saving file...");

	int flags = O_WRONLY | O_TRUNC;
	mode_t permission = 0;

	if (not *name) {
		puts("name was found to be empty, creating a new name...");
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(name, 4096, "%s%s_%08x%08x.txt", directory, datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
		printf("created name: \"%s\", writing to this file...\n", name);
	}
	int file = open(name, flags, permission);
	if (file < 0) { 
		puts("write_file: open error"); 
		perror("open"); 
		printf(" filename: \"%s\"\n", name);
		printf("[continue]");
		fflush(stdout);
		getchar();
	}
	write(file, text, count);
	
	printf("successfully wrote %llu bytes to \"%s\" (file %d).\n", count, filename, file);

	close(file);
}



static void mid_print(void) {
	const nat half = page_size >> 1;
	const nat amount_down = half < count - cursor ? half : count - cursor;
	const nat amount_up = half < cursor ? half : cursor;

	puts("::");
	for (nat i = cursor - amount_up; i < cursor + amount_down; i++) {
		if (i == cursor) {
			if (text[i] == 10) printf("\033[7m%s\033[0m\n", "[N]");
			else printf("\033[7m%c\033[0m", text[i]);
		} else if (i == anchor) {
			if (text[i] == 10) printf("\033[7m%s\033[0m\n", "[N]");
			else printf("\033[7m%cA\033[0m", text[i]);
		} else putchar(text[i]);
	}
	puts("");
}

//fwrite(text + cursor - amount_up, 1, amount_up, stdout);
//fwrite(text + cursor + 1, 1, amount_down, stdout);

static void print_command_list(void) {

	puts("commands:\n"
		" q quit editor\n"
		" o clear screen\n"
		" t go into insert mode\n"
		" g insert datetime at cursor\n"
		" f display current filename\n"
		" n get cursor and count file information\n"
		" j<string> change directory\n"
		" p<number> set page size\n"
		" e<string> execute shell command\n"
		" z undo last action\n"
		" x redo last action\n"
		" k delete character bebind cursor\n"
		" r<string> select-for-replacement a string in document (unimplemented) \n"
		" s<string> search for string forwards\n"
		" a<string> search for string backwards\n"
		" d display entire document\n"
		" m print page before cursor\n"
		" c print page after cursor\n"
		" w write file contents\n"
		" ? this help menu\n"
		" \\ndrt\\n end insert"
	);
}

static void debug_command(char* input, nat n) {
	printf("debug: found command: \"");
	for (nat i = 0; i < n; i++) {
		printf("%c", input[i] == 10 ? '*' : input[i]);
	}
	puts("\"");
}

static void clear_screen(void) {
	for (nat i = 0; i < 100; i++) puts("");
	fflush(stdout);
	printf("\033[H");
	fflush(stdout);
}

static void backspace(void) {
	delete(1, 1); 
	printf("deleting 1 byte at cursor %llu, "
	"(document now %llub long)...", cursor, count);
}

static void scroll_up(char* input, nat n) {
	const nat amount = n == 2 ? page_size >> 1 : (nat) atoi(input + 1);
	if (cursor >= amount) cursor -= amount; else cursor = 0;
	printf("moving %llu bytes backwards...\n", amount);
	printf("cursor is now at %llu...\n", cursor);
	mid_print();
}

static void scroll_down(char* input, nat n) {
	const nat amount = n == 2 ? page_size >> 1 : (nat) atoi(input + 1);
	if (cursor + amount < count) cursor += amount; else cursor = count;
	printf("advancing %llu bytes forwards...\n", amount);
	printf("cursor is now at %llu...\n", cursor);
	mid_print();
}

static void information(char* input, nat n) {
	if (n > 2) {
		puts("setting page size...");
		page_size = (nat) atoi(input + 1);
	}
	printf("filename is \"%s\"\n", filename[0] ? filename : "(new file)");
	printf("page_size is set to %llu.\n", page_size);
	printf("cursor is at %llu.\n"
	       "anchor is at %llu.\n"
		"document is %llu bytes long.\n"
		"there are %llu actions done on the file.\n",
		cursor, anchor, count, action_count
	);
}

static void print_document(void) {
	printf(	"printing full document: "
		"(%llu bytes, cursor at %llu, anchor at %llu)\n", 
		count, cursor, anchor);
	fwrite(text, 1, count, stdout);
	puts("");
}

static void search_forwards(char* input, nat n) {
	if (n > 2) searchf(input + 1, n - 2); 
	else {
		puts("error: search_forwards with no arguments is unimplemented...");
	}

	printf("cursor is at %llu.\n", cursor);
	mid_print();
}

static void search_backwards(char* input, nat n) {
	if (n > 2) searchb(input + 1, n - 2); 
	else {
		puts("error: search_backwards with no arguments is unimplemented...");
	}

	printf("cursor is at %llu.\n", cursor);
	mid_print();
}

static void set_anchor(void) {
	printf("setting anchor to be at cursor, at %llu.\n", cursor);
	anchor = cursor;
}

static void jump_line(/*char* input, nat n*/__attribute__((unused)) char* input, __attribute__((unused)) nat n) {

	puts("jump line: unimplemented...");
	return;
}

static void cut(__attribute__((unused)) char* input, __attribute__((unused)) nat n) {

	puts("cut: replacing a selection, ...unimplemented at the moment.");

	return;

	// amount = cursor - anchor; // if anchor is first. 
	// calls     delete(amount, 1);
}

static void copy(__attribute__((unused)) char* input, __attribute__((unused)) nat n) {

	puts("copy: copying a selection, ...unimplemented at the moment.");

	return;

	// amount = cursor - anchor; // if anchor is first. 
	// calls     copy();
}


int main(int argc, const char** argv) {
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);

	struct termios terminal = {0};
	tcgetattr(0, &terminal);
	terminal.c_cc[VKILL] = _POSIX_VDISABLE;
	terminal.c_cc[VWERASE] = _POSIX_VDISABLE;
	tcsetattr(0, TCSANOW, &terminal);
	
	char input[128] = {0}; 
	nat inserting = 0, inserted_count = 0;
	finish_action((struct action){.parent = (nat) ~0}, 0, 0);

	if (argc < 2) goto loop;
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
loop:
	memset(input, 0, sizeof input);
	if (not inserting) { printf(". "); fflush(stdout); }
	
	const int64_t r = read(0, input, sizeof input);
	const nat n = (nat) r;
	if (r < 0) {
		puts("error: read returned < 0:");
		perror("read");
		printf("attempting to save the current file contents and quit...\n");
		printf("[continue]"); fflush(stdout); getchar(); goto quit;

	} else if (not r) { puts("read returned 0, end of stdin!"); goto quit; }
	
	if (inserting) {
		if (not strcmp(input, "drt\n")) {
			puts("exited insert mode");
			printf("inserted %llub\n", inserted_count);
			inserted_count = 0;
			inserting = 0;
		} else {
			inserted_count += n;
			insert(input, n, 1);
		}
	} else {
		debug_command(input, n);
		char c = *input;

		if (c == 10) {}
		if (c == ' ') {}
		else if (c == 'a') set_anchor();
		else if (c == 'b') {}
		else if (c == 'c') {}
		else if (c == 'd') {}
		else if (c == 'e') execute(input + 1);
		else if (c == 'f') information(input, n);
		else if (c == 'g') insert_dt();
		else if (c == 'h') scroll_up(input, n);
		else if (c == 'i') {}
		else if (c == 'j') change_directory("default_directory_name");
		else if (c == 'k') backspace();
		else if (c == 'l') jump_line(input, n);
		else if (c == 'm') scroll_down(input, n);
		else if (c == 'n') search_forwards(input, n);
		else if (c == 'o') clear_screen();
		else if (c == 'p') print_document();
		else if (c == 'q') goto quit;
		else if (c == 'r') cut(input, n);
		else if (c == 's') copy(input, n);
		else if (c == 't') { inserting = 1; puts("entered insert mode"); } 
		else if (c == 'u') search_backwards(input, n);
		else if (c == 'v') {}
		else if (c == 'w') write_file("./", filename);
		else if (c == 'x') redo();
		else if (c == 'y') print_command_list();
		else if (c == 'z') undo();
		else printf("error: unknown command: %c\n", c);
	}
	goto loop;
quit:
	write_file("./", filename);
	printf("exiting and saving %llub to \"%s\"...\n", count, filename);
	exit(0);
}




















/*





user manual:
=============

	

	q 	quit the editor.

	t	enter insert mode.

	y 	print a helpful list of commands and their keybindings.

	w 	write contents of buffer to file named filename.




	a 	set anchor position to current cursor position.



	n<string> 	search forwards for a string, and move cursor to after the string.
			cursors position is displayed after the move.
	
	u<string>	search backwards for a string, and move cursor to before the string.
			cursors position is displayed after the move.


	m<number>	move cursor forwards by number characters, or page_size/2 if absent.
			cursors position is displayed after the move.
	
	h<number>	move cursor backwards by number characters, or page_size/2 if absent.
			cursors position is displayed after the move.

	
		




	x	redo last insert/delete action performed.

	z 	undo last insert/delete action performed.


















*/














/*
		not useful anymore, really,   since mid_print exists:





		else if (c == 'c') {
			const nat amount = page_size < count - cursor ? page_size : count - cursor;
			puts("printing page after cursor:");
			puts("--------------");
			fwrite(text + cursor, 1, amount, stdout);
			puts("\n--------------");
		}

		else if (c == 'v') {
			const nat amount = page_size < cursor ? page_size : cursor;
			puts("printing page before cursor:");
			puts("--------------");
			fwrite(text + cursor - amount, 1, amount, stdout);
			puts("\n--------------");
		}





*/






//static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";     // autosaving unimplementd yet.
// static const nat autosave_frequency = 100; // (nat) -1; // -1 disables autosaving
// autosave_counter = 0;
// anchor = 0, cliplength = 0,           * clipboard = NULL;         selecting unimplemented.  
// static char autosavename[4096] = {0};



