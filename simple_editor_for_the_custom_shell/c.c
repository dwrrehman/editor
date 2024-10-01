// an editor that is more easily usable with 
// the asychronous shell that we wrote recently. 
// written on 1202409297.214859 by dwrr. 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iso646.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

typedef uint64_t nat;

int main(void) {

	char filename[4096] = {0};
	write(1, "filename: ", 10);
	read(0, filename, sizeof filename);
	size_t filename_length = strlen(filename);
	if (filename_length) filename[--filename_length] = 0;


load_file:
	printf("editing file: %s\n", filename); fflush(stdout);

	int file = open(filename, O_RDONLY);
	if (file < 0) {
		printf("error: open(r): %s\n", strerror(errno)); fflush(stdout);
		exit(1);
	}

	nat text_length = (nat) lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	char* text = malloc(text_length);
	read(file, text, text_length);
	close(file);

	struct stat attr_;
	stat(filename, &attr_);
	char* last_saved = strdup(ctime(&attr_.st_mtime));
	printf("Last modified time: %s\n", last_saved); fflush(stdout);

	nat cursor = 0, anchor = 0, page_size = 1000;

	char input[4096] = {0};
	printf("read %llu bytes\n", text_length); fflush(stdout);

	nat saved = 1;

	while (1) {
		write(1, ": ", 2);
		memset(input, 0, sizeof input);
		ssize_t n = read(0, input, sizeof input);
		if (n < 0) { printf("%s", strerror(errno)); sleep(1); } 
		else if (n == 0) { write(1, "EOF???", 6); sleep(1); } 

		nat input_length = (nat) n;
		if (input_length) input[--input_length] = 0;
		else continue;

		const char* string = input + 1;
		const nat string_length = input_length - 1;

		     if (input[0] == 'q') { if (saved) break; else puts("unsaved"); } 
		else if (input[0] == 'h') { puts("q h u n b e a p c g d w f t r l s"); fflush(stdout); }
		else if (input[0] == 'u') { puts("incr"); fflush(stdout); if (cursor < text_length) cursor++; } 
		else if (input[0] == 'n') { puts("decr"); fflush(stdout); if (cursor) cursor--; } 
		else if (input[0] == 'b') { puts("cursor at begin"); fflush(stdout); cursor = 0; } 
		else if (input[0] == 'e') { puts("cursor at end"); fflush(stdout); cursor = text_length; } 
		else if (input[0] == 'a') { puts("anchored at cursor"); fflush(stdout); anchor = cursor; } 
		else if (input[0] == 'p') { puts("set page size"); fflush(stdout); page_size = (nat) atoi(string); }
		else if (input[0] == 'c') {
			printf("filename: %s\n"
				"c%llu,a%llu,p%llu,l%llu\n", 
				filename, cursor, anchor, 
				page_size, text_length
			); fflush(stdout);

		} else if (input[0] == 'g') { 
			cursor = (nat) atoi(string); 
			if (cursor >= text_length) cursor = text_length;
			printf("set: c%llu\n", cursor); fflush(stdout);

		} else if (input[0] == 'd') {
			nat amount = text_length - cursor;
			if (amount > page_size) amount = page_size;
			fwrite(text + cursor, 1, amount, stdout); fflush(stdout);

		} else if (input[0] == 'w') {
			if (anchor > text_length) anchor = text_length;
			nat amount = text_length - anchor;
			if (amount > page_size) amount = page_size;
			fwrite(text + anchor, 1, amount, stdout); fflush(stdout);

		} else if (input[0] == 'f') {
			nat t = 0;
			for (nat i = cursor; i < text_length; i++) {
				if (text[i] == string[t]) {
					t++;
					if (t == string_length) { cursor = i + 1; goto found; } 
				} else t = 0;
			}
			printf("not found: \"%.*s\"\n", (int) string_length, string); fflush(stdout);
			continue;
		found:
			printf("found: c%llu\n", cursor); fflush(stdout);


		} else if (input[0] == 't') {
			printf("inserting %llub at %llu: \"%.*s\"\n", 
				string_length, cursor, 
				(int) string_length, string
			); fflush(stdout);

			text = realloc(text, text_length + string_length);
			memmove(text + cursor + string_length, text + cursor, text_length - cursor);
			memcpy(text + cursor, string, string_length);
			cursor += string_length;
			text_length += string_length;


		} else if (input[0] == 'l') { 					// technically not neccessary if we werent using canonical mode...
			printf("inserting newline at %llu: \n", cursor);
			text = realloc(text, text_length + 1);
			memmove(text + cursor + 1, text + cursor, text_length - cursor);
			memcpy(text + cursor, "\n", 1);
			cursor += 1; text_length += 1;


		} else if (input[0] == 'r') {
			printf("removing: range %llu .. %llu\n", anchor, cursor); fflush(stdout);
			if (anchor > cursor) {
				nat temp = anchor;
				anchor = cursor;
				cursor = temp;
			}
			const nat len = cursor - anchor;
			char* deleted = strndup(text + anchor, len);

			printf("warning: about to remove %llub: <<<%s>>>\nconfirm: (y/n) ", len, deleted); fflush(stdout);
			char c[3] = {0};
			read(0, &c, 2);
			if (c[0] == 'y') { puts("removing..."); fflush(stdout); } 
			else { puts("not removing"); fflush(stdout); goto skip_remove; } 

			memmove(text + anchor, text + cursor, text_length - cursor);
			text_length -= len;
			cursor = anchor;
			text = realloc(text, text_length);
			printf("info: removed %llub: <<<%s>>>\n", len, deleted); fflush(stdout);
		skip_remove:;
			free(deleted);

		} else if (input[0] == 's') {

			const mode_t permissions = S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;

			printf("saving: %s: %llub: testing timestamps...\n", filename, text_length);fflush(stdout);
			struct stat attr;
			stat(filename, &attr);
			char* new_last_saved = strdup(ctime(&attr.st_mtime));
			printf("save: current last modified time: %s\n", new_last_saved);fflush(stdout);
			printf("save: existing last modfied time: %s\n", last_saved);fflush(stdout);

			if (not strcmp(new_last_saved, last_saved)) {
				puts("matched timestamps: saving...");fflush(stdout);
				printf("saving: %s: %llub\n", filename, text_length);fflush(stdout);

				int output_file = open(filename, O_WRONLY | O_TRUNC, permissions);
				if (output_file < 0) { 
					printf("error: save: %s\n", strerror(errno));fflush(stdout);
				} 
				write(output_file, text, text_length);
				close(output_file);
				free(last_saved);
				struct stat attrn;
				stat(filename, &attrn);
				last_saved = strdup(ctime(&attrn.st_mtime));
				saved = 1;

			} else {
				puts("timestamp mismatch: refusing to save to previous location");fflush(stdout);
				puts("force saving file contents to a new file...");fflush(stdout);

				char name[4096] = {0};
				srand((unsigned)time(0)); rand();
				char datetime[32] = {0};
				struct timeval t = {0};
				gettimeofday(&t, NULL);
				struct tm* tm = localtime(&t.tv_sec);
				strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
				snprintf(name, sizeof name, "%s_%08x%08x_forcesave.txt", 
					datetime, rand(), rand());
				int flags = O_WRONLY | O_CREAT | O_EXCL;

				printf("saving: %s: %llub\n", name, text_length);fflush(stdout);
				int output_file = open(name, flags, permissions);
				if (output_file < 0) { 
					printf("error: save: %s\n", strerror(errno));
				} 
				write(output_file, text, text_length);
				close(output_file);

				printf("would you like to reload the file? (y/n)");fflush(stdout);
				char c[3] = {0};
				read(0, &c, 2);
				if (c[0] == 'y') { puts("reloading.."); fflush(stdout);free(text); goto load_file; } 
				else puts("warning: not reloading."); 
				fflush(stdout);
			}

		} else {
			puts("error: bad input");fflush(stdout);
		}

		if (input[0] != 's') saved = 0;
	}

	puts("exiting editor...");fflush(stdout);
	exit(0);
}










