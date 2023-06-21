#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>

int main(int argc, const char** argv) {
	typedef size_t nat;
	struct termios terminal; 
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON | IEXTEN); 			//   | ISIG
	copy.c_iflag &= ~((size_t) IXON); 					// BRKINT | ICRNL  | 
	tcsetattr(0, TCSAFLUSH, &copy);

	struct winsize window;
	ioctl(0, TIOCGWINSZ, &window);    					// do this more often?.., 

	nat count = 0;
	char* text = NULL;
	
	nat tab_count = 0;
	uint8_t* tabs = NULL;

	nat newline_count = 0;
	uint16_t* newlines = NULL;

	nat cursor = 0;
	uint16_t column = 0;

	char c = 0;

	//printf("\033[r");
	//fflush(stdout);

	char filename[2048] = {0};
	
	
	FILE* file = argc >= 2 ? fopen(argv[1], "r") : fopen("old/a.c", "r");	
	if (not file) { perror("fopen"); goto loop; }

	fseek(file, 0, SEEK_END);

        count = (size_t) ftell(file); 
	text = malloc(count);

        fseek(file, 0, SEEK_SET); 
	fread(text, 1, count, file); fclose(file); 

	strlcpy(filename, argc >= 2 ? argv[1] : "old/a.c", sizeof filename);

	// printf("%s r %lu\n", filename, count);

	fwrite(text, count, 1, stdout);
	fflush(stdout);

/*

	

	for (nat i = 0; i < 100; i++) {
	usleep(500000);
	printf("\033M\033M\033M");
	fflush(stdout);
	}

*/






loop: 	read(0, &c, 1);

	if (c == 27) goto loop;

	else if (c == 'H') { printf("\033[1;1H"); fflush(stdout); }

	else if (c == 'Q') goto done;

	else if (c == 127) {

		if (not cursor) goto loop;
		count--; cursor--;

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
	} 

	else if (c == 10) {
		newlines = realloc(newlines, sizeof(uint16_t) * (newline_count + 1));
		newlines[newline_count++] = column;
		column = 0;
		write(1, &c, 1);
		text = realloc(text, count + 1);
		text[cursor++] = c;
		count++;
	} 

	else if (c == 9) {
		const uint8_t amount = 8 - column % 8;
		column += amount; 
		column %= window.ws_col;
		write(1, "        ", amount);
		tabs = realloc(tabs, tab_count + 1);
		tabs[tab_count++] = amount;
		text = realloc(text, count + 1);
		text[cursor++] = c;
		count++; 
	} 

	else {
		if (column >= window.ws_col) column = 0;
		if((unsigned char) c >> 6 != 2) column++;
		write(1, &c, 1);
		text = realloc(text, count + 1);
		text[cursor++] = c;
		count++; 
	}

	goto loop;
done:	tcsetattr(0, TCSAFLUSH, &terminal);
}







































/*





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

