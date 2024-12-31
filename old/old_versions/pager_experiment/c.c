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































	/*printf("forwards-inserting text in replace of %ld-length string, positioned at %lld... "
			"replacing with \"%s\" which is of length %lu\n", 
			selection, cursor, string, length
		);*/

		/*printf("variables: {%lld, %ld, %ld} :    %ld - %ld = %ld   \n", 
			cursor, selection, length, 
			length, selection, (ssize_t) length - selection);*/




/*printf("backwards-inserting text in replace of %ld-length string, positioned at %lld... "
				"replacing with \"%s\" which is of length %lu\n", 
				selection, cursor, string, length
			);*/



/*



	 to implement the replace functionality, we first, must shift down the existing second half of the file by a certain number of charcters.


		note, that we might have to either pad with zeros (or garbage)    if we are shifting it down, 

			

				we are going to do this one character at a time, i think, to minimize the memory that we will need. no dynamic memory allocation ideally lol.

				and theres no constraint on this being fast, or anything like that. just reliable.


		so we either pad zeros, if we are shifting down, ie, we need more room, 

		or we are delete existing text, if we are inserting a string which is much smaller (or the empty string)  than  what is currently selected. 


			in that case, we are simply going to do a truncate on the file at the end? hmmmmm.. not sure though.. yikes... how do we handle the end?... hm..... 

			the loop which copies the file contents over by one looks like 

			char c = 0;
			while (1) {
				ssize_t n = read(file, &c, 1);
				if (not n) break;
				write(file, &c, 1);
			}






		nope that doesnt work 


				so lets actually just do a single read or write call 


							and allocate a buffer, becuase doing it one by one is wayy harder than i thought lol
							i think we have to do it backwards, for it to work at all lol. yikes. so yeah, i'm pretty sure we need to call truncate on the file at the end anyays, so we are going to need the length in order to do that, i think. yikes. 



				hmm, i wish this was less complicated, dang... 




			so we'll do 


			int diff = found - length;
			int rest = count - cursor;
			buffer = malloc(length + rest);
			read(file, buffer + length, rest);
			lseek(file, diff, seek_cur);
			write(file, buffer, length + rest);

			
			


	that will peform a single write and a single read and a single lseek to do everything, i think. hope this works... it probably has a bug lol. 


			but lets test it now










*/



/*

	r<string> 	replace  current found selection with <string>

	f<string>	modulo find-and-select. prints page starting at selection.
			cursor positioned at start of selection.

	n<number> 	if number < 0, sets page size.
			if number >= 0 sets file pos. 
			if number is empty, prints file pos. 


*/















