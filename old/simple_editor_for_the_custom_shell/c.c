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
#include <stdnoreturn.h>
#include <signal.h>


typedef uint64_t nat;

/*

workman key layout:

q d r w b j f u p ; 
 a s h t g y n e o i 
  z x m c v k l , .


keys left:
. . . . b j f . . ; 
 . . . . g y . . . . 
  z x . . v . l , .


*/

static const char* help_string = 
"q	quit, must be saved first.\n"
"z	this help string.\n"
"s	save the current file contents.\n"
"<sp>	exit insert mode.\n"
"<sp>	move up one line.\n"
"<nl>	move down one line.\n"
"a	anchor at cursor.\n"
"p	display cursor/file information.\n"
"t	go into insert mode.\n"
"u<N>	cursor += N.\n"
"n<N>	cursor -= N.\n"
"c<N>	cursor = N.\n"
"d[N]	display page of text starting from cursor.\n"
"w[N]	display page of text starting from anchor.\n"
"m<S>	search forwards from cursor for string S.\n"
"h<S>	search backwards from cursor for string S.\n"
"o	copy current selection to clipboard.\n"
"k	inserts current datetime at cursor and advances.\n"
"e<S>	inserts S at cursor and advances.\n"
"i	inserts clipboard at cursor, and advances.\n"
"r	removes current selection. requires confirming.\n";



static char* text = NULL;
static nat text_length = 0;


static void emergency_save(const char* call) {
	printf("save error: %s: %s\n", call, strerror(errno)); fflush(stdout); sleep(2);
	puts("printing stored document: "); fflush(stdout); sleep(1);
	fwrite(text, 1, count, stdout); fflush(stdout); sleep(1);
	fwrite(text, 1, count, stdout); fflush(stdout); sleep(1);
	puts("emergency save: printed out all contents"); fflush(stdout); sleep(1);
	printf("save error: %s: %s\n", call, strerror(errno)); fflush(stdout); sleep(2);
	return; 
}

static void save(void) {
	const mode_t permissions = S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;
	int flags = 0;
	bool matches = false;
	if (not *filename) {
		char name[4096] = {0}, datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(name, sizeof name, "%s_%08x%08x.txt", datetime, rand(), rand());
		strlcpy(filename, name, sizeof filename);
		flags = O_WRONLY | O_CREAT | O_EXCL;
		matches = true; 
	} else {
		char empirically_last_modified[32] = {0};
		struct stat attr;
		stat(filename, &attr);
		strftime(empirically_last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attr.st_mtime));
		if (not strcmp(empirically_last_modified, last_modified)) {
			flags = O_WRONLY | O_TRUNC;
			matches = true; 
		}
	}
	if (matches) {
		int output_file = open(filename, flags, permissions);
		if (output_file < 0) { emergency_save("open"); goto recover; }
		if (write(output_file, text, count) < 0){ emergency_save("write"); goto recover; }
		if (close(output_file) < 0) { emergency_save("close"); goto recover; }
		struct stat attrn;
		stat(filename, &attrn);
		strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", localtime(&attrn.st_mtime));
		return;
	}
	recover: write(1, "\033[?25h", 6); tcsetattr(0, TCSANOW, &terminal);
	printf("--- emergency save ---\nplease give a valid filename.\n(enter: <path>\\n)\n");
	loop: fflush(stdout); char input[4096] = {0};
	printf(":: "); fflush(stdout);
	const ssize_t n = read(0, input, sizeof input);
	if (n <= 0) {
		emergency_save("read");
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u_%H%M%S", tm);
		snprintf(input, sizeof input, "/Users/dwrr/%s_%08x%08x_emergency.txt", datetime, rand(), rand());
	} else input[n - 1] = 0;
	int output_file = open(input, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, permissions);
	if (output_file < 0) { emergency_save("open"); goto loop; }
	if (write(output_file, text, count) < 0) { emergency_save("write"); goto loop; }
	if (close(output_file) < 0) { emergency_save("close"); goto loop; }
	printf("successfully emergency saved, exiting editor..."); fflush(stdout); 
	exit(0);
}

static noreturn void interrupted(int _) {if(_){} 
	puts("interrupted, saving and exiting...");
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
	const int flags = O_WRONLY | O_CREAT | O_EXCL;
	const mode_t permissions = 
		S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP;

	printf("saving: %s: %llub\n", name, text_length);
	fflush(stdout);
	
	int output_file = open(name, flags, permissions);
	if (output_file < 0) { 
		printf("error: save: %s\n", strerror(errno));
		fflush(stdout);
	} 

	write(output_file, text, text_length); // TODO: check these. 
	close(output_file);
	puts("done force saving, exiting");
	exit(1); 
}

int main(int argc, __attribute__((unused)) const char** argv) {
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);

	srand((unsigned) time(NULL)); rand();

	if (argc > 1) return puts("usage error, no arguments should be given.");
	char filename[4096] = {0};
	write(1, "filename: ", 10);
	read(0, filename, sizeof filename);
	size_t filename_length = strlen(filename);
	if (filename_length) filename[--filename_length] = 0;

	puts("type 'z' and enter for help.");

load_file:
	printf("loading file: %s\n", filename); 
	fflush(stdout);

	int file = open(filename, O_RDONLY);
	if (file < 0) {
		printf("error: open(r): %s\n", strerror(errno)); 
		fflush(stdout);
		exit(1);
	}

	text_length = (nat) lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	text = malloc(text_length);
	read(file, text, text_length);
	close(file);

	char last_modified[32] = {0};
	struct stat attr_;
	stat(filename, &attr_);
	strftime(last_modified, 32, "1%Y%m%d%u.%H%M%S", 
		localtime(&attr_.st_mtime)
	);
	printf("last modified time: %s\n", last_modified); 
	fflush(stdout);

	nat cursor = 0, anchor = 0, saved = 1, inserting = 0;
	char input[4096] = {0};

	char* clipboard = NULL;
	nat cliplength = 0;

	printf("read %llu bytes\n", text_length); 
	fflush(stdout);

	while (1) {
		memset(input, 0, sizeof input);
		ssize_t n = read(0, input, sizeof input);
		if (n < 0) { 
			printf("read: %s\n", strerror(errno));
			fflush(stdout); 
			sleep(1); 
		} 
		else if (n == 0) { write(1, "EOF???", 6); sleep(1); }

		if (inserting) {
			if (n == 2 and input[0] == ' ') { write(1, ".\n", 2); inserting = 0; continue; }
			const char* string = input;
			const nat string_length = (nat) n;
			text = realloc(text, text_length + string_length);
			memmove(text + cursor + string_length, 
				text + cursor, text_length - cursor
			);
			memcpy(text + cursor, string, string_length);
			cursor += string_length;
			text_length += string_length;
			saved = 0;
			continue;
		} 

		nat input_length = (nat) n;
		if (input_length) input[--input_length] = 0;
		else continue;

		const char* string = input + 1;
		const nat string_length = input_length - 1;

		if (not input[0]) {}
		else if (input[0] == ' ') write(1, "\033[2A", 4); 
		else if (input[0] == 'z') { puts(help_string); fflush(stdout); }
		else if (input[0] == 'q') { if (saved) break; else puts("unsaved"); }
		else if (input[0] == 'a') { anchor = cursor; } 
		else if (input[0] == 't') { inserting = 1; }
		else if (input[0] == 'u') { 
			cursor += (nat) atoi(string); 
			if (cursor > text_length) cursor = text_length; 
			printf("c%llu\n", cursor); 
			fflush(stdout); 
		} else if (input[0] == 'n') { 
			cursor -= (nat) atoi(string); 
			if ((int64_t) cursor < (int64_t) 0) cursor = 0; 
			printf("c%llu\n", cursor); 
			fflush(stdout); 
		} else if (input[0] == 'c') { 
			cursor = (nat) atoi(string); 
			if (cursor > text_length) cursor = text_length; 
			printf("c%llu\n", cursor); 
			fflush(stdout); 
		} else if (input[0] == 'p') {
			printf("file: %s %s\nlast modified: %s\nc%llu,a%llu,len%llu\n", 
				filename, saved ? "(saved)" : "[contains unsaved changes]", 
				last_modified, cursor, anchor, text_length
			); fflush(stdout);
		} else if (input[0] == 'd') {
			nat start = cursor;
			nat k = (nat) atoi(string); 
			if (not k) k = 1000;
			nat amount = text_length - start;
			if (k and k < amount) amount = k;
			fwrite(text + start, 1, amount, stdout); fflush(stdout);
		} else if (input[0] == 'w') {
			nat start = anchor;
			nat k = (nat) atoi(string); 
			if (not k) k = 1000;
			nat amount = text_length - start;
			if (k and k < amount) amount = k;
			fwrite(text + start, 1, amount, stdout); fflush(stdout);


		} else if (input[0] == 'm') {
			nat t = 0;
			for (nat i = cursor; i < text_length; i++) {
				if (text[i] == string[t]) {
					t++;
					if (t == string_length) { cursor = i + 1; goto found; } 
				} else t = 0;
			}
			printf("not found: \"%.*s\"\n", (int) string_length, string);
			fflush(stdout);
			continue;
		found:	printf("c%llu\n", cursor); 
			fflush(stdout);

		} else if (input[0] == 'h') {
			nat t = string_length;
			for (nat i = cursor; i--; ) {
				t--;
				if (text[i] == string[t]) {	
					if (not t) { cursor = i; goto foundb; } 
				} else t = string_length;
			}
			printf("not found: \"%.*s\"\n", (int) string_length, string);
			fflush(stdout);
			continue;
		foundb:	printf("c%llu\n", cursor); 
			fflush(stdout);

		} else if (input[0] == 'e') {

			text = realloc(text, text_length + string_length);
			memmove(text + cursor + string_length, 
				text + cursor, text_length - cursor
			);
			memcpy(text + cursor, string, string_length);
			cursor += string_length;
			text_length += string_length;
			saved = 0;

		} else if (input[0] == 'i') {
			text = realloc(text, text_length + cliplength);
			memmove(text + cursor + cliplength, 
				text + cursor, text_length - cursor
			);
			memcpy(text + cursor, clipboard, cliplength);
			cursor += cliplength;
			text_length += cliplength;
			saved = 0;

		} else if (input[0] == 'k') {

			char dt[32] = {0};
			struct timeval t = {0};
			gettimeofday(&t, NULL);
			struct tm* tm = localtime(&t.tv_sec);
			strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
			const nat dt_length = (nat) strlen(dt);

			text = realloc(text, text_length + dt_length);
			memmove(text + cursor + dt_length, 
				text + cursor, text_length - cursor
			);
			memcpy(text + cursor, dt, dt_length);
			cursor += dt_length;
			text_length += dt_length;
			saved = 0;

		} else if (input[0] == 'o') {

			cliplength = anchor < cursor ? cursor - anchor : anchor - cursor;
			free(clipboard);
			clipboard = strndup(text + (anchor < cursor ? anchor : cursor, cliplength);

		} else if (input[0] == 'r') {
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
			save();

		} else {
			printf("error: bad input: %d\n", input[0]);
			fflush(stdout);
		}
	}
	puts("exiting editor...");
	fflush(stdout);
	exit(0);
}





















/*


			//printf("inserting %llub at %llu: \"%.*s\"\n", 
			//	string_length, cursor, 
			//	(int) string_length, string
			//); 
			//fflush(stdout);



			printf("inserting %llub at %llu: \"%.*s\"\n", 
				string_length, cursor, 
				(int) string_length, string
			); 
			fflush(stdout);




			printf("inserting dt-string of %llub at %llu: \"%.*s\"\n", 
				dt_length, cursor, 
				(int) dt_length, dt
			); 
			fflush(stdout);



			printf("copied %llub: <<<%s>>>\n", cliplength, clipboard);
			fflush(stdout);




			printf("copying: range %llu .. %llu\n", 
				anchor, cursor
			);
			fflush(stdout);



			printf("removing: range %llu .. %llu\n", 
				anchor, cursor
			);
			fflush(stdout);



			printf("inserting clipboard of %llub at %llu: \"%.*s\"\n", 
				cliplength, cursor, 
				(int) cliplength, clipboard
			); 
			fflush(stdout);


*/



































/*

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


*/















/*


else if (input[0] == 'p') { 
			puts("set page size"); 
			fflush(stdout); 
			page_size = (nat) atoi(string); 
		}


*/


