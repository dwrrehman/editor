// a text-based choose your own adventure game i am coding up to test my using my editor with programming. 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

static const int max_text_width = 50;

static void say(const char* string) {
	const int length = (int) strlen(string);
	int width = 0;
	putchar(9);
	for (int i = 0; i < length; i++) {
		putchar(string[i]);
		if (width > max_text_width and isspace(string[i])) { printf("\n\t"); width = 0; }
		else width++;
		usleep((unsigned) (rand() % 50000)); 
		fflush(stdout);
	}
	fflush(stdout);
	getchar();
	puts("");
}

static int choose(const char* actions) {
	printf("\t  0. ");
	int number = 0;
	for (int i = 0; actions[i]; i++) {
		if (actions[i] == '.') { i++; number++; if (actions[i] == ' ') printf("\n\t  %d. ", number); else if (not actions[i]) break; }
		else putchar(actions[i]);
	}
	printf("\n\n\t: ");
	char buffer[4096] = {0};
	fgets(buffer, sizeof buffer, stdin);
	puts("");	
	return atoi(buffer);
}

int main(void) {
	srand((unsigned) time(0)); 
	puts("");
	
	say("hello! this is a choose your own adventure game that i am coding up to test my editor. press enter, and enjoy the experience!");
	say("[press enter to begin the game]");

	say("you find yourself in a very dark room, with a single candle on the floor lighting up a small area around it. you see a few stones on the ground, near the candle.");
	say("what do you do?");

begin_c:;
	int c = choose("look around the room. go closer to the candle. pick up one of the stones."); 

	if (c == 0) {
		say("the room is very dark, and you cannot see where you are going.");
		
		goto begin_c;

	} else if (c == 1) {
		say("the candle is brings with it some warmth as you approach. you see a small hand written note on a scrap of paper on the small metal tray which the candle rests on.");
		say("it reads \"i'm sorry\""); 
		
		c = choose("take scrap of paper. blow out candle. say \"is anyone there?\".");

		if (c == 0) {
			say("you put the scrap of paper in the left pocket of your jeans, thinking this might come in handy later.");
			
		} else if (c == 1) {

			say("you blow out the candle. the room turns completely pitch black.");

			say("shortly after doing so, you hear a noise, like that of a lizard. some foot step noises follow, and you frantically look around to try to find the direction they are coming from. for a couple seconds there is just silence. you wish you did not blow out the candle, as it was the only way of seeing anything in this room.");

		} else if (c == 2) {
			say("you ask into the darkness, \"is anyone there?\". there is no reply, but you also notice you cannot hear your voice echo at all, as if there are no walls to this room, which slightly terrifies you.");
		}
	} else if (c == 2) {

		say("you pick up one of the stones near the candle. it is a shiny black color, and somewhat wet and slippery. ");

		say("you notice a couple more stones on the ground similar to it around the candle, and soon realize that the floor of this room is completely covered in these small black stones.");

	}
	say("end of the game. thanks for playing!!!");
}

