/// Simple virtual editor implementation
/// for use with libFuzzer testing, using clang.
///    written by Daniel Rehman on 2010062.220926

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

static const size_t wrap_width = 40;
static const size_t tab_width = 8;

static const size_t window_ws_col = 30;
static const size_t window_ws_row = 30;

enum key_bindings {
    up_key = '9',//'o',
    down_key = '8',//'i',
    left_key = '7',//'j',
    right_key = '0',//';',
    right_key = '0',//';',
};

typedef char* unicode;

struct location {
    size_t line;
    size_t column;
};

struct line {
    unicode* line;
    size_t length;
    bool continued;
};

static const char
    *left_exit = "wf",
    *right_exit = "oj",
    *edit_exit = "wq",
    *jump_top = "ko",
    *jump_bottom = "km",
    *jump_begin = "kj",
    *jump_end = "kl";

//static const char
//    *set_cursor = "\033[%lu;%luH",
//    *clear_screen = "\033[1;1H\033[2J",
//    *clear_line = "\033[2K",
//    *save_screen = "\033[?1049h",
//    *restore_screen = "\033[?1049l";


//
//// debug:
//void debug_lines(struct line* lines, size_t line_count) {
//    printf("[%lu lines]\n", line_count);
//    for (size_t l = 0; l < line_count; l++) {
//        struct line current = lines[l];
//        printf("LINE %3lu [%3lu][%3d] | /", l, current.length, current.continued);
//        for (size_t c = 0; c < current.length; c++) {
//            printf("%s", lines[l].line[c]);
//        }
//        printf("/\n");
//    } fflush(stdout);
//}
//

/*
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
 */
//

//
//struct termios terminal = {0};
//
//
//
//static inline void restore_terminal() {
//    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
//        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
////        dump_and_panic();
//    }
//}

//static inline void configure_terminal() {
//    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
//        perror("tcgetattr(STDIN_FILENO, &terminal)");
////        dump_and_panic();
//    }
//    atexit(restore_terminal);
//    struct termios raw = terminal;
//    raw.c_lflag &= ~(ECHO | ICANON);
//    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
//        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
////        dump_and_panic();
//    }
//}

//static inline char read_byte_from_stdin() {
//    char c = 0;
//    const ssize_t n = read(STDIN_FILENO, &c, 1);
//    if (n < 0) {
//        printf("n < 0 : ");
//        perror("read(STDIN_FILENO, &c, 1) syscall");
////        dump_and_panic();
//    } else if (n == 0) {
//        printf("n == 0 : ");
//        perror("read(STDIN_FILENO, &c, 1) syscall");
////        dump_and_panic();
//    }
//    return c;
//}

static inline bool is(unicode u, char c) { return u && !strncmp(u, &c, 1); }

static inline unicode read_unicode(char c) {
    size_t total = 0;
    unicode bytes = malloc(sizeof(char));
    bytes[total++] = c; // read_byte_from_stdin();
//    if ((c >> 3) == 30) count = 3;
//    else if ((c >> 4) == 14) count = 2;
//    else if ((c >> 5) == 6) count = 1;
//    for (size_t i = 0; i < count; i++) {
//        bytes = realloc(bytes, sizeof(char) * (total + 1));
//        bytes[total++] = read_byte_from_stdin();
//    }
    bytes = realloc(bytes, sizeof(char) * (total + 1));
    bytes[total++] = '\0';
    return bytes;
}

//static inline void display (struct location origin, struct location cursor, struct line* lines, size_t line_count) {
//    size_t screen_length = 0;
//    char screen[window_ws_col * window_ws_row + 1];
//    memset(screen, 0, sizeof screen);
//    struct location screen_cursor = {1,1};
//    for (size_t line = origin.line; line < fmin(origin.line + window_ws_row - 1, line_count); line++) {
//        size_t line_length = 0;
//        for (size_t column = origin.column; column < fmin(origin.column + window_ws_col - 1, lines[line].length); column++) {
//            unicode g = lines[line].line[column];
//            size_t space_count_for_tab = 0;
//            if (is(g, '\t')) {
//                do {
//                    screen[screen_length++] = ' ';
//                    line_length++;
//                    space_count_for_tab++;
//                } while (line_length % tab_width);
//            } else {
//                for (size_t i = 0; i < strlen(g); i++) {
//                    screen[screen_length++] = g[i];
//                    line_length++;
//                }
//            }
//            if (line < cursor.line || (line == cursor.line && column < cursor.column)) {
//                if (is(g, '\t'))
//                    screen_cursor.column += space_count_for_tab;
//                else
//                    screen_cursor.column++;
//            }
//        } if (line < cursor.line) {
//            screen_cursor.line++;
//            screen_cursor.column = 1;
//        } screen[screen_length++] = '\n';
//    }
//    printf("%s%s", clear_screen, screen);
//    printf(set_cursor, screen_cursor.line, screen_cursor.column);
//}

static inline struct line* generate_line_view(unicode* text, size_t text_length, size_t* count) {
    size_t length = 0;
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
            if (wrap_width && length >= wrap_width) {
                lines[*count - 1].continued = true;
                length = 0;
            }
        }
        if (!length) {
            lines = realloc(lines, sizeof(struct line) * (*count + 1));
            lines[*count].line = text + i + 1;
            lines[*count].continued = false;
            lines[(*count)++].length = 0;
        }
    }
    return lines;
}

static inline void insert(unicode c, size_t at, unicode** text, size_t* length) {
    if (at > *length) { abort(); }
    *text = realloc(*text, sizeof(unicode) * (*length + 1));
    memmove(*text + at + 1, *text + at, sizeof(unicode) * (*length - at));
    ++*length; (*text)[at] = c;
}

static inline void delete(size_t at, unicode** text, size_t* length) {
    if (at > *length) { abort(); }
    if (!at || !*length) return;
    free((*text)[at - 1]);
    memmove((*text) + at - 1, (*text) + at, sizeof(unicode) * (*length - at));
    *text = realloc(*text, sizeof(unicode) * (--*length));
}

static inline void move_left
 (struct location *cursor, struct location *origin, struct location *screen,
  struct line* lines, struct location* desired, bool user) {
     if (cursor->column) {
         cursor->column--;
         if (screen->column > 5) screen->column--; else if (origin->column) origin->column--; else screen->column--;
     } else if (cursor->line) {
         cursor->line--;
         cursor->column = lines[cursor->line].length - lines[cursor->line].continued;
         if (screen->line > 5) screen->line--; else if (origin->line) origin->line--; else screen->line--;
         if (cursor->column > window_ws_col - 5) {
             screen->column = window_ws_col - 5;
             origin->column = cursor->column - screen->column;
         } else {
             screen->column = cursor->column;
             origin->column = 0;
         }
     } if (user) *desired = *cursor;
}

static inline void move_right
 (struct location* cursor, struct location* origin, struct location* screen,
  struct line* lines, size_t line_count, size_t length, struct location* desired, bool user) {
     if (cursor->column < lines[cursor->line].length) {
         cursor->column++;
         if (screen->column < window_ws_col - 5) screen->column++; else origin->column++;
     } else if (cursor->line < line_count - 1) {
         cursor->column = lines[cursor->line++].continued;
         if (screen->line < window_ws_row - 3) screen->line++; else origin->line++;
         if (cursor->column > window_ws_col - 5) {
             screen->column = window_ws_col - 5;
             origin->column = cursor->column - screen->column;
         } else {
             screen->column = cursor->column;
             origin->column = 0;
         }
     } if (user) *desired = *cursor;
}

static inline void move_up
(struct location *cursor, struct location *origin, struct location *screen,
 size_t* point, struct line* lines, struct location* desired) {
    if (!cursor->line) {
        *screen = *cursor = *origin = (struct location){0, 0};
        *point = 0; return;
    }
    const size_t column_target = fmin(lines[cursor->line - 1].length, desired->column);
    const size_t line_target = cursor->line - 1;
    while (cursor->column > column_target || cursor->line > line_target) {
        move_left(cursor, origin, screen, lines, desired, false);
        (*point)--;
    }
    if (cursor->column > window_ws_col - 5) {
        screen->column = window_ws_col - 5;
        origin->column = cursor->column - screen->column;
    } else {
        screen->column = cursor->column;
        origin->column = 0;
    }
}

static inline void move_down
 (struct location *cursor, struct location *origin, struct location *screen,
  size_t* point, struct line* lines, size_t line_count, size_t length, struct location* desired) {
    if (cursor->line >= line_count - 1) {
        while (*point < length) {
            move_right(cursor, origin, screen, lines, line_count, length, desired, false);
            (*point)++;
        } return;
    }
    const size_t column_target = fmin(lines[cursor->line + 1].length, desired->column);
    const size_t line_target = cursor->line + 1;
    while ((cursor->column < column_target || cursor->line < line_target) && (*point) < length)  {
        move_right(cursor, origin, screen, lines, line_count, length, desired, false);
        (*point)++;
    }
    if (cursor->line && lines[cursor->line - 1].continued && !column_target) {
        move_left(cursor, origin, screen, lines, desired, false);
        (*point)--;
    }
}

static void jump(char c, unicode c0,
  struct location *cursor, struct location *origin, struct location *screen, struct location *desired,
  struct winsize window, size_t* point, size_t length, struct line* lines, size_t line_count) {
    unicode c1 = read_unicode();
    if (bigraph(jump_top, c0, c1)) { *screen = *cursor = *origin = (struct location){0, 0}; *point = 0; }
    else if (bigraph(jump_bottom, c0, c1)) while (*point < length - 1) move_down(cursor, origin, screen, window, point, lines, line_count, length, desired);
    else if (bigraph(jump_begin, c0, c1)) while (cursor->column) { (*point)--; move_left(cursor, origin, screen, window, lines, desired, true); }
    else if (bigraph(jump_end, c0, c1)) while (cursor->column < lines[cursor->line].length) {  (*point)++; move_right(cursor, origin, screen, window, lines, line_count, length, desired, true); }
}




void editor(const uint8_t* data, size_t size) {
    size_t i = 0;
        
    size_t length = 0, at = 0, line_count = 0;
    unicode* text = NULL, c = 0, p = 0;
    struct line* lines = generate_line_view(text, length, &line_count);
        
//    configure_terminal();
    
    struct location
        origin = {0,0},
        cursor = {0,0},
        screen = {0,0},
        desired = {0,0};
        
    while (i < size) { //!is(c, 'q')

//        puts("\n");
//        debug_lines(lines, line_count);
//        printf("at: %lu | o: [%lu,%lu] | c: [%lu,%lu] | s: [%lu,%lu] | d: [%lu,%lu] \n",
//               at,
//               origin.line, origin.column,
//               cursor.line, cursor.column,
//               screen.line, screen.column,
//               desired.line, desired.column);

//        display(origin, cursor, lines, line_count);
//        fflush(stdout);
        
        c = read_unicode(data[i++]); //data[i++]
        
//        printf("processing: %s\n", c);
        
        if (is(c, jump_key) && i < size) jump(data[i++], c, &cursor, &origin, &screen, &desired, window, &at, length, lines, line_count);
        
        else if (is(c, up_key))
            move_up(&cursor, &origin, &screen, &at, lines, &desired);
        
        else if (is(c, down_key))
            move_down(&cursor, &origin, &screen, &at, lines, line_count, length, &desired);
        
        else
            if (is(c, right_key)) {
            if (at < length) {
                at++;
                move_right(&cursor, &origin, &screen, lines, line_count, length, &desired, true);
            }
        } else if (is(c, left_key)) {
            if (at) {
                at--;
                move_left(&cursor, &origin, &screen, lines, &desired, true);
            }
        } else if (is(c, 127)) {
            if (at && length) {
                delete(at, &text, &length);
                at--;
                move_left(&cursor, &origin, &screen, lines, &desired, true);
                free(lines); lines = generate_line_view(text, length, &line_count);
            }
        } else {
                insert(c, at, &text, &length);
                at++;
                free(lines); lines = generate_line_view(text, length, &line_count);
                move_right(&cursor, &origin, &screen, lines, line_count, length, &desired, true);
        }
        
        p = c;
    }
    
    for (size_t i = 0; i < length; i++)
        free(text[i]);
    free(text);
    free(lines);

//    restore_terminal();
}

extern int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    editor(data, size);
    return 0;
}
//
//int main() {
////    const char* s = "ab\nid";
////    editor((const uint8_t*) s, strlen(s));
//    editor(NULL, 0);
//}
