#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
	int f = open("a.txt", O_RDONLY);
	if (f < 0) perror("open");

	const off_t length = lseek(f, 0, SEEK_END);

	printf("%u\n", (int) length);
}

