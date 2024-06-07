#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	typedef unsigned long long nat;
	nat count = 1000000;
	for (nat i = 0; i < count; i++) {

		for (nat j = 2; j < i; j++) {

			if (i % j == 0) {
				goto composite;
			}
		}
		printf("%llu\n", i);
		composite: continue;
	}
}

