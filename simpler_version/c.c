#include <stdio.h>  // 202310017.201349:   
#include <stdlib.h> //  a combination of the ed-like simpler editor, with the nonvolatile data structure! 
#include <string.h> 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

extern char** environ;

static char active = 0x01, inserting = 0x02;

extern char** environ;
static int file = -1;//, tree = -1;
static int mode = 0;
static off_t cursor = 0, count = 0; 

int main(int argc, const char** argv) {

	char filename[4096] = {0}, directory[4096] = {0};
	getcwd(directory, sizeof directory);

	int flags = O_RDWR;
	mode_t permission = 0;
	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t;
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%x%x_%s.txt", rand(), rand(), datetime);
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	}

	else if (argc == 2) strlcpy(filename, argv[1], sizeof filename);
	else exit(puts("usage error"));
	const int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { perror("read open directory"); exit(1); }
	int df = openat(dir, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	file = openat(dir, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }

	struct termios original = {0};
	tcgetattr(0, &original);
	struct termios terminal = original; 
	terminal.c_cc[VKILL]   = _POSIX_VDISABLE;
	terminal.c_cc[VWERASE] = _POSIX_VDISABLE;
	terminal.c_cc[VQUIT]   = _POSIX_VDISABLE;

	struct stat s;
	fstat(file, &s);
	count = s.st_size;

	char buffer[32] = {0};
	mode = active;

exiti:
	mode &= ~inserting;
	terminal.c_cc[VEOL] = ' ';
	tcsetattr(0, TCSAFLUSH, &terminal);
	
loop:;	ssize_t n = read(0, buffer, sizeof buffer);

	if (n < 0) { perror("read"); exit(errno); } 
	else if (n == 0) goto exiti;

	if (mode & inserting) {
		printf("inserting: \"%.*s\"\n", (int) n, buffer);
		if (n == 4 and not memcmp(buffer, "quit", 4)) goto exiti;
		fstat(file, &s);
		count = s.st_size;
		const size_t length = (size_t) n;
		const size_t size = (size_t) (count - cursor);
		char* rest = malloc(size + length); 
		memcpy(rest, buffer, length);
		lseek(file, cursor, SEEK_SET);
		read(file, rest + length, size);
		lseek(file, cursor, SEEK_SET);
		write(file, rest, size + length);
		count += length;
		cursor += length;
		fsync(file);
		free(rest);
		goto loop;
	} 
	const char c = *buffer;
	if (c == 'q') mode = 0;

	else if (c == 't') {
		mode |= inserting;
		terminal.c_cc[VEOL] = _POSIX_VDISABLE;
		tcsetattr(0, TCSAFLUSH, &terminal);
		write(1, "\n", 1);
		goto loop;
	}

	else if (c == 'd') { 		// display
		printf("display command!\n");
	}
	else if (c == 's') { 		// search

		printf("search command!\n");
		/*static void forwards(void) {
			nat t = 0;
			loop: if (t == cliplength or cursor >= count) return;
			if (text[cursor] != clipboard[t]) t = 0; else t++;
			move_right(); 
			goto loop;
		}*/
	} 
	else write(1, "?\n", 2);
	if (mode) goto loop;
	close(file);
	close(dir);
	tcsetattr(0, TCSAFLUSH, &original);
}





















// static struct winsize window = {0};
// , anchor = 0;
// origin = 0,
// static int cursor_row = 0, cursor_column = 0, desired_column = 0;


/*
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



static void get_count(void) {
	
}

static void insert(char* string, nat length) {
	struct stat s;
	fstat(file, &s);
	count = s.st_size;

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
	move_right();
}

static void delete(off_t length) {
	if (cursor < length) return;

	struct stat s;
	fstat(file, &s);
	count = s.st_size;

	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size);
	lseek(file, cursor, SEEK_SET);
	read(file, rest, size);
	lseek(file, cursor - length, SEEK_SET);
	write(file, rest, size);
	count -= length;
	ftruncate(file, count);
	fsync(file);
	free(rest);
	move_left();
}


*/




/*
	} else if (n < (ssize_t) sizeof buffer) {
		printf("[small line]\n");
	} else if (n == (ssize_t) sizeof buffer) {
		printf("[normal buffer read]\n");
	}
*/

