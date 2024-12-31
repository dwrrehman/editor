// this is a program to print out prime numbers, which i am writing with my own text editor!
// the text editor is similar to ed, kinda. and only has 4 commands to use it, r, t, u, and n.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char** argv) {

	const int n = 10;

	printf("n = %d\n", n);
	for (int i = 0; i < n; i++) {
		printf("%d: ", i);

		if (i % 2 == 0) puts("even");
		else if (i % 3 == 0) puts("triple");
		else { printf("number is prime-ish...\n"); }

	}


	for (int i = 0; i < n; i++) {
	
		for (int j = 2; j < i; i++) {
		
			if (i % j == 0) goto not_prime;

		}	

	}

}


/* 

	this is a text document that i am typing in order to basically using the text editor to type english instead of c code. it isnt great for c code, because there is alot of formatting that generally gets in the way of finding and searching for things. but yeah,

		if you don't do stuff like that, and use it only for searching for words, and working with simple things with no formatting required, then its actually super nice to use lol. yay. i am quite happy with it actualy.


			but yeah, i am still trying to get used to it. i don't know if i am going to actually use it, but it definitely its kinda growing on me in some ways lol. alot of things that are usually hard are super easy, and alot of stuff that is usually really easy and quick takes like a solid 5 minutes to do lol. which isnt great. but i mean, i'm sure we'll adjust the editor to make it easier to use.. this is just the first step kinda. hmm.


			i dunno. just really want to actually try using it, because it has like THE most efficient possible editing paradigm, as far as from the cpu's perrspective lol. only making replacements that you actually want to make, and moving and displaying the minimal amount of text to make that happen. pretty efficienttttt.


			i also feel like the editor has a lot of potential for writing code in my language, becuase its alot more word based, and doesnt use punctuation, or whitespace nearly as much in idiomatic code.. so idk. ill have to experiment with it. its definitely actually quite nice for writing text documents, though!


				even like, the stuff where i write my own type of way of doing things too lol.





*/

