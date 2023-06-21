/*

2306143.012025
------------------------------------------------------------------------------------------------------


	this is the input mechanism that i am going to use in my ed-like text editor. 
	it allows for text editing in an editor like format, however you can only type text, 
	consisting of visual (utf8) characters, tabs, and newlines, and backspaces.  
	you cannot use arrow keys at all, or move the cursor at any position other than the end of the line. yay.   
	 deleting newlines or tabs works as you would expect!


	its main features/advantages over simply using canonical mode is:
	
		- more power-efficient overall than canonical mode...?
		- tabs on wrap_width works better..?
		- tabs and newlines are deleteable!!!!!
		- ergonomic exit sequence of "drt"!!!!

------------------------------------------------------------------------------------------------------


current dilemma:

-------------------



		we need to only print newlines for the tab  on wrapwidth  case 

						and never print any other newlines in other cases. 

								additionally,  we need to keep track of the visual column, based on when we actually wrap. 
									ie, we need to synchonize our code with the terminal driver's code, ie, finding out what column it wraps text on, and making our code keep track of that internally.  we have the window width to work with now, so this should be possible. yay. 






yay we are so close to being done! the solutions is so effective too! and efficient!! but not exactly shortttt lol. but yeah.its good. 




			wait no,




					these two solutions arent totatlly orthogonal!


							we can do the wrap width  newline insertion    for tabs  only 


							because like,      how often do we care about it being copyable text for tabs on wrapwidth lol


						but then, we just need to keep track of it internally for the wrapping for both tabs and general characters!


							ie, reflect the tereminal driver code's functionlity. cool. yay. 





*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <iso646.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>

static struct winsize window = {0}; 
static void handler(int __attribute__((unused))_) {
	ioctl(0, TIOCGWINSZ, &window);
	//printf("\0337\033[6B\033[600D\033[K len=%lu cap=%lu tc=%lu nc=%lu c%lu w%lu\0338", len, capacity, tab_count, newline_count, column, (size_t) window.ws_col);
	//fflush(stdout);
}

int main(void) {
	handler(0);
	//sigaction(SIGINT, (struct sigaction[]) {{.sa_handler = handler}}, NULL);
	sigaction(SIGWINCH, (struct sigaction[]) {{.sa_handler = handler}}, NULL);

	struct termios terminal; tcgetattr(0, &terminal);
	struct termios copy = terminal; copy.c_lflag &= ~((size_t)ICANON); 
	tcsetattr(0, TCSAFLUSH, &copy);
	
loop:;	size_t change_count = 0, len = 0, capacity = 0, column = 0;
	ssize_t* changes = NULL;
	char* input = NULL, w = 0, p = 0, d = 0;

rc: 	if (read(0, &w, 1) <= 0) goto process;

	if (w == 127) goto delete;
	if (w != 't' or p != 'r' or d != 'd') goto push;
	if (len >= 2) len -= 2;
	goto process;

push:
	if (w == 10) {
		const size_t save = column;
		column = 0;
		changes = realloc(changes, (change_count + 1) * sizeof(ssize_t));
		changes[change_count++] = (ssize_t) (column - save);

	} else if (w == 9) {
		const size_t amount = 8 - (column % 8);
		
		const size_t save = column;
		if (column + amount >= window.ws_col) {
			putchar(10);
			column = 0;
		}
		column += (size_t) amount;

		changes = realloc(changes, (change_count + 1) * sizeof(ssize_t));
		changes[change_count++] = (ssize_t) (column - save);

	} else {
		const size_t save = column;
		if (column >= window.ws_col) {
			column = 0;
		}
		column++;

		changes = realloc(changes, (change_count + 1) * sizeof(ssize_t));
		changes[change_count++] = (ssize_t) (column - save);
	}

	if (len + 1 >= capacity) {
		capacity = 4 * (capacity + 1);
		input = realloc(input, capacity);
	}
	input[len++] = w;
	goto next;

delete: 
	if (not len) {
		printf("\033[D\033[K\033[D\033[K"); 
		fflush(stdout);
		goto next;
	}

	if (input[len - 1] == 10) {

		const ssize_t amount = changes[--change_count];
		len--;
		column -= (size_t) amount;

		printf("\033[D\033[K\033[D\033[K"); 
		printf("\033[A");
		if (column) printf("\033[%luC", column);
		fflush(stdout);
		
	} else if (input[len - 1] == 9) {
		
		const ssize_t amount = changes[--change_count];
		
		len--; 
		column -= (size_t) amount;

		if (amount > 0) {
			printf("\033[D\033[K\033[D\033[K"); 
			printf("\033[%luD\033[K", amount);
			fflush(stdout);
		} else {
			printf("\033[D\033[K\033[D\033[K"); 
			printf("\033[A");
			if (column) printf("\033[%luC", column);
			fflush(stdout);
		}

	} else {
		const ssize_t amount = changes[--change_count];
		
		do len--; while ((unsigned char) input[len] >> 6 == 2);

		column -= (size_t) amount;

		if (amount > 0) {
			printf("\033[D\033[K\033[D\033[K");
			printf("\033[D\033[K");
			fflush(stdout);
		} else {
			printf("\033[D\033[K\033[D\033[K"); 
			printf("\033[A");
			if (column) printf("\033[%luC", column);
			fflush(stdout);
		}
	}

next:	d = p; p = w; w = 0;
	goto rc;



process: 
	printf("\n\trecieved input(%lu): \n\n\t\t\"", len);
	fwrite(input, len, 1, stdout); 
	printf("\"\n\n\n");
	fflush(stdout);



	if (len == 4 and not strncmp(input, "quit", 4)) goto done;
	if (len == 5 and not strncmp(input, "clear", 5)) { printf("\033[H\033[2J"); fflush(stdout); } 

	goto loop;


done:	tcsetattr(0, TCSAFLUSH, &terminal);
}






/*
		if (column + 1 >= win.col) {

			 newline_code;

		}
		*/







	//printf("%c", w); 
	//fflush(stdout);







/*


// printf("SIGWINCH event: window: %d rows, %d columns\n", window.ws_row, window.ws_col);


















	known bug:   if the screen gets scrolled because there are alot of lines on the screen,
			then deleting text i:past the current line;, will not work, b:visually; 
			because the cursor cache is wrong now. 


		note:	this bug is intentionaly left unsolved because it is too hard to fix, 
				and not worth fixing because it is hardly a problem usually. 




									actually 







							no 



									we should solve this bug. 






			if we solve this one bug, then its literally perfect. 




					like LITERALLYYYYY perfect. 



						with nothing wrong in it AT ALL. it will have every possible feature we would ever want lol. which is pretty amazing. 



								lets do it. i think we can. i know we can. 





						yayyyyyy




									







		AHHHHH






















//   printf("\033[%lu;%luH", line + 1, column + 1); 






	if ((unsigned char) w >> 5 == 6) point_count = 1;
	if ((unsigned char) w >> 4 == 14) point_count = 2;
	if ((unsigned char) w >> 3 == 30) point_count = 3;






, point_count = 0;





if (point_count) { point_count--; goto read_ch; }





char* report = calloc(32, 1);



char q = 0;
		printf("\033[6n"); fflush(stdout);
		size_t alen = 0; read(0, &q, 1); read(0, &q, 1); 
		while (alen < 31 and read(0, report + alen, 1) == 1) 
			if (report[alen++] == 'R') break;
		report[alen] = 0; sscanf(report, "%hd;%hdR", &new.r, &new.c);*/


	// execute this right before you print a new newline.





	// then push new.r/c    to the newlines_coords array. i think. yeah. 






	// and actually, all we really need is the width,   because we know that we have to go up a line lol.    thats it.   just a single   int16


	// but, the trickier problem is soft wrapping....      thats the difficult problem. 

///		hmmmm

//						lets just start with this, though.

/*

			oh wait 

						....we never actually need to prompt lol. 

								we can always just 


									compute it ourselves. lol. okay fine lets try to... lol..



									its so much busy work lol.


										but its okay because it will mean better performance of the editor lol. so yeah. 

									









*/





/*
if (w == 27) {
		printf("found escape.\n");
		fflush(stdout);
	}
*/

//fd_set fds; FD_ZERO(&fds); FD_SET(0, &fds);
	//if (select(1, &fds, 0, 0, (struct timeval[]){{0}}) == 1) goto skip;





/*
	
*/










// resize: if (len + 1 < capacity) goto push;







	/*sleep(1);

	if () { 
		//printf("@%hhu", w); 
		//fflush(stdout); 
		goto print; 

	} else { 
		//printf("$%hhu", w); 
		//fflush(stdout); 
		goto print; 
	}
	//if (w != 27) { }
	*/





// if (w == 10) { b = false; printf("\0337"); }








/*
		if (input[input_length - 1] == 10 and not b) { printf("\0338"); b = true; input_length--; }
		else if (input[input_length - 1] == 9) { 
			do printf("\b");
			while (cursor_column % 8); 
			printf("\033[K"); input_length--; */












/*

else {
		printf("FATAL ERROR: error, the input needs to be at "
			"least 2 chars because of drt seq. input_length = %lu\n", 
			input_length
		);
	}


static int get(char** buffer, size_t* count) {
	char c = 0, p = 0;
	*count = 0;
	while (1) {
		ssize_t n = read(0, &c, 1);
		if (n <= 0) goto error;
		if (c == '\n' and p == '`') { (*count)--; break; }
		*buffer = realloc(*buffer, *count + 1);
		(*buffer)[(*count)++] = c;
		p = c;
	}
	return 0;
error:
	printf("encountered error: ");
	perror("read");
	return 1;
}


static size_t input(char** buffer, size_t* capacity) {
	char c = 0, p = 0;
	size_t count = 0;
loop: 	if (read(0, &c, 1) <= 0) return count;
	if (c != 'c' or p != 'h') goto resize;
	if (read(0, &c, 1) == 10) return --count;
resize: if (count + 1 <= *capacity) goto push;
	*buffer = realloc(*buffer, ++*capacity);
push: 	(*buffer)[count++] = c; p = c; goto loop;
}


















































int main(void) {
	char* inserted = NULL; 
	size_t inserted_length = 0;
	char input[512] = {0};

loop: 	fgets(input, sizeof input, stdin);

	if (not strcmp(input, ".\n")) { if (inserted_length) inserted_length--; } 
	else {
		const size_t length = strlen(input);
		inserted = realloc(inserted, inserted_length + length);
		memcpy(inserted + inserted_length, input, length);
		inserted_length += length;
		printf("appending... (%lu)\n", inserted_length);
		goto loop;
	}

	printf("\n\trecieved inserted(%lu): \n\n\t\t\"", inserted_length);
	fwrite(inserted, inserted_length, 1, stdout); 
	printf("\"\n\n\n\n");

	if (inserted_length == 4 and not strncmp(inserted, "quit", 4)) goto done;

	inserted_length = 0;
	goto loop;
done:
	puts("quitting...");
}





























*/


/* 512 chars:    per command. 

xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx


*/









/*
static struct termios terminal = {0};





//static void clear_screen(void) { printf("\033[2J\033[H"); }
static inline void restore_terminal(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal); }
//static inline char read_from_stdin(void) { char c = 0; read(0, &c, 1); return c; }

static inline void configure_terminal(void) {
	tcgetattr(STDIN_FILENO, &terminal);
	atexit(restore_terminal);
	struct termios raw = terminal;
	raw.c_lflag &= ~((size_t)ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}




static int get(char** buffer, size_t* count) {
	char c = 0, p = 0;
	*count = 0;
	while (1) {
		ssize_t n = read(0, &c, 1);
		if (n <= 0) goto error;
		if (c == '\n' and p == '`') { (*count)--; break; }
		*buffer = realloc(*buffer, *count + 1);
		(*buffer)[(*count)++] = c;
		p = c;
	}
	return 0;
error:
	printf("encountered error: ");
	perror("read");
	return 1;
}
*/

/*
static size_t input(char** buffer, size_t* capacity) {
	char c = 0, p = 0;
	size_t count = 0;
loop: 	if (read(0, &c, 1) <= 0) return count;
	if (c != 'c' or p != 'h') goto resize;
	if (read(0, &c, 1) == 10) return --count;
resize: if (count + 1 <= *capacity) goto push;
	*buffer = realloc(*buffer, ++*capacity);
push: 	(*buffer)[count++] = c; p = c; goto loop;
}
*/








/*

int main(void) {

	configure_terminal();

	char* input = NULL; 
	char p = 0, d = 0;
	size_t input_length = 0, capacity = 0;


	while (1) {
		


	input_length = 0;
	char w = 0;
	d = 0;
	p = 0;

read_char:;

	w = 0;
	if (read(0, &w, 1) <= 0) {
		printf("FATAL ERROR: error in read() call!!\n");
		goto process;
	}

	if (w != 't' or p != 'r' or d != 'd') goto resize;

	if (input_length >= 2) input_length -= 2;
	else {
		printf("FATAL ERROR: error, the input needs to be at "
			"least 2 chars because of drt seq. input_length = %lu\n", 
			input_length
		);
	}

	goto process;

resize: 
	if (input_length + 1 < capacity) goto push;
	input = realloc(input, ++capacity);
push: 	
	input[input_length++] = w; 
	d = p;
	p = w;
	goto read_char; 


process: 

	printf("\n\trecieved input(%lu): \n\n\t\t\"", input_length);
	fwrite(input, input_length, 1, stdout); 
	printf("\"\n\n\n\n");

	fflush(stdout);

	if (input_length == 4 and not strncmp(input, "quit", 4)) break;

		
	}

	puts("quitting...");
	restore_terminal();
}
*/






























/*
char* get(char str[], size_t len, int fileno) {
    size_t count;
    for (count = 0; count < len; ++count) {
        int result;
        if (result = read(fileno, str + count, 1), result != 1) {
            if (result == 0) {
                if (count > 0) {
                    str[count] = 0;
                    return str;
                } else {
                    if (count == 0) {
                        return NULL;
                    }
                }
            } else {
                perror("read");
                return NULL;
            }
        } else {
            if (str[count] == '\n') {
                if (count < len - 1) {
                    str[count + 1] = 0;
                } else {
                    str[count] = 0;
                }
                return str;
            }
        }
    }
    if (count < len) {
        str[count] = 0;
    }
    return str;
}

*/















/*

ut_mechanism: b
warning: include location '/usr/local/include' is unsafe for cross-compilation [-Wpoison-system-directories]
1 warning generated.
new_input_mechanism:

*/



