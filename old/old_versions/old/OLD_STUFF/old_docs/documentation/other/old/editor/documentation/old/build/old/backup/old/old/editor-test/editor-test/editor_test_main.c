/// The "e" editor, written in C.
///
/// Designed as a better, more ergonomic textedit.
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
const char* editor_version = "0.0.1";

const long rename_color = 214L;
const long status_color = 245L;
const long edited_flag_color = 9L;

const nat max_path_length = 4096;
const nat max_message_length = 1024;
const char delete_key = 127;

const char* left_exit = "wef";
const char* right_exit = "oij";
const char* edit_exit = "wq";

enum key_bindings {
    
    // modes:
    edit_key = 'e',        select_key = 's',
    
    // navigation:
    up_key = 'o',           down_key = 'i',
    left_key = 'j',         right_key = ';',
    jump_key = 'k',         find_key = 'l',
    
    // text commands:
    cut_key = 'd',          paste_key = 'a',
    
    // undo and redo:
    redo_key = 'r',         undo_key = 'u',
    
    // save and quitting:
    rename_key = 'W',       save_key = 'w',
    force_quit_key = 'Q',   quit_key = 'q',
    
    
    // others:
    option_key = 'p',       function_key = 'f',
    
    // display:
    status_bar_key = '.',   clear_key = '\t',
    cursor_key = '\'',
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

static inline void save_cursor() { printf("\033[s"); fflush(stdout); }
static inline void restore_cursor() { printf("\033[u"); fflush(stdout); }
static inline void set_cursor(nat x, nat y) { printf("\033[%lu;%luH", y, x); fflush(stdout); }
static inline void hide_cursor() { printf("\033[?25l"); fflush(stdout); }
static inline void show_cursor() { printf("\033[?25h"); fflush(stdout); }
static inline void save_screen() { printf("\033[?1049h"); fflush(stdout); }
static inline void restore_screen() { printf("\033[?1049l"); fflush(stdout); }
static inline void clear_screen() { printf("\033[1;1H\033[2J"); fflush(stdout); }
static inline void clear_line() { printf("\033[2K"); }
static inline void error(const char* message) { perror(message); exit(1); }
static inline bool bigraph(const char* seq, char c0, char c1) { return c0 == seq[1] and c1 == seq[0]; }
static inline bool trigraph(const char* seq, char c0, char c1, char c2) { return c0 == seq[2] and c1 == seq[1] and c2 == seq[0]; }

static inline char get_character() {
    struct termios t = {0}; if (tcgetattr(0, &t) < 0) error("tcsetattr()");
    t.c_lflag &= ~ICANON; t.c_lflag &= ~ECHO; t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &t) < 0) error("tcsetattr ICANON");
    char c = 0; if (read(0, &c, 1) < 0) error("read()"); t.c_lflag |= ICANON; t.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &t) < 0) error("tcsetattr ~ICANON");
    return c;
}

static inline void get_datetime(char buffer[16]) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline void open_file(int argc, const char** argv, char** source, nat* length, char* filename) {
    if (argc == 1) return;
    strncpy(filename, argv[1], max_path_length);
    FILE* file = fopen(filename, "a+");
    if (not file) error("fopen");
    fseek(file, 0L, SEEK_END);
    *length = ftell(file);
    free(*source);
    *source = calloc(*length + 1, sizeof(char));
    fseek(file, 0L, SEEK_SET);
    fread(*source, sizeof(char), *length, file);
    if (ferror(file)) error("read");
    fclose(file);
}

void prompt_filename(char* filename) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    save_cursor(); set_cursor(0, window.ws_row); clear_line();
    printf(set_color "filename: " reset_color, rename_color);
    memset(filename, 0, sizeof(char) * (max_path_length));
    fgets(filename, max_path_length, stdin);
    filename[strlen(filename) - 1] = '\0';
    restore_cursor();
}

static void save(char* source, nat source_length, char* name, bool* saved) {
    if (not name or not strlen(name)) prompt_filename(name);
    FILE* file = fopen(name, "w+");
    if (not file) error("fopen");
    fwrite(source, sizeof(char), source_length, file);
    if (ferror(file)) error("write");
    fclose(file);
    *saved = true;
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

void insert(char toinsert, nat at, char** source, nat* length) {
    if (at > *length) abort();
    *source = realloc(*source, sizeof(char) * (*length + 1));
    if (at == *length) (*source)[(*length)++] = toinsert;
    else {
        memmove((*source) + at + 1, (*source) + at, (*length) - at);
        (*length)++;
        (*source)[at] = toinsert;
    }
}

void delete_at(nat at, char** source, nat* length) {
    if (at > *length) abort();
    else if (!*length) return;
    if (at != *length) memmove((*source) + at, (*source) + at + 1, (*length) - at);
    *source = realloc(*source, sizeof(char) * (*length - 1));
    (*length)--;
    (*source)[*length] = '\0'; // do we need this?
}

struct location get_cursor_from_point(nat point, char* source, nat length) {
    struct location result = {.line = 0, .column = 0};
    for (nat i = 0; i < length; i++) {
        if (i == point) return result;
        if (source[i] == '\n') {
            result.line++;
            result.column = 0;
        } else result.column++;
    }
    return result;
}

nat get_point_from_cursor(struct location cursor, char* source, nat length) {
    nat line = 0, column = 0;
    for (nat i = 0; i < length; i++) {
        if (source[i] == '\n') {
            line++;
            column = 0;
        } else column++;
        if (line == cursor.line and column == cursor.column) return i;
    }
    return 0;
}

struct line {
    char* line;
    nat length;
};

static inline void display(char* source, nat length, struct location origin, struct winsize window) {
    clear_screen();
    printf("%s", source);
    fflush(stdout);
}

void print_status_bar(enum editor_mode mode, char* filename, bool saved, char* message,
                      nat point, struct location cursor, struct location origin, struct location screen, struct location desired,
                      struct winsize window) {
    save_cursor();
    set_cursor(0, window.ws_row);
    clear_line();
    char datetime[16] = {0};
    get_datetime(datetime);
    printf(set_color "%s : %s%s : ", status_color,
           datetime, filename, saved ? "" : " (edited)");
    printf(" p=%lu; c=%lu,%lu; o=%lu,%lu; s=%lu,%lu; d=%lu,%lu   ",
           point,
           cursor.line, cursor.column,
           origin.line, origin.column,
           screen.line, screen.column,
           desired.line, desired.column);
    printf("   %s", message);
    printf(reset_color);
    restore_cursor();
    fflush(stdout);
}

int main(int argc, const char** argv) {
    
    char* source = calloc(1, sizeof(char));
    nat length = 1;
    
    bool saved = true, is_clear = false, show_status = false, should_show_cursor = true;
    enum editor_mode mode = command_mode;
    
    char name[max_path_length], message[max_message_length], c1 = 0, c2 = 0;
    memset(name, 0, sizeof name);
    memset(message, 0, sizeof message);
    
    nat point = 0;
    struct location
    cursor = {.line = 0, .column = 0},
    origin = {.line = 0, .column = 0},
    screen = {.line = 0, .column = 0},
    desired = {.line = 0, .column = 0};
    
    save_screen();
    
    open_file(argc, argv, &source, &length, name);
    nat line_count = 0;
    struct line* lines = convert_into_lines(source, length, &line_count);

    while (mode != quit) {
        
        
        
        point = get_point_from_cursor(cursor, source, length);
        struct winsize window;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
        
        if (is_clear) {
            clear_screen();
            hide_cursor();
        } else {
            if (should_show_cursor) show_cursor(); else hide_cursor();
            display(source, length, origin, window);
            if (show_status) print_status_bar(mode, name, saved, message, point, cursor, origin, screen, desired, window);
            set_cursor(screen.column + 1, screen.line + 1);
        }
        const char c = get_character();
        
        if (mode == command_mode) {
            
            if (c == up_key) {
                if (screen.line) { screen.line--; cursor.line--; }
                else if (origin.line) { origin.line--; cursor.line--; }
            }
            
            else if (c == down_key) {
                if (screen.line < window.ws_row - 1 - !!show_status) { screen.line++; cursor.line++; }
                else { origin.line++; cursor.line++; }
            }
            
            else if (c == left_key) {
                if (screen.column) { screen.column--; cursor.column--; }
                else if (origin.column) { origin.column--; cursor.column--; }
            }
            
            else if (c == right_key) {
                if (screen.column < window.ws_col - 1) { screen.column++; cursor.column++; }
                else { origin.column++; cursor.column++; }
            }
            
            else if (c == save_key) save(source, length, name, &saved);
            else if (c == rename_key) rename_file(name, message);
            else if (c == quit_key and saved or c == force_quit_key) mode = quit;
            else if (c == edit_key) mode = edit_mode;
            else if (c == status_bar_key) show_status = not show_status;
            else if (c == clear_key) is_clear = not is_clear;
            else if (c == cursor_key) should_show_cursor = not should_show_cursor;
        
        } else if (mode == edit_mode) {
            if (trigraph(left_exit, c, c1, c2) or trigraph(right_exit, c, c1, c2)) {
                if (point > 0) delete_at(point--, &source, &length);
                if (point > 0) delete_at(point--, &source, &length);
                mode = command_mode;
            } else if (bigraph(edit_exit, c, c1)) {
                if (point > 0) delete_at(point--, &source, &length);
                save(source, length, name, &saved);
                mode = quit;
            } else {
                saved = false;
                if (c == delete_key and point > 0) delete_at(point--, &source, &length);
                else if (c != delete_key) insert(c, point++, &source, &length);
            }
        }
        c2 = c1;
        c1 = c;
        free(lines);
    }
    free(source);
    show_cursor();
    restore_screen();
}
