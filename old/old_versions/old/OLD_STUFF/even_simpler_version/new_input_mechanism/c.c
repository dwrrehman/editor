#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>

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

int main(void) {
	char* buffer = NULL; char c = 0, p = 0;
	size_t count = 0, capacity = 0;
	while (1) {
		count = 0;
		loop: if (read(0, &c, 1) <= 0) goto done;
		if (c != 'c' or p != 'h') goto resize;
		count--; goto done;
		resize: if (count + 1 <= capacity) goto push;
		buffer = realloc(buffer, ++capacity);
		push: buffer[count++] = c; p = c; goto loop; done:;

		printf("read: #%.*s#\n", (int) count, buffer);
		fflush(stdout);
		if (not strncmp(buffer, "quit", count)) break;
	}
	puts("quitting...");
}































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

