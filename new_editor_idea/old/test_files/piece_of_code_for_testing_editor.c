// okay, so i kinda want to try to use this editor to write a piece of code. 
// it doesnt have to be anything fancy, but yeah, i need to test writing code with this thing. 
// lets see how it will go! yay. [202306305.184617]

#include <stdio.h>
#include <stdlib.h>

int main() {
	char buffer[512] = {0};

	fgets(buffer, sizeof buffer, stdin);

	int a = atoi(buffer);
	
	fgets(buffer, sizeof buffer, stdin);
	
	int b = atoi(buffer);

	printf("%d + %d = %d\n", a, b, a + b);
	
	typedef uint64_t nat;
	for (nat i = 0; i < 1234; i++) {
		a += b * b;
		a %= 23;
	}

	printf("okay, thats the end of this program!\n");

	puts("the only thing we are going to do now, is just go to bed now! lol.");

	/*

		202307016.045338:

		i am literally so tired i should just go to bed now lol


		but i am so happy that my editor actually works!!!

		yay!!!!! very happy it works so well. yay. 


		i just realized that you can do selections in the coolest way possible by using controlA text to find to goto other point in text  controlQD controlA controlD/H controlQD


		ie, anchor() text-you-want-to-type cut() anchor() find-forwards/backwards() cut()


				thats it! super simple lol. i love how this edttor works so much. its literally so minimalist and cool. its crazy you can actually get away with that little features lol. i love it so much lol. yay. 





	*/


	exit(0);
}


