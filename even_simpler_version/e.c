#include <stdio.h>    // a character-based modal programmable ed-like command-line text editor for my own use. 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iso646.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>

static int mode = 1;
static void handler(int __attribute__((unused))_) { mode = 2; }
int main(int argc, const char** argv) {
	int delimiter = '`';
	char* text = NULL, * input = NULL;
	size_t capacity = 0, count = 0, cursor = 0, saved = 1;
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
	fclose(file);
	strlcpy(filename, argv[1], sizeof filename);
	printf("%s: %lu", filename, count);
	mode = 2;
loop:;	ssize_t r = getdelim(&input, &capacity, delimiter, stdin);
	if (r <= 0) goto save_exit; getchar();
	size_t length = (size_t) r;
	input[--length] = 0;

	if (mode == 1) {
		text = realloc(text, count + length);
		memcpy(text + cursor + length, text + cursor, count - cursor);
		for (size_t i = 0; i < length; i++) text[cursor + i] = input[i];
		count += length; cursor += length; mode = 2; saved = 0;
	} else if (mode == 2) {
		if (false) {}
		else if (not strcmp(input, "")) puts("[empty input]");
		else if (not strcmp(input, "quit") and saved) mode = 0;
		else if (not strcmp(input, "t")) mode = 1;
		else if (not strcmp(input, "clear")) printf("\033[2J\033[H");
		else if (not strcmp(input, "delimiter")) delimiter = getchar();
		else if (not strcmp(input, "print_contents")) { fwrite(text, count, 1, stdout); puts(""); }
		else if (not strcmp(input, "print")) { fwrite(text + cursor, count - cursor, 1, stdout); puts(""); }
		else if (not strcmp(input, "print_cursor")) printf("%lu\n", cursor);
		else if (not strcmp(input, "count")) printf("%lu\n", count);
		else if (not strcmp(input, "where")) printf("here\n");
		else puts("? unknown command.");
	}
	if (mode) goto loop; 
save_exit:
	;
done: 	printf("exiting...\n");
}




















/*

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

*/



//printf("debug: received command: #");
//fwrite(input, length, 1, stdout);
//puts("#");


// #include <unistd.h>
