// hello there from space! this is my editor.
// its pretty cool. 

// i mean, technically, this is already ready to use as my editor, lol. not even kidding. it has all the basics lol. which is pretty incredible. 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void my_function(int* array, int count) {
	// this function prints some integers lol. why not. 
	
	printf("ints (%d){ \n", count);
	for (int i = 0; i < count; i++) printf("%d ", array[i]);
	printf("}\n"); 	
}

int main(int argc, const char** argv) {
	printf("hello there from space!\n");
	int a[10] = {0};
	my_function(a, 10);
	return puts("usage is correct! yay.");
}


