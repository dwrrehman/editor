// an editor that is more easily usable with 
// the asychronous shell that we wrote recently. 
// written on 1202409297.214859 by dwrr. 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

typedef uint64_t nat;

int main(void) {

	char filename[4096] = {0};
	write(1, "filename: ", 10);
	read(0, filename, sizeof filename);
	size_t filename_length = strlen(filename);
	if (filename_length) filename[--filename_length] = 0;

	printf("editing file: %s\n", filename);
	fflush(stdout);

	int file = open(filename, O_RDONLY);
	if (file < 0) {
		printf("error: open(r): %s\n", strerror(errno));
		exit(1);
	}

	const nat text_length = (nat) lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	char* text = malloc(text_length);
	read(file, text, text_length);
	close(file);

	nat cursor = 0;
	nat anchor = 0;
	nat page_size = 100;

	char input[4096] = {0};
	printf("read %llu bytes\n", text_length);

	while (1) {
		write(1, ": ", 2);
		read(0, input, sizeof input);

		nat input_length = (nat) strlen(input);
		if (input_length) input[--input_length] = 0;
		else continue;

		const char* string = input + 1;
		const nat string_length = input_length - 1;

		     if (input[0] == 'q') break;
		else if (input[0] == 'c') printf("c%llu,a%llu,p%llu\n", cursor, anchor, page_size);
		else if (input[0] == 'b') cursor = 0;
		else if (input[0] == 'e') cursor = text_length;
		else if (input[0] == 'a') anchor = cursor;
		else if (input[0] == 'p') page_size = (nat) atoi(string);


		else if (input[0] == 'n') { 
			cursor = (nat) atoi(string); 
			if (cursor >= text_length) cursor = text_length;


		} else if (input[0] == 'd') {
			nat amount = text_length - cursor;
			if (amount > page_size) amount = page_size;
			fwrite(text + cursor, 1, amount, stdout);


		} else if (input[0] == 'f') {
			nat t = 0;
			for (nat i = cursor; i < text_length; i++) {
				if (text[i] == string[t]) {
					t++;
					if (t == string_length) { cursor = i + 1; goto found; } 
				} else t = 0;
			}
			printf("not found: \"%.*s\"\n", (int) string_length, string);
			continue;
		found:
			printf("found: c%llu\n", cursor);


		} else if (input[0] == 't') {
			printf("inserting %llub at %llu: \"%.*s\"\n", 
				string_length, cursor, 
				(int) string_length, string
			);

			


		} else if (input[0] == 'r') {
			printf("removing: %llu .. %llu\n", anchor, cursor);


		} else {
			puts("error: bad input");
		}
	}

	puts("exiting editor...");
	exit(0);
}










