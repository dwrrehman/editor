#include <iso646.h>       // simplified rewrite of the text editor by dwrr on 2304307.024007
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>    
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

typedef uint64_t nat;
struct word { char* data; nat count; };
static nat m = 0, n = 0, cm = 0, cn = 0, mode = 0;
static struct word* text = NULL;

static bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2; }
static bool stdin_is_empty(void) {
	fd_set f; FD_ZERO(&f); FD_SET(0, &f);
	struct timeval timeout = {0};
	return select(1, &f, 0, 0, &timeout) != 1;
}

static struct termios configure_terminal(void) {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	raw.c_oflag &= ~((size_t)OPOST);
	raw.c_iflag &= ~((size_t)BRKINT | (size_t)ICRNL | (size_t)INPCK | (size_t)IXON);
	raw.c_lflag &= ~((size_t)ECHO | (size_t)ICANON | (size_t)IEXTEN);
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}

static void insert(char c) {
	 if (cm == m or cn == n) {
		text = realloc(text, (n + 1) * sizeof *text);
		memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
		if (cn < n) cn++; 
		text[cn].data = malloc(m);
		text[cn].data[0] = c;
		text[cn].count = 1;
		n++; cm = 1;
	} else if (text[cn].count < m) {
		memmove(text[cn].data + cm + 1, text[cn].data + cm, text[cn].count - cm);
		text[cn].data[cm++] = c;
		text[cn].count++;
	} else {
		text = realloc(text, (n + 1) * sizeof *text);
		memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
		n++;
		text[cn + 1].data = malloc(m);
		memmove(text[cn + 1].data, text[cn].data + cm, text[cn].count - cm);
		text[cn + 1].count = text[cn].count - cm;
		text[cn].data[cm++] = c;
		text[cn].count = cm;
	}
}

static char delete(void) {
	top: if (cm) {
		cm--;
		char c = text[cn].data[cm];
		text[cn].count--;
		memmove(text[cn].data + cm, text[cn].data + cm + 1, text[cn].count - cm);
		if (text[cn].count) goto r;
		n--;
		memmove(text + cn, text + cn + 1, (n - cn) * sizeof *text);
		text = realloc(text, n * sizeof *text);
		if (not n or not cn) goto r;
		cn--; cm = text[cn].count;
		r: return c;
	} else if (cn < n and text[cn].count) {
		if (not cn) return 0;
		cn--;
		cm = text[cn].count;
		goto top;
	} else return 0;
}

static void move_left(void) {
	if (not n) return;
	do if (cm) cm--; else if (cn) { cn--; cm = text[cn].count - 1; }
	while ( cm  < text[cn].count and zero_width(text[cn].data[cm]) or
		cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);
}

static void move_right(void) {
	if (not n) return;
	do if (cm < text[cn].count) cm++; else if (cn < n - 1) { cn++; cm = 1; }
	while ( cm  < text[cn].count and zero_width(text[cn].data[cm]) or
		cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);
}

static void display(nat display_mode) {

	static char* screen = NULL;
	static nat screen_size = 0, window_rows = 0, window_columns = 0, cursor_column = 0, cursor_row = 0, om = 0, on = 0;
	
	static struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	if (not window.ws_row or not window.ws_col) { window.ws_row = 24; window.ws_col = 60; }
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen_size = 32 + window_rows * (window_columns * 4 + 5);
		screen = realloc(screen, (size_t) screen_size);
	}
	nat screen_column = 0, screen_row = 0; bool should_move_origin_forwards = false;
	int length = snprintf(screen, screen_size, "\033[?25l\033[H");
	if (cursor_row == window_rows - 1) should_move_origin_forwards = true;

	if (cursor_row == 0) {}// move_origin_backwards();  // but we are checking for it to be zero here!!!!!        FUNDEMENTAL PROBLEM. YIKES. :/
	
	nat i = on, j = om;
	while (i <= n) {
		while (j <= m) {
			if (i == cn and j == cm) { cursor_row = screen_row; cursor_column = screen_column; }    // the fund prob is we are setting it to zero here

			if (i >= n or j >= text[i].count) break;
			const char c = text[i].data[j];

			if (c == 10) { next_char_newline: j++; goto print_newline; }
			else if (c == 9) {
				do {
					if (screen_column < window_columns) screen_column++; else goto next_char_newline;
					length += snprintf(screen + length, screen_size, " ");
				} while (screen_column % 8);
			} else {
				if (zero_width(c)) goto print_char;
				if (screen_column < window_columns) screen_column++; else goto print_newline;
				print_char: length += snprintf(screen + length, screen_size, "%c", c);
			}

			j++; continue;
			print_newline: length += snprintf(screen + length, screen_size, "\033[K");
			if (display_mode == 1 or screen_row >= window_rows - 1) goto print_cursor;
			length += snprintf(screen + length, screen_size, "\r\n");
			screen_row++; screen_column = 0;


			if (should_move_origin_forwards and screen_row == 1) {
				screen_row = 0; length = 9; should_move_origin_forwards = false; on = i; om = j;
			}
		}
		i++; j = 0;
	}
	if (screen_row < window_rows) goto print_newline;
	print_cursor: length += snprintf(screen + length, screen_size, "\033[%llu;%lluH\033[?25h", cursor_row + 1, cursor_column + 1);
	write(1, screen, (size_t) length);
}

static void interpret_sequence(void) { 
	char c = 0; read(0, &c, 1);  
	if (c != '[') return; read(0, &c, 1);
	if (c == 'D') move_left();
	else if (c == 'C') move_right();
	else if (c == 'M') {
		read(0, &c, 1);
		if (c == 97) { read(0, &c, 1); read(0, &c, 1); 
		} else if (c == 96) { read(0, &c, 1); read(0, &c, 1); 
		} else { char str[3] = {0}; read(0, str + 0, 1); read(0, str + 1, 1); }
	}
}

int main(void) {
	m = 10; n = 0; mode = 1;
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h\033[7l\033[r", 23);
	char c = 0; 
loop:	display(0); 
	read(0, &c, 1);
	if (c == 27 and stdin_is_empty()) mode = 0;
	else if (c == 27) interpret_sequence(); // &scroll
	else if (c == 127) while (zero_width(delete()));
	else if (c == 13) insert(10);
	else insert(c);
	if (mode) goto loop;
	write(1, "\033[?1049l\033[?1000l\033[7h", 20);
	tcsetattr(0, TCSAFLUSH, &terminal);
}



























//  bool* scroll

//  bool scroll = false;

//  if (not scroll)  else scroll = false;   //debug_display();









/*

static void move_origin_backwards(void) {
	if (not n) return;

	int i = 0; 

	do {
		do if (om) om--; else if (on) { on--; om = text[on].count - 1; } else goto done;
		while ( om  < text[on].count and zero_width(text[on].data[om]) or
			om >= text[on].count and on + 1 < n and zero_width(text[on + 1].data[0])
		);

		i++;

	} while (i < 4);

	done:;
}

*/












/*



			although, i am still happy that this code is only 200ish lines long!!!


				so amazing. i can't believe it still. so cool.  yayyyyy. alotta hard work and thought pays off!!!





*/







			/// THIS CODE SUCKS. make it more idiomatic. and simpler. 
			/// and fix this bug where the veiw doesnt shift until 
			/// theres a single character on that line. thats so dumb. yeah. fix that. 







/*
	do do if (om) om--; else if (on) { on--; om = text[on].count - 1; } else goto done;
		while ( om  < text[on].count and zero_width(text[on].data[om]) or
			om >= text[on].count and on + 1 < n and zero_width(text[on + 1].data[0])
		);
	while (   om  < text[on].count and text[on].data[om] != 10 or
		  om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10
	);







	/// ALSO make this code not buggy as heck. 
		/// it doesnt print the top line until you are on it, 
		/// and it doesnt move through softwrapped lines properly. 
		/// we need to be keeping track of the cursor_column, cursor_row, 
		/// and calculating the visual position based on the characters we move over.







	if (not shifted and cursor_row == window_rows - 1) {
		int start = 9, end = start;
		while (screen[end] != 10) end++; end++;
		memmove(screen + start, screen + end, (size_t)(length - end));
		length -= end - start;
		cursor_row--; screen_row--; screen_column = 0; 
		on = first_on; om = first_om; shifted = true;
		goto fill_screen;
	}







 // move view down by deleting the first line, and finding the first newline in the screen. 

nat start = 9, end = start;
		while (screen[end] != 10) end++;
		memmove(screen + start, screen + end, end - start;

	}

*/





























/*


static void debug_display(void) {
	printf("\033[H\033[J");
	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);
	for (nat i = 0; i < n; i++) {
		printf("%-3llu %c ", i, i == cn ? '*' : ':' );
		printf("%-3llu", text[i].count);
		if (i and not text[i].count) abort();
		for (nat j = 0; j < m; j++) {
			putchar(j == cm and i == cn ? '[' : ' ');
			if (j < text[i].count) 
				{ if (((unsigned char)text[i].data[j]) >> 7) 
					printf("(%02hhx)", (unsigned char) text[i].data[j]); 
				else printf("%c", text[i].data[j]);  }
			else  printf("-");
			putchar(j == cm and i == cn ? ']' : ' ');
		}
		puts(" | ");
	}
	puts(".");
	printf("(cm=%llu,cn=%llu): ", cm, cn);
}
*/



















///   2305066.170942
//OH MY GOD!!! WE HAVE THE CURSOR_COLUMN!!!!! that solves the problem!!! that makes it so that we can know where the beginning!!! obviously!!!!




/*


//write(1, "\033D", 2); 
			do {
				if (om < text[on].count) om++; 
				else if (on < n - 1) { on++; om = 0; } 
			} while (
				om < text[on].count and text[on].data[om] != 10 
				or 
				om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10
			);
					/////TODO:  BUG:   this code currently does not account for soft-wrapping lines. it needs to. so yeah. 

					//// ie, we need to break out of the loop whenever the number of characters on a window_row seems to be 0 beacuse of softwrap too. so yeah. ie, we need a little bit of the display code lol.






			cursor_row--;

			//display(2);

			// *scroll = true;




















			//if (not on and not om) return;


			


			here, we need to move back    on,om     until we see that the screen_column becomes 0. 
								

				this will happen because the line previously is full in terms of screen width, 
				or because we genuinely found a newline char. 


			the first one is very computationally expensive lol. 

			

	
			i think our best bet is just to try to move back a certain number of lines, actually. not just one. hm... 

	
			interestinggg.. 

							wait no thats not how this works. 



						there are two types of things happening:

							1. moving the origin because we are moving up and the cursor is now technically out of veiw. 

								--> here, we need to be moving the origin a large amount, to see more of the text. we need to do a full re print of the screen, basically. so yeah.


							2. moving the origin because of scrolling. this is quite different. here, it is preferred to just move one line up only. not many lines. so yeah.




		


			//write(1, "\033M", 2); 

			//display(1);

			// *scroll = true;












*/

























//	2. vertical scrolling, and vertical SMOOTH scrolling by moving origin









/*

[a] [b] [c] [d] {-}
[-] [-] [-] [-] [-]






*/







































/*
static inline void interpret_escape_code(void) {         // get scrolling and mouse support working. please. its critical feature.
	static nat scroll_counter = 0;
	char c = read_stdin();
	if (c == '[') {
		c = read_stdin();
		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') { move_right(); vdc = vcc; }
		else if (c == 'D') { move_left(); vdc = vcc; }
		else if (c == 'M') {
			read_stdin();
			if (c == 97) {

				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "scroll reporting: [%d:%d:%d]", c, str[0], str[1]);
				if (not scroll_counter) {
					move_down();
				}
				scroll_counter = (scroll_counter + 1) % 4;
				

			} else if (c == 96) {

				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "scroll reporting: [%d:%d:%d]", c, str[0], str[1]);
				if (not scroll_counter) {
					move_up();
				}
				scroll_counter = (scroll_counter + 1) % 4;
				
			} else {
				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "mouse reporting: [%d:%d:%d].", c, str[0], str[1]);
			}
		}
	} 
}

<ESC>D

*/



/*

	oh my gosh i love this text editor so muchhhhh 
	everything is coming to gether so well, and so easily,  and SO SIMPLYYYYY

	i love it so much.        LITERALLY ONLY LIKE 220 LINES OF CODE

						like SERIOUSLYY!?!?!??!




	wow we should work on making the data be written to an actual file after we finish implementing the smooth scrolling functionality 

		which i think is basically the last critical couple features we need to add

			...then we are basicallyyyyyy done lol. 

			i guess we have to add the undo-tree system actually. 
			and also copy/paste functionty lol. 


		thats importanat 




	but yeah, those two things are pretty standard, and easy to implement 

			well 

					undo-tree isnt THATTTTT easy     		but its alright lol



		

*/
	























/* 





<27> <[> <A>

<27> <[> <B>

<27> <[> <C>

<27> <[> <D>



<27> <[>  32 m

		






-------------------------------------------------- 

    a vastly simplified rewrite of the text editor, 
	which uses an improved data structure 
	for performance and stability. 

-----------------------------------------------------



todo implement:
====================


x	1. tabs, newlines and cursor  display


	2. vertical scrolling, and vertical SMOOTH scrolling by moving origin


x	3. soft line wrapping at the window width.





todo:

               /// todo:  make n the address of the last line!! so this should be n = 0;.








------------------------------------------------------ 
























static void debug_display(void) {
	printf("\033[H\033[J");
	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);
	for (nat i = 0; i < n; i++) {
		printf("%-3llu %c ", i, i == cn ? '*' : ':' );
		printf("%-3llu", text[i].count);
		if (i and not text[i].count) abort();
		for (nat j = 0; j < m; j++) {
			putchar(j == cm and i == cn ? '[' : ' ');
			if (j < text[i].count) 
				{ if (((unsigned char)text[i].data[j]) >> 7) 
					printf("(%02hhx)", (unsigned char) text[i].data[j]); 
				else printf("%c", text[i].data[j]);  }
			else  printf("-");
			putchar(j == cm and i == cn ? ']' : ' ');
		}
		puts(" | ");
	}
	puts(".");
	printf("(cm=%llu,cn=%llu): ", cm, cn);
}

static void string_display(void) {
	printf("\033[H\033[J");
	printf("displaying the text { (m=%llu,n=%llu)(cm=%llu,cn=%llu) }: \n", m, n, cm, cn);
	for (nat i = 0; i < n; i++) {
		if (i and not text[i].count) abort();
		for (nat j = 0; j < m; j++) 
			if (j < text[i].count) printf("%c", text[i].data[j]); 
	}
	puts("");
}












*/


































































































/*

[0]  [0]  [0]  [0]  [0]  {45} 

[54] [56] [-]  [-]  [-]  [-]



*/




/*
		(cm < text[cn].count and zero_width(text[cn].data[cm]))
		or
		(cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);

*/



















/*
		if (c == 'c' and p == 'h') { undo(); mode = 2; }
		else if (c == 27 and stdin_is_empty()) mode = 2;
		else if (c == 27) interpret_escape_code();
		else if (c == 127) delete(1);
		else if (c == '\r') insert('\n', 1);
		else insert(c, 1);
*/


















/*
	n = 4;
	text = malloc(sizeof *text * n);


	text[0].data = malloc(m);
	for (int i = 0; i < 5; i++) text[0].data[i] =  "beans"[i];
	text[0].count = m;

	text[1].data = malloc(m);
	for (int i = 0; i < 5; i++) text[1].data[i] = " are "[i];
	text[1].count = 5;

	text[2].data = malloc(m);
	for (int i = 0; i < 5; i++) text[2].data[i] =  "cool "[i];
	text[2].count = 5;

	text[3].data = malloc(m);
	for (int i = 0; i < 5; i++) text[3].data[i] =  "so.."[i];
	text[3].count = 4;
*/






















//printf("0x%02hhx", text[i].data[j]);





/*

text = realloc(text, (n + 1) * sizeof *text);
memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
memmove(text[cn + 1].data, text[cn].data + cm, text[cn].count - cm);
text[cn + 1].count = text[cn].count - cm;
n++;

text[cn].data = malloc(m);
memcpy(text[cn].data, text[cn + 1].data, cm);

*/








/*
m = 7;

		ABC
		DEFG
*/


// memcpy(text[cn].data, text[cn + 1].data, cm);





//if (text[cn].count) {
/*
		if (not cm) {
			cn--;
			cm = text[cn].count;
		}	
		cm--;
*/




//#define reset "\x1B[0m"
//#define red   "\x1B[31m"
//#define green   "\x1B[32m"
//#define blue   "\x1B[34m"
//#define yellow   "\x1B[33m"
//#define magenta  "\x1B[35m"
//#define cyan     "\x1B[36m"
//#define white  yellow

















/*

static inline void display(void) {
	adjust_window_size();
	
	nat length = 9;
	memcpy(screen, "\033[?25l\033[H", 9);

	nat sl = 0, sc = 0, vl = vol, vc = voc;
	struct logical_state state = {0};
	record_logical_state(&state);
	while (1) { 
		if (vcl <= 0 and vcc <= 0) break;
		if (vcl <= state.vol and vcc <= state.voc) break;
		move_left();
	}

	const double f = floor(log10((double) count)) + 1;
	const int line_number_digits = (int)f;
	line_number_width = show_line_numbers * (line_number_digits + 2);

 	nat line = lcl, col = lcc; 
	require_logical_state(&state); 

	do {
		if (line >= count) goto next_visual_line;

		if (show_line_numbers and vl >= vol and vl < vol + window_rows) {
			if (not col or (not sc and not sl)) 
				length += sprintf(screen + length, 
					"\033[38;5;%ldm%*ld\033[0m  ", 
					236L + (line == lcl ? 5 : 0), line_number_digits, line
				);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}
		do {
			if (col >= lines[line].count) goto next_logical_line;  
			
			char k = lines[line].data[col++];
			if (k == '\t') {

				if (vc + (tab_width - vc % tab_width) >= wrap_width and wrap_width) goto next_visual_line;
				do { 
					if (	vc >= voc and vc < voc + window_columns - line_number_width
					and 	vl >= vol and vl < vol + window_rows
					) {
						screen[length++] = ' ';
						sc++;
					}
					vc++;
				} while (vc % tab_width);

			} else {
				if (	vc >= voc and vc < voc + window_columns - line_number_width
				and 	vl >= vol and vl < vol + window_rows
				and 	(sc or visual(k))
				) { 
					screen[length++] = k;
					if (visual(k)) sc++;	
				}
				if (visual(k)) {
					if (vc >= wrap_width and wrap_width) goto next_visual_line; 
					vc++; 
				} 
			}

		} while (sc < window_columns - line_number_width or col < lines[line].count);

	next_logical_line:
		line++; col = 0;

	next_visual_line:
		if (vl >= vol and vl < vol + window_rows) {
			screen[length++] = '\033';
			screen[length++] = '[';	
			screen[length++] = 'K';
			if (sl < window_rows - 1) {
				screen[length++] = '\r';
				screen[length++] = '\n';
			}
			sl++; sc = 0;
		}
		vl++; vc = 0; 
	} while (sl < window_rows);

	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", vsl + 1, vsc + 1 + line_number_width);

	if (not fuzz) write(1, screen, (size_t) length);
}






*/











	//for (nat r = 0; r < window_rows; r++) {



/*
		if (in >= n) goto next_line;

		for (nat c = 0; c < window_columns; c++) {
	

		
			if (screen_column >= window_columns) goto next_line;
			if (text[in].data[im] == 10) {

				
				if (im < text[in].count - 1) im++;
				else {
				if (in >= n - 1) {}
				im = 0; 
				in++; } 
				goto next_line;
			}

			if (im == cm and in == cn) { cursor_row = screen_row; cursor_column = screen_column; }

			if (text[in].data[im] == 9) {
				do {
					if (screen_column >= window_columns) goto next_line;
					length += sprintf(screen + length, " ");
					screen_column++;
				} while (screen_column % 8);

				if (im < text[in].count - 1) im++;
				else {if (in >= n - 1) goto next_line;
				im = 0; 
				in++; } 

			} else {
				length += sprintf(screen + length, "%c", text[in].data[im]);
				if (visual(text[in].data[im])) screen_column++;

				if (im < text[in].count - 1) im++;
				else {if (in >= n - 1) goto next_line;
				im = 0; 
				in++; } 
			}

		


		}


	*/
















/*

	struct node** address_of_xs_next_pointer = &x.next;








----------this declares a variable:---------------



	struct node * *  address_of_xs_next_pointer = ...



-----------this accesess memory:-------------



	 * * address_of_xs_next_pointer = ...



-------------------------



	...

	int v_size = 5;
	int* v = ...;

	push_element_to_vector(42, &v, &v_size);

	cout << "vector pointer: " << v << endl;
	cout << "vector element count: " << v_size << endl;

	...


	void push_element_to_vector(int element, int** vector, int* size) {
		
		// vector  has   5 elements   [0,0,0,0,0]


		(*vector) = realloc((*vector), ((*size) + 1) * sizeof(int));    
		

		// now vector has space for 6 int's, but 5 are in there:   [0,0,0,0,0,?]


		(*vector)[(*size)++] = element;
	}




	*p = 42;



	for (int i = 0; i < 10; i++) {

		*(p + i * sizeof(int)) = 42;                // same as   p[i] = 42;

	}




--------------------------------------------------

	struct person {
		int weight;
		string color;
		char** dogs;
		
		struct person* next;
	};

	char** dogs = malloc(3 * sizeof(char*));

	dogs[0] = "amelia";
	dogs[1] = "rosie";
	dogs[2] = "elle";

	struct person daniel = {0, "", dogs, NULL};

	struct person* p = &daniel;

	p->weight = 120;        //  same as (*p).weight = 120;

	*((*p).dogs) = "peaches"; 

	*((*p).dogs + 2 * sizeof(char*)) = "peaches"; 



------

	int array[5] = {0,0,0,0,0};

	*(array + 2 * sizeof(int)) = 42;









	






*/
	







  ////bug:     this case shouldnt exist:   we should be sitting at the cn+1  cm=0   position when we are at the end of the previous line. ie move instantly. make moveright and movelft do this too!   dont avoid the 0th position. 







/*
	if (cm == m)  {
		text = realloc(text, (n + 1) * sizeof *text);
		memmove(text + cn + 1, text + cn, (n - cn) * sizeof *text);
		n++;
		cn++; 
		text[cn].data = malloc(m);
		text[cn].data[0] = c;
		text[cn].count = 1;
		cm = 1;
	} else 
*/







