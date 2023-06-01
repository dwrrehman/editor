#include <stdio.h>    // a character-based modal programmable ed-like command-line text editor for my own use. 
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <unistd.h>
#include <signal.h>
struct action { 
	size_t* children; char* text;
	size_t parent, type, choice, count, length, pre, post;
};
static struct action* actions = 0;
static size_t action_count = 0, head = 0;
static inline void create_action(struct action new) {      // inline this. 
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(size_t) * (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	actions = realloc(actions, sizeof(struct action) * (action_count + 1));
	head = action_count; actions[action_count++] = new;
}
static void handler(int __attribute__((unused))_) {}
int main(int argc, const char** argv) {
	char* text = NULL, * input = NULL;
	size_t capacity = 0, count = 0, cursor = 0, anchor1 = 0, anchor2 = 0, saved = 1, mode = 1, max = 128;
	char filename[4096] = {0};
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
	fclose(file); mode = 2; 
	strlcpy(filename, argv[1], sizeof filename);
	printf("%lu\n", count); cursor = 0;
loop:;	ssize_t r = getline(&input, &capacity, stdin);
	if (r <= 0) { mode = 0; goto save; }
	size_t input_length = (size_t) r;
	if (mode == 1) {
		if (input_length >= 2 and input[input_length - 2] == '`') { input_length -= 2; mode = 2; }
		text = realloc(text, count + input_length);
		memmove(text + cursor + input_length, text + cursor, count - cursor);
		memcpy(text + cursor, input, input_length);
		count += input_length; cursor += input_length; saved = 0;
	} else if (mode == 2) {
		input[--input_length] = 0;
		if (not strcmp(input, "")) {}
		else if (*input == 'o') printf("\033[2J\033[H");
		else if (*input == 'q') { if (saved) mode = 0; else puts("modified"); }
		else if (not strcmp(input, "discard_and_quit")) mode = 0;
		else if (*input == 't') mode = 1;
		else if (not strcmp(input, "a2c")) anchor2 = cursor;
		else if (not strcmp(input, "a2a1")) anchor2 = anchor1;
		else if (not strcmp(input, "a1c")) anchor1 = cursor;
		else if (not strcmp(input, "a1a2")) anchor1 = anchor2;
		else if (not strcmp(input, "ca1")) cursor = anchor1;
		else if (not strcmp(input, "ca2")) cursor = anchor2;
		else if (not strcmp(input, "debug")) printf("%lu %lu %lu %lu\n", count, cursor, anchor1, anchor2);
		else if (*input == '.') max = (size_t) atoi(input + 1);
		else if (*input == 'n') { fwrite(text + cursor, count - cursor < max ? count - cursor : max, 1, stdout); puts(""); }
		else if (not strcmp(input, "print_back")) { fwrite(text + cursor, count - cursor < max ? count - cursor : max, 1, stdout); puts(""); }
		else if (not strcmp(input, "r")) {
			if (cursor < anchor2) { size_t temp = cursor; cursor = anchor2; anchor2 = temp; }
			memmove(text + anchor2, text + cursor, count - cursor);
			count -= cursor - anchor2; cursor = anchor2;
		} else if (*input == 'u') {
			const char* tofind = input + 1; size_t length = input_length - 1;
			if (input_length < 2) { tofind = text + anchor1; length = cursor - anchor1; }
			size_t i = cursor, t = 0;
		f:	if (t == length) { cursor = i; anchor1 = cursor - length; goto loop; }
			if (i >= count) { puts("absent"); cursor = i; goto loop; }
			if (text[i] == tofind[t]) t++; else t = 0; i++; goto f;
		} else if (*input == 'e') {
			const char* tofind = input + 1; size_t length = input_length - 1;
			if (input_length < 2) { tofind = text + anchor1; length = cursor - anchor1; }
			size_t i = cursor, t = length;
		b:	if (not t) { cursor = i; anchor1 = cursor + length; goto loop; }
			if (not i) { puts("absent"); cursor = i; goto loop; }
			i--; t--; if (text[i] != tofind[t]) t = length; goto b;
		} else if (*input == 'h') {
			if (not saved) { puts("modified"); goto loop; }
			argv[1] = input + 1; goto read_file;
		} else if (*input == 's') {
			if (*filename) goto save;
			if (access(input + 1, F_OK) != -1 or input_length < 1 or not strlen(input + 1)) { puts("file exists"); goto loop; }
			else strlcpy(filename, input + 1, sizeof filename);
		save:;	FILE* output_file = fopen(filename, "w");
			if (not output_file) { perror("fopen"); goto loop; }
			fwrite(text, count, 1, output_file);
			fclose(output_file); saved = 1;
			printf("%lu\n", count);
		} else printf("unintelligible %.*s\n", (int) input_length, input);
	} if (mode) goto loop; done:;
}

/*
e.c:82:29: runtime error: addition of unsigned offset to 0x0001039003f2 overflowed to 0x0001039003ed
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior e.c:82:29 in 
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
