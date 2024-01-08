/*

do /usr/bin/clang
-Weverything
c.c

do ./a.out

*/

#include <stdio.h>
#include <string.h>

int main(void) {

	printf("%d\n", !!strncmp("hello there", "hel", 3));            // 0     equal
	printf("%d\n", !!strncmp("hello", "hello bub", 5 + 1 + 3));    // 1     not equal

	printf("%d\n", !!strncmp("", "", 0));     // 0       equal
	printf("%d\n", !!strncmp("t", "", 1));    // 1       not equal

	printf("%d\n", !!strncmp("", "t", 1));    // 1       not equal
	printf("%d\n", !!strncmp("f", "t", 1));   // 1       not equal

	printf("%d\n", !!strncmp("f ", "f", 1));   // 0      equal
	printf("%d\n", !!strncmp("f ", "f", 2));   // 1      not equal

	printf("%d\n", !!strncmp("f", "f ", 1));   // 0      equal
	printf("%d\n", !!strncmp("f", "f ", 2));   // 1      not equal

	printf("%d\n", !!strncmp("f ", "f ", 2));        // 0     equal
	printf("%d\n", !!strncmp("f asht", "f ", 2));    // 0     equal

	puts("end of my cool program. what do you think? yay.");



}


/*
0
1
0
1
1
1
0
1
0
1
0
0
end of my cool program. what do you think? yay.
*/

// insert ./a.out

// insert cal

 /*

    January 2024      
Su Mo Tu We Th Fr Sa  
    1  2  3  4  5  6  
 7 _ _8  9 10 11 12 13  
14 15 16 17 18 19 20  
21 22 23 24 25 26 27  
28 29 30 31           



/Users/dwrr/Documents/projects/editor








*/


/*
output:


0
1

0
1

1
1

0
1

0
1

0
0










	change ..insert lsbuild
c.c
code_file_example.c
editor
editor.dSYM
example_file.txt
simple.txt










build
c.c
code_file_example.c
editor
editor.dSYM
example_file.txt
simple.txt






*/
