#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

int main(void) {

	const char* restrict filename = "file.txt";

	int d = open(filename, O_RDONLY | O_DIRECTORY);
	if (d >= 0) { close(d); errno = EISDIR; goto read_error; }

	const int file = open(filename, O_RDONLY, 0);
	if (file < 0) { read_error: perror("open"); exit(1); }

	size_t count = (size_t) lseek(file, 0, SEEK_END);
	uint8_t* contents = malloc(count);
	lseek(file, 0, SEEK_SET);
	read(file, contents, count);
	close(file); 

	fwrite(contents, 1, count, stdout);





	



}

