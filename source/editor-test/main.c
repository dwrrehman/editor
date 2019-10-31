/// this is an editor, named "e" which im writing in C,
/// designed as a better "ed", and textedit.
/// it is focused on minimalism, ergonomics, and intuitiveness.

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

#define and &&
#define or ||
#define not !

#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

const char* editor_name = "e";

const long rename_color = 214L;

const char* left_command_sequence = "wef";
const char* right_command_sequence = "oij";
const char* quit_save_command_sequence = "ewq";
enum key_bindings {
    up_key = 'o',
    down_key = 'i',
    left_key = 'j',
    right_key = ';',
    jump_key = 'k',
    find_key = 'l',
    
    edit_key = 'e',
    cut_key = 'd',
    copy_key = 'f',
    paste_key = 'a',
    select_key = 's',
    
    resave_key = 'W',
    save_key = 'w',
    
    redo_key = 'r',
    undo_key = 'u',
    
    force_quit_key = 'Q',
    quit_key = 'q',
};

typedef size_t nat;

char get_character() {
    struct termios t = {0}; if (tcgetattr(0, &t) < 0) perror("tcsetattr()");
    t.c_lflag &= ~ICANON; t.c_lflag &= ~ECHO; t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &t) < 0) perror("tcsetattr ICANON");
    char c = 0; if (read(0, &c, 1) < 0) perror("read()"); t.c_lflag |= ICANON; t.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &t) < 0) perror("tcsetattr ~ICANON");
    return c;
}

static inline void get_datetime(char buffer[16]) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

void save_cursor() { printf("\033[s"); fflush(stdout); }
void restore_cursor() { printf("\033[u"); fflush(stdout); }
void set_cursor(nat x, nat y) { printf("\033[%lu;%luH", y, x); fflush(stdout); }
void save_screen() { printf("\033[?1049h"); fflush(stdout); }
void restore_screen() { printf("\033[?1049l"); fflush(stdout); }
void clear_screen() { printf("\033[1;1H\033[2J"); fflush(stdout); }
void clear_line() { printf("\033[2K"); }
void move_up() { printf("\033[A"); fflush(stdout); }
void move_down() { printf("\033[B"); fflush(stdout); }
void move_right() { printf("\033[C"); fflush(stdout); }
void move_left() { printf("\033[D"); fflush(stdout); }


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


static bool trigraph(const char* sequence, char input, char previous1, char previous2) {
    return (input == sequence[2] and previous1 == sequence[1] and previous2 == sequence[0]);
}

static void init_source(int argc, const char** argv, nat* length, char** source) {
    if (argc > 1) {
        FILE* file = fopen(argv[1], "a+");
        if (file) {
            if (!fseek(file, 0L, SEEK_END)) {
                *length = ftell(file);
                *source = calloc(*length + 1, sizeof(char));
                fseek(file, 0L, SEEK_SET);
                fread(*source, sizeof(char), *length, file);
                if (ferror(file)) { perror("read"); exit(1); }
            }
        } else { perror(editor_name); exit(1); }
        fclose(file);
    } else *source = calloc(*length + 1, sizeof(char));
}


char* prompt_filename() {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    save_cursor();
    set_cursor(0, window.ws_row);
    clear_line();
    printf(set_color "filename: " reset_color, rename_color);
    fflush(stdout);
    char* filename = calloc(255, sizeof(char));
    fgets(filename, 254, stdin);
    filename[strlen(filename) - 1] = '\0';
    restore_cursor();
    return filename;
}

static void save(char* source, nat source_length, int argc, const char** argv) {
    
    ///TODO: make this store the given prompted filename, somewhere,
    /// so that next save doesnt require giving the filename.
    
    const char* filename = argc > 1 ? argv[1] : prompt_filename();
    FILE* file = fopen(filename, "w+");
    if (file) {
        fputs(source, file);
        if (ferror(file)) { perror("write"); exit(1); }
    } else { perror(editor_name); exit(1); }
    fclose(file);
    if (argc <= 1) free((char*) filename);
}

static inline void resave() {
    /// unimplemented.
}

static void display(char* source) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    clear_screen();
    
    ///TODO: do scrolling based display of local view of text.
    
    fflush(stdout);
}

void print_sidebar(enum editor_mode mode, nat point, struct location current, bool saved) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    save_cursor();
    set_cursor(0, window.ws_row);
    clear_line();
    const char* filename = "replace me";
    
    char datetime[16] = {0};
    get_datetime(datetime);
    printf(set_color "%s (%lu,%lu) %s" reset_color, 245L, filename, current.line, current.column, datetime);
    restore_cursor();
    fflush(stdout);
}

void insert(char toinsert, nat at, char** source, nat* source_length) {
    if (at > *source_length) abort();
    *source = realloc(*source, sizeof(char) * (*source_length + 1));
    if (at == *source_length) (*source)[(*source_length)++] = toinsert;
    else {
        (*source_length)++;
        memmove((*source)+at+1, (*source)+at, (*source_length) - at);
        (*source)[at] = toinsert;
    }
    (*source)[*source_length] = '\0';
}

void delete_at(nat at, char** source, nat* source_length) {
    if (at > *source_length) abort();
    else if (*source_length <= 0) return;
    if (at != *source_length) memmove((*source)+at, (*source)+at+1, (*source_length) - at);
    *source = realloc(*source, sizeof(char) * (*source_length - 1));
    (*source_length)--;
    (*source)[*source_length] = '\0';
}

//struct location get_location(nat point, char* source, nat source_length) {
//    struct location result = {.line = 0, .column = 0};
//    for (nat i = 0; i < source_length; i++) {
//        if (i == point) return result;
//        if (source[i] == '\n') {
//            result.line++;
//            result.column = 0;
//        } else result.column++;
//    }
//    return result;
//}
//
//static nat get_insertion_point(struct location desired, char* source, nat source_length) {
//    nat current_line = 0;
//    for (nat i = 0; i < source_length; i++) {
//        if (desired.line == current_line) return i + desired.column;
//        else if (source[i] == '\n') current_line++;
//    }
//    return 0;
//}

int main(int argc, const char** argv) {
    
    char * source = NULL, * clipboard = NULL, * selection = NULL;
    nat source_length = 0, clipboard_length = 0, selection_length = 0;
    
    char filename[255] = {0};
    bool saved = true;
    enum editor_mode mode = command_mode;
    char prev1 = 0, prev2 = 0;
    nat point = 0;
    struct location desired = {.line = 0, .column = 0};
    //struct location target = {.line = 0, .column = 0};
    
    init_source(argc, argv, &source_length, &source);
    save_screen();
    
    ///if (target.column) target.column--;
    
    while (mode != quit) {
        
        struct location cursor = get_location(point, source, source_length);
        
        display(source);
        
        if (mode != edit_mode)
            print_sidebar(mode, point, cursor, saved);
        
        set_cursor(cursor.column+1, cursor.line+1);
        char input = get_character();
        
        if (mode == command_mode) {
            
            if (input == edit_key) {
                mode = edit_mode;
                
            } else if (input == up_key and cursor.line > 0) {
                move_up();
                
            } else if (input == down_key) {
                move_down();
                
            } else if (input == left_key and point > 0) {
                move_left();
                point--;
                
            } else if (input == right_key and point < source_length) {
                move_right();
                point++;
                
            } else if (input == jump_key) {
                
            } else if (input == cut_key) {
            } else if (input == copy_key) {
            } else if (input == paste_key) {
            } else if (input == find_key) {
            } else if (input == save_key) { save(source, source_length, argc, argv); saved = true; }
            else if (input == resave_key) { resave(/*source, source_length, argc, argv*/); saved = true; }
            else if (input == quit_key and saved) mode = quit;
            else if (input == force_quit_key) mode = quit;
            
        } else if (mode == edit_mode) {
            if (trigraph(left_command_sequence, input, prev1, prev2) or
                trigraph(right_command_sequence, input, prev1, prev2)) {
                if (point > 0) delete_at(point--, &source, &source_length);
                if (point > 0) delete_at(point--, &source, &source_length);
                mode = command_mode;
            } else if (trigraph(quit_save_command_sequence, input, prev1, prev2)) {
                if (point > 0) delete_at(point--, &source, &source_length);
                if (point > 0) delete_at(point--, &source, &source_length);
                save(source, source_length, argc, argv);
                mode = quit;
            } else {
                saved = false;
                if (input == 127) { if (point > 0) delete_at(point--, &source, &source_length); }
                else insert(input, point++, &source, &source_length);
            }
        }
        prev2 = prev1;
        prev1 = input;
    }
    free(source);
    restore_screen();
}
