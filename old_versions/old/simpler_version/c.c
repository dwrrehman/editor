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
int main(int argc, const char** argv) {
	typedef size_t nat;
	const char active = 0x01, inserting = 0x02;
	int file = -1, tree = -1;
	int mode = 0;
	off_t cursor = 0, count = 0, anchor = 0, m = 0;
	const char* help_string = "q z c m d<str> t<str> h<str> r<str> a<num> s<num> ";
	char filename[4096] = {0}, directory[4096] = {0};
	getcwd(directory, sizeof directory);
	int flags = O_RDWR;
	mode_t permission = 0;
	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t; gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%x%x_%s.txt", rand(), rand(), datetime);
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	} else if (argc == 2) strlcpy(filename, argv[1], sizeof filename);
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
	tcsetattr(0, TCSAFLUSH, &terminal);

	struct stat s;
	fstat(file, &s);
	count = s.st_size;
	anchor = 0;
	cursor = count;
	char buffer[64] = {0};
	mode = active;
	goto loop;

sw:	mode ^= inserting;
	write(1, "\n", 1);
loop:;	ssize_t n = read(0, buffer, sizeof buffer - 1); // printf("read: \"%.*s\" : %ld\n", (int) n, buffer, n);
	if (n < 0) { perror("read"); exit(errno); } 
	else if (n == 0) goto sw;

	if (mode & inserting) {
		if (n == 4 and not memcmp(buffer, "drt\n", 4)) goto sw;
	insert: fstat(file, &s);
		count = s.st_size;
		const nat length = (nat) n;
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
	else if (c == 'z') puts(help_string); 
	else if (c == 'c') write(1, "\033[H\033[2J", 7);
	else if (c == 'm') printf("%d %llu %llu-%llu %lld\n", mode, count, cursor, anchor, cursor - anchor);
	else if (c == 'a') anchor = cursor + atoi(buffer + 1); 
	else if (c == 't') {
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		if (n == 1) goto sw; 
		memmove(buffer, buffer + 1, (size_t) --n);
		goto insert;
	} else if (c == 's') { // delete <num>
		fstat(file, &s);
		count = s.st_size;
		if (anchor > count) anchor = count;
		if (cursor > count) cursor = count;
		if (anchor > cursor) { off_t temp = anchor; anchor = cursor; cursor = temp; }
		const off_t length = cursor - anchor;
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		if (length != atoi(buffer + 1)) goto error;
		const nat size = (nat)(count - cursor);
		char* rest = malloc(size);
		lseek(file, cursor, SEEK_SET);
		read(file, rest, size);
		lseek(file, cursor - length, SEEK_SET);
		write(file, rest, size);
		count -= length;
		ftruncate(file, count);
		fsync(file);
		free(rest);
		cursor = anchor;


				///                  fund problem       we can't do anything with newlines. we need to fix this. newlines shouldnt cause so much of an issue. we need to fix this. 
	} else if (c == 'd') {    // display <number>         TODO: delete the  first ternary expresion, only print a-c selection. 
		nat begin = 0, end = 0;
		fstat(file, &s);
		count = s.st_size;
		if (anchor > count) anchor = count;
		if (cursor > count) cursor = count;
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		if (n > 1) m = atoi(buffer + 1);
		begin = m ? (cursor - m >= 0    ? (nat) (cursor - m) : (nat) 0)     : (anchor < cursor ? (nat) anchor : (nat) cursor);
		end   = m ? (cursor + m < count ? (nat) (cursor + m) : (nat) count) : (anchor < cursor ? (nat) cursor : (nat) anchor);
		char* screen = malloc(end - begin);
		lseek(file, (off_t) begin, SEEK_SET);
		read(file, screen, end - begin);
		write(1, screen, end - begin);

	} else if (*buffer == 'r') {    // forwards <string>
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		const char* clipboard = buffer + 1;
		const nat cliplength = (nat) n - 1;
		nat t = 0;
		sloop: if (t == cliplength or cursor >= count) goto loop;
		char cc = 0;
		lseek(file, cursor, SEEK_SET);
		n = read(file, &cc, 1);
		if (n == 0) goto loop;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (cc != clipboard[t]) t = 0; else t++;
		cursor++; goto sloop;

	} else if (*buffer == 'h') {    //  backwards <string>
		if (buffer[n - 1] == 10) buffer[--n] = 0;
		const char* clipboard = buffer + 1;
		const nat cliplength = (nat) n - 1;
		nat t = cliplength;
		bloop: if (not t or not cursor) goto loop;
		cursor--; t--; 		
		char cc = 0;
		lseek(file, cursor, SEEK_SET);
		n = read(file, &cc, 1);
		if (n == 0) goto loop;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (cc != clipboard[t]) t = cliplength;
		goto bloop;

	} else error: write(1, "?\n", 2);

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

