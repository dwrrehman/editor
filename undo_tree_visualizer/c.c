#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

int main(void) {
	int fd = open("41a710d63af1_1202312052.154749.history", O_RDONLY);
	int filepos = 0;

	while (1) {
		char c = 0;
		ssize_t n = read(fd, &c, 1);
		if (n <= 0) break;	
		printf("read c = %d (%c) at filepos = %d\n", c, isprint(c) ? c : '.', filepos);
		filepos++;
	}

	printf("finished reading file!\n");
	close(fd);
}

