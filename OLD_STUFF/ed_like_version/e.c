// a minimalist text editor based on ed.
#include <iso646.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#pragma clang diagnostic push
#pragma clang diagnostic warning "-Weverything"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"

typedef size_t nat;

struct action { 
	char* text;
	nat* children;
	nat parent, type, choice, count, length, pre_saved, post_saved, pre_c, post_c;
};

static char* data = NULL;
static struct action* actions = NULL;
static nat 
	count = 0, capacity = 0, saved = 0, mode = 0, 
	action_count = 0, head = 0, cursor = 0;
static char filename[4096] = {0};

static inline nat file_exists(const char* f) { return access(f, F_OK) != -1; }

static struct termios configure_terminal() {	
	struct termios terminal; 
	tcgetattr(0, &terminal);
	struct termios raw = terminal;
	raw.c_iflag &= ~( (unsigned long)BRKINT 
			| (unsigned long)INPCK 
			| (unsigned long)IXON );
	raw.c_lflag &= ~( (unsigned long)ICANON 
			| (unsigned long)IEXTEN );
	tcsetattr(0, TCSAFLUSH, &raw);
	return terminal;
}

static void create_buffer() {
	buffers = realloc(buffers, sizeof(struct buffer) * (buffer_count + 1));
	buffers[buffer_count] = (struct buffer){0};
	active_index = buffer_count++;
	_.saved = 1;
	_.action_count = 1;
	_.actions = calloc(1, sizeof(struct action));
}

static void prompt(const char* prompt, char* out, const nat out_size) {
	printf("\n%s", prompt);
	nat length = 0;
	char c = 0;
loop:	if (length >= out_size) goto done;
	fflush(stdout);
	read(0, &c, 1);
	if (c == 10) goto done;
	else if (c == 127) { if (length) length--; }
	else out[length++] = c;
	goto loop;
done: 	return;
}

static void new_filename(const char* p, char* new, nat size) {
loop:	strlcpy(new, "", size);
	prompt(p, new, size);
	if (not strlen(new)) return;
	if (file_exists(new)) goto loop;
}

static void rename_file() {
	char new[4096] = {0}; 
	new_filename("rename: ", new, sizeof new);
	if (not strlen(new)) { printf("aborted rename\n"); return; }
	if (rename(_.filename, new)) printf("error: %s\n", strerror(errno));
	else strlcpy(_.filename, new, sizeof _.filename);
}

static void save() {
	if (strlen(_.filename)) goto op;
	new_filename("save as: ", _.filename, sizeof _.filename);
	if (not strlen(_.filename)) { printf("aborted save\n"); return; }
op:;	int file = creat(_.filename, 0644);
	if (file < 0) { printf("error: %s\n", strerror(errno)); return; }
	ssize_t bytes = write(file, _.data, _.count);
	close(file); printf("%ld\n", bytes); _.saved = 1;
}

static void open_file(const char* given_filename) {
	int file = open(given_filename, O_RDONLY); 
	if (file < 0) { printf("error: open: %s\n", strerror(errno)); return; } 
	create_buffer();
	struct stat s; fstat(file, &s);
	_.count = (size_t) s.st_size;
	_.capacity = _.count;
	_.data = malloc(_.count);
	read(file, _.data, _.count);
	close(file);
	strlcpy(_.filename, given_filename, sizeof _.filename);
	_.mode = 1; printf("%lub\n", _.count);
}

static void prompt_open() {
	char new[4096] = {0};
loop:	strlcpy(new, "", sizeof new);
	prompt("open: ", new, sizeof new);
	if (not strlen(new)) { printf("aborted open\n"); return; }
	if (not file_exists(new)) goto loop;
	open_file(new);
}

static void close_buffer() {
	
}

int main(int argc, const char** argv) {
	struct termios terminal = configure_terminal();
	if (argc == 1) create_buffer();
	else for (int i = 1; i < argc; i++) open_file(argv[i]);
	char c = 0;
	nat length = 0;
	char* string = NULL;
loop:	fflush(stdout);
	read(0, &c, 1);
	if (_.mode) goto insert;
	if (c == 'Q') goto done;
	if (c == 'q') { if (saved) goto done; }
	if (c == 'a') _.mode = 0;
	if (c == 'p') printf("%.*s", (int) _.count, _.data);
	if (c == 'o') prompt_open();
	if (c == 'm') rename_file();
	if (c == 's') save();
	if (c >= '0' and c <= '9') {}      // add to the index-number buffer
	goto loop;
insert: if (c == 'A') {
		_.data = realloc(_.data, _.count + length);
		memmove(_.data + _.c + length, _.data + _.c, _.count - _.c);
		_.count += length; length = 0; free(string);
		_.mode = 1;
	} else if (c == 127) { if (length) length--; }
	else {
		string = realloc(string, length + 1);  		//todo: capacity optimize this.
		string[length++] = c;
	}
	goto loop;
done: 	close_buffer();
	puts(""); tcsetattr(0, TCSAFLUSH, &terminal);
}





























/*static void handle_signal_interrupt(int code) {

	srand((unsigned)time(NULL));
	emergency_save_all_buffers();

	printf("press '.' to continue running process\n\r");
	int c = getchar(); 
	if (c != '.') exit(1);	
}*/





/*


static inline void emergency_save_to_file() {

	char dt[16] = {0};
	get_datetime(dt);
	char id[32] = {0};
	sprintf(id, "%08x%08x", rand(), rand());
	char local_filename[4096] = {0};
	sprintf(local_filename, "EMERGENCY_FILE_SAVE__%s__%s__.txt", dt, id);

	int file = open(given_filename, O_RDONLY); 
	if (file < 0) { 
		printf("error: open: %s", strerror(errno));
		return;
	} 

	_.count = lseek(fd, 0, SEEK_END);
	_.capacity = _.count;
	_.data = malloc(length);
        lseek(file, 0, SEEK_SET);
        read(file, _.data, _.count);
	close(file);

	_.saved = true; 
	_.mode = 1;
	_.c = 0;
	_.actions = calloc(1, sizeof(struct action));
	_.head = 0;

	strlcpy(_.filename, given_filename, sizeof _.filename);
	printf("interrupt: emergency wrote %lldb;%ldl to %s\n\r", bytes, _.count, local_filename);
}

*/






// static inline bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2; }





// #include <stdint.h>
// #include <time.h>
// #include <signal.h>
// #include <dirent.h>
// #include <sys/time.h> 
// #include <sys/types.h>


