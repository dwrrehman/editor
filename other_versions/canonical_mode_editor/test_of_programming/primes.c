#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <stdio.h>

int main() {

	const int n = 0;

	for (int i = 0; i < n; i++) {

		for (int j = 2; j < i; j++) {

			if (i % j == 0) {
				goto composite;
			}


 

		}

	}

	exit(0);

}


