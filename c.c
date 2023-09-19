#include <stdio.h>  // 202309074.165637:   
#include <stdlib.h> //  another rewrite to make the editor simpler, and non-volatile- to never require saving. 
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
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>

typedef size_t nat;
extern char** environ;
static struct winsize window = {0};

static int file = -1;  // , tree = -1;
static off_t cursor = 0, origin = 0, count = 0, anchor = 0;
static nat mode = 0, selecting = 0;
static nat cursor_row = 0, cursor_column = 0, desired_column = 0;

static void display(void) {
	ioctl(0, TIOCGWINSZ, &window);
	const nat screen_size = window.ws_row * window.ws_col * 4;
	char* screen = calloc(screen_size, 1);
	memcpy(screen, "\033[?25l\033[H", 9);
	nat length = 9;
	nat row = 0, column = 0;
	off_t i = origin;
	lseek(file, origin, SEEK_SET);

	const off_t begin = anchor < cursor ? anchor : cursor;
	const off_t end = anchor < cursor ? cursor : anchor;
	if (selecting and anchor < origin) { memcpy(screen + length, "\033[7m", 4); length += 4; }
	ssize_t n = 0;
	while (1) {
		if (selecting and i == begin) { memcpy(screen + length, "\033[7m", 4); length += 4; }
		if (selecting and i == end) { memcpy(screen + length, "\033[0m", 4); length += 4; }
		if (i == cursor) { cursor_row = row; cursor_column = column; }
		if (row >= window.ws_row) break;
		char k = 0;
		n = read(file, &k, 1); i++;
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }

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

	while (row < window.ws_row) {
		row++;
		memcpy(screen + length, "\033[K", 3);
		length += 3; 
		if (row < window.ws_row) screen[length++] = 10;
	} 

	if (cursor == count and not n and cursor_row == window.ws_row - 1 and not cursor_column)
		length += (nat) snprintf(screen + length, screen_size - length, "\033[%d;1H\033[K", window.ws_row);
	length += (nat) snprintf(screen + length, screen_size - length, "\033[K\033[%lu;%luH\033[?25h", cursor_row + 1, cursor_column + 1);
	write(1, screen, length);
	free(screen);
}

static void get_count(void) {
	struct stat s;
	fstat(file, &s);
	count = s.st_size;
}

static void move_left_raw(void) {
	get_count();
	if (not cursor) return;
	if (origin < cursor) goto decr;
	origin--;
loop: 	if (not origin) goto decr;
	origin--;
	char c = 0;
	lseek(file, origin, SEEK_SET);
	ssize_t n = read(file, &c, 1);
	if (n == 0) return;
	if (n < 0) { perror("read() syscall"); exit(1); }
	if (c != 10) goto loop;
	origin++;
decr: 	cursor--;
}

static void move_right_raw(void) {
	get_count();
	if (cursor >= count) return;
	char c = 0;
	lseek(file, cursor, SEEK_SET);
	ssize_t n = read(file, &c, 1);
	if (n == 0) return;
	if (n < 0) { perror("read() syscall"); exit(1); }
	cursor++;
	if (c == 10) { cursor_column = 0; cursor_row++; }
	else if (c == 9) cursor_column += 8 - cursor_column % 8;
	else if (cursor_column >= window.ws_col - 1) { cursor_column = 0; cursor_row++; }
	else if ((unsigned char) c >> 6 != 2) cursor_column++; 
	if (cursor_row < window.ws_row) return;
	nat column = 0;
loop: 	if (origin >= count) return;
	lseek(file, origin, SEEK_SET);
	n = read(file, &c, 1);
	if (n == 0) return;
	if (n < 0) { perror("read() syscall"); exit(1); }
	origin++;
	if (c == 10) goto done;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col - 1) goto done;
	else if ((unsigned char) c >> 6 != 2) column++;
	goto loop; 
done: 	cursor_row--;
}

static void move_left(void) {
	move_left_raw();
	desired_column = cursor_column;
}

static void move_right(void) {
	move_right_raw();
	desired_column = cursor_column;
}

static void move_up_begin(void) {
	while (cursor) {
		move_left();
		char c = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &c, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (cursor and c == 10) break;
	}
}

static void move_down_end(void) {
	while (cursor < count) {
		move_right();
		char c = 0;
		lseek(file, cursor, SEEK_SET);
		ssize_t n = read(file, &c, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (cursor < count and c == 10) break;
	}
}

static void move_up(void) {
	
}

static void move_down(void) {

	while (cursor < count) {
		move_right_raw();
		if (not cursor_column) break;
	}
	while (cursor < count) {
		if (cursor_column >= desired_column) break;
		char c = 0;
		lseek(file, cursor, SEEK_SET);
		ssize_t n = read(file, &c, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (c == 10) break;
		move_right_raw(); 
	}
}

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
*/

static void move_word_left(void) {
	move_left();
	while (cursor) {
		char here = 0, behind = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &behind, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
			n = read(file, &here, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }

		if (not (not isalnum(here) or isalnum(behind))) break;
		move_left();
	}
}

static void move_word_right(void) {
	move_right();
	while (cursor < count) {
		char here = 0, behind = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &behind, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
			n = read(file, &here, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }

		if (not (isalnum(here) or not isalnum(behind))) break;
		move_right();
	}
}

static void insert(char c) {
	get_count();
	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size + 1); 
	*rest = c;
	lseek(file, cursor, SEEK_SET);
	read(file, rest + 1, size);
	lseek(file, cursor, SEEK_SET);
	write(file, rest, size + 1);
	fsync(file);
	free(rest);
	move_right();
}

static void delete(void) {
	if (not cursor) return;
	get_count();
	const size_t size = (size_t) (count - cursor);
	char* rest = malloc(size); 
	lseek(file, cursor, SEEK_SET);
	read(file, rest, size);
	lseek(file, cursor - 1, SEEK_SET);
	write(file, rest, size);
	ftruncate(file, --count);
	fsync(file);
	free(rest);
	move_left();
}

static void interpret_arrow_key(void) {    // customize in Terminal.app  the escape sequences for all arrow key combinations we need!!
	char c = 0; read(0, &c, 1);
	if (false) {}
	else if (c == 'b') move_word_left();
	else if (c == 'f') move_word_right();
	else if (c == 'd') move_down_end();
	else if (c == 'u') move_up_begin();
	else if (c == '[') {
		read(0, &c, 1); 
		if (false) {}
		else if (c == 'A') move_up(); 
		else if (c == 'B') move_down();
		else if (c == 'C') move_right(); 
		else if (c == 'D') move_left();
		else if (c == 49) {
			read(0, &c, 1); read(0, &c, 1); read(0, &c, 1); 
			if (false) {}
			else if (c == 68) { anchor = cursor; selecting = not selecting; }
			else if (c == 67) {}
		}
	} 
}

static inline void now(char datetime[32]) {
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
}

int main(int argc, const char** argv) {

	struct termios terminal;
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSAFLUSH, &copy);

	char filename[4096] = {0}, directory[4096] = {0};
	getcwd(directory, sizeof directory);

	int flags = O_RDWR;
	mode_t permission = 0;
	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		now(datetime);
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

	get_count();
	char c = 0;
	mode = 1;

loop:	display();
	read(0, &c, 1);
	if (c == 'Q') mode = 0;
	else if (c == 27) interpret_arrow_key();
	else if (c == 127) delete();
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c);
	if (mode) goto loop;
	close(file); 
	close(dir);
	printf("\033[H\033[2J"); 	// todo: make this use the alt screen!
	tcsetattr(0, TCSAFLUSH, &terminal);
}











































/*

 // todo: make this move up visual lines properly, not skipping the origin to the next logical line. 



	todos:

	x	- make move_right and move_left move the origin as well   up and down   visual lines.   
				 thats the secret building block that everything builds off of. 


		- make move_right and move_left go up and down visual lines.

		- make move_up and move_down use visual lines as well. 


		- have very good support for visual movement on softwrapped lines. 

	x	- have very good support for displaying softwrapped lines. 





		- make smooth scrolling a thing!!!! use terminal sequences to print just what you need to print in order to see the next line scroll!!!


					- on second thought:      we should acttually just discard the mouse/trackpad entirely. 

									no support for anything related to the mouse, 

									this includes smooth scrolling?.. hm...... hm...




*/









































/*
static void move_begin(void) { 
	while (cursor) {
		char k = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &k, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (k == 10) break;
		move_left();  
	}
}

static void move_end(void) {
	while (cursor < count) {
		char k = 0;
		lseek(file, cursor, SEEK_SET);
		ssize_t n = read(file, &k, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (k == 10) break;
		move_right(); 
	}
}










// if (cursor_column >= desired_column) break; // and cursor_row >= desired_row// 


// if (origin_s == origin or save == cursor_row)





// const	 nat origin_s = origin;	
	// const nat save = cursor_row;
	// const nat desired_row = cursor_row < window.ws_row - 1 ? cursor_row + 1 : cursor_row;
	//if (cursor_row < window.ws_row - 1 and cursor_row >= desired_row) break;










static void move_left(void) {
	if (not cursor) return;
	if (origin < cursor) goto decr;

	
	



decr: 	cursor--;
}



static void move_left(void) {
	if (not cursor) return;

	if (origin < cursor) goto decr;
	origin--;
e: 	if (not origin) goto decr;
	origin--;
	if (text[origin] != 10) goto e;
	origin++;

decr: 	cursor--;
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
*/






























/*





static void move_begin(void) { 
	while (cursor) {
		char k = 0;
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &k, 1);
		lseek(file, cursor, SEEK_SET);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (k == 10) break;
		move_left();  
	}
}

static void move_end(void) {
	while (cursor < count and text[cursor] != 10) move_right(); 
}

static void move_word_left(void) {
	move_left();
	while (cursor) {

		char here = 0, behind = 0;
		
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &behind, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
			n = read(file, &here, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		lseek(file, cursor, SEEK_SET);

		if(not (
		not isalnum(here) or 
		isalnum(behind) 
		)) break;
		move_left();
	}
}

static void move_word_right(void) {
	move_right();
	while (cursor < count) {

		char here = 0, behind = 0;
		
		lseek(file, cursor - 1, SEEK_SET);
		ssize_t n = read(file, &behind, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
			n = read(file, &here, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		lseek(file, cursor, SEEK_SET);

		if(not (
		isalnum(here) or 
		not isalnum(behind) 
		)) break;
		move_right();
	}
}

static void move_word_right(void) {
	//do move_right();
	//while (cursor < count and (isalnum(text[cursor]) or not isalnum(text[cursor - 1])));
}


static void move_word_left(void) {
	move_left();
	while (cursor and (not isalnum(text[cursor]) or isalnum(text[cursor - 1])));
}
static void move_word_right(void) {
	//do move_right(); 
	//while (cursor < count and (isalnum(text[cursor]) or not isalnum(text[cursor - 1])));
}


static void move_up(void) {}

static void move_down(void) {}






// lseek(file, cursor, SEEK_SET);
	// lseek(file, cursor, SEEK_SET);
	// lseek(file, -1, SEEK_CUR);






	if (origin < cursor or not origin) goto decr;
	origin--;
	e: if (not origin) goto decr;
	origin--;
	if (text[origin] != 10) goto e;
	origin++;
	decr: if (cursor) cursor--;



	if (not should_record) return;
	new.insert = 0;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);







	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};

	const char c = text[cursor - 1];

	memmove(text + cursor - 1, text + cursor, count - cursor);

	count--;

	text = realloc(text, count);




static void move_right(void) {          // todo: make this function NOT move down visual lines. only logical lines. its simpler. 
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


	// struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = mode & saved};

	//text = realloc(text, count + 1);
	//memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c;
	count++; 

if (not should_record) return;
	new.insert = 1;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);


*/






/*




fn delete  =    27 91 51 126

shift fn delete  =    27 91 51 59 50 126








	read()
	write()
	open()
	openat()
	close()
	lseek()
	exit()


	If whence is SEEK_SET, the offset is set to offset bytes.
	If whence is SEEK_CUR, the offset is set to its current location plus offset bytes.
	If whence is SEEK_END, the offset is set to the size of the file plus offset bytes.
*/





/*


//      keys:           a s h t   n e o i                 d r     u p 




	else if (c == 8)  {} // H
	else if (c == 20) {} // T

	else if (c == 18) {} // R
	
	
	else if (c == 14) {} // N
	else if (c == 5)  {} // E
	else if (c == 15) {} // O

	else if (c == 21) {} // U
	else if (c == 16) {} // P







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







	else if (c == 'R') cut();
	else if (c == 'A') sendc();

	else if (c == 'C') copy();
	else if (c == 'V') insert_output("pbpaste");






	else if (c == 'X') redo();
	else if (c == 'Z') undo();
	else if (c == 'A') alternate();






 else if (c == 'T') { anchor = cursor; mode ^= selecting; }
 if (not (mode & selecting)) anchor = previous_cursor;



	else if (c == 'U') move_word_left();
	else if (c == 'P') move_word_right();
	else if (c == 'K') move_begin();
	else if (c == 'I') move_end();
	else if (c == 'E') move_up();


	else if (c == 'H') backwards();
	else if (c == 'M') forwards();










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





*/































/*

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
	else if (c == 8  H) {}
	else if (c == 18 R) {}
	else if (c == 24 X) {}
	else if (c == 26 Z) {}
	else if (c == 17 Q) { if (mode & saved) mode &= ~active; }
	else if (c == 4  D) insert_now();
	else if (c == 1  A) { read(0, &c, 1); insert(c, 1); }
	else if (c == 19 S) alternate();
	else if (c == 27)  interpret_arrow_key();
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











static void insert(char c) {

	struct stat s;
	fstat(file, &s);
	count = s.st_size;
	size_t size = (size_t) (count - cursor);
	char* r = malloc(size + 1);
	r[0] = c;

	lseek(file, (off_t) cursor, SEEK_SET);
	read(file, r + 1, size);
	lseek(file, (off_t) cursor, SEEK_SET);
	write(file, r, size + 1);

	free(r);

	move_right();

	fsync(file);





}










char c = 0;
		lseek(file, cursor, SEEK_SET);
		ssize_t n = read(file, &c, 1);
		if (n == 0) break;
		if (n < 0) { perror("read() syscall"); exit(1); }
		if (c == 10) break;
*/






