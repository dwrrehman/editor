#include <stdio.h>    // a character-based modal programmable ed-like command-line text editor for my own use. 
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>





/*

		struct action new = {0};
		new.pre = cursor;

		char* deleted = malloc(dlength);
		memcpy(deleted, text + cursor - dlength, dlength);
		memmove(text + cursor + ilength - dlength, text + cursor, count - cursor);
		memcpy(text + cursor, inserted, ilength);
		count  += ilength - dlength;  
		cursor += ilength - dlength;

		new.post = cursor;
		new.inserted = strndup(inserted, ilength);
		new.ilength = ilength;
		new.deleted = deleted;
		new.dlength = dlength;
		new.parent = head;
		actions[head].children = realloc(actions[head].children, sizeof(size_t) * (size_t) (actions[head].count + 1));
		actions[head].choice = actions[head].count;
		actions[head].children[actions[head].count++] = action_count;
		actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
		head = action_count;
		actions[action_count++] = new;
*/





struct action { 
	size_t* children; char* text;
	size_t parent, type, choice, count, length, pre, post;
};

static struct action* actions = 0;
static size_t action_count = 0, head = 0;

static inline void create_action(struct action new) {      // inline this. 
	
}

static void handler(int __attribute__((unused))_) {}

int main(int argc, const char** argv) {

	char filename[4096] = {0};
	char* text = NULL, * input = NULL;
	char p = 0, d = 0;

	size_t capacity = 0, count = 0, cursor = 0, anchor1 = 0, anchor2 = 0, 
		saved = 1, mode = 1, max = 128;

	struct sigaction action = {.sa_handler = handler}; 
	sigaction(SIGINT, &action, NULL);

	if (argc < 2) goto loop;

	read_file:; FILE* file = fopen(argv[1], "r");
	if (not file) { perror("fopen"); goto done; }

	fseek(file, 0, SEEK_END);
        count = (size_t) ftell(file); 
	text = malloc(count);
        fseek(file, 0, SEEK_SET); 
	fread(text, 1, count, file);
	fclose(file); 
	strlcpy(filename, argv[1], sizeof filename);

	cursor = 0; 
	mode = 2;

show:	printf("%lu\n", count); 
	saved = 1;
loop: 	
	input_length = 0;
read_char: 
	char c = 0;
	if (read(0, &c, 1) <= 0) goto process;
	if (c != 't' or p != 'r' or d != 'd') goto resize; 
	input_length -= 2;
	goto process;
resize: 
	if (input_length + 1 <= capacity) goto push;
	buffer = realloc(input, ++capacity);
push: 	
	input[input_length++] = c; 
	d = p; p = c; 
	goto read_char; 
process: 
	if (mode == 1) {
		
		if (cursor < anchor2) { size_t temp = cursor; cursor = anchor2; anchor2 = temp; }

		struct action new = {0};
		new.pre = cursor;

		dlength = anchor3


		text = realloc(text, count + input_length);
		memcpy(deleted, text + cursor - dlength, dlength);
		memmove(text + cursor + input_length, text + cursor, count - cursor);
		memcpy(text + cursor, input, input_length);
		count += input_length; 
		cursor += input_length; 
		saved = 0; 
		mode = 2;



		char* deleted = malloc(dlength);
		memcpy(deleted, text + cursor - dlength, dlength);
		memmove(text + cursor + ilength - dlength, text + cursor, count - cursor);
		memcpy(text + cursor, inserted, ilength);
		count  += ilength - dlength;  
		cursor += ilength - dlength;











		memmove(text + anchor2, text + cursor, count - cursor);
		count -= cursor - anchor2; 
		cursor = anchor2; 
		saved = 0;






		new.post = cursor;
		new.inserted = strndup(inserted, ilength);
		new.ilength = ilength;
		new.deleted = deleted;
		new.dlength = dlength;
		new.parent = head;

		actions[head].children = realloc(actions[head].children, sizeof(size_t) * (size_t) (actions[head].count + 1));
		actions[head].choice = actions[head].count;
		actions[head].children[actions[head].count++] = action_count;
		actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
		head = action_count;
		actions[action_count++] = new;




	} else if (mode == 2) {

		size_t at = 0;
	execute: 
		if (mode != 2 or i >= input_length) goto halt;
		const char c = input[at++]; 
		const size_t remaining = input_length - i;

		if (input_length == 16 and not strcmp(input, "discard_and_quit")) mode = 0;

		else if (c == 10) {}
		else if (c == 'o') printf("\033[2J\033[H");

		else if (c == 'q') { if (saved) mode = 0; else puts("modified"); }  // HALTS

		else if (c == 't') { anchor2 = cursor; cut_insert: mode = 1; read(0, &c, 1); goto halt; }  // HALTS
		else if (c == 'r') goto cut_insert;  // HALTS
		else if (c == 'D') printf("%lu %lu %lu %lu\n", count, cursor, anchor1, anchor2); // temp
		else if (c == 'i') { max = (size_t) atoi(input + 1); goto halt; }  // HALTS
		else if (c == 'p') fwrite(text + cursor, count - cursor < max ? count - cursor : max, 1, stdout); 
		else if (c == '1') anchor2 = cursor;
		else if (c == 'g') anchor2 = anchor1;
		else if (c == 'b') anchor1 = cursor;
		else if (c == '4') anchor1 = anchor2;
		else if (c == '5') cursor = anchor1;
		else if (c == '6') cursor = anchor2;
		else if (c == 'b') fwrite(text + cursor - 10, 10, 1, stdout);  // temp

		} else if (c == 'u') { // HALTS
			const char* tofind = input + at; size_t tofind_count = remaining;
			if (not tofind_count) { tofind = text + anchor1; tofind_count = cursor - anchor1; }
			size_t i = cursor, t = 0;
		f:	if (t == tofind_count) { 
				cursor = i; 
				anchor1 = cursor - tofind_count; 
				goto halt; 
			}
			if (i >= count) { 
				puts("absent"); 
				cursor = i; 
				goto halt; 
			}
			if (text[i] == tofind[t]) t++; else t = 0; 
			i++; 
			goto f;

		} else if (c == 'n') { // HALTS
			const char* tofind = input + at; size_t tofind_count = remaining;
			if (not tofind_count) { tofind = text + anchor1; tofind_count = cursor - anchor1; }
			size_t i = cursor, t = tofind_count;
		b:	if (not t) { 
				anchor1 = cursor + tofind_count; 
				cursor = i; 
				goto halt; 
			}
			if (not i) { 
				puts("absent"); 
				cursor = i; 
				goto halt; 
			}
			i--; t--;
			if (text[i] != tofind[t]) t = tofind_count; 
			goto b;

		} else if (c == 'e') { // HALTS
			if (not saved) { puts("modified"); goto halt; }
			input[input_length] = 0;
			argv[1] = input + at; 
			goto read_file;

		} else if (c == 's') { // HALTS

			input[input_length] = 0;

			if (*filename) goto save;
			if (not input_length) { puts("no filename given"); goto halt; }
			else if (not access(fname, F_OK)) { puts("file exists"); goto halt; }
			else strlcpy(filename, input + at, sizeof filename);

		save:;	FILE* output_file = fopen(filename, "w");
			if (not output_file) { perror("fopen"); goto halt; }
			fwrite(text, count, 1, output_file);
			fclose(output_file); 
			printf("%lu\n", count); 
			saved = 1; 
			goto halt;

		} else { printf("unintelligible %c\n", c); goto halt; }
		goto execute;
	}
	halt: if (mode) goto loop; done:;
}






///           drt     is the escape sequence.  most ergonomic. yay. 


///   a d r t     n u p i 

//  a : anchor?...
//  d : 			-->	[intentionally unbound]
//  r : remove
//  t : insert

//  n : backwards search
//  u : forwards search
//  p : print text
//  i : set print width



/*
e.c:82:29: runtime error: addition of unsigned offset to 0x0001039003f2 overflowed to 0x0001039003ed
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior e.c:82:29 in 
*/









/*



loop:;	ssize_t r = getline(&input, &capacity, stdin);
	if (r <= 0) { mode = 0; goto save; }
	size_t input_length = (size_t) r;








we should be using popen to do copy paste!! 

and we should make our own getline() function with a custom delimiter system, 

make us not insert lines right when they are entered, in insert mode, 


and make commands still delimited by newlines, though. 

delete ca1 ca2, i think.

delete the puts("")   after all print commands. 

make n print based on an argument, which is the number of characers to print!

ie,   n13  prints 13 characters. yay.

but,   just        n          prints the previous amount of characters.        which is set by the call to print. i think. 

but, then, we should inch along the cursor by (at most)        n characters, i think. 

ie,   a   page utility. 

also we should make the delimiter       hc   and then we should make the   OHH ALSO we should NOT buffer on newlines, 

	we should delimit by       hc       even in command mode, itihnk1!?!?
	but then we still need to send a newline, i tihnk?... 
		or maybe we use eithero f them, actually. 

		yeah, i like that. 

		hm.... idk.. interesting... 
	hmm

	yeah, idon't really like newline, i think... becuaes newlines are too useful, actually. 

	so i tihnk i want to have newlines    not be special at all. 
	and justuse them for submitting the text, when you are ready to send it to the process. 

		i think 





*/

































// rename to find_forwards    // make this use    anchor1-cursor, always,      if argument is empty.     YAY!


// rename to find_backwards            same semantics as forwards, exactly. 








		// also, as seperate commmands, allow the user to  do       drop_anchor1   drop_anchor2      
		//  as well as   back_anchor1 and back_anchor2, i think...? maybe. we'll see. looks like replace kinda does a back, kindaaaa. idk.








/*
	ripped out:



//static char* find(char* text, char* tofind, size_t count, size_t length, size_t cursor) {      // inline this.
	inlined
//}

//static char* find_reverse(char* text, char* tofind, size_t length, size_t cursor) {        // inline this.
	inlined
//}






		else if (not strcmp(input, "back")) cursor = anchor;						 /// DELETE ME
		else if (not strcmp(input, "top")) cursor = 0;							 /// DELETE ME
		else if (not strcmp(input, "bottom")) cursor = count - 1;					 /// DELETE ME
		else if (not strcmp(input, "right")) cursor++;							 /// DELETE ME
		else if (not strcmp(input, "left")) cursor--;							 /// DELETE ME
		else if (not strcmp(input, "filename")) puts(filename);						 /// DELETE ME
		else if (input[0] == '.') max = (size_t) atoi(input + 1);					 /// DELETE ME
		else if (not strcmp(input, "count")) printf("%lu %lu %lu\n", count, anchor, cursor);			 /// DELETE ME
		else if (not strcmp(input, "print_contents")) fwrite(text, count, 1, stdout);				 /// DELETE ME
		else if (not strcmp(input, "print_after")) fwrite(text + cursor, count - cursor, 1, stdout); 		 /// DELETE ME




		else if (not strcmp(input, "print_anchor")) {  								/// DELETE ME
			fwrite(text + anchor, count - anchor < max ? count - anchor : max, 1, stdout); puts("");
		}




		printf("%lu\n", cursor);					 /// DELETE ME






		else if (not strcmp(input, "up")) { 								 /// DELETE ME
			size_t save = cursor;
			if (cursor > max) cursor -= max; else cursor = 0;
			fwrite(text + cursor, save - cursor, 1, stdout); 
			puts("");
		}
		else if (not strcmp(input, "down")) {  								 /// DELETE ME
			size_t total = count - cursor < max ? count - cursor : max;
			fwrite(text + cursor, total, 1, stdout); 
			puts("");
			cursor += total;
		}







*/









/*
    146 lines of code, without the undo tree implemented. 

    i feel like we can get this down to 100 if we just start removing some features..... i mean...

     what do we REALLYYYYYY need,       like seriously...









so i think if we get rid of all the features with the DELETE ME listed on them, 

	then we actually get the editor down to only    120   lines of code. 

			which is starting to become acceptable. 


				now, note, we still have yet to inline the 


						find_reverse/find functions         and create_action functions,        all those will be inlined.




	
					so thats like another 10 lines that we can assume wont be there, if they are implemented.  i think. 



			so that makes it downt to only 110 now 

								which is ALMOSTTTT there.
									i want it to be sub 100 lines.       thats my goal, now, because i know its possible. 


												yay 





			okay, lets delete them then! 

					yay 

					
						






*/




































































/*






	if (zero_width(c)) {
				actions[head].text = realloc(actions[head].text, (size_t) actions[head].length + 1);
				actions[head].text[actions[head].length++] = c;
				record_logical_state(&(actions[head].post));
			}

			new_action.post->saved = saved;
			new_action.post->cursor = cursor;
			new_action.type = insert_action;
			new_action.text = malloc(1);
			new_action.text[0] = c;
			new_action.length = 1;
			create_action(new_action);
















	todo:

		we need to add a suplementary way of submitting a command, where we type the text in the document, and "cut_execute", which deletes it from the document (appending a new action node to the undo tree of course)  and then saves the text it cut to then immediately treat it like a command string that was passed from getline().  it executes it as a command. 

		this is useful for like, including commands with newlines, or ones that are more complex to type out, that you would want better whitespace formatting, and stuff like that. generally whenever you want to make a large command, use this feature. yay!!





	2305291.163419

		TODO:
			- currently we are in the process of simplifying the undo-tree system, to make it on par in simplicity with the rest of the editor. because it kinda sticks out like a sore thumb right now. 

	
			- we still have to implement copy/paste'ing to the global clipboard. thats important. 


			- 



*/





































/*












int delimiter = '`';




ssize_t r = getdelim(&input, &capacity, mode == 1 ? delimiter : 10, stdin);
	if (r <= 0) goto save_exit;
	size_t length = (size_t) r;
	input[--length] = 0;






		else if (not strcmp(input, "clear")) printf("\033[2J\033[H");
		else if (not strcmp(input, "delimiter")) delimiter = getchar();
		else if (not strcmp(input, "print_contents")) { fwrite(text, count, 1, stdout); puts(""); }
		else if (not strcmp(input, "print")) { fwrite(text + cursor, count - cursor, 1, stdout); puts(""); }
		else if (not strcmp(input, "print_cursor")) printf("%lu\n", cursor);
		else if (not strcmp(input, "count")) printf("%lu\n", count);

		else if (input[0] == '/') {
			const char* offset = strstr(text, input + 1);
			if (not offset) printf("string %s not found.\n", input + 1);
			else {
				cursor = (size_t) (offset - text);
				printf("%lu\n", cursor);
			}

		} 








char filename[4096] = {0};
	struct sigaction action = {.sa_handler = handler};
	sigaction(SIGINT, &action, NULL);
	if (argc < 2) goto loop;
	FILE* file = fopen(argv[1], "r");
	if (not file) { perror("fopen"); goto done; }
	fseek(file, 0, SEEK_END);
        count = (size_t) ftell(file);
	text = malloc(count);
        fseek(file, 0, SEEK_SET);
        fread(text, 1, count, file);
	fclose(file); mode = 2; delimiter = 10;
	strlcpy(filename, argv[1], sizeof filename);
	printf("%s: %lu", filename, count);














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



int main(int argc, const char** argv) {
	int delimiter = '`';
	char* text = NULL;
	char input[4096] = {0};
	size_t capacity = 0, count = 0, cursor = 0, saved = 1;
	
loop:;	ssize_t r = getdelim(&input, &capacity, mode == 1 ? delimiter : 10, stdin);
	if (r <= 0) goto save_exit;
	size_t length = (size_t) r;
	input[--length] = 0;

	if (mode == 1) {
		getchar();
		text = realloc(text, count + length);
		memmove(text + cursor + length, text + cursor, count - cursor);
		for (size_t i = 0; i < length; i++) text[cursor + i] = input[i];
		count += length; cursor += length; mode = 2; saved = 0;

	} else if (mode == 2) {

		if (not strcmp(input, "")) puts("[empty input]");
		else if (not strcmp(input, "quit")) if (saved) mode = 0; else puts("unsaved");
		else if (not strcmp(input, "t")) mode = 1;
		else printf("unintelligible %s\n", input);
	}

	if (mode) goto loop; 
save_exit:
	;
done: 	printf("exiting...\n");
}






*/



//printf("debug: received command: #");
//fwrite(input, length, 1, stdout);
//puts("#");


// #include <unistd.h>
