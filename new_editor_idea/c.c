#include <stdio.h>  // 2306202.165852:  
#include <stdlib.h> // a screen based simpler editor based on the ed-like one, 
#include <string.h> // uses a string ds, and is not modal. uses capital letters for commands! 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>         //  MAKE THE EDITOR ONLY USE ALPHA KEYS FOR KEYBINDINGSSSSSSSS PLEASEEEEEEEEE MODALLLLLL PLZ
#include <stdbool.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>    // todo:     - fix memory leaks       - write manual        x- test test test test test
#include <sys/ioctl.h>    //           - write feature list     x- redo readme.md     x- test with unicode    
typedef size_t nat;       //           x- fix the display(0) performance bug on move_right(). 

struct action {
	char* text;
	nat* children;
	nat parent, choice, count, length, insert,
	pre_cursor, post_cursor, pre_origin, 
	post_origin, pre_saved, post_saved;
};

static struct winsize window;
static char filename[4096] = {0};
static char* text = NULL;
static char* clipboard = NULL;
static struct action* actions = NULL;
static nat cursor = 0, origin = 0, anchor = 0, cursor_row = 0, cursor_column = 0, 
	   count = 0, saved = 1, cliplength = 0, mode = 0, action_count = 0, head = 0;

static void handler(int __attribute__((unused))_) {}
static void display(bool output) {
	if (output) ioctl(0, TIOCGWINSZ, &window);
	const nat screen_size = window.ws_row * window.ws_col * 4;
	char* screen = output ? calloc(screen_size, 1) : 0;
	if (output) memcpy(screen, "\033[?25l\033[H", 9);
	nat length = 9, row = 0, column = 0, i = origin;
	for (; i < count; i++) {
		if (i == cursor) { cursor_row = row; cursor_column = column; }
		if (row >= window.ws_row) break;
		char k = text[i];
		if (k == 10) {
			column = 0; row++;
			if (output) { 
				memcpy(screen + length, "\033[K", 3); 
				length += 3; 
				if (row < window.ws_row) screen[length++] = 10;
			}
		} else if (k == 9) {
			const uint8_t amount = 8 - column % 8;
			column += amount;
			if (output) memcpy(screen + length, "        ", amount);
			if (output) length += amount;
		} else {
			if (column >= window.ws_col) { column = 0; row++; }
			if ((unsigned char) k >> 6 != 2) column++;
			if (output) screen[length++] = k;
		}
	}
	if (i == cursor) { cursor_row = row; cursor_column = column; }
	if (output) length += (nat) snprintf(screen + length, 30, "\033[K\033[%lu;%luH\033[?25h", cursor_row + 1, cursor_column + 1);
	if (output) write(1, screen, length);
	if (output) free(screen);
}

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
	if (cursor >= count) return;
	char c = text[cursor++];
	if (c == 10) { cursor_column = 0; cursor_row++; }
	else if (c == 9) cursor_column += 8 - cursor_column % 8;
	else if (cursor_column >= window.ws_col) { cursor_column = 0; cursor_row++; }
	else if ((unsigned char) c >> 6 != 2) cursor_column++; 
	if (cursor_row < window.ws_row) return;
	nat column = 0; 
	l: if (origin >= count) return;
	c = text[origin++];
	if (c == 10) goto done;
	else if (c == 9) column += 8 - column % 8;
	else if (column >= window.ws_col) goto done;
	else if ((unsigned char) c >> 6 != 2) column++;
	goto l; done: cursor_row--;
}

static void create_action(struct action new) {
	new.post_cursor = cursor; 
	new.post_origin = origin; 
	new.post_saved = saved;
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
	new.insert = 0;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);
}

static void insert(char c, bool should_record) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};
	text = realloc(text, count + 1);
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c;
	count++; saved = 0;
	move_right();
	if (not should_record) return;
	new.insert = 1;
	new.length = 1;
	new.text = malloc(1);
	new.text[0] = c;
	create_action(new);
}

static void forwards(void) {
	nat t = 0;
loop:	if (t == cliplength or cursor >= count) return;
	if (text[cursor] != clipboard[t]) t = 0; else t++;
	move_right(); 
	goto loop;
}

static void backwards(void) {
	nat t = cliplength;
loop:	if (not t or not cursor) return;
	move_left(); t--; 
	if (text[cursor] != clipboard[t]) t = cliplength;
	goto loop;
}

static void insert_string(char* string, nat length) {
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};
	for (nat i = 0; i < length; i++) insert(string[i], 0); 
	new.insert = 1;
	new.text = strndup(string, length);
	new.length = length;
	create_action(new);
}

static void paste(void) { insert_string(clipboard, cliplength); }
static void cut(void) {
	if (anchor > cursor) { nat temp = anchor; anchor = cursor; cursor = temp; }
	struct action new = {.pre_cursor = cursor, .pre_origin = origin, .pre_saved = saved};
	cliplength = cursor - anchor;
	clipboard = strndup(text + anchor, cliplength);
	for (nat i = 0; i < cliplength; i++) delete(0);
	new.insert = 0;
	new.text = strdup(clipboard);
	new.length = cliplength;
	create_action(new);
}

static struct termios configure_terminal(void) {
	struct termios terminal;
	tcgetattr(0, &terminal);
	struct termios copy = terminal; 
	copy.c_lflag &= ~((size_t) ECHO | ICANON | IEXTEN | ISIG);
	copy.c_iflag &= ~((size_t) BRKINT | IXON);
	tcsetattr(0, TCSAFLUSH, &copy);
	return terminal;
}

static inline void copy(void) {
	FILE* file = popen("pbcopy", "w");
	if (not file) {
		perror("copy popen");
		getchar(); return;
	}
	if (anchor < cursor) fwrite(text + anchor, 1, cursor - anchor, file);
	else fwrite(text + cursor, 1, anchor - cursor, file);
	pclose(file);
}

static void execute(const char* input_command) {
	char command[4096] = {0};
	strlcpy(command, input_command, sizeof command);
	strlcat(command, " 2>&1", sizeof command);
	printf("executing: %s\n", command);
	FILE* f = popen(command, "r");
	if (not f) {
		printf("error: could not execute command \"%s\"\n", command);
		perror("execute popen");
		getchar(); return;
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

static void load(const char* this) {
	if (not saved) {
		puts("load: error: unsaved changes"); 
		getchar(); return;
	}
	FILE* file = fopen(this, "r");	
	if (not file) {
		perror("load fopen"); 
		getchar(); return;
	}
	fseek(file, 0, SEEK_END);
	count = (size_t) ftell(file);
	text = malloc(count);
	fseek(file, 0, SEEK_SET);
	fread(text, 1, count, file);
	fclose(file);
	anchor = 0; cursor = 0; origin = 0;
	clipboard = NULL; cliplength = 0;
	cursor_row = 0; cursor_column = 0;
	saved = 1; head = 0;
	actions = calloc(1, sizeof(struct action));
	action_count = 1; mode = 2;
	strlcpy(filename, this, sizeof filename);
}

static void save(void) {
	if (not *filename) {
		puts("save: error: currently unnamed"); 
		getchar(); return;
	}
	FILE* output_file = fopen(filename, "w");
	if (not output_file) { 
		perror("save fopen");
		getchar(); return;
	}
	fwrite(text, 1, count, output_file); 
	fclose(output_file); 
	saved = 1;
}

static void rename_file(const char* new) {
	if (not access(new, F_OK)) { 
		printf("rename: file \"%s\" exists", new); 
		getchar(); return; 
	}
	if (*filename and rename(filename, new)) {
		printf("rename: error renaming to: \"%s\": \n", new);
		perror("rename"); getchar(); return;
	} else strlcpy(filename, new, sizeof filename);
}

static void undo(void) {
	if (not head) return;
	struct action a = actions[head];
	cursor = a.post_cursor; origin = a.post_origin; saved = a.post_saved;
	if (not a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);	
	cursor = a.pre_cursor; origin = a.pre_origin; saved = a.pre_saved;
	head = a.parent; a = actions[head];
	if (a.count > 1) { printf("\033[0;44m[%lu:%lu]\033[0m", a.count, a.choice); getchar(); }
}

static void redo(void) {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	const struct action a = actions[head];
	cursor = a.pre_cursor; origin = a.pre_origin; saved = a.pre_saved;
	if (a.insert) for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	else for (nat i = 0; i < a.length; i++) delete(0);
	cursor = a.post_cursor; origin = a.post_origin; saved = a.post_saved;
	if (a.choice + 1 < a.count) actions[head].choice++; else actions[head].choice = 0;
	if (a.count > 1) { printf("\033[0;44m[%lu:%lu]\033[0m", a.count, actions[head].choice); getchar(); }
}

static void interpret_arrow_key(void) {
	char c = 0;
	read(0, &c, 1); 
	read(0, &c, 1); 
	if (c == 'C') move_right(); 
	if (c == 'D') move_left();
}

static void sendc(void) {
	if (not strcmp(clipboard, "discard and quit")) mode = 0;
	else if (not strcmp(clipboard, "copy")) copy();
	else if (cliplength > 5 and not strncmp(clipboard, "do ", 3)) execute(clipboard + 3);
	else if (cliplength > 5 and not strncmp(clipboard, "open ", 5)) load(clipboard + 5);
	else if (cliplength > 7 and not strncmp(clipboard, "rename ", 7)) rename_file(clipboard + 7);
	else { printf("unknown command: %s\n", clipboard); getchar(); }
}

int main(int argc, const char** argv) {
	struct sigaction action = {.sa_handler = handler}; 
	sigaction(SIGINT, &action, NULL);
	struct termios terminal = configure_terminal();
	actions = calloc(1, sizeof(struct action));
	action_count = 1; mode = 1;
	char c = 0, p1 = 0, p2 = 0, state = 0;
	if (argc >= 2) load(argv[1]);
	cliplength = 1; clipboard = strdup("\n");
loop:	display(1);
	read(0, &c, 1);
	if (mode == 1) {
		if (c == 17) mode = 2;
		else if (c == 27) interpret_arrow_key();
		else if (c == 127) delete(1);
		else if (c == 't' and p1 == 'r' and p2 == 'd') { undo(); undo(); p1 = 0; p2 = 0; mode = 2; }
		else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);
	} else if (mode == 2) {
		if (state == 1) {
			if (c == 'a') {}
			else if (c == 'd') paste();
			else if (c == 'r') save();
			else if (c == 't') sendc(); 
			else if (c == 'n') {}
			else if (c == 'u') cut();
			else if (c == 'p') {}
			else if (c == 'i') { if (saved) mode = 0; }
			state = 0;
		} else {
			if (c == 'a') undo();
			else if (c == 'd') { anchor = cursor; mode = 1; }
			else if (c == 'r') backwards();
			else if (c == 't') forwards();
			else if (c == 'n') state = 1;
			else if (c == 'u') move_left();
			else if (c == 'p') move_right();
			else if (c == 'i') redo();
		}
	} p2 = p1; p1 = c;
	if (mode) goto loop;
	printf("\033[H\033[2J");
	tcsetattr(0, TCSAFLUSH, &terminal);
}























//		if (c == 'q') { if (b) 	{}		else 	{ b = 1; goto loop; } 	} /* Q */
//		if (c == 'd') { if (b) 	cut();		else 	backwards(); 		} /* D */
//		if (c == 'r') { if (b) 	paste(); 	else 	undo(); 		} /* R */
//		if (c == 'a') { if (b) 	sendc();	else 	anchor = cursor; 	} /* A */
//		if (c == 's') { if (b) 	save();		else 	move_left(); 		} /* S */
//		if (c == 'h') { if (b) 	{}		else 	forwards(); 		} /* H */
//		if (c == 'x') { if (b) 	{} 		else 	move_right(); 		} /* X */
//		if (c == 'z') { if (b)  {}		else 	redo();  		} /* Z */
//

/*

		




	else if (c == 17) { if (b) 			else 	{ b = 1; goto loop; } 	}  Q 
	else if (c == 4)  { if (b) 	cut();		else 	backwards(); 		}  D 
	else if (c == 18) { if (b) 	paste(); 	else 	undo(); 		}  R 
	else if (c == 1)  { if (b) 	sendc();	else 	anchor = cursor; 	}  A 
	else if (c == 19) { if (b) 	save();		else 	move_left(); 		}  S 
	else if (c == 8)  { if (b) 	copy();		else 	forwards(); 		}  H 
	else if (c == 24) { if (b) 	execute(); 	else 	move_right(); 		}  X 
	else if (c == 26) { if (b)  	{}		else 	redo();  		}  Z 
	else if (c == 127) delete(1);
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c, 1);
*/








































/*

 Q	quit() 		deadstop()
 D 	cut()		backwards()
 R 	paste() 	undo()
 A 	sendc()		anchor()
 S 	save()		move_left()
 H 	copy()		forwards()
 X 	execute() 	move_right()
 Z 	load()		redo()

*/

















/** ------------------layer0------------------------------


	control-Q 	:	(deadstop)
		
	control-D	:	backwards()

	control-R	:	undo()

	control-A	:	anchor()

	control-S	:	move_left()

	control-H	:	forwards()

	control-X	:	move_right()

	control-Z	:	redo()



------------------------layer1-------------------------




	control-Q 	:	quit()
		
	control-D	:	cut()

	control-R	:	

	control-A	:	send_command()

	control-S	:	save()

	control-H	:	copy()

	control-X	:	execute()

	control-Z	:	


-------------------------------------------------------






*/























		//puts("rename: unimplemented!"); 
		//getchar(); 

	//if (*filename and not len) goto save;
	//if (not len) { puts("s:no filename given"); goto loop; }







/*
if (anchor < cursor) {

		deleted = text + anchor;
		length = cursor - anchor;

	}
*/









/*

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




*/





























//if (c >= 32) { printf("\033[0;44m[%lu]\033[0m", (nat) c); getchar(); }
















/*








		wait i found an error with the execute command, using it to implement copy and paste though!!!


				we actually can't give input of the current selection to the command, 

							because we need to know what the current command is 


							based on what is selected!!


							thats like a critical thing that it needs to do. 


						it cuts the current selection, and executes it as a bash command. thats what it does. 
				so like, how do we give input of another selection then?


					we don't lol.    we need another command for copy lol. 



							i feel like giving input to the command is like      basicallyyyy futile, i think?.. idk.. lol. it seems futile lol.
						so yeah. i think so honestly 


								we should totally have a copy command. 













	 ---------------------- final keybindings: -------------------------------

			lets add a second anchor key!!!! that should help a ton. yay.     a     and    q      i think. 
a	- anchor 
d	- move_left_chunk
h	- move_right_chunk
q	- deadstop (*)
r	- cut
s	- move_left
x	- move_right

qa	- undo
qd	- execute
qh	- copy
qq	- quit
qr	- 
qs	- save
qx	- redo_alternate





------------------------------------------------------------



q	- deadstop0 (*)
r	- redo
z	- undo
a	- anchor
s	- move_left
x	- move_right
d	- move_left_chunk
h	- move_right_chunk
	

qq	- quit
qs	- save

qz	- find selection ("next")
qd	- cut  [anchor-cursor sel]			sets selection
qr	- copy [anchor-cursor sel]			sets selection
qh	- cut-find anchor-cursor sel.			sets selection
qx	- cut-execute anchor-cursor sel.		sets selection
qa	- open [anchor-cursor sel]			sets selection














d s h x r

q a z 



qd qs qh qx qr

qa qz qq




------------------------------------------------------------




*/











/* ---------------------- final keybindings: -------------------------------

a	- move_left
d	- move_left_chunk
h	- move_right_chunk
q	- deadstop (*)
r	- cut
s	- move_right


qa	- redo_alternate          
qd	- save
qh	- anchor
qq	- quit
qr	- execute       (sends selection as input, always, pastes output always. undoable.
qs	- undo

	--------			OLD 











x	else if (c == 'Q') quit();
x	else if (c == 'S') save();
del	else if (c == 'O') open();
x	else if (c == 'K') execute();

x	else if (c == 'A') anchor = cursor;
x	else if (c == 'R') cut();
x	else if (c == 'M') copy();
del	else if (c == 'L') paste();
	
x	else if (c == 'N') move_left();
x	else if (c == 'E') move_right();
x	else if (c == 'U') move_left_chunk();
x	else if (c == 'P') move_right_chunk();
	
	else if (c == 'C') undo();
	else if (c == 'K') redo();
	else if (c == 'I') alternate();
*/













/*














editor primitives:
-----------------------



q	- deadstop (*)
r	- cut
a	- move_left
s	- move_right
d	- move_left_chunk
t	- move_right_chunk


qq	- quit
qd	- save
qr	- execute
qh	- anchor
qs	- undo
qa	- redo_alternate









	


very very tempted to make cut() actually do a copy() as well. that would simplify one further keybind, lol. 
	kindaaa seems like the wrong call though... just because its so wasteful.. like if we just want to copy something. 
				idk...... hm...  we have to cut, it, only to paste it again. like seriously? idk.... 

							yeah, i think we should keep them seperate. it just makes more sense that way. often you want to delete stuff, without copying it to your clipboard. i think. 
								hm.. 


					welppppp we have 1 too many keybindings then for only 6 keys loll. not great. 
				hmmm




					literally nothing else can get eliminated though... thats the problem lol. like thats everything that we have. hm. 


		




welll


	wait


			what if we just unconditionally sent the current selection to the program!!!!

					that we executed!!!



								then, like we could literally have MORE FEATURES

								with less keybindssss




			because then we don't need copy()!!!!

					nice!!!! lets do that. cool beans. 




	so pbcopy and pbpaste, 

		are user senthesized commands

		yay 


								that way its platform independent, 



									andddd has little keybinds. 



		yay. 
			i love this lol. 



cool beans 



	yay. 


lets do that.

lets see if it works








	

----------------------extraneous---------------------------



x	- alternate

	- copy







































Q [17]
D [4]
R [18]

A [1]
S [19]
H [8]



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
	else if (c == 'R') {} //  cut();
	else if (c == 'L') paste();
	else if (c == 'K') execute();
*/











/*


  [Q] [D] [R] [W]

   [A] [S] [H] [T]
	
     [Z] [X] [M]              <--- this row is not usable.









  [17] [4] [18] [23]

   [1] [19] [8] [20]

     [-]  [-]  [-]



1   (start of heading)````````````````
4   (end of transmission)```````````````
8   (backspace)	       ```````````````````
17  (device control 1) ``````````````````
18  (device control 2)``````````````````
19  (device control 3)`````````````````
20  (device control 4)`````````````````
23  (end of trans. block)`````````````````









????	
????	

a	moveleft
s	moveright
d	moveleftw
r	moverightw

q	*
d	anchor
r	cut
w	history-mode

q q	quit
q d	save
q r	execute
q w	open

q a	-
q s	copy
q h	paste
q t 	[UNDEFINED]

history mode:
----------------
a	undo
s	redo
d	alternate
q	*





  [ * / quit] [leftw/save]  

    [left/] [right/] [rightw/] 



  [ * / quit] [lw]

   [l] [r] [rw] 





  [17] [4] [18]

   [1] [19] [8]












  [17] [4] [18] [23]

   [1] [19] [8] [20]











2	else if (c == 'Q') quit();
2	else if (c == 'A') anchor();
2	else if (c == 'S') save();
3	else if (c == 'O') open();
1	else if (c == 'N') move_left();				
1	else if (c == 'E') move_right();
1	else if (c == 'U') move_left_chunk();
1	else if (c == 'P') move_right_chunk();
m1	else if (c == 'C') undo();
m1	else if (c == 'K') redo();
m2	else if (c == 'I') alternate();
2	else if (c == 'M') copy();
2	else if (c == 'R') cut();
2	else if (c == 'L') paste();
3	else if (c == 'K') execute();






*/



























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


//if (b) { memcpy(screen + length, "\033[0;41m", 7); length += 7; } 
//if (i == cursor) { if (b) { memcpy(screen + length, "\033[0m", 4); length += 4; } }  








/*

// okay, so i kinda want to try to use this editor to write a piece of code. 
// it doesnt have to be anything fancy, but yeah, i need to test writing code with this thing. 
// lets see how it will go! yay. [202306305.184617]

#include <stdio.h>
#include <stdlib.h>

int main() {
	char buffer[512] = {0};

	fgets(buffer, sizeof buffer, stdin);

	int a = atoi(buffer);
	
	fgets(buffer, sizeof buffer, stdin);
	
	int b = atoi(buffer);

	printf("%d + %d = %d\n", a, b, a + b);

	exit(0);
}




*/


