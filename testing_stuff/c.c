#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct buffer {

	int a, b, c, d;

	char string[128];
	char data[128];
};


static void print_buffer(struct buffer t) {
	printf("{a=%d,b=%d,c=%d,d=%d,string=%s,data=%s}\n", 
		t.a, t.b, t.c, t.d, t.string, t.data
	);
}


int main() {


	struct buffer b = {
		.a = 2, .b = 3, .c = 5, .d = 7, 
		.string = "lol", 
		.data = "this is some data." 
	};
	
	struct buffer a = {
		.a = 1, .b = 1, .c = 1, .d = 1, 
		.string = "STUFF", 
		.data = "MORE STUFF"
	};


	print_buffer(b);
	print_buffer(a);

	a = b;

	print_buffer(b);
	print_buffer(a);
	
}
