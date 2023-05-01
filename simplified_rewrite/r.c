#include <iso646.h>     
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>      // simplified rewrite of the text editor by dwrr on 2304307.024007
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

typedef uint64_t nat;
struct word { char* data; nat count; };
static nat m = 0, n = 0, cm = 0, cn = 0, om = 0, on = 0, window_rows = 0, window_columns = 0, mode = 0, cursor_column = 0, cursor_row = 0;
static struct word* text = NULL;
static char* screen = NULL;

static bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2; }
static bool stdin_is_empty(void) {
	fd_set f;
	FD_ZERO(&f);
	FD_SET(0, &f);
	struct timeval timeout = {0};
	return select(1, &f, 0, 0, &timeout) != 1;
}

static struct termios configure_terminal(void) {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	raw.c_oflag &= ~( (unsigned long)OPOST );
	raw.c_iflag &= ~( (unsigned long)BRKINT 
			| (unsigned long)ICRNL 
			| (unsigned long)INPCK 
			| (unsigned long)IXON );
	raw.c_lflag &= ~( (unsigned long)ECHO 
			| (unsigned long)ICANON 
			| (unsigned long)IEXTEN );
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

static bool delete(void) {
	if (cm) {
		delete_char: cm--;
		text[cn].count--;
		bool b = zero_width(text[cn].data[cm]);
		memmove(text[cn].data + cm, text[cn].data + cm + 1, text[cn].count - cm);
		if (text[cn].count) return b;
		if (not cn) return b;
		n--;
		memmove(text + cn, text + cn + 1, (n - cn) * sizeof *text);
		text = realloc(text, n * sizeof *text); 
		cn--;
		cm = text[cn].count;
		return b;
	} else if (cn < n and text[cn].count) {
		if (not cn) return 0;
		cn--;
		cm = text[cn].count;
		goto delete_char;
	} else return 0;
}

static void move_left(void) {
	do 
	if (cm) cm--; else if (cn) { cn--; cm = text[cn].count - 1; }
	while ( cm  < text[cn].count and zero_width(text[cn].data[cm]) or
		cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);
}

static void move_right(void) {
	do 
	if (cm < text[cn].count) cm++; else if (cn < n - 1) { cn++; cm = 1; }
	while ( cm  < text[cn].count and zero_width(text[cn].data[cm]) or
		cm >= text[cn].count and cn + 1 < n and zero_width(text[cn + 1].data[0])
	);
}

static inline void adjust_window_size(void) {
	static struct winsize window = {0}; 
	ioctl(1, TIOCGWINSZ, &window);
	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 27; window.ws_col = 70; }
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen = realloc(screen, (size_t) (window_rows * window_columns * 4));	
	}
}

static void display(void) {
	adjust_window_size();
	nat screen_column = 0, screen_row = 0;
	cursor_column = 0; cursor_row = 0;
	nat i = 0, j = 0;
	int length = 9;
	memcpy(screen, "\033[?25l\033[H", 9);

	for (i = 0; i < n; i++) {
		for (j = 0; j <= m; ) {
			if (i >= n) break;
			if (i == cn and j == cm) { cursor_row = screen_row; cursor_column = screen_column; }
			if (j >= text[i].count) break;
			const char c = text[i].data[j];

			if (c == 10) { j++; goto print_newline; }
			if (c == 9) {
				do {
					if (screen_column < window_columns) screen_column++; else { j++; goto print_newline; }
					length += sprintf(screen + length, " ");
				} while (screen_column % 8);
			} else {
				if (not zero_width(c)) {
					if (screen_column < window_columns) screen_column++; else goto print_newline;
				}
				length += sprintf(screen + length, "%c", c);
			}
			j++;
			continue;
			print_newline:
			screen[length++] = '\033';
			screen[length++] = '[';
			screen[length++] = 'K';
			if (screen_row < window_rows - 1) {
				screen[length++] = '\r';
				screen[length++] = '\n';
			} else goto print_cursor;
			screen_row++;
			screen_column = 0;
		}
	}
	if (screen_row < window_rows) goto print_newline;
	print_cursor: 
	length += sprintf(screen + length, "\033[%llu;%lluH\033[?25h", cursor_row + 1, cursor_column + 1);
	write(1, screen, (size_t) length);
}

int main(void) {
	m = 10; n = 0; mode = 1;
	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[?1000h\033[7l\033[r", 23);
	char c = 0; bool scroll = false;
loop:	if (not scroll) display(); else scroll = false;
	read(0, &c, 1);
	if (c == 27 and stdin_is_empty()) mode = 0;
	else if (c == 27) {
		read(0, &c, 1);  if (c != '[') goto loop; read(0, &c, 1);
		     if (c == 'D') move_left();
		else if (c == 'C') move_right();
		else if (c == 'M') {
			read(0, &c, 1);
			if (c == 97) { read(0, &c, 1); read(0, &c, 1); write(1, "\033D", 2); /*display_bottom_row();*/ } 
			else if (c == 96) { read(0, &c, 1); read(0, &c, 1); write(1, "\033M", 2); /*display_top_row();*/ } 
			else { char str[3] = {0}; read(0, str + 0, 1); read(0, str + 1, 1); }
		}
	}
	else if (c == 127) while (delete());
	else if (c == 13) insert(10);
	else insert(c);
	if (mode) goto loop;
	write(1, "\033[?1049l\033[?1000l\033[7h", 20);
	tcsetattr(0, TCSAFLUSH, &terminal);
}

















static void display_bottom_row(void) {
	return;
	adjust_window_size();
		
	int length = 6;
	memcpy(screen, "\033[?25l", 6);
	length += sprintf(screen + length, "\033[%llu;%lluH", window_rows, 1LLU);
	length += sprintf(screen + length, "BUBBLES AND BEANS LOLOLOLLOL\033[K");
	length += sprintf(screen + length, "\033[%llu;%lluH\033[?25h", cursor_row + 1, cursor_column + 1);

	write(1, screen, (size_t) length);
}

static void display_top_row(void) {
	return;
	adjust_window_size();
	
	int length = 6;
	memcpy(screen, "\033[?25l", 6);
	length += sprintf(screen + length, "\033[%llu;%lluH", 1LLU, 1LLU);
	length += sprintf(screen + length, "BUBBLES AND BEANS LOLOLOLLOL\033[K");
	length += sprintf(screen + length, "\033[%llu;%lluH\033[?25h", cursor_row + 1, cursor_column + 1);

	write(1, screen, (size_t) length);
}


























/*





*/












//	2. vertical scrolling, and vertical SMOOTH scrolling by moving origin









/*

[a] [b] [c] [d] {-}
[-] [-] [-] [-] [-]






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



