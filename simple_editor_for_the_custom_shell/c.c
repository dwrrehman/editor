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

/*

workman key layout:

q d r w b j f u p ; 
 a s h t g y n e o i 
  z x m c v k l , .


left:
. . . . b j . . . ; 
 . . . . . y . . . i 
  z x . . v . . 

*/

static const char* help_string = 
"q	quit, must be saved first.\n"
"h	this help string.\n"
"s	save the file. if lastwritten time doesnt matter, or if \n"
"	   unable to get write access, then performs an emengency save.\n"
"c	display cursor/file information.\n"
"p<N>	set page size to N.\n"
"d	display page of text starting from cursor.\n"
"w	display page of text starting from anchor.\n"
"a	place anchor at cursors position.\n"
"u	next, cursor increment.\n"
"n	previous, cursor decrement.\n"
"o	beginning cursor setzero.\n"
"e	end, cursor set to document length.\n"
"m	inserts the current datetime at the position\n"
"	   of the cursor, and advances the cursor.\n"
"g<N>	set the cursor to a file offset N.\n"
"f<s>	search forwards starting from cursor for string \"s\".\n"
"t<s>	insert string \"s\" at position of cursor, and advance \n"
"	   cursor by length of s.\n"
"l	insert neline character at position of \n"
"	   cursor, and advance cursor by 1\n"
"r	removes all characters between anchor and \n"
"	   cursor. requires confirming.\n"
"x	unused\n";


int main(void) {
	srand((unsigned) time(NULL)); rand();

	char filename[4096] = {0};
	write(1, "filename: ", 10);
	read(0, filename, sizeof filename);
	size_t filename_length = strlen(filename);
	if (filename_length) filename[--filename_length] = 0;

load_file:
	printf("loading file: %s\n", filename); 
	fflush(stdout);

	int file = open(filename, O_RDONLY);
	if (file < 0) {
		printf("error: open(r): %s\n", strerror(errno)); 
		fflush(stdout);
		exit(1);
	}

	nat text_length = (nat) lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	char* text = malloc(text_length);
	read(file, text, text_length);
	close(file);

	char last_modified[32] = {0};
	struct stat attr_;
	stat(filename, &attr_);
	strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", 
		localtime(&attr_.st_mtime)
	);
	printf("Last modified time: %s\n", last_modified); 
	fflush(stdout);

	nat cursor = 0, anchor = 0, page_size = 1000, saved = 1;
	char input[4096] = {0};

	printf("read %llu bytes\n", text_length); 
	fflush(stdout);	

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

		if (input[0] == 'q') { 
			if (saved) break; 
			else puts("unsaved"); 
		} 
		else if (input[0] == 'h') { 
			puts(help_string); 
			fflush(stdout); 
		}
		else if (input[0] == 'u') { 
			puts("incr"); 
			fflush(stdout); 
			if (cursor < text_length) cursor++; 
		} 
		else if (input[0] == 'n') { 
			puts("decr"); 
			fflush(stdout); 
			if (cursor) cursor--; 
		} 
		else if (input[0] == 'o') { 
			puts("cursor at begin"); 
			fflush(stdout); 
			cursor = 0; 
		}
		else if (input[0] == 'e') { 
			puts("cursor at end"); 
			fflush(stdout); 
			cursor = text_length; 
		} 
		else if (input[0] == 'a') { 
			puts("anchored at cursor"); 
			fflush(stdout); 
			anchor = cursor; 
		} 
		else if (input[0] == 'p') { 
			puts("set page size"); 
			fflush(stdout); 
			page_size = (nat) atoi(string); 
		}
		else if (input[0] == 'c') {
			printf("filename: %s %s\n"
				"last modified: %s\n"
				"c%llu,a%llu,p%llu,l%llu\n", 
				filename, 
				saved ? "(saved)" : 
				"[contains unsaved changes]", 
				last_modified,
				cursor, anchor, page_size, text_length
			); fflush(stdout);

		} else if (input[0] == 'g') { 
			cursor = (nat) atoi(string); 
			if (cursor >= text_length) cursor = text_length;
			printf("set: c%llu\n", cursor); 
			fflush(stdout);

		} else if (input[0] == 'd') {
			nat amount = text_length - cursor;
			if (amount > page_size) amount = page_size;
			fwrite(text + cursor, 1, amount, stdout); 
			fflush(stdout);

		} else if (input[0] == 'w') {
			if (anchor > text_length) anchor = text_length;
			nat amount = text_length - anchor;
			if (amount > page_size) amount = page_size;
			fwrite(text + anchor, 1, amount, stdout); 
			fflush(stdout);

		} else if (input[0] == 'f') {
			nat t = 0;
			for (nat i = cursor; i < text_length; i++) {
				if (text[i] == string[t]) {
					t++;
					if (t == string_length) { 
						cursor = i + 1; 
						goto found; 
					} 
				} else t = 0;
			}
			printf("not found: \"%.*s\"\n", 
				(int) string_length, string
			);
			fflush(stdout);
			continue;
		found:	printf("found: c%llu\n", cursor); 
			fflush(stdout);

		} else if (input[0] == 't') {
			printf("inserting %llub at %llu: \"%.*s\"\n", 
				string_length, cursor, 
				(int) string_length, string
			); 
			fflush(stdout);
			text = realloc(text, text_length + string_length);
			memmove(text + cursor + string_length, 
				text + cursor, text_length - cursor
			);
			memcpy(text + cursor, string, string_length);
			cursor += string_length;
			text_length += string_length;
			saved = 0;

		} else if (input[0] == 'k') {
			char dt[32] = {0};
			struct timeval t = {0};
			gettimeofday(&t, NULL);
			struct tm* tm = localtime(&t.tv_sec);
			strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
			const nat dt_length = (nat) strlen(dt);

			printf("inserting dt-string of %llub at %llu: \"%.*s\"\n", 
				dt_length, cursor, 
				(int) dt_length, dt
			); 
			fflush(stdout);
			text = realloc(text, text_length + dt_length);
			memmove(text + cursor + dt_length, 
				text + cursor, text_length - cursor
			);
			memcpy(text + cursor, dt, dt_length);
			cursor += dt_length;
			text_length += dt_length;
			saved = 0;

		} else if (input[0] == 'l') { // required from canonical mode.
			printf("inserting newline at %llu: \n", cursor);
			text = realloc(text, text_length + 1);
			memmove(text + cursor + 1, text + cursor, text_length - cursor);
			memcpy(text + cursor, "\n", 1);
			cursor += 1; 
			text_length += 1;
			saved = 0;

		} else if (input[0] == 'r') {
			printf("removing: range %llu .. %llu\n", 
				anchor, cursor
			);
			fflush(stdout);
			if (anchor > cursor) {
				nat temp = anchor;
				anchor = cursor;
				cursor = temp;
			}
			const nat len = cursor - anchor;
			char* deleted = strndup(text + anchor, len);

			printf("warning: about to remove %llub: "
				"<<<%s>>>\nconfirm: (y/n) ", 
				len, deleted
			); 
			fflush(stdout);

			char c[3] = {0};
			read(0, &c, 2);
			if (c[0] == 'y') { 
				puts("removing..."); 
				fflush(stdout); 
			} else { 
				puts("not removing"); 
				fflush(stdout); 
				goto skip_remove; 
			} 

			memmove(text + anchor, text + cursor, 
				text_length - cursor
			);
			saved = 0;
			text_length -= len;
			cursor = anchor;
			text = realloc(text, text_length);
			printf("info: removed %llub: <<<%s>>>\n", 
				len, deleted
			); 
			fflush(stdout);
		skip_remove: 
			free(deleted);

		} else if (input[0] == 's') {

			const mode_t permissions = 
				S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;
			printf("saving: %s: %llub: testing timestamps...\n", 
				filename, text_length
			);
			fflush(stdout);

			char new_last_modified[32] = {0};
			struct stat attr;
			stat(filename, &attr);
			strftime(new_last_modified, 32, "1%Y%m%d%u.%H%M%S", 
				localtime(&attr.st_mtime)
			);
			printf("save: current last modified time: %s\n", new_last_modified);
			fflush(stdout);
			printf("save: existing last modfied time: %s\n", last_modified);
			fflush(stdout);

			if (not strcmp(new_last_modified, last_modified)) {

				puts("matched timestamps: saving...");
				fflush(stdout);

				printf("saving: %s: %llub\n", filename, text_length);
				fflush(stdout);

				int output_file = open(filename, O_WRONLY | O_TRUNC, permissions);
				if (output_file < 0) { 
					printf("error: save: %s\n", strerror(errno));
					fflush(stdout);
					goto emergency_save;
				}

				write(output_file, text, text_length);  // TODO: check these. 
				close(output_file);

				struct stat attrn;
				stat(filename, &attrn);
				strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", 
					localtime(&attrn.st_mtime)
				);
				saved = 1;

			} else {
				puts("timestamp mismatch: refusing to save to prior path");
				fflush(stdout);
				puts("force saving file contents to a new file...");
				fflush(stdout);

				char name[4096] = {0};
				char datetime[32] = {0};
				struct timeval t = {0};
				gettimeofday(&t, NULL);
				struct tm* tm = localtime(&t.tv_sec);
				strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
				snprintf(name, sizeof name, "%s_%08x%08x_forcesave.txt", 
					datetime, rand(), rand());
				int flags = O_WRONLY | O_CREAT | O_EXCL;

				printf("saving: %s: %llub\n", name, text_length);
				fflush(stdout);
				int output_file = open(name, flags, permissions);
				if (output_file < 0) { 
					printf("error: save: %s\n", strerror(errno));
					fflush(stdout);
					goto emergency_save;
				} 
				write(output_file, text, text_length); // TODO: check these. 
				close(output_file);

				printf("would you like to reload the file? (y/n)");
				fflush(stdout);

				char c[3] = {0};
				read(0, &c, 2);
				if (c[0] == 'y') { 
					puts("reloading.."); 
					fflush(stdout);
					free(text);
					goto load_file; 
				}
				else puts("warning: not reloading."); 
				fflush(stdout);
			}
			continue;
		emergency_save:
			puts("printing stored document so far: ");
			fflush(stdout);
			fwrite(text, 1, text_length, stdout);
			fflush(stdout);
			puts("emergency save: force saving to the home directory.");
			fflush(stdout);
			puts("force saving file contents to a new file at /Users/dwrr/...");
			fflush(stdout);

			char name[4096] = {0};
			char datetime[32] = {0};
			struct timeval t = {0};
			gettimeofday(&t, NULL);
			struct tm* tm = localtime(&t.tv_sec);
			strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
			snprintf(name, sizeof name, 
				"/Users/dwrr/%s_%08x%08x_emergency_save.txt", 
				datetime, rand(), rand()
			);
			int flags = O_WRONLY | O_CREAT | O_EXCL;

			printf("saving: %s: %llub\n", name, text_length);
			fflush(stdout);

			int output_file = open(name, flags, permissions);
			if (output_file < 0) { 
				printf("error: save: %s\n", strerror(errno));
				fflush(stdout);
				puts("failed emergency save. try again? ");
				char c[3] = {0};
				read(0, &c, 2);
				if (c[0] == 'y') { 
					puts("trying to emergency save again..."); 
					fflush(stdout);
					goto emergency_save; 
				} else 
				puts("warning: not saving again, exiting editor."); 
				fflush(stdout);
				break;
			} 
			write(output_file, text, text_length); // TODO: check these. 
			close(output_file);
		} else {
			printf("error: bad input: %d\n", input[0]);
			fflush(stdout);
		}
	}
	puts("exiting editor...");
	fflush(stdout);
	exit(0);
}










