#include <stdio.h>    // a character-based modal programmable ed-like command-line text editor for my own use. 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iso646.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
static void handler(int __attribute__((unused))_) {}
int main(int argc, const char** argv) {
	char* text = NULL, * input = NULL;
	size_t capacity = 0, count = 0, cursor = 0, anchor = 0, saved = 1, mode = 1, max = 128;
	char filename[4096] = {0};
	struct sigaction action = {.sa_handler = handler};
	sigaction(SIGINT, &action, NULL);

	if (argc < 2) goto loop;

	FILE* file = fopen(argv[1], "r");
	if (not file) { perror("fopen"); goto done; }
	fseek(file, 0, SEEK_END);
        count = (size_t) ftell(file); text = malloc(count);
        fseek(file, 0, SEEK_SET); fread(text, 1, count, file);
	fclose(file); mode = 2; 
	strlcpy(filename, argv[1], sizeof filename);
	printf("%lu\n", count);
	cursor = 0;

loop:;	ssize_t r = getline(&input, &capacity, stdin);
	if (r <= 0) goto save_exit;
	size_t length = (size_t) r;
	if (mode == 1) {
		if (length >= 2 and input[length - 2] == '`') { 
			length -= 2; mode = 2; puts("."); 
		}
		text = realloc(text, count + length);
		memmove(text + cursor + length, text + cursor, count - cursor);
		for (size_t i = 0; i < length; i++) text[cursor + i] = input[i];
		count += length; cursor += length; saved = 0;
		
	} else if (mode == 2) {
		input[--length] = 0;
		if (not strcmp(input, "")) puts("[empty]");
		else if (not strcmp(input, "clear")) printf("\033[2J\033[H");
		else if (not strcmp(input, "quit")) { if (saved) mode = 0; else puts("modified"); }

		else if (not strcmp(input, "discard_and_quit")) mode = 0;
		else if (not strcmp(input, "insert")) mode = 1;
		else if (not strcmp(input, "drop")) anchor = cursor;
		else if (not strcmp(input, "back")) cursor = anchor;
		else if (not strcmp(input, "top")) cursor = 0;
		else if (not strcmp(input, "bottom")) cursor = count - 1;
		else if (not strcmp(input, "right")) cursor++;
		else if (not strcmp(input, "left")) cursor--;
		else if (not strcmp(input, "filename")) puts(filename);
		else if (input[0] == '.') max = (size_t) atoi(input + 1);

		else if (not strcmp(input, "count")) printf("%lu %lu %lu\n", count, anchor, cursor);
		else if (not strcmp(input, "print_contents")) { fwrite(text, count, 1, stdout); }
		else if (not strcmp(input, "print_after")) { fwrite(text + cursor, count - cursor, 1, stdout);  }

		else if (not strcmp(input, "print")) { 
			fwrite(text + cursor, count - cursor < max ? count - cursor : max, 1, stdout); 
			puts("");
		}

		else if (not strcmp(input, "print_anchor")) { 
			fwrite(text + anchor, count - anchor < max ? count - anchor : max, 1, stdout); 
			puts("");
		}



		else if (not strcmp(input, "cut")) {
			if (cursor < anchor) { size_t temp = cursor; cursor = anchor; anchor = temp; }
			memmove(text + anchor, text + cursor, count - cursor);
			count -= cursor - anchor;
			text = realloc(text, count);
			cursor = anchor;
		}

		else if (input[0] == '/') {
			if (cursor >= count) { puts("/ error"); goto loop; }

			const char* offset = strstr(text + cursor + 1, input + 1);
			if (not offset) printf("absent %s\n", input + 1);
			else {
				cursor += (size_t) (offset - (text + cursor));
				printf("%lu\n", cursor);
			}
		}

		else if (input[0] == '\\') {
			puts("backwards is currently unimplemented");
		}

		else if (not strcmp(input, "up")) { 
			size_t save = cursor;
			if (cursor > max) cursor -= max; else cursor = 0;
			fwrite(text + cursor, save - cursor, 1, stdout); 
			puts("");
		} 

		else if (not strcmp(input, "down")) { 
			size_t total = count - cursor < max ? count - cursor : max;
			fwrite(text + cursor, total, 1, stdout); 
			puts("");
			cursor += total;
		}

		else if (not strncmp(input, "read ", 5)) { 
			if (not saved) { puts("modified"); goto loop; }
			FILE* infile = fopen(input + 5, "r");
			if (not infile) { perror("fopen"); goto loop; }

			fseek(infile, 0, SEEK_END); free(text); 
			count = (size_t) ftell(infile); text = malloc(count);
			fseek(infile, 0, SEEK_SET); fread(text, 1, count, infile);
			fclose(infile); mode = 2; 
			strlcpy(filename, input + 5, sizeof filename);
			printf("%s: %lu\n", filename, count);
		} 

		else if (not strncmp(input, "name ", 7)) {
			if (access(input + 7, F_OK) != -1) puts("file exists");
			else strlcpy(filename, input + 7, sizeof filename);
		}

		else if (not strcmp(input, "write")) {
			if (not *filename) { puts("unnamed"); goto loop; }
			FILE* output_file = fopen(filename, "w");
			if (not output_file) { perror("fopen"); goto loop; }
			fwrite(text, count, 1, output_file);
			fclose(output_file);
			saved = 1;
		}

		else printf("unintelligible %s\n", input);
	}
	if (mode) goto loop;
	save_exit:; done: printf("exiting...\n");
}









































































/*












int delimiter = '`';




ssize_t r = getdelim(&input, &capacity, mode == 1 ? delimiter : 10, stdin);
	if (r <= 0) goto save_exit;
	size_t length = (size_t) r;
	input[--length] = 0;






		else if (not strcmp(input, "clear")) printf("\033[2J\033[H");
		else if (not strcmp(input, "delimiter")) delimiter = getchar();
		else if (not strcmp(input, "print_contents")) { fwrite(text, count, 1, stdout); puts(""); }
		else if (not strcmp(input, "print")) { fwrite(text + cursor, count - cursor, 1, stdout); puts(""); }
		else if (not strcmp(input, "print_cursor")) printf("%lu\n", cursor);
		else if (not strcmp(input, "count")) printf("%lu\n", count);

		else if (input[0] == '/') {
			const char* offset = strstr(text, input + 1);
			if (not offset) printf("string %s not found.\n", input + 1);
			else {
				cursor = (size_t) (offset - text);
				printf("%lu\n", cursor);
			}

		} 








char filename[4096] = {0};
	struct sigaction action = {.sa_handler = handler};
	sigaction(SIGINT, &action, NULL);
	if (argc < 2) goto loop;
	FILE* file = fopen(argv[1], "r");
	if (not file) { perror("fopen"); goto done; }
	fseek(file, 0, SEEK_END);
        count = (size_t) ftell(file);
	text = malloc(count);
        fseek(file, 0, SEEK_SET);
        fread(text, 1, count, file);
	fclose(file); mode = 2; delimiter = 10;
	strlcpy(filename, argv[1], sizeof filename);
	printf("%s: %lu", filename, count);














	if (argc < 2) goto here;
	FILE* file = fopen(argv[1], "r");
	if (not file) { printf("error: fopen: %s", strerror(errno)); return 0; }
	fseek(file, 0, SEEK_END);
        size_t length = (size_t) ftell(file);
	char* local_text = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(local_text, sizeof(char), length, file);
	fclose(file);
	for (size_t i = 0; i < length; i++) insert(local_text[i]);
	cn = 0; cm = 0; on = 0; om = 0; cursor_moved = 0; mode = 1;
	free(local_text);



int main(int argc, const char** argv) {
	int delimiter = '`';
	char* text = NULL;
	char input[4096] = {0};
	size_t capacity = 0, count = 0, cursor = 0, saved = 1;
	
loop:;	ssize_t r = getdelim(&input, &capacity, mode == 1 ? delimiter : 10, stdin);
	if (r <= 0) goto save_exit;
	size_t length = (size_t) r;
	input[--length] = 0;

	if (mode == 1) {
		getchar();
		text = realloc(text, count + length);
		memmove(text + cursor + length, text + cursor, count - cursor);
		for (size_t i = 0; i < length; i++) text[cursor + i] = input[i];
		count += length; cursor += length; mode = 2; saved = 0;

	} else if (mode == 2) {

		if (not strcmp(input, "")) puts("[empty input]");
		else if (not strcmp(input, "quit")) if (saved) mode = 0; else puts("unsaved");
		else if (not strcmp(input, "t")) mode = 1;
		else printf("unintelligible %s\n", input);
	}

	if (mode) goto loop; 
save_exit:
	;
done: 	printf("exiting...\n");
}






*/



//printf("debug: received command: #");
//fwrite(input, length, 1, stdout);
//puts("#");


// #include <unistd.h>
