// this is a file to test out the programming ability of the editor. hopefully this works!  i guess we'll see.
// this program is going to be a simple program to print out person structs lol. or something like that.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct person {
	const char* name;
	int age;
	const char* height;
};


static void print_person(struct person p} {
	printf("printing person: {\n");
	printf("\t.name = %s\n", p.name);
	printf("\t.age = %d\n", p.age);
	printf("\t.height = %s\n", p.height);
	printf("}\n");
}
int main(void) {
	
	struct person daniel = {.name = "daniel", .age = 25, .height = "5 11" };

	print_person(daniel);

	return 0;
}
