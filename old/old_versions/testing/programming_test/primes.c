#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	const int n = 1000000;

	printf("printing all the primes less than %d...\n", n); 

	for (int i = 0; i < n; i++) {
		for (int j = 2; j < i; j++) {
		if (i % j == 0) goto composite;
		}
		printf("%d is prime!\n", i);
		composite: continue;
	}

	// lets see if this works lol. i think it will though lol. yay. nice. 	


	// hello there from space! this is my cool file lol. lets see if this works lol. 


			// okay, cool. so i feel like this actually might be stable, and usable. like actually. nice. thats so cool lol. i love this. yay. we didnt even really need the persistent undo tree, but it is quite nice. we still need to actaully implement the thing where we do redo, though. thats still to do. 202312251.165024

			// but yeah, i think this works. nice. thats super cool. yay. 



}


