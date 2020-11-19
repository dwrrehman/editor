// undo-recognizer:
// combining my signature recognition system with my undo system.
// to allow for signatures to be substrings within other ones.
// Daniel Rehman, CE202011146.092810

#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <iso646.h>
#include <stdbool.h>

#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

struct choice { // this is the "action" in our undo system. it needs a .found member.
    size_t action;
    size_t found;
    // this needs to keep track of whether the buffer was saved!  i think....
};

static struct termios terminal = {0};

static inline void restore_terminal() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) abort();
}

static inline void configure_terminal() {
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) abort();
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) abort();
}

void start() {
    configure_terminal();
    
    const char* actions[] = {"",
        "open",
        "write",
        "close",
        "rename",
        "move",
        "quit",
        "line",
        "cb",
        "define",
        "toggle numbers",
        "column",
        "f", "e", "w", "a",
        "j", "i", "o", ";",
    0}; // required
    
    char message[2048] = {0}; // required
    uint8_t history[32] = {0}; // required
    size_t count = 0, guess = 0, found = 0; // required
    
    struct choice* choices = malloc(4096 * sizeof(struct choice)); // this is simply the undo tree.
    size_t choice_count = 0; // part of the undo tree; its height.
    
    while (1) {
        
        printf("\033[H\033[2J");
        
        // temp: for debugging
        printf("selection: found = %lu\n\n", found);
        for (size_t i = 0; actions[i]; i++) {
            const long color = i == guess ? 33L : 234L;
            printf("\t" set_color "%s\n" reset_color, color, actions[i]);
        }
        
        printf("\n\n%s\n", message);
        printf("\n\n:> ");
        fflush(stdout);
        
        uint8_t c = 0; read(0, &c, 1);
        
        if (c == 27) break; // temp
        
        else if (c == '\\') { // temp
            memset(message, 0, sizeof message);
            memset(history, 0, sizeof history);
            
        } else {
            memmove(history, history + 1, sizeof history - 1);
            history[sizeof history - 1] = c;
            count++;
            
            for (size_t a = 1; actions[a]; a++) {
                size_t h = sizeof history, H = count;
                for (size_t c = strlen(actions[a]); c--; H--)
                    if (not h or history[--h] != actions[a][c])
                        goto next;
                guess = a; found = H; break;
                next: guess = 0; found = 0;
            }
            if (guess) {
                for (size_t i = choice_count; i--;) { // go through each undo-node from the head, parent by parent.
                    if (choices[i].found >= found) {
                        // in the real thing, you wouldnt log the fact that you undid something.
                        sprintf(message + strlen(message), ">> undoing %s...\n", actions[choices[i].action]);
                        --choice_count; // undo the action if it was found after or at our location. (make sure to do a hard undo! (ie, not redoable).)
                    }
                }
                // in the real thing, you wouldnt log the fact that you did something.
                sprintf(message + strlen(message), ">> doing %s...\n", actions[guess]);
                choices[choice_count].action = guess; // do action:  push to the undo tree, with its .found as well.
                choices[choice_count++].found = found;
            }
        }
    }
    printf("\033[H\033[2J");
    restore_terminal();
}











///
/// there are 6 intrinsics in the undo system:
///
///     do-action  hard-undo-action             (aka, append_action, delete_action)
///
///     undo-action redo-action                 (aka, down, up)
///
///     branch-incr, branch-decr                (aka, left, right)
///
///
///              we need to come up with a keylayout for these intrinsics...
///                 well... all but the do-action one, lol.
///





//        printf("\n\thistory: (%lu): \"", count);
//        for (size_t i = 0; i < 32; i++) printf("%c", (char) history[i]);
//        printf("\"");

//        printf("to undo: (%lu) : [ ",choice_count);
//        for (size_t i = 0; i < choice_count; i++) printf("%s [@%lu], ", actions[choices[i].action], choices[i].found);
//        printf("]\n");

//        } else if (c == 127) { // backspace
//            count--;
//            memmove(history + 1, history, sizeof history - 1);
//            *history = 0;
//            sprintf(message + strlen(message), ">> history: ");
//            for (size_t i = 0; i < 32; i++)
//                if (history[i]) sprintf(message + strlen(message), "%c", (char) history[i]);
//            sprintf(message + strlen(message), "\n");
//

/*
 if (not strcmp(actions[guess], "line")) {
     restore_terminal();
     printf("line number: ");
     char line[128] = {0};
     fgets(line, sizeof line, stdin);
     int n = atoi(line);
     sprintf(message + strlen(message), ">> going to line #%d\n", n);
     configure_terminal();
 }
 
 if (not strcmp(actions[guess], "column")) {
     restore_terminal();
     printf("column number: ");
     char column[128] = {0};
     fgets(column, sizeof column, stdin);
     int n = atoi(column);
     sprintf(message + strlen(message), ">> going to column #%d\n", n);
     configure_terminal();
 }
 
 if (not strcmp(actions[guess], "open")) {
     restore_terminal();
     printf("file name: ");
     char file[128] = {0};
     fgets(file, sizeof file, stdin);
     sprintf(message + strlen(message), ">> opening file \"%s\"\n", file);
     configure_terminal();
 }
 
 if (not strcmp(actions[guess], "rename")) {
     restore_terminal();
     printf("new file name: ");
     char file[128] = {0};
     fgets(file, sizeof file, stdin);
     sprintf(message + strlen(message), ">> renaming file to \"%s\"\n", file);
     configure_terminal();
 }
 
 if (not strcmp(actions[guess], "write")) {
     restore_terminal();
     printf("please give file name: ");
     char file[128] = {0};
     fgets(file, sizeof file, stdin);
     sprintf(message + strlen(message), ">> saving file to \"%s\"\n", file);
     configure_terminal();
 }
 
 if (not strcmp(actions[guess], "close")) {
     sprintf(message + strlen(message), ">> closing buffer...\n");
 }
                 
 if (not strcmp(actions[guess], "quit")) {
     sprintf(message + strlen(message), ">> quiting editor...\n");
     break;
 }
 
 if (not strcmp(actions[guess], "f")) {
     sprintf(message + strlen(message), ">> in insert mode...\n");
 }
 
 if (not strcmp(actions[guess], "e")) {
     sprintf(message + strlen(message), ">> deleted text...\n");
 }
 
 if (not strcmp(actions[guess], "j")) {
     sprintf(message + strlen(message), ">> moved left...\n");
 }
 
 if (not strcmp(actions[guess], ";")) {
     sprintf(message + strlen(message), ">> moved right...\n");
 }
 
 if (not strcmp(actions[guess], "i")) {
     sprintf(message + strlen(message), ">> moved down...\n");
 }
 
 if (not strcmp(actions[guess], "o")) {
     sprintf(message + strlen(message), ">> moved up...\n");
 }
 */

