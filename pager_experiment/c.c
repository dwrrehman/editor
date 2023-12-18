#include <stdio.h>     // revised editor, based on less, more, and ed. 
#include <stdlib.h>    // only uses three commands, find-select, replace-selection, and set-numeric
#include <unistd.h>    // written by dwrr on 202312177.223938
#include <string.h>
#include <iso646.h>
#include <fcntl.h>
#include <stdbool.h>
#include <termios.h>

static const bool debug = 0;

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

loop:;
	char line[4096] = {0};
	read(0, line, sizeof line);
	if (not strcmp(line, "exit\n")) goto done;
	else if (not strcmp(line, "n\n")) {
		if (not length) {
			printf("%lld\n", lseek(file, 0, SEEK_CUR));
			if (debug) printf("currently at file position %lld\n", lseek(file, 0, SEEK_CUR));
			goto loop;

		} else {
			string[--length] = 0;

			const off_t n = atoi(string);

			if (n < 0) {
				page = realloc(page, page_size = (size_t) -n);
				if (debug) printf("page_size is now %lu\n", page_size);
			} else {
				lseek(file, n, SEEK_SET);
				if (debug) printf("file position is now %lld\n", n);
			}
			memset(string, 0, sizeof string); 
			length = 0;
			goto loop;
		}

	} else if (not strcmp(line, "f\n")) {

		if (not length) {
			ssize_t nn = read(file, page, page_size);
			if (nn < 0) abort();
			if (nn > 0) { write(1, page, (size_t) nn); write(1, "\033[7m/\033[0m", 9); }
			else lseek(file, 0, SEEK_SET);
			
			goto loop;
		}

		string[--length] = 0;

		const off_t start = lseek(file, 0, SEEK_CUR);

		if (debug) printf("info: searching for: string = \"%s\", @ offset = %lld\n", string, start);

		size_t i = 0;
		char c = 0;
		bool was_reset = false;

	search:; 
		ssize_t n = read(file, &c, 1);

		if (not n) { 

			if (was_reset) puts("not found");

			if (debug) printf("error: %s: found end of file before string.\n", was_reset ? "CONTAINS_RESET" : "single-pass"); 
			
			if (not was_reset) {
				if (debug) printf("info: looping back around to the beginning of the document to search more.\n"); 
				lseek(file, 0, SEEK_SET);
				was_reset = true;
				goto search;

			} else {
				memset(string, 0, sizeof string); 
				length = 0;
				found = 0;
				goto loop;
			}

			
		}

		if (debug) printf("info: @%lld: i%lu: comparing: c=\"%c\" and str=\"%c\"....\n", lseek(file, 0, SEEK_CUR), i, c, string[i]);

		if (c == string[i]) { 
			if (debug) printf("info: found matching c=\"%c\" and str=\"%c\"!\n", c, string[i]); 
			i++; 

		} else {
			if (debug) printf("info: no match, resetting i...\n");
			i = 0;
		}
		
		if (i == length) {

			if (debug) printf("info: we found it!!! matched all i%lu / len%lu characters. SEARCH SUCESSSFUL\n", i, length);

			const off_t at = lseek(file,  -(off_t)length, SEEK_CUR);
			ssize_t nn = read(file, page, page_size);
			lseek(file, at, SEEK_SET);
			if (not nn) {
				printf("ERROR: found a page size of 0. are we at the end?...\n");
			}
			if (nn < 0) abort();
			if (nn > 0) { write(1, page, (size_t) nn); write(1, "\033[7m/\033[0m", 9); }

			memset(string, 0, sizeof string); 
			length = 0;

			goto loop;
		}

		goto search;

	}  else length = strlcat(string, line, sizeof string);
	goto loop;

done:
	tcsetattr(0, TCSAFLUSH, &original);
}




























/*

	r<string> 	replace  current found selection with <string>

	f<string>	modulo find-and-select. prints page starting at selection.
			cursor positioned at start of selection.

	n<number> 	if number < 0, sets page size.
			if number >= 0 sets file pos. 
			if number is empty, prints file pos. 


*/















