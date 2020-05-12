//
//  main.c
//  editor
//
//  Created by Daniel Rehman on 2004245.
//  Copyright © 2020 Daniel Rehman. All rights reserved.
//
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
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

struct termios terminal = {0};

static const size_t wrap_width = 100;
static const size_t tab_width = 4;

static inline void debug_panic() {
    printf("internal error! aborting...\n");
    abort();
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
        debug_panic();
    } else if (n == 0) {
        printf("n == 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        debug_panic();
    }
    return c;
}

static inline bool is(unicode u, char c) {
    return u && !strncmp(u, &c, 1);
}

static inline unicode read_unicode() {
    unicode bytes = NULL;
    size_t byte_count = 0;
    char c;
    do {
        c = read_byte_from_stdin();
        bytes = realloc(bytes, sizeof(char) * (byte_count + 1));
        bytes[byte_count++] = c;
    } while (c > 127);
    bytes = realloc(bytes, sizeof(char) * (byte_count + 1));
    bytes[byte_count++] = '\0';
    return bytes;
}

void insert(unicode c, size_t at, unicode** text, size_t* length) {
    if (at > *length) debug_panic();
    *text = realloc(*text, sizeof(unicode) * (*length + 1));
    memmove(*text + at + 1, *text + at, sizeof(unicode) * (*length - at));
    ++*length; (*text)[at] = c;
}

static inline void delete(size_t at, unicode** text, size_t* length) {
    if (at > *length) debug_panic();
    if (!at || !*length) return;
    memmove((*text) + at - 1, (*text) + at, sizeof(unicode) * (*length - at));
    *text = realloc(*text, sizeof(unicode) * (--*length));
}


static inline void display(struct line* lines, size_t line_count, struct location origin, struct location cursor, struct winsize window) {
    
    size_t text_length = 0;
    char screen_text[window.ws_col * window.ws_row];
    memset(screen_text, 0, sizeof screen_text);
    struct location pointer_loc = {1,1};
    
    for (size_t line = origin.line; line < fmin(origin.line + window.ws_row - 1, line_count); line++) {
        for (size_t column = origin.column; column < fmin(origin.column + window.ws_col - 1, lines[line].length); column++) {
            unicode g = lines[line].line[column];
            if (line < cursor.line || (line == cursor.line && column < cursor.column)) {
                if (is(g, '\t')) { pointer_loc.column += tab_width; }
                else pointer_loc.column++;
            }
            if (is(g, '\t')) for (size_t i = 0; i < tab_width; i++) screen_text[text_length++] = ' ';
            else for (size_t i = 0; i < strlen(g); i++) screen_text[text_length++] = g[i];
        }
        if (line < cursor.line) { pointer_loc.line++; pointer_loc.column = 1; }
        screen_text[text_length++] = '\n';
    }
    printf("%s", clear_screen);
    printf("%s", screen_text);
    printf(set_cursor, pointer_loc.line, pointer_loc.column);
}



//printf("%s", before);





//        if (0 && line < cursor.line)

//            before[before_count++] = '\n';

//        else








static inline struct line* generate_line_view(unicode* text, size_t text_length, size_t* count) {
    *count = 0;
    struct line* lines = NULL;
    
    size_t length = 0;
    bool continued = false;
    
    if (!length) {
        lines = realloc(lines, sizeof(struct line) * (*count + 1));
        lines[*count].line = text + 0;
        lines[*count].continued = continued;
        lines[(*count)++].length = 0;
    }
    
    for (size_t i = 0; i < text_length; i++) {
        
        
    
        if (is(text[i], '\n')) {
            lines[*count - 1].continued = false;
            length = 0;
        } else if (is(text[i], '\t')) {
            length += tab_width;
            lines[*count - 1].length++;
        } else {
            length++;
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
    }
    return lines;
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
 (struct location *cursor,
  struct location *origin,
  struct location *screen,
  struct winsize window,
  struct line* lines,
  struct location* desired,
  bool user) {
    if (cursor->column) {
        cursor->column--;
        if (screen->column) screen->column--;
        else if (origin->column) origin->column--;
    } else if (cursor->line) {
        cursor->column = lines[cursor->line - 1].length - lines[cursor->line].continued;
        cursor->line--;
        if (screen->line) screen->line--;
        else if (origin->line) origin->line--;
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
 (struct location* cursor,
  struct location* origin,
  struct location* screen,
  struct winsize window,
  struct line* lines,
  size_t line_count,
  size_t length,
  struct location* desired,
  bool user) {
    if (cursor->column < lines[cursor->line].length) {
        cursor->column++;
        if (screen->column < window.ws_col - 1) screen->column++;
        else origin->column++;
    } else if (cursor->line < line_count - 1) {
        cursor->line++;
        if (screen->line < window.ws_row - 2) screen->line++;
        else origin->line++;
        cursor->column = lines[cursor->line].continued;
        if (cursor->column > window.ws_col - 1) {
            screen->column = window.ws_col - 1;
            origin->column = cursor->column - screen->column;
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
    }
    if (user) *desired = *cursor;
}


static inline void move_up
(struct location *cursor,
 struct location *origin,
 struct location *screen,
 struct winsize window,
 size_t* point,
 struct line* lines,
 struct location* desired) {
    
    if (!cursor->line) {
        *screen = *cursor = *origin = (struct location){0, 0};
        *point = 0;
        return;
    }
    
    const size_t column_target = fmin(lines[cursor->line - 1].length, desired->column);
    const size_t line_target = cursor->line - 1;
    while (cursor->column > column_target || cursor->line > line_target) {
        move_left(cursor, origin, screen, window, lines, desired, false);
        (*point)++;
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
 (struct location *cursor,
  struct location *origin,
  struct location *screen,
  struct winsize window,
  size_t* point,
  struct line* lines,
  size_t line_count,
  size_t length,
  struct location* desired) {
     
    if (cursor->line >= line_count - 1) {
        while (*point < length - 1) {
            move_right(cursor, origin, screen, window, lines, line_count, length, desired, false);
            (*point)++;
        }
        return;
    }
     
    const size_t column_target = fmin(lines[cursor->line + 1].length, desired->column);
    const size_t line_target = cursor->line + 1;
    
    while (cursor->column < column_target || cursor->line < line_target) {
        move_right(cursor, origin, screen, window, lines, line_count, length, desired, false);
        (*point)++;
    }
    
    if (lines[cursor->line].continued && !column_target) {
        move_left(cursor, origin, screen, window, lines, desired, false);
        (*point)--;
    }
}




int main(const int argc, const char** argv) {
        
    printf("%s", save_screen);
    configure_terminal();
    
    size_t length = 0, at = 0, line_count = 0;
    unicode *text = 0, c = 0, p = 0;
    
    struct location
        origin = {0,0},
        cursor = {0,0},
        screen = {0,0},
        desired = {0,0};
    
    struct winsize window;
    struct line* lines = generate_line_view(text, length, &line_count);
    
    while (!is(c, 'q')) {
        
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
        display(lines, line_count, origin, cursor, window);
//        debug_lines(lines, line_count);
        fflush(stdout);
        c = read_unicode();
        
        if (is(c, 127)) {
            if (at && length) {
                delete(at--, &text, &length);
                move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                free(lines);
                lines = generate_line_view(text, length, &line_count);
            }
        } else if (is(c, 27)) {
            c = read_unicode();
            if (is(c, '[')) {
                c = read_unicode();
                if (is(c, 'A')) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
                else if (is(c, 'B')) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
                if (is(c, 'C') && at < length) {
                    move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true);
                    at++;
                }
                else if (is(c, 'D') && at) {
                    move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                    at--;
                }
            }
        } else {
            insert(c, at++, &text, &length);
            free(lines);
            lines = generate_line_view(text, length, &line_count);
            move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true);
        }
        p = c;
    }
        
    restore_terminal();
    printf("%s", restore_screen);
}














//static inline size_t count_tabs(struct location cursor, struct location origin, struct line* lines) {
//    size_t count = 0;
//    for (size_t i = origin.column; i < cursor.column; i++)
//        if (is(lines[cursor.line].line[i], '\t')) count++;
//    return count;
//}


        
//       + (tab_width - 1) * count_tabs(cursor, origin, lines)

