#include <stdio.h>     // revised editor, based on less, more, and ed. 
#include <stdlib.h>    // only uses three commands, find-select, replace-selection, and set-numeric
#include <unistd.h>    // written by dwrr on 202312177.223938
#include <string.h>
#include <iso646.h>
#include <fcntl.h>
#include <termios.h>

int main(int argc, const char** argv) {
	if (argc != 2) exit(puts("usage ./editor <file>"));
	int file = open(argv[1], O_RDWR, 0);
	if (file < 0) { perror("read open file"); exit(1); }

	struct termios original = {0};
	tcgetattr(0, &original);
	struct termios terminal = original; 
	terminal.c_cc[VKILL]   = _POSIX_VDISABLE;
	terminal.c_cc[VWERASE] = _POSIX_VDISABLE;
	tcsetattr(0, TCSAFLUSH, &terminal);

	size_t page_size = 4096;
	char* page = malloc(page_size);
	const size_t max_input_string_size = 4096;
	char string[max_input_string_size] = {0};
	size_t length = 0, found = 0;

	loop:; char line[4096] = {0}, c = 0;
	read(0, line, sizeof line);
	if (not strcmp(line, "exit\n")) goto done;
	else if (not strcmp(line, "n\n")) {
		if (not length) printf("%lld\n", lseek(file, 0, SEEK_CUR));
		else {
			string[--length] = 0;
			const off_t n = atoi(string);
			if (n < 0) page = realloc(page, page_size = (size_t) -n);
			else lseek(file, n, SEEK_SET);
			memset(string, 0, sizeof string); 
			length = 0;
		}

	} else if (not strcmp(line, "f\n")) {
		if (not length) goto print_page;
		string[--length] = 0;
		size_t i = 0;
		search:; ssize_t n = read(file, &c, 1);
		if (not n) { found = 0; goto clear; }
		if (c == string[i]) i++; else i = 0;
		if (i == length) {
			print_page:; ssize_t nn = read(file, page, page_size);
			if (nn < 0) abort();
			if (nn > 0) { write(1, page, (size_t) nn); write(1, "\033[7m/\033[0m", 9); }
			found = length;
			clear: memset(string, 0, sizeof string); 
			length = 0; goto loop;
		} else goto search;
	} else length = strlcat(string, line, sizeof string);
	goto loop;
	done: tcsetattr(0, TCSAFLUSH, &original);
}






































/*

	r<string> 	replace  current found selection with <string>

	f<string>	modulo find-and-select. prints page starting at selection.
			cursor positioned at start of selection.

	n<number> 	if number < 0, sets page size.
			if number >= 0 sets file pos. 
			if number is empty, prints file pos. 


*/















