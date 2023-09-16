// cleanup and revision and simplififcation to the undo tree system, using a simple string/textbox as the file.
// experimenting with implementing an undo system.
// undoing and redoing action, insert and delete, on a simple textbox.

#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <iso646.h>
#include <stdint.h>
#include <stdbool.h>

struct action {
	size_t* children;
	char* deleted, * inserted;
	size_t parent, choice, count, ilength, dlength, pre, post;
};
static char* text = NULL;
static struct action* actions = NULL;
static size_t action_count = 0, head = 0, count = 0, cursor = 0;
static char message[4096] = {0};
static struct termios terminal = {0};

static void sub(size_t dlength, char* inserted, size_t ilength) {
	if (cursor < dlength) dlength = cursor;
	if (not (ilength + dlength)) return;
	struct action new = {0};
	new.pre = cursor;
	char* deleted = malloc(dlength);
	memcpy(deleted, text + cursor - dlength, dlength);
	memmove(text + cursor + ilength - dlength, text + cursor, count - cursor);
	memcpy(text + cursor, inserted, ilength);
	count  += ilength - dlength;  cursor += ilength - dlength;
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
}



static void clear_screen(void) { printf("\033[2J\033[H"); }
static inline void restore_terminal(void) { tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal); }
static inline char read_from_stdin(void) { char c = 0; read(0, &c, 1); return c; }

static inline void configure_terminal(void) {
	tcgetattr(STDIN_FILENO, &terminal);
	atexit(restore_terminal);
	struct termios raw = terminal;
	raw.c_lflag &= ~((size_t)ECHO | (size_t)ICANON);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void display_string(char* string, size_t length) {
	printf("text(cursor=%lu,count=%lu):\n\n\t\t\"", cursor, count);
	size_t i = 0;
	for (; i < count; i++) {
		if (i == cursor) printf("\033[32m");
		printf("%c", text[i]);
		if (i == cursor) printf("\033[0m");
	}
	if (i == cursor) printf("\033[32m");
	printf("#");
	if (i == cursor) printf("\033[0m");
	printf("\"\n\n");
	printf("\n\tmessage: %s\n\n", message);
	printf("string: %lu : %.*s\033[K", length, (int) length, string);
	puts("");
	fflush(stdout);
}

int main(void) {
	configure_terminal();
	text = malloc(4096);
	action_count = 1; head = 0;
	actions = calloc(1, sizeof(struct action));
	char string[4096] = {0};
	size_t length = 0;
	loop: clear_screen();
	display_string(string, length);
	char c = read_from_stdin();
	if (c == 27) goto done;

	else if (c == '0') { memset(message, 0, sizeof message); }
	else if (c == '1') { 
		if (not head) goto loop;
		snprintf(message, sizeof message, "undoing %lu %lu %lu...", actions[head].count, actions[head].parent, actions[head].choice);
		const struct action a = actions[head];
		cursor = a.post;
		memmove(text + cursor + a.dlength - a.ilength, text + cursor, count - cursor);
		memcpy(text + cursor, a.deleted, a.dlength);
		count += a.dlength - a.ilength; cursor += a.dlength - a.ilength;
		cursor = a.pre;
		head = actions[head].parent;
	}
	else if (c == '2') { 
		if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0;
		snprintf(message, sizeof message, "switched %ld %ld", actions[head].choice, actions[head].count);
	}
	else if (c == '3') { 
		if (not actions[head].count) goto loop;
		snprintf(message, sizeof message, "redoing %lu %lu %lu...", actions[head].count, actions[head].parent, actions[head].choice);
		head = actions[head].children[actions[head].choice];
		const struct action a = actions[head];
		cursor = a.pre;
		memmove(text + cursor + a.ilength - a.dlength, text + cursor, count - cursor);
		memcpy(text + cursor, a.inserted, a.ilength);
		count += a.ilength - a.dlength; cursor += a.ilength - a.dlength;
		cursor = a.post;
	 }
	else if (c == 10) { sub(0, string, length); length = 0; }
	else if (c == '\\') { sub(10, 0, 0); }
	else if (c == '[') { if (cursor) cursor--; }
	else if (c == ']') { if (cursor < count) cursor++; }
	else if (c == 127) { if (length) length--; }
	else { string[length++] = c; }
	goto loop;
	done: restore_terminal();
}









//   19 + 16  is what we would add to the editor if we added this.       which is really not that bad, i think. yay. 

// 




/*

//// OH MY GOSH

	////     INSERT MODE IS THE REPLACE COMMAND


		/// WE DONT NEED TO ADD 

			OH MY GOD


		we need to have the comand system buffer on `      i think 

		right!?!?!



		wait...   uhh..... crappp is this is a good idea....


				uhhh










				i think we are just about ready to add in the undotree system into the editor. 

					and then i think we will be done...? technically?... because we can copy paste from outside the editor, technically!! lol. so we don't need to implement that feature if we don't want to technicallyyyyyyy lol. very cool benefit of print'ing the text to stdout lol, as a display function. YAY. 


		*/










































//else if (c == 'P') paste("daniel");


// else if (a.type == paste_action) { while (lcc > a.pre.lcc or lcl > a.pre.lcl) --count; } 










/*
static inline void display_undo_tree(struct action* root, int d, struct action* head) {
    if (!root) { printf("%*c(NULL)\n", 2 * d, ' '); return; }
    if (root == head) printf("%*c[HEAD]\n", 2 * d, ' ');
    printf("%*ctype = %s, count = %lu, data = %p\n", 2 * d,' ',
           stringify_action(root->type), root->count, root->text);
    for (size_t i = 0; i < root->count; i++) {
        printf("%*cchild #%lu:\n", 2 * (d + 1), ' ', i);
        display_undo_tree(root->children[i], d + 1, head);
    }
}
*/


/*
	okay, so i think i found a thing in this, which is incorrect, 

	its the alternate things, 

	
			they arent quite good-



			i need to simply increment or decrement the choice, as long as its less than the children count. 



or a.type == cut_action





static void paste(const char* string) {

    	struct action new = {0};
	record_logical_state(&new.pre);

	const size_t length = strlen(string);

	for (size_t i = 0; i < length; i++) 
		text[count++] = string[i];
	
	record_logical_state(&new.post);
	new.type = paste_action;
	new.text = strndup(string, (size_t) length);
	new.length = length;
	create_action(new);
}


or a.type == paste_action



*/




