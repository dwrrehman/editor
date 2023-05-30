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
	char* text;
	size_t parent, type, choice, count, length, pre_cursor, post_cursor;
};

static char* text = NULL;
static struct action* actions = NULL;
static size_t action_count = 0, head = 0, count = 0, cursor = 0;
static char message[4096] = {0};
static struct termios terminal = {0};

static inline void create_action(struct action new) {
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(size_t) * (size_t) (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	head = action_count;
	actions[action_count++] = new;
}

static void insert_char(char c) {
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor++] = c;
	count++;
}

static void insert(char* string, size_t length) {
	struct action new_action = {0};
	new_action.pre_cursor = cursor;
	for (size_t i = 0; i < length; i++) insert_char(string[i]);
	new_action.post_cursor = cursor;
	new_action.type = 0;
	new_action.text = strndup(string, length);
	new_action.length = length;
	create_action(new_action);
}

static char delete_char(void) {
	if (not cursor) return 0;
	count--;
	const char c = text[--cursor];
	memmove(text + cursor, text + cursor + 1, count - cursor);
	return c;
}

static void delete(size_t total) {
	if (not cursor) return;
	struct action new_action = {0};
	new_action.pre_cursor = cursor;
	size_t deleted_count = 0;
	char* deleted_string = malloc(total);
	for (size_t i = 0; i < total; i++) {
		deleted_string[deleted_count++] = delete_char();
		if (deleted_string[deleted_count - 1]) { deleted_count--; break; }
	}
	new_action.post_cursor = cursor;
	new_action.type = 1;
	new_action.text = deleted_string;
	new_action.length = deleted_count;
	create_action(new_action);
}

static void undo(void) {
	if (not head) return;
	snprintf(message, sizeof message, "undoing %lu %lu %lu %lu %lu...", actions[head].type, actions[head].count, actions[head].parent, actions[head].choice, actions[head].length);
	const struct action a = actions[head];
	cursor = a.post_cursor;
	if (a.type == 0) {}
	else if (a.type == 0) { for (size_t i = 0; i < a.length; i++) delete_char(); }
	else if (a.type == 1) { for (size_t i = 0; i < a.length; i++) insert_char(a.text[i]); }
	cursor = a.pre_cursor;
	head = actions[head].parent;
}

static void redo(void) {
	if (not actions[head].count) return;
	snprintf(message, sizeof message, "redoing %lu %lu %lu %lu %lu...", actions[head].type, actions[head].count, actions[head].parent, actions[head].choice, actions[head].length);
	head = actions[head].children[actions[head].choice];
	const struct action a = actions[head];
	cursor = a.pre_cursor;
	     if (a.type == 1) { for (size_t i = 0; i < a.length; i++) delete_char(); } 
	else if (a.type == 0) { for (size_t i = 0; i < a.length; i++) insert_char(a.text[i]); } 
	cursor = a.post_cursor;
}

static void alternate(void) {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++; else actions[head].choice = 0;
	snprintf(message, sizeof message, "switched %ld %ld", actions[head].choice, actions[head].count);
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
	action_count = 1;
	actions = calloc(1, sizeof(struct action));
	head = 0;
	char string[4096] = {0};
	size_t length = 0;

	while (1) {
		clear_screen();
		display_string(string, length);
		char c = read_from_stdin();
		if (c == 27) break;
		else if (c == '0') memset(message, 0, sizeof message);
		else if (c == '1') undo();
		else if (c == '2') alternate();
		else if (c == '3') redo();
		else if (c == 10) { insert(string, length); length = 0; }
		else if (c == '\\') { delete(10); }
		else if (c == '[') { if (cursor) cursor--; }
		else if (c == ']') { if (cursor < count) cursor++; }
		else if (c == 127) length--;
		else string[length++] = c;
	}
	restore_terminal();
}



















































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




