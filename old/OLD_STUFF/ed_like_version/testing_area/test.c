#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

int main() {
	int f = open("a.txt", O_RDONLY);
	if (f < 0) perror("open");

	struct stat s; 
	fstat(f, &s);
	const off_t length = s.st_size;

	printf("%u\n", (int) length);
	fflush(stdout);
}

