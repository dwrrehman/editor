
// a character based version of ed.
// just for fun.
// Created by Daniel Rehman.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#define and   &&
#define or    ||
#define not   !
#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

typedef ssize_t nat;

const long rename_color = 214L;
const long confirm_color = 196L;
//const long edit_status_color = 235L;
//const long command_status_color = 245L;
//const long edited_flag_color = 130L;
const char backspace_key = 127;
//const char escape_key = 27;
struct termios terminal = {0};

const char
    *save_cursor = "\033[s",
    *restore_cursor = "\033[u",
    *hide_cursor = "\033[?25l",
    *show_cursor = "\033[?25h",
//    *set_cursor = "\033[%lu;%luH",
//    *clear_screen = "\033[1;1H\033[2J",
    *clear_line = "\033[2K",
    *edit_exit = "fj";              ///TODO: make me not this. make me more ergonimic?

enum key_bindings {
    edit_key = 'e',         hard_edit_key = 'E',
    file_key = 'w',         hard_file_key = 'W',
    select_key = 's',       hard_select_key = 'S',
    quit_key = 'q',         force_quit_key = 'Q',
    cut_key = 'd',          paste_key = 'a',
    redo_key = 'r',         undo_key = 'u',
    
    backwards_key = 'j',    forwards_key = 'i',
    output_key = 'o',       option_key = ';',
    
    ZZZ_key = 'k',          XXXX_key = 'l',
        
    function_key = 'f',
};

enum editor_mode {
    quit,
    command_mode,
    edit_mode,
    hard_edit_mode,
    select_mode,
    hard_select_mode,
};

static inline void restore_terminal() {
    printf("%s", show_cursor);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
}

static inline void configure_terminal() {
    printf("%s", hide_cursor);
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) perror("tcgetattr(STDIN_FILENO, &terminal)");
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
}

static inline char get_character(char* source) {
    
    char c = 0;
    fflush(stdout);
    const nat n = read(STDIN_FILENO, &c, 1);
    if (n < 0) {
        printf("n < 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        printf("%s", source);
        abort();
    } else if (n == 0) {
        printf("n == 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        printf("%s", source);
        abort();
    } else return c;
}

static inline void open_file(int argc, const char** argv, char** source, nat* length, char* filename) {
    if (argc == 1) return;
    strncpy(filename, argv[1], 4096);
    FILE* file = fopen(filename, "r");
    if (not file) { restore_terminal(); perror("fopen"); exit(1); }
    fseek(file, 0L, SEEK_END);
    *length = ftell(file) + 1;
    *source = realloc(*source, sizeof(char) * (*length));
    (*source)[*length - 1] = 0;
    
    ///TODO: get this editor to work with unicode characters, and ignore non printable characters.
    
    fseek(file, 0L, SEEK_SET);
    fread(*source, sizeof(char), *length - 1, file);
    if (ferror(file)) { restore_terminal(); perror("read"); exit(1); }
    fclose(file);
}

static inline bool confirmed(const char* question) {
    printf(set_color "%s? " reset_color, confirm_color, question);
    char response[6] = {0};
    restore_terminal();
    fgets(response, 5, stdin);
    configure_terminal();
    return not strncmp(response, "yes", 3);
}

static inline void prompt_filename(char* filename) {
    printf(set_color "filename: " reset_color, rename_color);
    memset(filename, 0, sizeof(char) * (4096));
    restore_terminal();
    fgets(filename, 4095, stdin);
    configure_terminal();
    filename[strlen(filename) - 1] = 0;
}

static inline void save(char* source, nat length, char* name, bool* saved) {
    
    bool prompted = false;
    if (not name or not strlen(name)) {
        prompt_filename(name);
        prompted = true;
    }

    if (not strlen(name)) {
        printf("save aborted");
        return;
    } else if (prompted and access(name, F_OK) != -1) {
        strcpy(name, "");
        printf("file exists");
        return;
    }
    
    FILE* file = fopen(name, "w+");
    if (not file) { perror("save"); return; }
    else fwrite(source, sizeof(char), length - 1, file);
    if (ferror(file)) perror("write");
    fclose(file);
    *saved = true;
}

static inline void rename_file(char* filename) {
    char new[4096];
    prompt_filename(new);
    if (not strlen(new)) printf("rename aborted");
    else if (access(new, F_OK) != -1) printf("file exists");
    else if (rename(filename, new)) perror("rename");
    else strncpy(filename, new, 4096);
}

static inline void insert(char toinsert, nat at, char** source, nat* length) {
    if (at >= *length) return;
    *source = realloc(*source, sizeof(char) * (*length + 1));
    memmove((*source) + at + 1, (*source) + at, ((*length)++) - at);
    (*source)[at] = toinsert;
}

static inline void delete(nat at, char** source, nat* length) {
    if (at >= *length or !at or *length == 1) return;
    memmove((*source) + at - 1, (*source) + at, (*length) - at);
    *source = realloc(*source, sizeof(char) * (--(*length)));
}

static void file_action(char* source, nat length, char* name, bool* saved) {
    char c = get_character(source);
    if (c == 'r') rename_file(name);
    else if (c == 'e') save(source, length, name, saved);
}

static inline void reset_line() {
    printf("%s", clear_line);
    fputc('\r', stdout);
}

static inline void print_around_point(char* source, nat length, nat point, const nat width) {
    const nat offset = fmax(point - width, 0);
    reset_line();
    fwrite(source + offset, sizeof(char), fmin(point + width, length - 1) - offset, stdout);
    fflush(stdout);
}

int main(int argc, const char** argv) {
        
    nat length = 1, point = 0;
    char *source = calloc(1, sizeof(char)), name[4096] = {0}, c = 0, c1 = 0;
    
    nat width = 10;
    
    configure_terminal();
    open_file(argc, argv, &source, &length, name);
    
    bool saved = true;
    enum editor_mode mode = command_mode;
    while (mode) {

        c = get_character(source);
        reset_line();
        
        if (mode == command_mode) {
            if ((c == quit_key and saved) or c == force_quit_key and (saved or confirmed("discard"))) mode = quit;
            
            else if (c == file_key) file_action(source, length, name, &saved);
            else if (c == edit_key) mode = edit_mode;
            else if (c == hard_edit_key) mode = hard_edit_mode;
            
            else if (c == backwards_key and point) point--;
            else if (c == forwards_key and point < length - 1) point++;
            
            else if (c == output_key) {
                print_around_point(source, length, point, width);
            }
            
            else if (isdigit(c)) { // temp
                printf("note: setting width to be: %d", c - '0');
                width = c - '0';
            }
            
        } else if (mode == edit_mode or mode == hard_edit_mode) {
            saved = false;
            if (c == 'j' and c1 == 'f' and mode == edit_mode and point) { delete(point--, &source, &length); mode = command_mode; }
            else if (c == backspace_key and point) delete(point--, &source, &length);
            else if (c != backspace_key and point < length) insert(c, point++, &source, &length);
            
        } else {
            printf("error: mode not implemented yet");
            mode = command_mode;
        }
        c1 = c;
    }
    free(source);
    restore_terminal();
}


//                printf("source: \"%s\" :: inspecting around: l:%ld, p:%ld, at:%ld, n:%ld, b:%ld, a:%ld, w:%ld\n",
//                       source, length, point, at, n, before, after, width);


//        printf("source: \"%s\" :: debug: l:%ld, p:%ld\n", source, length, point);






/// TODO:

/**
    we just got the view to work, but its kinda hard to use, and ugly:
    
        - newlines are a problem.
 
        - we cant handle unicode chracters, in the display properly, and we dont move according to their length.
 
        - i kinda want to see the text...?   i ilke the visual mode better, i think.... i dunno... ill think about it somemore.
 
    we have to go all out for this unicode thing:
 
    we need to know the protocol inside and out, if we want to do this right.
 
    
    we have to change FJ exit to be something else...
 
 
    colored text support is missing!     as in, syntax highlighting!
 
 
    the ergonomic keylayout is really nice, though. i like that.
 
 */
