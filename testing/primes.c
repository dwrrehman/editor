#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	const int n = 1000;

	printf("printing all the primes less than %d...\n", n); 

	for (int i = 0; i < n; i++) {
		for (int j = 2; j < i; j++) {
			if (i % j == 0) goto composite;
		}
		printf("%d is prime!\n", i);
		composite: continue;
	}
}


struct record {
	int data;
	const char* name;
};

static void myfunction(struct record* ) {

	
}