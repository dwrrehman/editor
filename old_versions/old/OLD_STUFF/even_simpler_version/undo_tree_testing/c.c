




/*
	okay, so i think i found a thing in this, which is incorrect, 

	its the alternate things, 

	
			they arent quite good-



			i need to simply increment or decrement the choice, as long as its less than the children count. 

*/


// experimenting with implementing an undo system.
// undoing and redoing action, insert and delete, on a simple textbox.

#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

enum action_type {
    no_action,
    insert_action,
    delete_action,
    paste_action,
    action_count
};

struct action {
    size_t type;
    size_t choice;
    size_t count;
    struct action** children;
    struct action* parent;
    uint8_t* text;
    size_t length;
};

static char message[4096] = {0};

static struct termios terminal = {0};

static inline void restore_terminal() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
}

static inline void configure_terminal() {
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        abort();
    }
    
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        abort();
    }
}

static inline uint8_t read_byte_from_stdin() {
    uint8_t c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

static inline const char* stringify_action(size_t type) {
    if (type == no_action) return "(none)";
    else if (type == insert_action) return "insert";
    else if (type == delete_action) return "delete";
    else if (type == paste_action) return "paste";
    else abort();
}

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

static inline void insert(char c, char* text, size_t* length, struct action** head) {
    text[(*length)++] = c;
    
    struct action* new = calloc(1, sizeof(struct action));
    new->type = insert_action;
    new->parent = *head;
    new->text = malloc(1);
    new->length = 1;
    new->text[0] = c;
    
    (*head)->children = realloc((*head)->children, sizeof(struct action*) * ((*head)->count + 1));
    (*head)->choice = (*head)->count;
    (*head)->children[(*head)->count++] = new;
    *head = new;
}

static inline void delete(char* text, size_t* length, struct action** head) {
    if (!*length) return;
    char c = text[--*length];
    
    struct action* new = calloc(1, sizeof(struct action));
    new->type = delete_action;
    new->parent = *head;
    new->text = malloc(1);
    new->length = 1;
    new->text[0] = c;

    (*head)->children = realloc((*head)->children, sizeof(struct action*) * ((*head)->count + 1));
    (*head)->choice = (*head)->count;
    (*head)->children[(*head)->count++] = new;
    *head = new;
}

static inline void paste(const char* string, char* text, size_t* length, struct action** head) {
    for (size_t i = 0; i < strlen(string); i++)
        text[(*length)++] = string[i];
    
    struct action* new = calloc(1, sizeof(struct action));
    new->type = paste_action;
    new->parent = *head;
    new->length = strlen(string);
    new->text = malloc(new->length);
    for (size_t i = 0; i < new->length; i++)
        new->text[i] = string[i];

    (*head)->children = realloc((*head)->children, sizeof(struct action*) * ((*head)->count + 1));
    (*head)->choice = (*head)->count;
    (*head)->children[(*head)->count++] = new;
    *head = new;
}

static inline void perform_action(struct action* action,  char* text, size_t* length) {
    if (action->type == no_action) {}
    else if (action->type == delete_action) *length -= action->length;
    else if (action->type == insert_action) {
        for (size_t i = 0; i < action->length; i++)
            text[(*length)++] = action->text[i];
    } else if (action->type == paste_action) {
        for (size_t i = 0; i < action->length; i++)
            text[(*length)++] = action->text[i];
    } else abort();
}

static inline void reverse_action(struct action* action, char* text, size_t* length) {
    if (action->type == no_action) return;
    else if (action->type == delete_action) {
        for (size_t i = 0; i < action->length; i++)
            text[(*length)++] = action->text[i];
    } else if (action->type == insert_action) *length -= action->length;
    else if (action->type == paste_action) *length -= action->length;
    else abort();
}
    
static inline void undo(char* text, size_t* length, struct action** head) {
    if (!(*head)->parent) return;
        
    reverse_action(*head, text, length);
    
    if ((*head)->parent->count == 1) {
        sprintf(message, "undoing %s\n", stringify_action((*head)->type));
    
    } else {
        sprintf(message, "selected #%lu from %lu histories: undoing %s",
                (*head)->parent->choice, (*head)->parent->count,
                stringify_action((*head)->type));
    }
    
    *head = (*head)->parent;
}

static inline void redo(char* text, size_t* length, struct action** head) {
    if (!(*head)->count) return;
    
    *head = (*head)->children[(*head)->choice];
    
    if ((*head)->parent->count == 1) {
        sprintf(message, "redoing %s", stringify_action((*head)->type));
        
    } else {
        sprintf(message, "selected #%lu from %lu histories: redoing %s",
                (*head)->parent->choice, (*head)->parent->count,
                stringify_action((*head)->type));
    }
    
    perform_action(*head, text, length);
}

static inline void alternate_up(char* text, size_t* length, struct action** head) {
    if ((*head)->parent &&
        (*head)->parent->choice + 1 < (*head)->parent->count) {
        undo(text, length, head);
        (*head)->choice++;
        redo(text, length, head);
    }
}

static inline void alternate_down(char* text, size_t* length, struct action** head) {
    if ((*head)->parent &&
        (*head)->parent->choice) {
        undo(text, length, head);
        (*head)->choice--;
        redo(text, length, head);
    }
}

int main() {
    configure_terminal();
    char* text = malloc(4096);
    size_t length = 0;
    struct action* tree = calloc(1, sizeof(struct action));
    struct action* head = tree;
    
    while (1) {
        // display_undo_tree(tree, 0, head);
        printf("\033[H\033[2J");
        printf("text:\n\n\t\t\"%.*s\"\n\n", (int) length, text);
        printf("\n\tmessage: %s\n\n", message);
        printf("action: ");
        fflush(stdout);
        uint8_t c = read_byte_from_stdin();
        
        if (c == 27) break;
        else if (c == '0') memset(message, 0, sizeof message);
        else if (c == 'U') undo(text, &length, &head);
        else if (c == 'R') redo(text, &length, &head);
        else if (c == 'W') alternate_up(text, &length, &head);
        else if (c == 'E') alternate_down(text, &length, &head);
        else if (c == 'P') paste("daniel", text, &length, &head);
        else if (c == 127) delete(text, &length, &head);
        else insert(c, text, &length, &head);
    }
    restore_terminal();
}
