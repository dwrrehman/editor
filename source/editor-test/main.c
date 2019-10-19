// this is an editor, named "e" which im writing in C, designed as a better "ed".
// it is focused on minimlaism, terseness, and intuitiveness.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#define and &&
#define or ||
#define not !

typedef size_t nat;

const char* editor_name = "e";

char get_character() {
    struct termios t = {0}; if (tcgetattr(0, &t) < 0) perror("tcsetattr()");
    t.c_lflag &= ~ICANON; t.c_lflag &= ~ECHO; t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &t) < 0) perror("tcsetattr ICANON");
    char c = 0; if (read(0, &c, 1) < 0) perror("read()"); t.c_lflag |= ICANON; t.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &t) < 0) perror("tcsetattr ~ICANON");
    return c;
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


#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

char* prompt_filename() { // produces a string.
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    save_cursor();
    set_cursor(0, window.ws_row);
    clear_line();
    printf(set_color "filename: " reset_color, 214L);
    fflush(stdout);
    char* filename = calloc(255, sizeof(char));
    fgets(filename, 254, stdin);
    filename[strlen(filename) - 1] = '\0';
    restore_cursor();
    return filename;
}

static void save(char* source, nat source_length, int argc, const char** argv) {
    const char* filename = argc > 1 ? argv[1] : prompt_filename();
    FILE* file = fopen(filename, "w+");
    if (file) {
        fputs(source, file);
        if (ferror(file)) { perror("write"); exit(1); }
    } else { perror(editor_name); exit(1); }
    fclose(file);
    if (argc <= 1) free((char*) filename);
}

static void display(char* source) {
    clear_screen();
    printf("%s", source); // temp: we need to do scrolling.
    fflush(stdout);
}

void print_sidebar(enum editor_mode mode, nat point, struct location current, bool saved) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    save_cursor();
    set_cursor(0, window.ws_row);
    clear_line();
    printf(set_color "%c : %lu = (%lu,%lu) : %s " reset_color, 245L, mode == edit_mode ? 'E' : 'C', point, current.line, current.column, saved ? "saved" : "edited");
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

static nat get_insertion_point(struct location desired, char* source, nat source_length) {
    nat current_line = 0;
    for (nat i = 0; i < source_length; i++) {
        if (desired.line == current_line) return i + desired.column;
        else if (source[i] == '\n') current_line++;
    }
    return 0;
}

int main(int argc, const char** argv) {
    
    char * source = NULL;       //* clipboard = NULL, * selection = NULL;
    nat source_length = 0;      //  clipboard_length = 0, selection_length = 0;
    
    bool saved = true;
    enum editor_mode mode = command_mode;
    char prev1 = 0, prev2 = 0;
    nat point = 0;
    
    init_source(argc, argv, &source_length, &source);
    save_screen();
    
    while (mode != quit) {
        
        struct location cursor = get_location(point, source, source_length);
        display(source);
        print_sidebar(mode, point, cursor, saved);
        set_cursor(cursor.column+1, cursor.line+1);
        char input = get_character();
        
        if (mode == command_mode) {
            
            if (input == 'e') {
                saved = false;
                mode = edit_mode;
                
            } else if (input == 'o' and cursor.line > 0) {
                move_up();
                
                
            } else if (input == 'i') {
                move_down();
                
                cursor.line++;
                point = get_insertion_point(cursor, source, source_length);
                

            } else if (input == 'j' and point > 0) {
                move_left();
                point--;
                
            } else if (input == ';' and point < source_length) {
                move_right();
                point++;
                
            } else if (input == 'w') { save(source, source_length, argc, argv); saved = true; }
            else if (input == 'W') { save(source, source_length, argc, argv); saved = true; } /// rename the file?
            else if (input == 'q' and saved) mode = quit;
            else if (input == 'Q') mode = quit;
            
        } else if (mode == edit_mode) {
            if (trigraph("wef", input, prev1, prev2) or trigraph("oij", input, prev1, prev2)) {
                if (point > 0) delete_at(point--, &source, &source_length);
                if (point > 0) delete_at(point--, &source, &source_length);
                mode = command_mode;
            } else if (trigraph("ewq", input, prev1, prev2)) {
                if (point > 0) delete_at(point--, &source, &source_length);
                if (point > 0) delete_at(point--, &source, &source_length);
                save(source, source_length, argc, argv);
                mode = quit;
            } else {
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
