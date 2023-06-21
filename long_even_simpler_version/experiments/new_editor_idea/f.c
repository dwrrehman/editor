#include <stdio.h>  // 2306202.165852:   a screen based simpler editor based on the ed-like one, uses a string ds, and is not modal. uses capital letters for commands! 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
int main(int argc, const char** argv) {
	char filename[2048] = {0};
	strlcpy(filename, argc >= 2 ? argv[1] : "Untitled.txt", sizeof filename);

	typedef size_t nat;
	struct termios terminal; 
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON | IEXTEN); 			//   | ISIG
	copy.c_iflag &= ~((size_t) IXON); 					// BRKINT | ICRNL  | 
	tcsetattr(0, TCSAFLUSH, &copy);

	struct winsize window;
	ioctl(0, TIOCGWINSZ, &window);
	const nat screen_size = window.ws_row * window.ws_col * 4;
	char* screen = calloc(screen_size, 1);

	char* text = NULL, c = 0;
	nat cursor = 0, origin = 0, cursor_row = 0, cursor_column = 0, length = 0, count = 0, saved = 1;

	FILE* file = fopen(filename, "r");	
	if (not file) { perror("fopen"); getchar(); goto loop; }
	fseek(file, 0, SEEK_END); count = (size_t) ftell(file); 
	text = malloc(count); fseek(file, 0, SEEK_SET); 
	fread(text, 1, count, file); fclose(file); 
loop:	
	memcpy(screen, "\033[H\033[2J", 7);
	length = 7;
	nat row = 0, column = 0;
	nat i = origin;
	for (; i < count; i++) {
		if (i == cursor) { cursor_row = row; cursor_column = column; }
		if (row >= window.ws_row - 1) break;
		char k = text[i];
		if (k == 10) {
			column = 0; row++;
			screen[length++] = 10;
		} else if (k == 9) {
			const uint8_t amount = 8 - column % 8;
			column += amount;
			memcpy(screen + length, "        ", amount);
			length += amount;
		} else {
			if (column >= window.ws_col) { column = 0; row++; }			//  or column == window.ws_col
			if ((unsigned char) k >> 6 != 2) column++;
			screen[length++] = k;
		}
	}
	if (i == cursor) { cursor_row = row; cursor_column = column; }
	length += (nat) snprintf(screen + length, 16, "\033[%lu;%luH", cursor_row + 1, cursor_column + 1);
	write(1, screen, length);
	read(0, &c, 1);
	if (c == 27) goto loop;
	else if (c == 'Q' and saved) goto done;
	else if (c == 'S') {
		if (*filename) goto save;
		puts("s:no filename given"); goto loop;
	save:;	FILE* output_file = fopen(filename, "w");
		if (not output_file) { perror("fopen"); getchar(); goto loop; }
		fwrite(text, count, 1, output_file); fclose(output_file); 
		printf("%s %s %lu\n", filename, saved ? "s" : "w", count); saved = 1;
	} else if (c == 'N') { 
		move_left: if (origin < cursor or not origin) goto decr; origin--; 
		e: if (not origin) goto decr; origin--; 
		if (text[origin] != 10) goto e; origin++;
		decr: if (cursor) cursor--;
	} else if (c == 'E') { 
		move_right: if (cursor < count) cursor++; 
		if (cursor_row != window.ws_row - 1) goto loop;
		l: origin++; if (origin >= count) goto loop;
		if (text[origin] != 10) goto l; origin++;
	} else if (c == 127) {
		if (not cursor) goto loop; 
		memmove(text + cursor - 1, text + cursor, count - cursor);
		count--; text = realloc(text, count); saved = 0; goto move_left;
	} else {
		text = realloc(text, count + 1);
		memmove(text + cursor + 1, text + cursor, count - cursor);
		text[cursor] = c;
		count++; saved = 0; goto move_right;
	}
	goto loop;
done:	printf("\033[H\033[2J");
	tcsetattr(0, TCSAFLUSH, &terminal);
}










		//if (not access(rest, F_OK)) { puts("s:file exists"); goto loop; }
		//else strlcpy(filename, rest, sizeof filename);

















// = (column + amount) % window.ws_col;






/*

if (column >= window.ws_col) column = 0;
		if((unsigned char) c >> 6 != 2) column++;
		write(1, &c, 1);






if (text[cursor] == 10) {
			column = newlines[--newline_count];
			printf("\033[A");
			if (column) printf("\033[%huC", column);
			fflush(stdout);

		} else if (text[cursor] == 9) {
			uint8_t amount = tabs[--tab_count];
			column -= amount;
			write(1, "\b\b\b\b\b\b\b\b", amount);

		} else {
			while ((unsigned char) text[cursor] >> 6 == 2) { count--; cursor--; }
			if (not column) {
				column = window.ws_col - 1;
				write(1, "\b", 1);
			} else {
				column--;
				write(1, "\b \b", 3);
			}
		}





	if (len + 1 >= capacity) {
		capacity = 4 * (capacity + 1);
		
	}






process:
	printf("\n\trecieved input(%lu): \n\n\t\t\"", len);
	//fwrite(input, len, 1, stdout);
	for (size_t i = 0; i < len; i++) {
		if (isprint(input[len])) putchar(input[len]);
		else printf("[%d]", input[len]);
	}
	printf("\"\n");
	
	fflush(stdout);
	if (len == 4 and not strncmp(input, "quit", 4)) goto done;
	if (len == 5 and not strncmp(input, "clear", 5)) { printf("\033[H\033[2J"); fflush(stdout); } 
	goto loop;




*/


//for (int i = 0; i < len; i++) {
	//	printf("%c", input[i] == 9 ? '@' : input[i]);
	//}













//printf("\033[%hhuD", amount);






/* 

typed in the editor itself:





okay, so i mean, i am going to try to stress test this code lol. i think we might have actually figured it out. like, its super easy to use and yeah, i just like it alot. i think its really nice actually. yay.

        okay, so i mean, i think we should probably do a tonnnn of testing with it. 

                i think its pretty much perfect though. the only problem is the fact that we cant resize the window like dynamically lol.                           


                but yeah





printf("\033[6n");
	scanf("\033[%lu;%luR", &row, &column);
	fflush(stdout);
	row--; column--;






//printf("[%lu %lu][%lu %lu]\n", window_rows, window_columns, row, column);



//printf("\033[D\033[K\033[D\033[K"); 
		//fflush(stdout);




	if we try to delete a newline....


	then just don't delete anything...?


		...  we avoid so many problems if we just don't allow deleteting lol.

				i feel like we should just not try to delete newlines. 


				idk. kinda feels like an unimportant change. like we don't reallyyyyy need to... 


						i mean 




							okay, technically we could allow for the deleting of the VERY LAST line maybe?... i feel like that would be fine. because thats all we really use really.. 

					but like, even at that point, i feel like deleting the last line is pointless lol. idk. 



			

*/






































/*

				tb that the editor couldnt save   because saving wasnt working properly lol




okay

        i think this is good?

lets try to write this file and see what happens lol.


 i mean,


        it kinda works?

        lol


                        i don't think its going to crash anymore, hopefully lol.


idk. 

it might. 


but probably not lol.



yeah, i really hope this thing doesnt crash lol. 



i mean. 


idk. lol.
i think we'll be okay.
i guess we just have to like
testing things, and make sure that we do everythin properly
lol.
theres not really any subsitute for that. 


                        but yeah, it seems like everything else works like     just fine though!


                                                so i am quite happy about that lol. 


                                                                yay 




                but yeah, lets try to write this now lol hopefully we don't loose it all. gah. 

                                hm
        

*/

