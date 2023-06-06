#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <iso646.h>
#include <stdint.h>
#include <stdbool.h>


int main(void) {
	char* inserted = NULL; 
	size_t inserted_length = 0;
	char input[512] = {0};

loop: 	fgets(input, sizeof input, stdin);

	if (not strcmp(input, ".\n")) { if (inserted_length) inserted_length--; } 
	else {
		const size_t length = strlen(input);
		inserted = realloc(inserted, inserted_length + length);
		memcpy(inserted + inserted_length, input, length);
		inserted_length += length;
		printf("appending... (%lu)\n", inserted_length);
		goto loop;
	}

	printf("\n\trecieved inserted(%lu): \n\n\t\t\"", inserted_length);
	fwrite(inserted, inserted_length, 1, stdout); 
	printf("\"\n\n\n\n");

	if (inserted_length == 4 and not strncmp(inserted, "quit", 4)) goto done;

	inserted_length = 0;
	goto loop;
done:
	puts("quitting...");
}



/* 512 chars:    per command. 

xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx


*/









/*
static struct termios terminal = {0};





//static void clear_screen(void) { printf("\033[2J\033[H"); }
static inline void restore_terminal(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal); }
//static inline char read_from_stdin(void) { char c = 0; read(0, &c, 1); return c; }

static inline void configure_terminal(void) {
	tcgetattr(STDIN_FILENO, &terminal);
	atexit(restore_terminal);
	struct termios raw = terminal;
	raw.c_lflag &= ~((size_t)ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}




static int get(char** buffer, size_t* count) {
	char c = 0, p = 0;
	*count = 0;
	while (1) {
		ssize_t n = read(0, &c, 1);
		if (n <= 0) goto error;
		if (c == '\n' and p == '`') { (*count)--; break; }
		*buffer = realloc(*buffer, *count + 1);
		(*buffer)[(*count)++] = c;
		p = c;
	}
	return 0;
error:
	printf("encountered error: ");
	perror("read");
	return 1;
}
*/

/*
static size_t input(char** buffer, size_t* capacity) {
	char c = 0, p = 0;
	size_t count = 0;
loop: 	if (read(0, &c, 1) <= 0) return count;
	if (c != 'c' or p != 'h') goto resize;
	if (read(0, &c, 1) == 10) return --count;
resize: if (count + 1 <= *capacity) goto push;
	*buffer = realloc(*buffer, ++*capacity);
push: 	(*buffer)[count++] = c; p = c; goto loop;
}
*/








/*

int main(void) {

	configure_terminal();

	char* input = NULL; 
	char p = 0, d = 0;
	size_t input_length = 0, capacity = 0;


	while (1) {
		


	input_length = 0;
	char w = 0;
	d = 0;
	p = 0;

read_char:;
	w = 0;
	if (read(0, &w, 1) <= 0) {
		printf("FATAL ERROR: error in read() call!!\n");
		goto process;
	}

	if (w != 't' or p != 'r' or d != 'd') goto resize;

	if (input_length >= 2) input_length -= 2;
	else {
		printf("FATAL ERROR: error, the input needs to be at "
			"least 2 chars because of drt seq. input_length = %lu\n", 
			input_length
		);
	}

	goto process;

resize: 
	if (input_length + 1 < capacity) goto push;
	input = realloc(input, ++capacity);
push: 	
	input[input_length++] = w; 
	d = p;
	p = w;
	goto read_char; 


process: 

	printf("\n\trecieved input(%lu): \n\n\t\t\"", input_length);
	fwrite(input, input_length, 1, stdout); 
	printf("\"\n\n\n\n");

	fflush(stdout);

	if (input_length == 4 and not strncmp(input, "quit", 4)) break;

		
	}

	puts("quitting...");
	restore_terminal();
}
*/






























/*
char* get(char str[], size_t len, int fileno) {
    size_t count;
    for (count = 0; count < len; ++count) {
        int result;
        if (result = read(fileno, str + count, 1), result != 1) {
            if (result == 0) {
                if (count > 0) {
                    str[count] = 0;
                    return str;
                } else {
                    if (count == 0) {
                        return NULL;
                    }
                }
            } else {
                perror("read");
                return NULL;
            }
        } else {
            if (str[count] == '\n') {
                if (count < len - 1) {
                    str[count + 1] = 0;
                } else {
                    str[count] = 0;
                }
                return str;
            }
        }
    }
    if (count < len) {
        str[count] = 0;
    }
    return str;
}

*/

