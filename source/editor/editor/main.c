
// My editor, written in C.
// Created by Daniel Rehman.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
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

typedef ssize_t nat;

const long rename_color = 214L;
const long confirm_color = 196L;
const long edit_status_color = 235L;
const long command_status_color = 245L;
const long edited_flag_color = 130L;
struct termios terminal = {0};

const char
    *save_cursor = "\033[s",
    *restore_cursor = "\033[u",
    *hide_cursor = "\033[?25l",
    *show_cursor = "\033[?25h",
    *set_cursor = "\033[%lu;%luH",
    *clear_screen = "\033[1;1H\033[2J",
    *clear_line = "\033[2K",
    *save_screen = "\033[?1049h",
    *restore_screen = "\033[?1049l";

const char
    *left_exit = "wef",
    *right_exit = "oij",
    *edit_exit = "wq",
    *jump_top = "ko",
    *jump_bottom = "km",
    *jump_begin = "kj",
    *jump_end = "kl";

// globals:
static nat wrap_width = 100;

enum key_bindings {
    edit_key = 'e',         hard_edit_key = 'E',
    select_key = 's',
    
    up_key = 'o',           down_key = 'i',
    left_key = 'j',         right_key = ';',
    jump_key = 'k',         find_key = 'l',

    cut_key = 'd',          paste_key = 'a',
    
    redo_key = 'r',         undo_key = 'u',
    rename_key = 'W',       save_key = 'w',
    force_quit_key = 'Q',   quit_key = 'q',

    option_key = 'p',       function_key = 'f',
    status_bar_key = '.',   help_key = '?',
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

struct clipboard {
    nat index;
    char* text;
};

struct file {
    struct line* lines;
    struct location cursor;
    struct location screen;
    struct location origin;
    struct location desired;
    enum editor_mode mode;
    nat line_count;
    nat length;
    bool saved;
    bool showing_cursor;
    bool showing_status;
    bool jump_to_line;
    char name[4096];
    char* source;
};

static inline bool bigraph(const char* seq, char c0, char c1) { return c0 == seq[1] and c1 == seq[0]; }
static inline bool trigraph(const char* seq, char c0, char c1, char c2) { return c0 == seq[2] and c1 == seq[1] and c2 == seq[0]; }

static inline void restore_terminal() {
    printf("%s", show_cursor);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
}

void configure_terminal() {
    printf("%s", hide_cursor);
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) { perror("tcgetattr(STDIN_FILENO, &terminal)"); abort(); }
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        abort();
    }
}

static inline char get_character() {
    char c = 0;
    const nat n = read(STDIN_FILENO, &c, 1);
    if (n < 0) {
        printf("n < 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        abort();
    } else if (n == 0) {
        printf("n == 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        abort();
    } else return c;
}

static inline void open_file(int argc, const char** argv, char** source, nat* length, char* filename) {
    if (argc == 1) return;
    strncpy(filename, argv[1], 4096);
    FILE* file = fopen(filename, "r");
    if (not file) { perror("fopen"); restore_terminal();printf("%s", show_cursor); printf("%s", restore_screen); exit(1); }
    fseek(file, 0L, SEEK_END);
    *length = ftell(file) + 1;
    free(*source);
    *source = calloc(*length, sizeof(char));
//    for (nat i = 0; i < *length - 1; i++) if (not isprint((*source)[i])) (*source)[i] = '?';
    ///TODO: get this editor to work with unicode characters, and ignore non printable characters.
    fseek(file, 0L, SEEK_SET);
    fread(*source, sizeof(char), *length - 1, file);
    if (ferror(file)) { perror("read"); restore_terminal(); exit(1); }
    fclose(file);
}

static inline bool confirmed(const char* question) {
    char response[6];
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    printf("%s", save_cursor);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    printf(set_color "%s? (yes/no): " reset_color, confirm_color, question);
    memset(response, 0, sizeof(char) * (6));
    restore_terminal();
    fgets(response, 5, stdin);
    configure_terminal();
    response[strlen(response) - 1] = '\0';
    printf("%s", restore_cursor);
    bool confirmed = not strncmp(response, "yes", 3);
    memset(response, 0, sizeof(char) * (6));
    return confirmed;
}

static inline void prompt_filename(char* filename) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    printf("%s", save_cursor);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    printf(set_color "filename: " reset_color, rename_color);
    memset(filename, 0, sizeof(char) * (4096));
    restore_terminal();
    fgets(filename, 4096, stdin);
    configure_terminal();
    filename[strlen(filename) - 1] = '\0';
    printf("%s", restore_cursor);
}

static inline void save(char* source, nat source_length, char* name, bool* saved, char* message) {
    bool prompted = false;
    if (not name or not strlen(name)) {
        prompt_filename(name);
        prompted = true;
    }
    
    if (not strlen(name)) {
        strcpy(message, "aborted save.");
        return;
    } else if (prompted and access(name, F_OK) != -1 and not confirmed("file already exists, overwrite")) {
        strcpy(name, "");
        strcpy(message, "aborted save.");
        return;
    }
    
    FILE* file = fopen(name, "w+");
    if (not file) {
        sprintf(message, "save unsuccessful: %s", strerror(errno));
        return;
    } else {
        fwrite(source, sizeof(char), source_length, file);
        memset(message, 0, 1024);
    }
    if (ferror(file)) perror("write");
    fclose(file);
    *saved = true;
}

static inline void rename_file(char* old, char* message) {
    char new[4096]; prompt_filename(new);
    if (not strlen(new)) strcpy(message, "aborted rename.");
    else if (access(new, F_OK) != -1 and not confirmed("file already exists, overwrite")) strcpy(message, "aborted rename.");
    else if (rename(old, new)) sprintf(message, "rename unsuccessful: %s", strerror(errno));
    else { strncpy(old, new, 4096); memset(message, 0, 1024); }
}

static inline void display(struct line* lines, nat line_count, struct location origin, struct winsize window, enum editor_mode mode, struct location cursor) {
    char buffer[window.ws_col * window.ws_row];
    memset(buffer, 0, sizeof buffer);
    nat b = 0;
    for (nat line = origin.line; line < fmin(origin.line + window.ws_row - 1, line_count); line++) {
        for (nat c = origin.column; c < fmin(origin.column + window.ws_col - 1, lines[line].length); c++) {
            
            const char g = lines[line].line[c];
            
            if (g == '\t') {
                for (nat i = 0; i < 8; i++) buffer[b++] = ' ';
            } else buffer[b++] = g;
        }
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

static inline struct line* generate_line_view(char* source, nat* count, nat width) {
    *count = 1;
    nat length = 0;
    bool continued = false;
    struct line* lines = calloc(1, sizeof(struct line));
    lines[0] = (struct line){source, 0, false};
    while (*source) {
        if (*source++ == '\n') { continued = false; length = 0; }
        else { length++; lines[*count - 1].length++; }
        if (width and length == width and *source != '\n') { continued = true; length = 0; }
        if (not length) {
            lines = realloc(lines, sizeof(struct line) * (*count + 1));
            lines[*count].line = source;
            lines[*count].continued = continued;
            lines[(*count)++].length = 0;
        }
    } return lines;
}

static inline void get_datetime(char buffer[16]) { // used via option, or function: datetime.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline void print_status_bar(enum editor_mode mode, char* filename, bool saved, char* message,
                                    struct location cursor, struct location screen, struct location origin, struct winsize window, nat point) {
    printf("%s", save_cursor);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    
    const long color = mode == edit_mode or mode == hard_edit_mode ? edit_status_color : command_status_color;

//    printf("c=%zd,%zd s=%zd,%zd o=%zd,%zd p=%zd | ",
//              cursor.line, cursor.column,
//              screen.line, screen.column,
//              origin.line, origin.column, point);

    printf(set_color "%s" reset_color
           set_color "%s" reset_color
           set_color "  %s" reset_color,
           color, filename,
           edited_flag_color, saved ? "" : " (e)",
           color, message);
    printf("%s", restore_cursor);
}

static inline void adjust_column_view(struct location *cursor, struct location *origin, struct location *screen, const struct winsize window) {
    if (cursor->column > window.ws_col - 1) {
        screen->column = window.ws_col - 1;
        origin->column = cursor->column - screen->column;
    } else {
        screen->column = cursor->column;
        origin->column = 0;
    }
}

static inline void adjust_line_view(struct location *cursor, struct location *origin, struct location *screen, const struct winsize window) {
    if (cursor->line > window.ws_row - 2) {
        screen->line = window.ws_row - 2;
        origin->line = cursor->line - screen->line;
    } else {
        screen->line = cursor->line;
        origin->line = 0;
    }
}

static inline void move_left(struct location *cursor, struct location *origin, struct location *screen, struct winsize window, nat* point, struct line* lines, struct location* desired, bool user) {
    if (not *point) return;
    if (cursor->column) {
        cursor->column--; (*point)--;
        if (screen->column) screen->column--; else if (origin->column) origin->column--;
    } else if (cursor->line) {
        cursor->column = lines[cursor->line - 1].length - lines[cursor->line].continued;
        cursor->line--; (*point)--;
        if (screen->line) screen->line--; else if (origin->line) origin->line--;
        adjust_column_view(cursor, origin, screen, window);
    } if (user) *desired = *cursor;
}

static inline void move_right(struct location* cursor, struct location* origin, struct location* screen, struct winsize window, nat* point, struct line* lines, nat line_count, nat length, struct location* desired, bool user) {
    if (*point >= length - 1) return;
    if (cursor->column < lines[cursor->line].length) {
        cursor->column++; (*point)++;
        if (screen->column < window.ws_col - 1) screen->column++; else origin->column++;
    } else if (cursor->line < line_count - 1) {
        cursor->line++; (*point)++;
        if (screen->line < window.ws_row - 2) screen->line++; else origin->line++;
        cursor->column = lines[cursor->line].continued;
        adjust_column_view(cursor, origin, screen, window);
    }
    if (user) *desired = *cursor;
}

static inline void move_up(struct location *cursor, struct location *origin, struct location *screen, struct winsize window, nat* point, struct line* lines, struct location* desired) {
    if (not cursor->line) {
        *screen = *cursor = *origin = (struct location){0, 0};
        *point = 0;
        return;
    }
    const nat column_target = fmin(lines[cursor->line - 1].length, desired->column);
    const nat line_target = cursor->line - 1;
    while (cursor->column > column_target or cursor->line > line_target)
        move_left(cursor, origin, screen, window, point, lines, desired, false);
    adjust_column_view(cursor, origin, screen, window);
}

static inline void move_down(struct location *cursor, struct location *origin, struct location *screen, struct winsize window, nat* point, struct line* lines, nat line_count, nat length, struct location* desired) {
    if (cursor->line >= line_count - 1) {
//        cursor->line = line_count - 1;
//        cursor->column = lines[cursor->line].length;
//        *point = length - 1;
//        adjust_column_view(cursor, &origin, &screen, &window);
//        adjust_line_view(cursor, &origin, &screen, &window);
        return;
    }
    const nat column_target = fmin(lines[cursor->line + 1].length, desired->column);
    const nat line_target = cursor->line + 1;
    while (cursor->column < column_target or cursor->line < line_target)
        move_right(cursor, origin, screen, window, point, lines, line_count, length, desired, false);
    if (lines[cursor->line].continued and not column_target) move_left(cursor, origin, screen, window, point, lines, desired, false);
}

static inline void backspace(struct location* cursor, struct location* desired, nat* length, nat* line_count, struct line** lines, struct location *origin, nat* point, struct location* screen, char** source, struct winsize window) {
    move_left(cursor, origin, screen, window, point, *lines, desired, true);
    delete((*point) + 1, source, length);
    free(*lines);
    *lines = generate_line_view(*source, line_count, wrap_width);
}

static inline void delete_forwards(struct location* cursor, struct location* desired, nat* length, nat* line_count, struct line** lines, struct location *origin, nat* point, struct location* screen, char** source, struct winsize window) {
    move_right(cursor, origin, screen, window, point, *lines, *line_count, *length, desired, true);
    delete(*point, source, length);
    move_left(cursor, origin, screen, window, point, *lines, desired, true);
    free(*lines);
    *lines = generate_line_view(*source, line_count, wrap_width);
}

static void jump(char c1, struct location *cursor, struct location *desired, bool *jump_to_line, nat length, nat line_count, struct line *lines, struct location *origin, nat *point, struct location *screen, struct winsize window) {
    const char c0 = get_character();
    if (bigraph(jump_top, c0, c1)) {
        *screen = *cursor = *origin = (struct location){0, 0};
        *point = 0;
    } else if (bigraph(jump_bottom, c0, c1)) {
//        cursor->line = line_count - 1;
//        cursor->column = lines[cursor->line].length;
//        *point = length - 1;
//        adjust_column_view(cursor, &origin, &screen, &window);
//        adjust_line_view(cursor, &origin, &screen, &window);
    } else if (bigraph(jump_begin, c0, c1)) {
        while (cursor->column)
            move_left(cursor, origin, screen, window, point, lines, desired, true);
    } else if (bigraph(jump_end, c0, c1)) {
        while (cursor->column < lines[cursor->line].length)
            move_right(cursor, origin, screen, window, point, lines, line_count, length, desired, true);
    } else if (c1 == 'n') *jump_to_line = true;
}

static inline void try_quit(char c, enum editor_mode* mode, bool saved) {
    if ((c == quit_key and saved) or c == force_quit_key and (saved or confirmed("quit without saving"))) *mode = quit;
}

int main(int argc, const char** argv) {
    
    enum editor_mode mode = command_mode;
    nat line_count = 0, length = 1, point = 0;
    bool saved = true, should_show_cursor = false, show_status = true, jump_to_line = false;
    struct location cursor = {0,0}, origin = {0,0}, screen = {0,0}, desired = {0,0};
    char* source = calloc(1, sizeof(char)), name[4096] = {0}, message[1024] = {0}, c = 0, c1 = 0, c2 = 0;
    
    printf("%s", save_screen);
    configure_terminal();
    
    open_file(argc, argv, &source, &length, name);
    struct line* lines = generate_line_view(source, &line_count, wrap_width);

    while (mode != quit) {
        
        struct winsize window;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
        
        if (should_show_cursor) printf("%s", show_cursor); else printf("%s", hide_cursor);
        display(lines, line_count, origin, window, mode, cursor);
        
        if (show_status) print_status_bar(mode, name, saved, message, cursor, screen, origin, window, point);
        printf(set_cursor, screen.line + 1, screen.column + 1);
        
        fflush(stdout);
        c = get_character();
        
        if (mode == command_mode) {
            
            if (false) {}
            
            else if (c == jump_key) jump(c, &cursor, &desired, &jump_to_line, length, line_count, lines, &origin, &point, &screen, window);
            
            else if (c == up_key) move_up(&cursor, &origin, &screen, window, &point, lines, &desired);
            else if (c == down_key) move_down(&cursor, &origin, &screen, window, &point, lines, line_count, length, &desired);
            else if (c == left_key) move_left(&cursor, &origin, &screen, window, &point, lines, &desired, true);
            else if (c == right_key) move_right(&cursor, &origin, &screen, window, &point, lines, line_count, length, &desired, true);
            
            else if (c == save_key) save(source, length, name, &saved, message);
            else if (c == rename_key) rename_file(name, message);
            else if (c == quit_key and saved or c == force_quit_key) try_quit(c, &mode, saved);
            
            else if (c == edit_key) mode = edit_mode;
            else if (c == hard_edit_key) mode = hard_edit_mode;

            else if (c == status_bar_key) show_status = not show_status;
            else if (c == cursor_key) should_show_cursor = not should_show_cursor;
            
            else if (c == help_key) strcpy(message, "qQwWeEsdaurkljio;pf.?|");
            
//            else if (c == function_key) {}
//            else if (c == paste_key) {}
//            else if (c == select_key) mode = select_mode;
//            else if (c == cut_key) { }
            
        } else if (mode == edit_mode or mode == hard_edit_mode) {
            saved = false;
            
            if ((trigraph(left_exit, c, c1, c2) or trigraph(right_exit, c, c1, c2)) and mode == edit_mode and point > 1) {
                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, window);
                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, window);
                mode = command_mode;

            } else if (bigraph(edit_exit, c, c1) and mode == edit_mode and point) {
                backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, window);
                save(source, length, name, &saved, message);
                try_quit('q', &mode, saved);
            }
            
            else if (c == 27) {
                c = get_character();
                if (c == 27) { mode = command_mode; }
                else if (c == 'd') delete_forwards(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, window);
                else if (c == '[') {
                    c = get_character();
                    if (c == 'A') move_up(&cursor, &origin, &screen, window, &point, lines, &desired);
                    else if (c == 'B') move_down(&cursor, &origin, &screen, window, &point, lines, line_count, length, &desired);
                    else if (c == 'C') move_right(&cursor, &origin, &screen, window, &point, lines, line_count, length, &desired, true);
                    else if (c == 'D') move_left(&cursor, &origin, &screen, window, &point, lines, &desired, true);
                } else {
                    sprintf(message, "error: unknown escape code: %c (%d)", c, (int) c);
                }
                
            } else if (c == 127 and point > 0) backspace(&cursor, &desired, &length, &line_count, &lines, &origin, &point, &screen, &source, window);
            else if ((c != 127 and c != 27 and (isprint(c) or c == '\n' or c == '\t')) or (c < 0)) {
                if (c < 0) strcpy(message, "found a unicode char!");
                else {
                    insert(c, point, &source, &length);
                    free(lines);
                    lines = generate_line_view(source, &line_count, wrap_width);
                    move_right(&cursor, &origin, &screen, window, &point, lines, line_count, length, &desired, true);
                }
            }
        } else mode = command_mode;
        c2 = c1;
        c1 = c;
    }
    free(source);
    restore_terminal();
    printf("%s", restore_screen);
}
