#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char** argv) {

	if (argc == 1) return puts("usage error");

	const int n = atoi(argv[1]);

	for (int i = 0; i < n; i++) {
		for (int j = 2; j < i >> 1; j++) {
			if (i % j == 0) goto composite;
		}
		printf("%d is prime!\n", i); 
		printf("continue? ");
		if (getchar() == 'n') break;
		composite: continue;
	} 

	abort();

	return 0;
}
/*

debug:

do /usr/bin/clang
-Weverything
-Wno-declaration-after-statement
testing_files/primes.c

execute:

do ./testing_files/gen_primes.out
5




*/
