#include <stdio.h>    // a character-based modal programmable ed-like command-line text editor for my own use. 
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
//#include <signal.h>
struct action {
	size_t* children, parent, choice, count, ilength, dlength, pre, post;
	char* deleted, * inserted;
	
};
//static void handler(int __attribute__((unused))_) {}
int main(int argc, const char** argv) {
	//struct sigaction action = {.sa_handler = handler}; 
	//sigaction(SIGINT, &action, NULL);
	struct termios terminal; 
	tcgetattr(0, &terminal);
	terminal.c_lflag &= ~((size_t) ICANON | ECHO);
	tcsetattr(0, TCSAFLUSH, &terminal);
	struct winsize window;
	uint8_t* tabs = NULL;
	uint16_t* newlines = NULL, column = 0;
	char filename[2048] = {0}, * text = NULL, * input = NULL, w = 0, p = 0, d = 0;
	const size_t cursor_count = 5;
	size_t cursor[cursor_count] = {0}, capacity = 0, tab_count = 0, 
	newline_count = 0, action_count = 1, head = 0, insert = 1,
	count = 0, saved = 1, length = 0, number = 128;	
	struct action* actions = calloc(1, sizeof(struct action));
	if (argc < 2) goto loop;
	read_file:; FILE* file = fopen(argv[1], "r");
	if (not file) { perror("fopen"); goto loop; }
	fseek(file, 0, SEEK_END);
        count = (size_t) ftell(file); text = malloc(count);
        fseek(file, 0, SEEK_SET); 
	fread(text, 1, count, file); fclose(file); 
	strlcpy(filename, argv[1], sizeof filename);
	printf("%s r %lu\n", filename, count);
loop: 	ioctl(0, TIOCGWINSZ, &window);
	fflush(stdout); write(1, "\r", 1);
	column = 0; w = 0; p = 0; d = 0; length = 0;
	tab_count = 0; newline_count = 0;
read:	if (read(0, &w, 1) <= 0) goto process;
	if (w == 127) goto delete;
	if (w != 't' or p != 'r' or d != 'd') goto push;
	if (length >= 2) length -= 2;
	goto process;
push:	if (w == 10) {
		newlines = realloc(newlines, sizeof(uint16_t) * (newline_count + 1));
		newlines[newline_count++] = column;
		column = 0; write(1, &w, 1);
	} else if (w == 9) {
		const uint8_t amount = 8 - column % 8;
		column += amount; column %= window.ws_col;
		write(1, "        ", amount);
		tabs = realloc(tabs, tab_count + 1);
		tabs[tab_count++] = amount;
	} else {
		if (column >= window.ws_col) column = 0;
		if((unsigned char) w >> 6 != 2) column++;
		write(1, &w, 1);
	}
	if (length + 1 >= capacity) input = realloc(input, capacity = 4 * (capacity + 1));
	input[length++] = w; goto next;
delete: if (not length) goto next; length--;
	if (input[length] == 10) {
		column = newlines[--newline_count];
		printf("\033[A");
		if (column) printf("\033[%huC", column);
		fflush(stdout);
	} else if (input[length] == 9) {
		uint8_t amount = tabs[--tab_count];
		column -= amount;
		write(1, "\b\b\b\b\b\b\b\b", amount);
	} else {
		while ((unsigned char) input[length] >> 6 == 2) length--;
		if (not column) { column = window.ws_col - 1; write(1, "\b", 1); } 
		else { column--; write(1, "\b \b", 3); }
	}
next:	d = p; p = w; goto read;
process: putchar(10);
	if (not length)	goto loop;
	input[length] = 0;
	char c = *input;
	char* rest = input + 1;
	const size_t len = length - 1;
	if (insert) {insert:
		if (cursor[2] > *cursor) { size_t temp = *cursor; *cursor = cursor[2]; cursor[2] = temp; }
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
	} else if (not strcmp(input, "discard_and_quit")) exit(0);
	else if (c == '\n') {}
	else if (c == '\t') printf("\033[2J\033[H");
	else if (c == 'q') { if (saved) exit(0); else puts("q:modified"); }  
	else if (c == 'a') { puts("unimplemented a"); goto loop; }
	else if (c == 'h') { puts("unimplemented h"); goto loop; }
	else if (c == 'p') { puts("unimplemented p"); goto loop; }
	else if (c == 't') { cursor[2] = *cursor; insert = 1; goto loop; }
	else if (c == 'r') {
		if (not len) { insert = 1; goto loop; }
		const uint8_t di = (uint8_t) (input[1] - '0'), si = (uint8_t) (input[2] - '0');
		if (len == 1 and di < cursor_count) printf("%hhu: %lu\n", di, cursor[di]);
		else if (len == 2 and di < cursor_count and si < cursor_count) cursor[di] = cursor[si]; 
		else puts("unkown transfer");
	} else if (c == 'm') {
		if (len) number = (size_t) (ssize_t) atoi(input + 1);
		*cursor += number;
		if ((ssize_t) *cursor < 0) *cursor = 0; else if (*cursor > count) *cursor = count;
	} else if (c == 'l') {
		if (len) number = (size_t) (ssize_t) atoi(input + 1);
		const size_t n = (ssize_t) number < 0 ? -number : number;
		const size_t total = count - *cursor < n ? count - *cursor : n;
		if (text) fwrite(text + *cursor, total, 1, stdout); putchar(10);
	} else if (c == 'u') {
		if (not len) {puts("unimplemented: n empty"); goto loop; }
		size_t t = 0;
	f:	if (t == len or *cursor >= count) {
			if (t == len) cursor[1] = *cursor - len;
			print_found: printf("%lu: %lu %lu\n", count, *cursor, cursor[1]); goto loop;
		} if (text[*cursor] != rest[t]) t = 0; else t++; ++*cursor; goto f;
	} else if (c == 'n') {
		if (not len) {puts("unimplemented: n empty"); goto loop; }
		size_t t = len;
	b:	if (not t or not *cursor) { if (not t) cursor[1] = *cursor + len; goto print_found; }
		--*cursor; t--; if (text[*cursor] != rest[t]) t = len; goto b;
	} else if (c == 'e' or c == 'o') {
		if (not len) { cursor[2] = cursor[c != 'e']; goto loop; }
		char command[4096] = {0};
		strlcpy(command, rest, sizeof command);
		strlcat(command, " 2>&1", sizeof command); printf("executing: %s\n", command);
		FILE* f = popen(command, "r");
		if (not f) { printf("error: could not run command \"%s\"\n", command); perror("popen"); goto loop; }
		length = 0;
		char line[2048] = {0};
		while (fgets(line, sizeof line, f)) {
			size_t l = strlen(line);
			if (length + l >= capacity) input = realloc(input, capacity = 4 * (capacity + l));
			memcpy(input + length, line, l);
			length += l;
		} pclose(f);
		if (c == 'o') { cursor[2] = *cursor; goto insert; }
		fwrite(input, length, 1, stdout);
	} else if (c == 'd') {
		if (not len) { puts("unimplemented d empty"); goto loop; }
		if (not saved) { puts("r:modified."); goto loop; }
		argv[1] = rest; goto read_file;
	} else if (c == 's') {
		if (*filename and not len) goto save;
		if (not len) { puts("s:no filename given"); goto loop; }
		else if (not access(rest, F_OK)) { puts("s:file exists"); goto loop; }
		else strlcpy(filename, rest, sizeof filename);
	save:;	FILE* output_file = fopen(filename, "w");
		if (not output_file) { perror("fopen"); goto loop; }
		fwrite(text, count, 1, output_file); fclose(output_file); 
		printf("%s %s %lu\n", filename, saved ? "s" : "w", count); saved = 1;
	} else if (c == 'i') {
		if (not len) {if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0;}
		else puts("unimplemented: i nonempty");
	} else if (c == 'c') {
		if (not head) goto loop;
		const struct action a = actions[head];
		printf("u +%lu -%lu, %lu:%lu\n", a.ilength, a.dlength, a.count, a.choice);
		*cursor = a.post;
		const size_t new = count + a.dlength - a.ilength;
		if (a.dlength > a.ilength) text = realloc(text, new);
		if (a.dlength != a.ilength) memmove(text + *cursor + a.dlength - a.ilength, text + *cursor, count - *cursor);
		memcpy(text + *cursor - a.ilength, a.deleted, a.dlength);
		if (a.dlength < a.ilength) text = realloc(text, new);
		count = new; *cursor = a.pre; saved = 0; head = a.parent;
	} else if (c == 'k') {
		if (not actions[head].count) goto loop;
		head = actions[head].children[actions[head].choice];
		const struct action a = actions[head];
		printf("r +%lu -%lu, %lu:%lu\n", a.ilength, a.dlength, a.count, a.choice);
		*cursor = a.pre;
		const size_t new = count + a.ilength - a.dlength;
		if (a.ilength > a.dlength) text = realloc(text, new);
		if (a.ilength != a.dlength) memmove(text + *cursor + a.ilength - a.dlength, text + *cursor, count - *cursor);
		memcpy(text + *cursor - a.dlength, a.inserted, a.ilength);
		if (a.ilength < a.dlength) text = realloc(text, new);
		count = new; *cursor = a.post; saved = 0;
	} else printf("unintelligible %s\n", input);
	goto loop;
}






















	/*



// else if (c == '0') printf("%lu: %lu %lu %lu %lu\n", count, cursor, anchor1, anchor2, anchor3);






if (not len) {
			if (cursor[1] > *cursor) { size_t temp = *cursor; *cursor = cursor[1]; cursor[1] = temp; }
			tofind = text + cursor[1]; tofind_count = *cursor - cursor[1];
		}


const char* tofind = rest;
		size_t tofind_count = len;





const char* tofind = rest;
		size_t tofind_count = len;
		if (not len) {
			if (cursor[1] > *cursor) { size_t temp = *cursor; *cursor = cursor[1]; cursor[1] = temp; }
			tofind = text + cursor[1]; tofind_count = *cursor - cursor[1];
		} 






	fflush(stdout); fgets(line, sizeof line, stdin);
	if (strcmp(line, "'\n")) {
		size_t l = strlen(line);
		input = realloc(input, length + l);
		memcpy(input + length, line, l);
		length += l; goto loop;
	} else if (length) length--;
	*/

	
	




	
//	if (length == 4 and not strncmp(input, "quit", 4)) goto done;
//	if (length == 5 and not strncmp(input, "clear", 5)) { printf("\033[H\033[2J"); fflush(stdout); } 
//	goto loop;

//done:	














/*

printf("\n\trecieved input(%lu): \n\n\t\t\"", length);
	fwrite(input, length, 1, stdout);
	printf("\"\n");
	fflush(stdout);


*/
































	//printf("received: ###");
	//fwrite(input, 1, length, stdout);
	//printf("###\n");






//printf("input: ");

















/*




// printf("program output \"%s\". [%ld bytes of output]\n", input, length);








			fwrite(input + 1, remaining, 1, stdout); 
			








static inline void prompt_run(void) {

	printf("run(2>&1): ");

	char* command = input + 1;

	// null terminate it.

	printf("running: %s\n", command);

	FILE* f = popen(command, "r");
	if (not f) {
		printf("error: could not run command \"%s\"\n", command);
		perror("popen");
		goto loop;
	}

	char line[4096] = {0};
	size_t output_length = 0;
	char* output = NULL;

	while (fgets(line, sizeof line, f)) {
		size_t l = strlen(line);
		output = realloc(output, output_length + l); 
		memcpy(output + output_length, line, l);
		output_length += l;
	}
	pclose(f);

	output = realloc(output, output_length + 1); 
	output[output_length] = 0;

	printf("output %ldb\n", output_length);



	anchor2 = cursor;

}








*/




















/*

{ const size_t total = cursor < number ? cursor : number;
			fwrite(text + cursor - total, total, 1, stdout); }
			printf("▉");

*/



/*

// if (not length) goto loop;
		
		// if (not length) goto loop;











	some changes i want to make to the editors language:


		- make the  print-backwards and print-forwards  functions the exact same one-    it will print    10(eg)  characters before the cursor, 
													then print a boxcharacter, or something, idk. 

												then print (up to) 10(eg) characters after the cursor. 




		- make the editor fully turing complete and programmable, using our programming language, or something simpler, 


		- make the editor only use the mode 1 machinery for insert and command mode. and make all commands take an argument. 


*/
























/*



if (mode == 3) {
			mode = 2;
			length = ilength;
			if (length >= sizeof input) length = sizeof input - 1;
			memcpy(input, insert, length);
			ilength = 0;
			input[length] = 0;
			printf("m: about to execute(%lu): \"%s\"\n", length, input);
			goto execute;
		}










		} else if (c == 'a') {
			//size_t save = cursor;
			//if (cursor > max) cursor -= max; else cursor = 0;
			//if (count) fwrite(text + cursor, save - cursor, 1, stdout); 
			//cursor = save;

		} else if (c == 0) {
			//size_t save = cursor;
			//if (cursor > max) cursor -= max; else cursor = 0;
			//if (count) fwrite(text + cursor, save - cursor, 1, stdout); 

		} else if (c == 0) {
			//size_t total = count - cursor < max ? count - cursor : max;
			//if (count) fwrite(text + cursor, total, 1, stdout); 
			//cursor += total;
		}

*/















/*

	features to implement:

	x	- add the         insert mode textbox functionality 	to the editor, so we can give commands with newlines.... literally. 


		- add the      shell command functionality to the text editor. 



	x	- rename max to be   number 
	x	- allow     max   to be negative.




	x	- simplify    o   p    a    and   d       to just be           d         for print,       and based on what     number is pos or neg,
									we will print either forwards or backwards. cool!

									then, add a command which always adds (up to)    number     to the cursor, to move it. 
									
													cursor += number;          ie. 









			- also make these changes with the editor itself! lol. (optional)















//length = strlen(input);
			//printf("m: about to execute(%lu): \"%s\"\n", length, input);


*/
































/*


static inline void run_shell_command(const char* command) {

	
}

static inline void prompt_run(void) {

	char command[4096] = {0};

	//prompt("run(2>&1): ", command, sizeof command);

	if (not strlen(command)) { sprintf(message, "aborted run"); return; }

	printf("running: %s\n", command);

	FILE* f = popen(command, "r");
	if (not f) {
		printf("error: could not run command \"%s\": %s\n", command, strerror(errno));
		goto loop;
	}

	char line[4096] = {0};
	nat length = 0;
	char* output = NULL;

	while (fgets(line, sizeof line, f)) {
		const nat line_length = (nat) strlen(line);
		output = realloc(output, (size_t) length + 1 + (size_t) line_length); 
		sprintf(output + length, "%s", line);
		length += line_length;
	}

	pclose(f);

	anchor2 = cursor;

	printf("output %ldb\n", length);




	///   insert_string(output, length);



}
*/
























































//printf("appending... (%lu)\n", insert_length);









//	printf("\n\trecieved input(%lu): \n\n\t\t\"", input_length);
//	fwrite(input, input_length, 1, stdout); 
//	printf("\"\n\n\n\n");





/*


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













/*






	//printf("given: cursor %lu anchor2 %lu\n", cursor, anchor2);
		//printf("actually using: cursor %lu anchor2 %lu\n", cursor, anchor2);








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

	if (w != 10 or p != 'r' or d != 't') goto resize;

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


















		const size_t deleted_length = cursor - anchor2;
		const size_t diff = input_length - deleted_length;
		char* deleted = strndup(text + anchor2, deleted_length);

	
		if (input_length > deleted_length)    text = realloc(text, count + diff);

		if (input_length != deleted_length) 
			memmove(text + cursor + diff, text + cursor, count - cursor);

		memcpy(text + anchor2, input, input_length);

		if (input_length < deleted_length)    text = realloc(text, count + diff);

		count  += diff;
		cursor += diff;

*/







	// how do we know if we get a newline!?!?
	//read(0, &w, 1);

//	if (w != 10) {
//		printf("FATAL ERROR: error, expected a newline after the drt sequence, got: %c ....\n", w);
//		goto process;
//	}




/*



		if (input_length > deleted_length) 
			text = realloc(text, count + input_length - deleted_length);

		if (input_length != deleted_length) 
			memmove(text + cursor + input_length, text + anchor2, count - anchor2);

		memcpy(text + cursor, input, input_length);

		if (input_length < deleted_length) 
			text = realloc(text, count + input_length - deleted_length);

		count  += input_length - deleted_length;
		cursor += input_length;






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


















/*

	char* deleted = malloc(dlength);
		memcpy(deleted, text + cursor - dlength, dlength);
		memmove(text + cursor + ilength - dlength, text + cursor, count - cursor);
		memcpy(text + cursor, inserted, ilength);
		count  += ilength - dlength;  
		cursor += ilength - dlength;






if (mode != 2 or i >= input_length) goto halt;
		const char c = input[at++]; 
		const size_t remaining = input_length - i;
*/



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



// printf("inserted \"%.*s\", deleted \"%.*s\".\n", (int) insert_length, insert, (int) deleted_length, deleted);

