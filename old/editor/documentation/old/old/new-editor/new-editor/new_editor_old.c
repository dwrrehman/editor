/// The "e" editor, written in C.
///
/// Designed as a better, more ergonomic ed.
/// it is focused on minimalism, ergonomics, and intuitiveness,
/// and is fast to pick up! simplicity is important.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

#define and &&
#define or ||
#define not !
#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

typedef size_t nat;

const char* editor_name = "e";

//const nat max_filename_length = 255;
const nat max_path_length = 4096;
const nat max_message_length = 1024;
const char delete_key = 127;

const char* left_command_sequence = "wef";
const char* right_command_sequence = "oij";
const char* quit_save_command_sequence = "ewq";

enum key_bindings {
    edit_key = 'e',         display_key = 'f',
    select_key = 's',
    
    up_key = 'o',           down_key = 'i',
    left_key = 'j',         right_key = ';',
    
    jump_key = 'k',         find_key = 'l',
    
    clear_key = 'c',
    
    cut_key = 'd',          paste_key = 'a',
    
    rename_key = 'W',       save_key = 'w',
    
    force_quit_key = 'Q',   quit_key = 'q',
    
    redo_key = 'r',         undo_key = 'u',
    
};

enum editor_mode {
    quit,
    command_mode,
    edit_mode,
    select_mode,
};

struct location {
    nat line;
    nat column;
};

static inline void error(const char* message) {
    perror(message);
    exit(1);
}

static inline char get_character() {
    struct termios t = {0}; if (tcgetattr(0, &t) < 0) error("tcsetattr()");
    t.c_lflag &= ~ICANON; t.c_lflag &= ~ECHO; t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &t) < 0) error("tcsetattr ICANON");
    char c = 0; if (read(0, &c, 1) < 0) error("read()"); t.c_lflag |= ICANON; t.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &t) < 0) error("tcsetattr ~ICANON");
    return c;
}

//static inline void save_cursor() { printf("\033[s"); fflush(stdout); }
//static inline void restore_cursor() { printf("\033[u"); fflush(stdout); }
//static inline void set_cursor(nat x, nat y) { printf("\033[%lu;%luH", y, x); fflush(stdout); }
static inline void save_screen() { printf("\033[?1049h"); fflush(stdout); }
static inline void restore_screen() { printf("\033[?1049l"); fflush(stdout); }
static inline void clear_screen() { printf("\033[1;1H\033[2J"); fflush(stdout); }
//static inline void clear_line() { printf("\033[2K"); }
//static inline void move_up() { printf("\033[A"); fflush(stdout); }
//static inline void move_down() { printf("\033[B"); fflush(stdout); }
//static inline void move_right() { printf("\033[C"); fflush(stdout); }
//static inline void move_left() { printf("\033[D"); fflush(stdout); }

static inline bool trigraph(const char* sequence, char input, char previous1, char previous2) {
    return (input == sequence[2] and previous1 == sequence[1] and previous2 == sequence[0]);
}

static inline void open_file(int argc, const char** argv, nat* length, char** source, char* filename) {
    if (argc > 1) {
        strncpy(filename, argv[1], max_path_length);
        FILE* file = fopen(filename, "a+");
        if (not file) error("fopen");
        fseek(file, 0L, SEEK_END);
        *length = ftell(file);
        *source = calloc(*length + 1, sizeof(char));
        fseek(file, 0L, SEEK_SET);
        fread(*source, sizeof(char), *length, file);
        if (ferror(file)) error("read");
        fclose(file);
    } else *source = calloc(*length + 1, sizeof(char));
}

void prompt_filename(char* filename) {
    printf("name: ");
    fflush(stdout);
    memset(filename, 0, sizeof(char) * (max_path_length));
    fgets(filename, max_path_length, stdin);
    filename[strlen(filename) - 1] = '\0';
}

static void save(char* source, nat source_length, char* name) {
    if (not name or not strlen(name)) prompt_filename(name);
    FILE* file = fopen(name, "w+");
    if (not file) error("fopen");
    fwrite(source, sizeof(char), source_length, file);
    if (ferror(file)) error("write");
    fclose(file);
}

static inline void rename_file(char* old, char* message) {
    char new[max_path_length + 1];
    prompt_filename(new);
    if (rename(old, new)) {
        char* error_string = strerror(errno);
        strcpy(message, "rename unsuccessful: ");
        strcat(message, error_string);
    } else strncpy(old, new, max_path_length);
}

void insert(char toinsert, nat at, char** source, nat* source_length) {
    if (at > *source_length) abort();
    *source = realloc(*source, sizeof(char) * (*source_length + 1));
    if (at == *source_length) (*source)[(*source_length)++] = toinsert;
    else {
        (*source_length)++;
        memmove((*source) + at + 1, (*source) + at, (*source_length) - at);
        (*source)[at] = toinsert;
    }
    (*source)[*source_length] = '\0';
}

void delete_at(nat at, char** source, nat* source_length) {
    if (at > *source_length) abort();
    else if (*source_length <= 0) return;
    if (at != *source_length) memmove((*source) + at, (*source) + at + 1, (*source_length) - at);
    *source = realloc(*source, sizeof(char) * (*source_length - 1));
    (*source_length)--;
    (*source)[*source_length] = '\0';
}

struct location get_location(nat point, char* source, nat source_length) {
    struct location result = {.line = 0, .column = 0};
    for (nat i = 0; i < source_length; i++) {
        if (i == point) return result;
        if (source[i] == '\n') {
            result.line++;
            result.column = 0;
        } else result.column++;
    }
    return result;
}


/*
 
 kjskld   : delete the current line, in command mode.
 
 wefoskldaiae     : copy the rest of the above line onto the current from edit mode.
 
 wefkoskmde     : delete a whole file from edit mode.
 
 
 
 */


int main(int argc, const char** argv) {
    
    char* source = NULL;
    nat source_length = 0;
    bool saved = true;
    
    char name[max_path_length], message[max_message_length];
    memset(name, 0, sizeof name);
    memset(message, 0, sizeof message);
    
    char prev1 = 0, prev2 = 0;
    nat point = 0, select_point = 0;
    enum editor_mode mode = command_mode;
    
    open_file(argc, argv, &source_length, &source, name);
    save_screen();
    
    while (mode != quit) {
        
        const struct location cursor = get_location(point, source, source_length);
        const struct location selected_cursor = get_location(select_point, source, source_length);
        const char input = get_character();
        
        if (mode == command_mode) {
            
            if (input == up_key and cursor.line > 0) {
                //move_up();
                
            } else if (input == down_key) {
                //move_down();
            }
            
            else if (input == left_key and point > 0) { point--; }
            else if (input == right_key and point < source_length) {point++; }
            
            else if (input == display_key) { printf("%s", source); fflush(stdout); }
            
            else if (input == save_key) { save(source, source_length, name); saved = true; }
            else if (input == rename_key) { rename_file(name, message); saved = true; }
            else if (input == quit_key and saved or input == force_quit_key) mode = quit;
            else if (input == edit_key) mode = edit_mode;
            else if (input == clear_key) clear_screen();
            
        } else if (mode == edit_mode) {
            if (trigraph(left_command_sequence, input, prev1, prev2) or trigraph(right_command_sequence, input, prev1, prev2)) {
                if (point > 0) delete_at(point--, &source, &source_length);
                if (point > 0) delete_at(point--, &source, &source_length);
                mode = command_mode;
            } else if (trigraph(quit_save_command_sequence, input, prev1, prev2)) {
                if (point > 0) delete_at(point--, &source, &source_length);
                if (point > 0) delete_at(point--, &source, &source_length);
                save(source, source_length, name);
                mode = quit;
            } else {
                saved = false;
                if (input == delete_key and point > 0) delete_at(point--, &source, &source_length);
                else if (input != delete_key) insert(input, point++, &source, &source_length);
            }
        }
        prev2 = prev1;
        prev1 = input;
    }
    free(source);
    restore_screen();
}
