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

	exit(0);
}


