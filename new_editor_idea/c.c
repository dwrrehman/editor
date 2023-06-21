#include <stdio.h>  // 2306202.165852:  
#include <stdlib.h> // a screen based simpler editor based on the ed-like one, 
#include <string.h> // uses a string ds, and is not modal. uses capital letters for commands! 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
typedef size_t nat;

struct action {
	char* text;
	nat* children;
	nat parent, choice, count, length, insert, pre_cursor, post_cursor, pre_origin, post_origin, pre_saved, post_saved;
};

static struct winsize window;
static char filename[2048] = {0};
static char* text = NULL;
static nat cursor = 0, origin = 0, anchor = 0, cursor_row = 0, cursor_column = 0, count = 0, saved = 1;

static nat action_count = 0, head = 0;
static struct action* actions = NULL;

static void move_left(void) {
	if (origin < cursor or not origin) goto decr; 
	origin--; 
	e: if (not origin) goto decr; 
	origin--; 
	if (text[origin] != 10) goto e; 
	origin++;
	decr: if (cursor) cursor--;
}

static void move_right(void) {
	if (cursor < count) cursor++; 
	if (cursor_row != window.ws_row - 1) return;
	l: origin++; 
	if (origin >= count) return;
	if (text[origin] != 10) goto l; 
	origin++;
}

static void create_action(struct action new) {
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(size_t) * (size_t) (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	head = action_count;
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	actions[action_count++] = new;
}

static void delete(bool should_record) {
	if (not cursor) return;
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};
	const char c = text[cursor - 1];
	memmove(text + cursor - 1, text + cursor, count - cursor);
	count--;
	text = realloc(text, count); 
	saved = 0; 
	move_left();
	if (not should_record) return;
	new.post_cursor = cursor; 
	new.post_origin = origin; 
	new.post_saved = saved;
	new.insert = 0;
	new.text = malloc(1);
	new.text[0] = c;
	new.length = 1;
	create_action(new);
}

static void insert(char c, bool should_record) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};
	text = realloc(text, count + 1);
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c;
	count++;
	saved = 0;
	move_right();
	if (not should_record) return;
	new.post_cursor = cursor;
	new.post_origin = origin;
	new.post_saved = saved;
	new.insert = 1;
	new.text = malloc(1);
	new.text[0] = c;
	new.length = 1;
	create_action(new);
}

static void insert_string(char* string, nat length) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};
	for (nat i = 0; i < length; i++) insert(string[i], 0);
	new.post_cursor = cursor; 
	new.post_origin = origin; 
	new.post_saved = saved;
	new.insert = 1;
	new.text = strndup(string, length); 
	new.length = length;
	create_action(new);
}

static void paste(void) {
	FILE* file = popen("pbpaste", "r");
	if (not file) { 
		perror("paste popen"); 
		getchar(); 
		return; 
	}
	char* string = NULL;
	nat length = 0;
	int c = 0;
	while ((c = fgetc(file)) != EOF) {
		string = realloc(string, length + 1);
		string[length++] = (char) c;
	}
	pclose(file);
	insert_string(string, length);
}

static inline void copy(void) {
	FILE* file = popen("pbcopy", "w");
	if (not file) {
		perror("copy popen");
		getchar();
		return;
	}
	if (anchor < cursor) fwrite(text + anchor, 1, cursor - anchor, file);
	else fwrite(text + cursor, 1, anchor - cursor, file);
	pclose(file);
}

static inline void cut(void) {
	
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};

		char* deleted = text + cursor;
		nat length = anchor - cursor;
	if (anchor < cursor) {

		deleted = text + anchor;
		length = cursor - anchor;

	}
	new.post_cursor = cursor; 
	new.post_origin = origin; 
	new.post_saved = saved;
	new.insert = 0;
	new.text = deleted;
	new.length = length;
	create_action(new);
}

static void display(bool output) {

	ioctl(0, TIOCGWINSZ, &window);
	const nat screen_size = window.ws_row * window.ws_col * 4;
	char* screen = calloc(screen_size, 1);

	memcpy(screen, "\033[H\033[2J", 7);
	nat length = 7;
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
			if (column >= window.ws_col) { column = 0; row++; }
			if ((unsigned char) k >> 6 != 2) column++;
			screen[length++] = k;
		}
	}
	if (i == cursor) { cursor_row = row; cursor_column = column; }
	length += (nat) snprintf(screen + length, 16, "\033[%lu;%luH", cursor_row + 1, cursor_column + 1);
	if (output) write(1, screen, length);
	free(screen);
}

static void move_left_chunk(void) {
	for (nat i = 0; i < 100; i++) {
		display(0);
		move_left();
	}
}

static void move_right_chunk(void) {
	for (nat i = 0; i < 100; i++) {
		display(0);
		move_right();
	}
}

static struct termios configure_terminal(void) {
	struct termios terminal;
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON | IEXTEN); 			//   | ISIG
	copy.c_iflag &= ~((size_t) IXON); 					// BRKINT  | 
	tcsetattr(0, TCSAFLUSH, &copy);
	return terminal;
}

static void load(void) {
	if (not saved) return;
	FILE* file = fopen(filename, "r");	
	if (not file) {
		perror("load fopen"); 
		getchar();
		return; 
	}
	fseek(file, 0, SEEK_END); 
	count = (size_t) ftell(file); 
	text = malloc(count); 
	fseek(file, 0, SEEK_SET); 
	fread(text, 1, count, file); 
	fclose(file);
}

static void save(void) {
	if (not *filename) {
		puts("save: error: empty filename"); 
		getchar();
		return;
	}
	FILE* output_file = fopen(filename, "w");
	if (not output_file) { 
		perror("save fopen");
		getchar(); 
		return;
	}
	fwrite(text, 1, count, output_file); 
	fclose(output_file); 
	saved = 1;
}

static void execute(void) {
	char command[4096] = {0};
	strlcpy(command, "input_string", sizeof command);
	strlcat(command, " 2>&1", sizeof command);
	printf("executing: %s\n", command);
	FILE* f = popen(command, "r");
	if (not f) {
		printf("error: could not run command \"%s\"\n", command);
		perror("execute popen");
		getchar();
		return;
	}
	char* string = NULL;
	nat length = 0;
	char line[2048] = {0};
	while (fgets(line, sizeof line, f)) {
		size_t l = strlen(line);
		string = realloc(string, length + l);
		memcpy(string + length, line, l);
		length += l;
	}
	pclose(f);
	insert_string(string, length);
}

static void alternate(void) {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0;
}

static void undo(void) {
	if (not head) return;
	const struct action a = actions[head];
	/// printf("u +%lu -%lu, %lu:%lu\n", a.ilength, a.dlength, a.count, a.choice);
	cursor = a.post_cursor;
	origin = a.post_origin;
	saved = a.post_saved;
	if (not a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);	
	cursor = a.pre_cursor;
	origin = a.pre_origin;
	saved = a.pre_saved;
	head = a.parent;
}

static void redo(void) {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	const struct action a = actions[head];
	///  printf("r +%lu -%lu, %lu:%lu\n", a.ilength, a.dlength, a.count, a.choice);
	cursor = a.pre_cursor;
	origin = a.pre_origin;
	saved = a.pre_saved;
	if (a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);
	cursor = a.post_cursor;
	origin = a.post_origin;
	saved = a.post_saved;
}

int main(int argc, const char** argv) {
	strlcpy(filename, argc >= 2 ? argv[1] : "Untitled.txt", sizeof filename);
	struct termios terminal = configure_terminal();
	actions = calloc(1, sizeof(struct action));
	action_count = 1;
	char c = 0;
	load();
loop:	display(1);
	read(0, &c, 1);
	if (c == 27) goto loop;
	else if (c == 'Q') { if (saved) goto done; }
	else if (c == 'A') anchor = cursor;
	else if (c == 'S') save();
	else if (c == 'N') move_left();
	else if (c == 'E') move_right();
	else if (c == 'U') move_left_chunk();
	else if (c == 'P') move_right_chunk();
	else if (c == 'C') undo();
	else if (c == 'K') redo();
	else if (c == 'I') alternate();
	else if (c == 'M') copy();
	else if (c == 'R') cut();
	else if (c == 'L') paste();
	else if (c == 'K') execute();
	else if (c == 127) delete(1);
	else insert(c, 1);
	goto loop;
done:	printf("\033[H\033[2J");
	tcsetattr(0, TCSAFLUSH, &terminal);
}











































/*


//////////printf("%s %s %lu\n", filename, saved ? "s" : "w", count); 






const size_t new = count + a.dlength - a.ilength;
	if (a.dlength > a.ilength) text = realloc(text, new);
	if (a.dlength != a.ilength) memmove(text + *cursor + a.dlength - a.ilength, text + *cursor, count - *cursor);
	memcpy(text + *cursor - a.ilength, a.deleted, a.dlength);
	if (a.dlength < a.ilength) text = realloc(text, new);
	count = new;





const size_t new = count + a.ilength - a.dlength;
	if (a.ilength > a.dlength) text = realloc(text, new);
	if (a.ilength != a.dlength) memmove(text + *cursor + a.ilength - a.dlength, text + *cursor, count - *cursor);
	memcpy(text + *cursor - a.dlength, a.inserted, a.ilength);
	if (a.ilength < a.dlength) text = realloc(text, new);
	count = new; 









		struct action new = {0};
		new.pre = *cursor;
		const size_t dlength = *cursor - cursor[2];
		char* deleted = text ? strndup(text + cursor[2], dlength) : 0;
		const size_t new_count = count + length - dlength;
		if (length > dlength) text = realloc(text, new_count);
		if (length != dlength) memmove(text + *cursor + length - dlength, text + *cursor, count - *cursor);
		if (length) memcpy(text + cursor[2], input, length);
		if (length < dlength) text = realloc(text, new_count);
		count = new_count; if (length or dlength) saved = 0;
		
		*cursor += length - dlength; cursor[2] = *cursor; cursor[1] = *cursor;
		new.post = *cursor; new.parent = head;
		new.inserted = strndup(input, length); new.ilength = length;
		new.deleted = deleted; new.dlength = dlength;
		actions[head].children = realloc(actions[head].children, sizeof(size_t) * (size_t) (actions[head].count + 1));
		actions[head].choice = actions[head].count;
		actions[head].children[actions[head].count++] = action_count;
		head = action_count;
		actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
		actions[action_count++] = new;
		printf("+%lu -%lu \n", length, dlength); insert = 0;
*/




// if (anchor > cursor) { size_t temp = cursor; cursor = anchor; anchor = temp; }










































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

