///     The "wef" text editor.
///
/// Created by Daniel Rehman.
/// Created on: 2005122.113101
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

#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"


/// Parameters:
static size_t wrap_width = 60;
static size_t tab_width = 8;
static const long rename_color = 214L;
static const long confirm_color = 196L;
static const long edit_status_color = 234L;
static const long command_status_color = 239L;
static const long edited_flag_color = 130L;

typedef char* unicode;

enum editor_mode {
    quit,
    command_mode,
    edit_mode,
    hard_edit_mode,
    select_mode,
};

struct location {
    size_t line;
    size_t column;
};

struct line {
    unicode* line;
    size_t length;
    bool continued;
};

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
};

static const char
    *save_cursor = "\033[s",
    *restore_cursor = "\033[u",
//    *hide_cursor = "\033[?25l",
//    *show_cursor = "\033[?25h",
    *set_cursor = "\033[%lu;%luH",
    *clear_screen = "\033[1;1H\033[2J",
    *clear_line = "\033[2K",
    *save_screen = "\033[?1049h",
    *restore_screen = "\033[?1049l";

static const char
    *left_exit = "wf",
    *right_exit = "oj",
    *edit_exit = "wq",

    *jump_top = "ko",
    *jump_bottom = "km",
    *jump_begin = "kj",
    *jump_end = "kl";


struct termios terminal = {0};

static inline void debug_panic() {
    printf("internal error! aborting...\n");
    abort();
}

static inline void debug_error() {
    printf("internal error! aborting...\n");
    sleep(1);
}

static inline void restore_terminal() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        debug_panic();
    }
}

static inline void configure_terminal() {
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        debug_panic();
    } atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        debug_panic();
    }
}

static inline char read_byte_from_stdin() {
    char c = 0;
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n < 0) {
        printf("n < 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        debug_error();
    } else if (n == 0) {
        printf("n == 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        debug_error();
    }
    return c;
}

static inline bool is(unicode u, char c) { return u && !strncmp(u, &c, 1); }
static inline bool bigraph(const char* seq, unicode c0, unicode c1) { return is(c0, seq[1]) && is(c1, seq[0]); }

static inline unicode read_unicode() {
    unicode bytes = 0; size_t total = 0, count = 0;
    unsigned char c = read_byte_from_stdin();
    bytes = realloc(bytes, sizeof(char) * (total + 1));
    bytes[total++] = c;
    if ((c >> 3) == 30) count = 3;
    else if ((c >> 4) == 14) count = 2;
    else if ((c >> 5) == 6) count = 1;
    for (size_t i = 0; i < count; i++) {
        bytes = realloc(bytes, sizeof(char) * (total + 1));
        bytes[total++] = read_byte_from_stdin();
    }
    bytes = realloc(bytes, sizeof(char) * (total + 1));
    bytes[total++] = '\0';
    return bytes;
}

static inline void insert(unicode c, size_t at, unicode** text, size_t* length) {
    if (at > *length) { debug_error(); return; }
    *text = realloc(*text, sizeof(unicode) * (*length + 1));
    memmove(*text + at + 1, *text + at, sizeof(unicode) * (*length - at));
    ++*length; (*text)[at] = c;
}

static inline void delete(size_t at, unicode** text, size_t* length) {
    if (at > *length) { debug_error(); return; }
    if (!at || !*length) return;
    memmove((*text) + at - 1, (*text) + at, sizeof(unicode) * (*length - at));
    *text = realloc(*text, sizeof(unicode) * (--*length));
}

static inline void print_status_bar
(enum editor_mode mode, bool saved,
 char* message, struct winsize window,
 unicode* filename, size_t filename_length) {
    
    printf(set_cursor, window.ws_row, 1);
    printf("%s", clear_line);
    const long color = mode == edit_mode || mode == hard_edit_mode ? edit_status_color : command_status_color;
    printf(set_color, color); for (size_t i = 0; i < filename_length; i++) printf("%s", filename[i]); printf(reset_color);
    printf(set_color "%s" reset_color, edited_flag_color, saved ? "" : " *");
    printf(set_color "  %s" reset_color, color, message);
}

static inline void display
 (enum editor_mode mode, struct location origin, struct location cursor, struct winsize window,
  struct line* lines, size_t line_count, char* message, bool saved, bool show_status,
  unicode* filename, size_t filename_length
  ) {
    size_t text_length = 0;
    char screen[window.ws_col * window.ws_row];
    memset(screen, 0, sizeof screen);
    struct location screen_cursor = {1,1};
    for (size_t line = origin.line; line < fmin(origin.line + window.ws_row - 1, line_count); line++) {
        for (size_t column = origin.column; column < fmin(origin.column + window.ws_col - 1, lines[line].length); column++) {
            unicode g = lines[line].line[column];
            if (line < cursor.line || (line == cursor.line && column < cursor.column)) {
                if (is(g, '\t')) screen_cursor.column += tab_width;
                else screen_cursor.column++;
            }
            if (is(g, '\t')) for (size_t i = 0; i < tab_width; i++) screen[text_length++] = ' ';
            else for (size_t i = 0; i < strlen(g); i++) screen[text_length++] = g[i];
        } if (line < cursor.line) {
            screen_cursor.line++;
            screen_cursor.column = 1;
        } screen[text_length++] = '\n';
    }
    printf("%s%s", clear_screen, screen);
     if (show_status) print_status_bar(mode, saved, message, window, filename, filename_length);
    printf(set_cursor, screen_cursor.line, screen_cursor.column);
}

static inline struct line* generate_line_view(unicode* text, size_t text_length, size_t* count) {
    size_t length = 0; bool continued = false;
    struct line* lines = malloc(sizeof(struct line));
    lines[0].line = text;
    lines[0].continued = 0;
    lines[0].length = 0;
    *count = 1;
    for (size_t i = 0; i < text_length; i++) {
        if (is(text[i], '\n')) {
            lines[*count - 1].continued = false;
            length = 0;
        } else {
            if (is(text[i], '\t')) length += tab_width; else length++;
            lines[*count - 1].length++;
        }
        if (wrap_width && length >= wrap_width && !is(text[i],'\n')) {
            lines[*count - 1].continued = true;
            length = 0;
        }
        if (!length) {
            lines = realloc(lines, sizeof(struct line) * (*count + 1));
            lines[*count].line = text + i + 1;
            lines[*count].continued = continued;
            lines[(*count)++].length = 0;
        }
    } return lines;
}

void debug_lines(struct line* lines, size_t line_count) {
    printf("%s", clear_screen);
    printf("[%lu lines]\n", line_count);
    for (size_t l = 0; l < line_count; l++) {
        struct line current = lines[l];
        printf("LINE %3lu [%3lu][%3d] | ", l, current.length, current.continued);
        for (size_t c = 0; c < current.length; c++) {
            printf("%s", lines[l].line[c]);
        }
        printf("\n");
    }
    fflush(stdout);
}

static inline void move_left
 (struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
  struct line* lines, struct location* desired, bool user) {
    if (cursor->column) {
        cursor->column--;
        if (screen->column) screen->column--; else if (origin->column) origin->column--;
    } else if (cursor->line) {
        cursor->line--;
        cursor->column = lines[cursor->line].length - lines[cursor->line].continued;
        if (screen->line) screen->line--; else if (origin->line) origin->line--;
        if (cursor->column > window.ws_col - 1) {
            screen->column = window.ws_col - 1;
            origin->column = cursor->column - screen->column;
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
    } if (user) *desired = *cursor;
}

static inline void move_right
 (struct location* cursor, struct location* origin, struct location* screen, struct winsize window,
  struct line* lines, size_t line_count, size_t length, struct location* desired, bool user) {
    if (cursor->column < lines[cursor->line].length) {
        cursor->column++;
        if (screen->column < window.ws_col - 1) screen->column++; else origin->column++;
    } else if (cursor->line < line_count - 1) {
        cursor->column = lines[cursor->line++].continued;
        if (screen->line < window.ws_row - 2) screen->line++; else origin->line++;
        if (cursor->column > window.ws_col - 1) {
            screen->column = window.ws_col - 1;
            origin->column = cursor->column - screen->column;
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
    } if (user) *desired = *cursor;
}

static inline void move_up
(struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
 size_t* point, struct line* lines, struct location* desired) {
    if (!cursor->line) {
        *screen = *cursor = *origin = (struct location){0, 0};
        *point = 0; return;
    }
    const size_t column_target = fmin(lines[cursor->line - 1].length, desired->column);
    const size_t line_target = cursor->line - 1;
    while (cursor->column > column_target || cursor->line > line_target) {
        move_left(cursor, origin, screen, window, lines, desired, false);
        (*point)--;
    }
    if (cursor->column > window.ws_col - 1) {
        screen->column = window.ws_col - 1;
        origin->column = cursor->column - screen->column;
    } else {
        screen->column = cursor->column;
        origin->column = 0;
    }
}

static inline void move_down
 (struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
  size_t* point, struct line* lines, size_t line_count, size_t length, struct location* desired) {
    if (cursor->line >= line_count - 1) {
        while (*point < length) {
            move_right(cursor, origin, screen, window, lines, line_count, length, desired, false);
            (*point)++;
        } return;
    }
    const size_t column_target = fmin(lines[cursor->line + 1].length, desired->column);
    const size_t line_target = cursor->line + 1;
    while (cursor->column < column_target || cursor->line < line_target) {
        move_right(cursor, origin, screen, window, lines, line_count, length, desired, false);
        (*point)++;
    }
    if (cursor->line && lines[cursor->line - 1].continued && !column_target) {
        move_left(cursor, origin, screen, window, lines, desired, false);
        (*point)--;
    }
}

static void jump(unicode c0,
  struct location *cursor, struct location *origin, struct location *screen, struct location *desired,
  struct winsize window, size_t* point, size_t length, struct line* lines, size_t line_count) {
    unicode c1 = read_unicode();
    if (bigraph(jump_top, c0, c1)) { *screen = *cursor = *origin = (struct location){0, 0}; *point = 0; }
    else if (bigraph(jump_bottom, c0, c1)) while (*point < length - 1) move_down(cursor, origin, screen, window, point, lines, line_count, length, desired);
    else if (bigraph(jump_begin, c0, c1)) while (cursor->column) { (*point)--; move_left(cursor, origin, screen, window, lines, desired, true); }
    else if (bigraph(jump_end, c0, c1)) while (cursor->column < lines[cursor->line].length) {  (*point)++; move_right(cursor, origin, screen, window, lines, line_count, length, desired, true); }
}

static inline void convert_string_to_utf8_string(unicode** dest, size_t* length, const char* source) {
    for (size_t i = 0; i < strlen(source); ) {
        unsigned char c = source[i++];
        size_t u_length = 1;
        unicode u = malloc(sizeof(char)); u[0] = c;
        size_t count = 0;
        if ((c >> 3) == 30) count = 3;
        else if ((c >> 4) == 14) count = 2;
        else if ((c >> 5) == 6) count = 1;
        for (size_t i = 0; i < count; i++) {
            u = realloc(u, sizeof(char) * (u_length + 1));
            u[u_length++] = source[i++];
        }
        u = realloc(u, sizeof(char) * (u_length + 1));
        u[u_length++] = 0;
        *dest = realloc(*dest, sizeof(unicode) * (*length + 1));
        (*dest)[(*length)++] = u;
    }
}

static inline void open_file(const char* filename, unicode** text, size_t* length) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        exit(1);
    }
    int c = 0;
    while ((c = fgetc(file)) != EOF) {
        size_t u_length = 1;
        unicode u = malloc(sizeof(char)); u[0] = c;
        size_t count = 0;
        unsigned char cc = c;
        if ((cc >> 3) == 30) count = 3;
        else if ((c >> 4) == 14) count = 2;
        else if ((cc >> 5) == 6) count = 1;
        for (size_t i = 0; i < count; i++) {
            u = realloc(u, sizeof(char) * (u_length + 1));
            u[u_length++] = fgetc(file);
        }
        u = realloc(u, sizeof(char) * (u_length + 1));
        u[u_length++] = 0;
        *text = realloc(*text, sizeof(unicode) * (*length + 1));
        (*text)[(*length)++] = u;
    }
    fclose(file);
}

static inline bool confirmed(const char* question) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    printf(set_cursor, window.ws_row, 1);
    printf("%s", clear_line);
    printf(set_color "%s? (yes/no): " reset_color, confirm_color, question);
    char response[8] = {0};
    restore_terminal();
    fgets(response, 8, stdin);
    configure_terminal();
    return !strncmp(response, "yes", 3);;
}

static inline void prompt_filename(unicode* filename, size_t filename_length) {
//    struct winsize window;
//    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
//    printf(set_cursor, window.ws_row, 1);
//    printf("%s", clear_line);
//    printf(set_color "filename: " reset_color, rename_color);
////    memset(filename, 0, sizeof(char) * (4096));
//
//    restore_terminal();
////    fgets(filename, 4096, stdin);
//    /// Call c = read_unicode() here.
//    configure_terminal();
//
//    filename[strlen(filename) - 1] = '\0';
}

static inline void save(unicode* source, size_t source_length, unicode* filename, size_t filename_length, bool* saved, char* message) {
//    bool prompted = false;
//    if (not name or not strlen(name)) {
//        prompt_filename(name);
//        prompted = true;
//    }
//
//    if (not strlen(name)) {
//        strcpy(message, "aborted save.");
//        return;
//    } else if (prompted and access(name, F_OK) != -1 and not confirmed("file already exists, overwrite")) {
//        strcpy(name, "");
//        strcpy(message, "aborted save.");
//        return;
//    }
//
//    FILE* file = fopen(name, "w+");
//    if (not file) {
//        sprintf(message, "save unsuccessful: %s", strerror(errno));
//        return;
//    } else {
//        fwrite(source, sizeof(char), source_length - 1, file); // dont write the null character.
//        memset(message, 0, 1024);
//    }
//    if (ferror(file)) perror("write");
//    fclose(file);
//    *saved = true;
}

static inline void rename_file(unicode* old_filename, size_t old_filename_length, char* message) {
//    char new[4096]; prompt_filename(new);
//    if (not strlen(new)) strcpy(message, "aborted rename.");
//    else if (access(new, F_OK) != -1 and not confirmed("file already exists, overwrite")) strcpy(message, "aborted rename.");
//    else if (rename(old, new)) sprintf(message, "rename unsuccessful: %s", strerror(errno));
//    else { strncpy(old, new, 4096); memset(message, 0, 1024); }
}

int main(const int argc, const char** argv) {
    
    enum editor_mode mode = command_mode;
    bool saved = true, show_status = true;
    char message[2048] = {0};
    size_t length = 0, at = 0, line_count = 0, filename_length = 0;
    unicode* text = NULL, * filename = NULL, c = 0, p = 0;
    struct winsize window;
    struct location origin = {0,0}, cursor = {0,0}, screen = {0,0}, desired = {0,0};
            
    if (argc != 1) {
        open_file(argv[1], &text, &length);
        convert_string_to_utf8_string(&filename, &filename_length, argv[1]);
    }
    
    printf("%s", save_screen);
    configure_terminal();
    struct line* lines = generate_line_view(text, length, &line_count);
    
    while (mode != quit) {
        
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
        display(mode, origin, cursor, window, lines, line_count, message, saved, show_status, filename, filename_length);
        fflush(stdout);
        c = read_unicode();
        
        if (mode == command_mode) {
            if (is(c, edit_key)) mode = edit_mode;
            else if (is(c, hard_edit_key)) mode = hard_edit_mode;
            else if (is(c, select_key)) mode = select_mode;
            else if (is(c, status_bar_key)) show_status = !show_status;
    
            else if (is(c, up_key)) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
            else if (is(c, down_key)) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
            else if (is(c, right_key) && at < length) { at++; move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true); }
            else if (is(c, left_key) && at) { at--; move_left(&cursor, &origin, &screen, window, lines, &desired, true); }
            else if (is(c, jump_key)) jump(c, &cursor, &origin, &screen, &desired, window, &at, length, lines, line_count);
            
            else if (is(c, save_key)) save(text, length, filename, filename_length, &saved, message);
            else if (is(c, rename_key)) rename_file(filename, filename_length, message);
            else if (is(c, quit_key) || is(c, force_quit_key)) { if (saved || (is(c, force_quit_key) && confirmed("quit without saving"))) mode = quit; }
            
            else if (is(c, option_key)) {
                sprintf(message, "error: option key not implemented.");
            }
            
            else { sprintf(message, "error: unknown command %s", c); }
        } else if (mode == select_mode) {
            sprintf(message, "error: select mode not implemented.");
            mode = command_mode;
        } else {
            if (mode == edit_mode && (bigraph(left_exit, c, p) ||
                bigraph(right_exit, c, p) ||
                bigraph(edit_exit, c, p))) {
                delete(at--, &text, &length);
                move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                free(lines);
                lines = generate_line_view(text, length, &line_count);
                mode = command_mode;
                if (bigraph(edit_exit, c, p)) { mode = quit; } /// THEN QUIT.
            
            } else if (is(c, 127)) {
                if (at && length) {
                    delete(at--, &text, &length);
                    move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                    free(lines);
                    lines = generate_line_view(text, length, &line_count);
                    saved = false;
                }
            } else if (is(c, 27)) {
                unicode c = read_unicode();
                if (is(c, '[')) {
                    unicode c = read_unicode();
                         if (is(c, 'A')) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
                    else if (is(c, 'B')) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
                    else if (is(c, 'C') && at < length) { at++; move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true); }
                    else if (is(c, 'D') && at) { at--; move_left(&cursor, &origin, &screen, window, lines, &desired, true); }
                } else if (is(c, 27)) mode = command_mode;
                
            } else {
                insert(c, at++, &text, &length);
                free(lines);
                lines = generate_line_view(text, length, &line_count);
                move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true);
                saved = false;
            }
        }
        p = c;
    }
    
    restore_terminal();
    printf("%s", restore_screen);
}
