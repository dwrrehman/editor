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
struct state { nat saved, cm, cn; };
struct action {
	nat* children; 
	char* text;
	nat parent, type, choice, count, length;
	struct state pre, post;
};

static nat m = 0, n = 0, cm = 0, cn = 0, om = 0, on = 0, mode = 0, head = 0, action_count = 0, saved = 0;
static nat window_rows = 0, window_columns = 0, cursor_row = 0, cursor_column = 0, cursor_moved = 0; //desired_column = 0, vertical_movement = 0;
static struct word* text = NULL;
static struct action* actions = NULL;

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
	raw.c_lflag &= ~((size_t)ECHO | (size_t)ICANON);
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}

static inline void initialize_buffer(void) {
	cn = 0; cm = 0; mode = 1;
	action_count = 1;
	actions = calloc(1, sizeof(struct action));
	saved = 1;
}

static inline void record_logical_state(struct logical_state* out) {
	out->saved = saved; out->cn = cn; out->cm = cm; 
}

static inline void require_logical_state(struct state* in) {  
	saved = in->saved; cn = in->cn;  cm = in->cm;
}

static inline void create_action(struct action new) {
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(nat) * (size_t) (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	head = action_count;
	actions[action_count++] = new;
}

static void insert(char c, bool should_record) {
	if (should_record and zero_width(c) 
		and not (
			actions[head].type == insert_action and 
			actions[head].post.lcl == lcl and
			actions[head].post.lcc == lcc and
			actions[head].count == 0
		)) return;

	struct action new_action = {0};
	if (should_record and not zero_width(c)) record_logical_state(&new_action.pre);

	cursor_moved = 1; //vertical_movement = 0;
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
	saved = 0;
	if (not should_record) return;

	if (zero_width(c)) {
		actions[head].text = realloc(actions[head].text, (size_t) actions[head].length + 1);
		actions[head].text[actions[head].length++] = c;
		record_logical_state(&(actions[head].post));
		return;
	}

	record_logical_state(&new_action.post);
	new_action.type = insert_action;
	new_action.text = malloc(1);
	new_action.text[0] = c;
	new_action.length = 1;
	create_action(new_action);
}

static char delete(void) {
	cursor_moved = 1; //vertical_movement = 0;

	struct action new_action = {0};
	if (should_record) record_logical_state(&new_action.pre);

	char r = 0;

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
		r: r = c;
	} else if (cn < n and text[cn].count) {
		if (not cn) goto done;
		cn--;
		cm = text[cn].count;
		goto top;
	}
done:	saved = 0;
	if (not should_record) return;

	if (zero_width(r)) {
		actions[head].text = realloc(actions[head].text, (size_t) actions[head].length + 1);
		actions[head].text[actions[head].length++] = r;
		record_logical_state(&(actions[head].post));
		return r;
	}

	record_logical_state(&new_action.post);
	new_action.type = delete_action;
	new_action.text = malloc(1);
	new_action.text[0] = r;
	new_action.length = 1;
	create_action(new_action);
	return r;
}

static void move_left(void) {
	if (not n) return; cursor_moved = 1; //vertical_movement = 0;
	do {	if (cm) cm--; else if (cn) { cn--; cm = text[cn].count - 1; } else break;
		if (cm < text[cn].count) { if (not zero_width(text[cn].data[cm])) break; }
		else if (cn + 1 < n) { if (not zero_width(text[cn + 1].data[0])) break; }
		else break;
	} while (1); 
}

static void move_right(void) {
	if (not n) return; cursor_moved = 1; //vertical_movement = 0;
	do {	if (cm < text[cn].count) cm++; else if (cn + 1 < n) { cn++; cm = 1; }
		if (cm < text[cn].count) { if (not zero_width(text[cn].data[cm])) break; }
		else if (cn + 1 < n) { if (not zero_width(text[cn + 1].data[0])) break; }
		else break;
	} while (1);
}

static void origin_move_up(void) {
	if (not n) return;
	if (om) om--; else if (on) { on--; om = text[on].count - 1; }
	do {	if (om) om--; else if (on) { on--; om = text[on].count - 1; } else break;
		if (om < text[on].count) { if (text[on].data[om] == 10) break; }
		else if (on + 1 < n) { if (text[on + 1].data[0] == 10) break; }
		else break;
	} while (1);
 	if (on or om) { if (om < text[on].count) om++; else if (on + 1 < n) { on++; om = 1; } }
}

static void origin_move_down(void) {
	if (not n) return;
	do {	if (om < text[on].count) om++; else if (on + 1 < n) { on++; om = 1; }
		if (om < text[on].count) { if (text[on].data[om] == 10) break; }
		else if (on + 1 < n) { if (text[on + 1].data[0] == 10) break; }
		else break;
	} while (1);
	if (om < text[on].count) om++; else if (on + 1 < n) { on++; om = 1; }
}

static bool cursor_is_in_view(void) {
	nat column = 0, row = 0, in = on, im = om; 
	while (in <= n) {
		while (im <= m) {
			if (in == cn and im == cm) { cursor_row = row; cursor_column = column; return true; }
			if (in >= n or im >= text[in].count) break;
			const char c = text[in].data[im];
			if (c == 10) { next_char_newline: im++; goto print_newline; }
			else if (c == 9) {
				do { if (column >= window_columns) goto next_char_newline; column++; } while (column % 8);
			} else if (not zero_width(c)) { if (column >= window_columns) goto print_newline; column++; }
			im++; continue; print_newline: if (row >= window_rows - 1) return false;
			row++; column = 0;
		} in++; im = 0;
	} return false;
}

static void put_cursor_in_view(void) {
	cursor_moved = 0;
	if (on < cn) goto origin_first; if (cn < on) goto cursor_first;
	if (om < cm) goto origin_first; if (cm < om) goto cursor_first;
	cursor_first: on = cn; om = cm; origin_move_up(); return;
	origin_first: while (not cursor_is_in_view()) origin_move_down(); return;
}

static void display(void) {
	static char* screen = NULL; static nat screen_size = 0;
	struct winsize window = {0}; ioctl(1, TIOCGWINSZ, &window);
	if (not window.ws_row or not window.ws_col) { window.ws_row = 24; window.ws_col = 60; }
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row; window_columns = window.ws_col - 1;
		screen_size = 32 + (window_rows + 2) * (window_columns * 4 + 5);
		screen = realloc(screen, (size_t) screen_size);
	}
	nat column = 0, row = 0, in = on, im = om, found = false;
	int length = snprintf(screen, screen_size, "\033[?25l\033[H");
	while (in <= n) {
		while (im <= m) {
			if (in == cn and im == cm) { cursor_row = row; cursor_column = column; found = true; }
			if (in >= n or im >= text[in].count) break;
			const char c = text[in].data[im];
			if (c == 10) { next_char_newline: im++; goto print_newline; }
			else if (c == 9) {
				do {
					if (column >= window_columns) goto next_char_newline; column++;
					length += snprintf(screen + length, screen_size, " ");
				} while (column % 8);
			} else {
				if (zero_width(c)) goto print_char;
				if (column >= window_columns) goto print_newline; column++; 
				print_char: length += snprintf(screen + length, screen_size, "%c", c);
			}
			im++; continue;
			print_newline: length += snprintf(screen + length, screen_size, "\033[K");
			if (row >= window_rows - 1) goto print_cursor;
			length += snprintf(screen + length, screen_size, "\n");
			row++; column = 0;
		}
		in++; im = 0;
	}
	if (row < window_rows) goto print_newline;
	print_cursor: if (found) length += snprintf(screen + length, screen_size, "\033[%llu;%lluH\033[?25h", cursor_row + 1, cursor_column + 1);
	write(1, screen, (size_t) length);
}

static inline void replay_action(struct action a) { 
	require_logical_state(&a.pre);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action or a.type == paste_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_action) cut_selection();
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {
	require_logical_state(&a.post);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_action) {
		while (lcc > a.pre.lcc or lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} require_logical_state(&a.pre);
}

static inline void undo(void) {
	if (not head) return;
	reverse_action(actions[head]);
	head = actions[head].parent;
}

static inline void redo(void) {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	replay_action(actions[head]);
}

static inline void alternate_incr(void) {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++;
	// sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void alternate_decr(void) {
	if (actions[head].choice) actions[head].choice--;
	// sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}







static void interpret_sequence(void) { 
	char c = 0; read(0, &c, 1); read(0, &c, 1);
//	     if (c == 'A') move_up(); 
//	else if (c == 'B') move_down();
	     if (c == 'C') move_right();
	else if (c == 'D') move_left(); 
}

int main(int argc, const char** argv) {
	m = 5; n = 0; // 5 debug   256 release.
	if (argc < 2) goto here;
	FILE* file = fopen(argv[1], "r");
	if (not file) { printf("error: fopen: %s", strerror(errno)); return 0; }
	fseek(file, 0, SEEK_END);
        size_t length = (size_t) ftell(file);
	char* local_text = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(local_text, sizeof(char), length, file);
	fclose(file);
	for (size_t i = 0; i < length; i++) insert(local_text[i]);
	cn = 0; cm = 0; on = 0; om = 0; cursor_moved = 0; mode = 1;
	free(local_text);
here:;	struct termios terminal = configure_terminal();
	write(1, "\033[?1049h\033[7l", 12);
	char c = 0;
loop:	if (cursor_moved) put_cursor_in_view();
	display(); read(0, &c, 1);
	if (c == 27 and stdin_is_empty()) mode = 0;
	else if (c == 27) interpret_sequence();
	else if (c == 127) while (zero_width(delete()));
	else if (c == ']') for (int i = 100; i--;) move_right(); // debug
	else if (c == '[') for (int i = 100; i--;) move_left(); // debug
	else if (c == '-') origin_move_up(); // debug
	else if (c == '=') origin_move_down(); // debug
	else insert(c);
	if (mode) goto loop;
	write(1, "\033[?1049l\033[7h\033[?25h", 18);
	tcsetattr(0, TCSAFLUSH, &terminal);
}







// sitting at 230 lines 		without adding move_up and move_down, so far.       not good. we have to ge that down. by like alot. 


/*

else if (c == '\\') { // debug 
		char string[1024] = {0};
		snprintf(string, sizeof string, "\n%llu,%llu(%c):%llu,%llu(%c)\n", om, on, text[on].data[om], cm, cn,  text[cn].data[cm]);
		nat string_length = strlen(string);
		for (nat i = 0; i < string_length; i++) insert(string[i]);
	}

*/

//// NOTE:  currently these move_up and move_down functions don't account for softwrapped lines at all. also they are HECKKAAAA SLOWWWWWWWW.

//static void move_up(void) {
//	if (not n) return;
//	cursor_is_in_view();
//	if (not vertical_movement) desired_column = cursor_column;
//	const nat target_column = desired_column, target_row = cursor_row - 1;
//	if (not cursor_row) { cn = 0; cm = 0; goto done; }
//	do { move_left(); if (not cursor_is_in_view()) put_cursor_in_view(); }
//	while ((cn or cm) and (cursor_row > target_row or cursor_row == target_row and cursor_column > target_column));
//done:	vertical_movement = 1; cursor_moved = 1;
//}

//static void move_down(void) {
//	if (not n) return;
//	cursor_is_in_view();
//	if (not vertical_movement) desired_column = cursor_column;
//	const nat target_column = desired_column, target_row = cursor_row + 1;
//	do { move_right(); if (not cursor_is_in_view()) put_cursor_in_view(); } 
//	while ((cn < n - 1 or cm < text[cn].count) and 
//	(cursor_row < target_row or cursor_row == target_row and cursor_column < target_column));
//	if (cursor_row > target_row) move_left();
//done:	vertical_movement = 1; cursor_moved = 1;
//}



































/*
		-------------- heres my solution, to solve this in general. -----------------






	bool found = false;
	nat in = on, im = om;

	while (in <= n) {
		while (im <= m) {
			if (in == cn and im == cm) { cursor_row = row; cursor_column = column; goto found; }
			if (in >= n or im >= text[in].count) break;
			const char c = text[in].data[im];

			if (c == 10) { next_char_newline: im++; goto print_newline; }
			else if (c == 9) {
				do {
					if (column >= window_columns) goto next_char_newline; column++;
				} while (column % 8);
			} else {
				if (zero_width(c)) goto print_char;
				if (column >= window_columns) goto print_newline; column++; print_char:;
			}
			im++; continue; print_newline:  if (row >= window_rows - 1) goto not_found; row++; column = 0;
		}
		in++; im = 0;
	}
	found: return 1;
	not_found: return 0;




			

				if (view_invalid and ) {
					if (origin < cursor) while (not_in_view()) move_origin_left();
					else if (origin > cursor) while (not_in_view()) move_origin_right();
				}



				// view_invalid is set to 1 when we scroll, ever. this will trigger the not_in_view() check to run,  (the above code)


					and then we will move the origin, line by line, (in screen space, ie, accounting for soft lines too!!) until not_in_view returns true.

							yes, its computationally expensive, 

							but it doesnt matter, because only have to do this once in a while... basically.


											cool huh!?






					oh, and then, theres another pass, i think, that will try to move OH





		OH MY GOSH



	AHHH


				THATS THE SOL TO THAT ONE PROBBBB





						we move the origin rightttt (forwardsssss)       until the cursor has its original value again!!!




							ie, the cursors position should be the same before and AFTER the origin move 




									THATSSSS HOW WE KNOW HOW FAR TO MOVE






				THATS HOW WE CORRECT FOR THE SMALL ERROR IN HOW MUCH TO MOVE THE ORIGIN BACKWARDS


								GAHHHHHHH okay that would work yayy







				yayyyyyyyyyy

								happy i found that.   can't wait to code it up 







			basically i just want to get this working now, 


									and make it correct in general. for all possible origin and cursor manips. 
			ill try to minimize the line count later lol. 




										we have a good starting place, though,   prior to doing this,  the code was only 188




												188 lines of code.





													so crazy!!!!!!! wow!!





												so incredible.  almost a fully working editor in only 188 lines lol.



											so crazy 





				anyways



lets push this now











								













	if (on == cn and om == cm) {
		do origin_move_left();
		while ((om or on) and  (  om < text[on].count and text[on].data[om] != 10 or 
			 om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10)
		);
	}
*/








/*
	if (barely_in_view()) {
		do origin_move_right();
		while (  om < text[on].count and text[on].data[om] != 10 or 
			 om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10
		);
	}
*/







/*
	else if (c == '=') {
		do origin_move_right();
		while (  om < text[on].count and text[on].data[om] != 10 or 
			 om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10
		);
		//origin_move_right();
	}
	else if (c == '-') {
		do origin_move_left();
		while ((om or on) and  (  om < text[on].count and text[on].data[om] != 10 or 
			 om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10)
		);
		//origin_move_left();
	}
*/








/*
static void origin_move_left(void) {
	if (not n) return;
	do 
	if (om) om--; else if (on) { on--; om = text[on].count - 1; }
	while ( om  < text[on].count and zero_width(text[on].data[om]) or
		om >= text[on].count and on + 1 < n and zero_width(text[on + 1].data[0])
	);
}

static void origin_move_right(void) {
	if (not n) return;
	do if (om < text[on].count) om++; else if (on < n - 1) { on++; om = 1; }
	while ( om  < text[on].count and zero_width(text[on].data[om]) or
		om >= text[on].count and on + 1 < n and zero_width(text[on + 1].data[0])
	);
}
*/











// 188 so far!!! not bad!!!! 
















  // if barely in view -->      if local_cursor_row == window_rows - 1 and not local_cursor_column    








// if barely in view -->     if not local_cursor_row and not local_cursor_column       i think.

















/*


static void origin_move_left(void) {
	if (not n) return;
	do if (om) om--; else if (on) { on--; om = text[on].count - 1; }
	while ( om  < text[on].count and zero_width(text[on].data[om]) or
		om >= text[on].count and on + 1 < n and zero_width(text[on + 1].data[0])
	);
}

static void origin_move_right(void) {
	if (not n) return;
	do if (om < text[on].count) om++; else if (on < n - 1) { on++; om = 1; }
	while ( om  < text[on].count and zero_width(text[on].data[om]) or
		om >= text[on].count and on + 1 < n and zero_width(text[on + 1].data[0])
	);
}









if (screen_row == 1) { on1 = i; om1 = j; newline_index1 = (nat) length - 1; }








if (not found) { }
	if (not shifted and cursor_row == window_rows - 1) {
		shift_down: screen[newline_index1] = 13; cursor_row--; screen_row--; screen_column = 0; 
		on = on1; om = om1; shifted = true; goto print_newline;
	} else if (not cursor_row and (on or om)) {
		shift_up: do origin_move_left();
		while ((om or on) and  (  om < text[on].count and text[on].data[om] != 10 or 
			 om >= text[on].count and on + 1 < n and text[on + 1].data[0] != 10)
		);
		//origin_move_right();
		
		//cursor_row++;

		
	}


























	else if (c == ';') {

		printf("shifted = %d, cursor_row = %llu, window_rows = %llu\n", 
			shifted,      cursor_row,        window_rows
		);
		fflush(stdout);
		abort();
	}

*/










//int index = 9; while (screen[ti] != 10) ti++; screen[ti] = 13;
		//int start = 9, end = start;
		//while (screen[end] != 10) end++; end++;
		//memmove(screen + start, screen + end, (size_t)(length - end));
		//length -= end - start;


		// start = window_columns * 4 + 5






















		// go backwards from on and om   at most     window_width number of chars, backwards,    or until you hit a newline.
		// this is the string we want to put before the current screen string. 

		// prepend a line at the beginning by using the empty space, at the beginning, 
		// then subtract from start according to how many things you put there.
		// remove the last line, by doing length --, until you hit the newline of the second-to-last line. 








    // the fund prob is we are setting it to zero here









	                  // d bottom, add top
						   //      :          subtract from length  to d bottom.
						   //      :          subtract from screen_start, and overwrite the header (9 chars) with line  to add top. 





/// if (cursor_row == window_rows - 1) {}      // add bottom, d top      
		    				       //      :          change \n to \r  on first line  to d top.
						       //      :          append another line  to add bottom.  easy!



















//  bool* scroll

//  bool scroll = false;

//  if (not scroll)  else scroll = false;   //debug_display();






/*




if (should_move_origin_forwards and screen_row == 1) {
				screen_row = 0; length = 9; should_move_origin_forwards = false; on = i; om = j;
			}





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







