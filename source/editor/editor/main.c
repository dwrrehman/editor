// the "wef" editor.
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
#include <ctype.h>

#define and   &&
#define or    ||
#define not   !
#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"
const char *save_cursor = "\033[s", *restore_cursor = "\033[u", *hide_cursor = "\033[?25l", *show_cursor = "\033[?25h", *set_cursor = "\033[%lu;%luH",
*clear_screen = "\033[1;1H\033[2J", *clear_line = "\033[2K", *save_screen = "\033[?1049h", *restore_screen = "\033[?1049l";

typedef ssize_t nat;

const char* editor_name = "e";
const char* editor_version = "0.0.1";

const long rename_color = 214L;
const long edit_status_color = 235L;
const long command_status_color = 245L;
const long edited_flag_color = 130L;

const char* left_exit = "wef";
const char* right_exit = "oij";
const char* edit_exit = "wq";
const char* jump_top = "ko";
const char* jump_bottom = "km";
const char* jump_begin = "kj";
const char* jump_end = "kl";

// globals:
static nat wrap_width = 100;

enum key_bindings {
    // modes:
    edit_key = 'e',         hard_edit_key = 'E',
    select_key = 's',
    
    // navigation:
    up_key = 'O',           down_key = 'I',
    left_key = 'J',         right_key = ';',
    
    jump_key = 'k',         find_key = 'l',
    
    // modify:
    cut_key = 'd',          paste_key = 'a',
    
    // undo/redo:
    redo_key = 'r',         undo_key = 'u',
    
    // saving:
    rename_key = 'W',       save_key = 'w',
    
    // quitting:
    force_quit_key = 'Q',   quit_key = 'q',
    
    // others:
    option_key = 'p',       function_key = 'f',
    status_bar_key = '.',   clear_key = '\t',
    cursor_key = '|',
};

enum editor_mode {
    quit,
    command_mode,
    edit_mode,
    hard_edit_mode,
    select_mode,
};
struct location {
    nat line;
    nat column;
};

struct line {
    char* line;
    nat length;
    bool continued;
};

struct buffer {
    nat index;
    char* text;
};

//static inline bool bigraph(const char* seq, char c0, char c1) { return c0 == seq[1] and c1 == seq[0]; }
//static inline bool trigraph(const char* seq, char c0, char c1, char c2) { return c0 == seq[2] and c1 == seq[1] and c2 == seq[0]; }

static inline char get_character() {
    struct termios t = {0}; if (tcgetattr(0, &t) < 0) perror("tcsetattr()");
    t.c_lflag &= ~ICANON; t.c_lflag &= ~ECHO; t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &t) < 0) perror("tcsetattr ICANON");
    char c = 0; if (read(0, &c, 1) < 0) perror("read()"); t.c_lflag |= ICANON; t.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &t) < 0) perror("tcsetattr ~ICANON");
    return c;
}






















static inline void open_file(int argc, const char** argv, char** source, nat* length, char* filename) {
    if (argc == 1) return;
    strncpy(filename, argv[1], 4096);
    FILE* file = fopen(filename, "a+");
    if (not file) { perror("fopen"); exit(1); }
    fseek(file, 0L, SEEK_END);
    *length = ftell(file);
    free(*source);
    *source = calloc(*length + 1, sizeof(char));
    fseek(file, 0L, SEEK_SET);
    fread(*source, sizeof(char), *length, file);
    if (ferror(file)) { perror("read"); exit(1); }
    fclose(file);
}
//
//static inline void prompt_filename(char* filename) {
//    struct winsize window;
//    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
//    save_cursor(); printf(set_cursor, window.ws_row, 0); clear_line();
//    printf(set_color "filename: " reset_color, rename_color);
//    memset(filename, 0, sizeof(char) * (4096));
//    fgets(filename, 4096, stdin);
//    filename[strlen(filename) - 1] = '\0';
//    restore_cursor();
//}

//static inline void save(char* source, nat source_length, char* name, bool* saved) {
//    if (not name or not strlen(name)) prompt_filename(name);
//    FILE* file = fopen(name, "w+");
//    if (not file) error("fopen");
//    fwrite(source, sizeof(char), source_length, file);
//    if (ferror(file)) error("write");
//    fclose(file);
//    *saved = true;
//}

//static inline void rename_file(char* old, char* message) {
//    char new[4096 + 1];
//    prompt_filename(new);
//    if (rename(old, new)) {
//        char* error_string = strerror(errno);
//        strcpy(message, "rename unsuccessful: ");
//        strcat(message, error_string);
//    } else strncpy(old, new, 4096);
//}

static inline void display(struct line* lines, nat line_count, struct location origin, struct winsize window) {
    char buffer[window.ws_col * window.ws_row];
    memset(buffer, 0, sizeof buffer);
    nat b = 0;
    for (nat line = origin.line; line < fmin(origin.line + window.ws_row, line_count); line++) {
        for (nat c = origin.column; c < fmin(origin.column + window.ws_col, lines[line].length); c++) buffer[b++] = lines[line].line[c];
        buffer[b++] = '\n';
    }
    printf("%s", clear_screen);
    fputs(buffer, stdout);
}

static inline void insert(char toinsert, nat at, char** source, nat* length) {
    if (at >= *length) abort();
    *source = realloc(*source, sizeof(char) * (*length + 1));
    memmove((*source) + at + 1, (*source) + at, ((*length)++) - at);
    (*source)[at] = toinsert;
}

static inline void delete(nat at, char** source, nat* length) {
    if (at >= *length) abort(); else if (!at or *length == 1) return;
    memmove((*source) + at - 1, (*source) + at, (*length) - at);
    *source = realloc(*source, sizeof(char) * (--(*length)));
}

static inline struct line* generate_line_view(char* source, nat source_length, nat* count, nat width) {
    *count = 0;
    nat length = 0;
    bool continued = false;
    struct line* lines = NULL;
    while (*source) {
        if (not length) {
            lines = realloc(lines, sizeof(struct line) * (*count + 1));
            lines[*count].line = source;
            lines[*count].continued = continued;
            lines[(*count)++].length = 0;
        }
        if (*source++ == '\n') { continued = false; length = 0; }
        else { length++; lines[*count - 1].length++; }
        if (width and length == width and *source != '\n') { continued = true; length = 0; }
    }
    if (source_length > 1 and *--source == '\n') {
        lines = realloc(lines, sizeof(struct line) * (*count + 1));
        lines[*count].line = source;
        lines[*count].continued = false;
        lines[(*count)++].length = 0;
    }
    return lines;
}

//static inline void get_datetime(char buffer[16]) {
//    struct timeval tv;
//    gettimeofday(&tv, NULL);
//    struct tm* tm_info = localtime(&tv.tv_sec);
//    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
//}

static inline void print_status_bar(enum editor_mode mode, char* filename, bool saved, char* message, struct location cursor, struct winsize window) {
    printf("%s", save_cursor);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    char datetime[16] = {0};
//    get_datetime(datetime);
    const long color = mode == edit_mode or mode == hard_edit_mode ? edit_status_color : command_status_color;
    printf("(%zd,%zd) ||| ", cursor.line, cursor.column);
    printf(set_color "%s : %s" reset_color
           set_color "%s" reset_color
           set_color ": %s" reset_color,
           color, datetime, filename,
           edited_flag_color, saved ? "" : " (e)",
           color, message);
    printf("%s", restore_cursor);
}

static inline void move_up(struct location *cursor, struct location *origin, struct location *screen, struct winsize* window, nat* point, struct line* lines, nat line_count) {
    
//    if (not cursor->line) {
//
//    }
//
//    cursor->line--;
//    if (screen->line) screen->line--; else if (origin->line) origin->line--;
    
}

static inline void move_down(struct location *cursor, struct location *origin, struct location *screen, struct winsize* window, nat* point, struct line* lines, nat line_count) {
    
//    if (cursor->line == line_count) {
//
//    }
//
//    cursor->line++;
//    if (screen->line < window->ws_row - 1) screen->line++; else origin->line++;
}

static inline void move_left(struct location *cursor, struct location *origin, struct location *screen, struct winsize* window,
                             nat* point, struct line* lines, nat line_count, struct location* desired) {
    if (not *point) return;
    if (cursor->column) {
        cursor->column--;
        if (screen->column) screen->column--;
        else if (origin->column) origin->column--;
        (*point)--;
    } else if (cursor->line) {
        
        // this is the bug. // made a change: delete the cursor->line - 1, to just cl.
        cursor->column = lines[cursor->line].length - (cursor->line == line_count or lines[cursor->line].continued);
        // right here ^
        
        if (cursor->column > window->ws_col - 1) {
            screen->column = window->ws_col - 1;
            origin->column = cursor->column - (window->ws_col - 1);
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
        cursor->line--;
        if (screen->line) screen->line--;
        else if (origin->line) origin->line--;
        (*point)--;
    }
    *desired = *cursor;
}

static inline void move_right(struct location* cursor, struct location* origin, struct location* screen, struct winsize* window,
                              nat* point, struct line* lines, nat line_count, nat length, struct location* desired) {
    if (*point >= length - 1) return;
    
    if (cursor->column < lines[cursor->line].length) {
        cursor->column++;
        if (screen->column < window->ws_col - 1) screen->column++; else origin->column++;
        (*point)++;
    } else if (cursor->line <= line_count) {
        cursor->line++;
        if (screen->line < window->ws_row - 1 - 1) screen->line++; else origin->line++;
        cursor->column = cursor->line == line_count or lines[cursor->line].continued;
        if (cursor->column > window->ws_col - 1) {
            screen->column = window->ws_col - 1;
            origin->column = cursor->column - (window->ws_col - 1);
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
        (*point)++;
    }
    *desired = *cursor;
}


/*
 
 theres a bug in the editor right now, which has to do with downards scrolling, andputting a bunch of newlines and then scrolling back up..
 
 i dont think that its too hard of a bug.
 
 */









//
//static inline void jump_to(nat line) {
//
//}

static inline void backspace(struct location *cursor, struct location *desired, nat *length, nat *line_count, struct line **lines, struct location *origin, nat *point, struct location *screen, char **source, struct winsize *window) {
    move_left(cursor, origin, screen, window, point, *lines, *line_count, desired);
    delete(++*point, source, length);
    --*point;
    free(*lines);
    *lines = generate_line_view(*source, *length, line_count, wrap_width);
}

//static void jump(char c1, struct location *cursor, struct location *desired, bool *jump_to_line, nat length, nat line_count,
//                 struct line *lines, struct location *origin, nat *point, struct location *screen, struct winsize *window) {
//    const char c0 = get_character();
//    if (bigraph(jump_top, c0, c1)) {
//        *screen = *cursor = *origin = (struct location){0, 0};
//        *point = 0;
//
//    } else if (bigraph(jump_bottom, c0, c1)) {
//        abort(); // unimplemented.
//
//    } else if (bigraph(jump_begin, c0, c1)) {
//        while (cursor->column) move_left(cursor, origin, screen, window, point, lines, line_count, desired);
//
//    } else if (bigraph(jump_end, c0, c1)) {
//        while (cursor->column < lines[cursor->line].length) move_right(cursor, origin, screen, window, point, lines, line_count, length, desired);
//
//    } else if (c1 == 'n') *jump_to_line = true;
//}

//static inline void get_numeric_input(char c, bool* jump_to_line, char* number) {
//    if (*jump_to_line and isdigit(c) and strlen(number) < 255) {
//        number[strlen(number)] = c;
//        jump_to(atoi(number));
//    } else if (*jump_to_line) {
//        memset(number, 0, sizeof(char) * 256);
//        *jump_to_line = false;
//    }
//}

int main(int argc, const char** argv) {
    
    enum editor_mode mode = command_mode;
    nat line_count = 0, length = 1, point = 0;
    bool saved = true, is_clear = false, should_show_cursor = true, show_status = true, jump_to_line = false;
    struct location cursor = {0,0}, origin = {0,0}, screen = {0,0}, desired = {0,0};
    char* source = calloc(1, sizeof(char)), name[4096] = {0}, message[1024] = {0}, number[256] = {0}, c = 0, c1 = 0, c2 = 0;
    
    printf("%s", save_screen);
    open_file(argc, argv, &source, &length, name);
    struct line* lines = generate_line_view(source, length, &line_count, wrap_width);
    
    while (mode != quit) {
        
        struct winsize window;
//        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
        window.ws_col = 10;
        window.ws_row = 10;
        
//        if (is_clear) { clear_screen(); hide_cursor(); }
//        else {
//            if (should_show_cursor) show_cursor(); else hide_cursor();
            display(lines, line_count, origin, window);
            if (show_status) print_status_bar(mode, name, saved, message, cursor, window);
            printf(set_cursor, screen.line + 1, screen.column + 1);
//        }
        fflush(stdout);
        c = get_character();
        
//        if (mode == command_mode) {
        
//            get_numeric_input(c, &jump_to_line, number);
            if (false) {}
//            else if (c == jump_key) jump(c, &cursor, &desired, &jump_to_line, length, line_count, lines, &origin, &point, &screen, &window);
            else if (c == up_key) move_up(&cursor, &origin, &screen, &window, &point, lines, line_count);
            else if (c == down_key) move_down(&cursor, &origin, &screen, &window, &point, lines, line_count);
            else if (c == left_key) move_left(&cursor, &origin, &screen, &window, &point, lines, line_count, &desired);
            else if (c == right_key) move_right(&cursor, &origin, &screen, &window, &point, lines, line_count, length, &desired);
            
//            else if (c == save_key) save(source, length, name, &saved);
//            else if (c == rename_key) rename_file(name, message);
//            else if (c == quit_key and saved or c == force_quit_key) mode = quit;
            else if (c == force_quit_key) mode = quit;
//            else if (c == edit_key) mode = edit_mode;
//            else if (c == hard_edit_key) mode = hard_edit_mode;
//            else if (c == select_key) mode = select_mode;
//            else if (c == status_bar_key) show_status = not show_status;
//            else if (c == clear_key) is_clear = not is_clear;
//            else if (c == cursor_key) should_show_cursor = not should_show_cursor;
        
//        } else if (mode == edit_mode or mode == hard_edit_mode) {
//            saved = false;
            
//            if ((trigraph(left_exit, c, c1, c2) or trigraph(right_exit, c, c1, c2)) and mode == edit_mode) {
//                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, &window);
//                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, &window);
//                mode = command_mode;
//
//            } else if (bigraph(edit_exit, c, c1) and mode == edit_mode) {
//                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, &window);
//                save(source, length, name, &saved);
//                mode = quit;
            
//            } else if (c == 27) mode = command_mode;
//                else if (c == 27) mode = command_mode;
            else if (c == 127 and point > 0)
                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, &window);
            else if (c != 127) {
                insert(c, point, &source, &length);
                free(lines);
                lines = generate_line_view(source, length, &line_count, wrap_width);
                move_right(&cursor, &origin, &screen, &window, &point, lines, line_count, length, &desired);
            }
//        }
//        c2 = c1;
//        c1 = c;
    }
    free(source);
    printf("%s", restore_screen);
}
