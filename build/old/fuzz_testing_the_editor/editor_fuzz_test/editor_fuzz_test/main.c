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
    up_key = '9',
    down_key = '8',
    left_key = '7',
    right_key = '0',
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

//static void jump(char c, unicode c0,
//  struct location *cursor, struct location *origin, struct location *screen, struct location *desired,
//  struct winsize window, size_t* point, size_t length, struct line* lines, size_t line_count) {
//    unicode c1 = read_unicode();
//    if (bigraph(jump_top, c0, c1)) { *screen = *cursor = *origin = (struct location){0, 0}; *point = 0; }
//    else if (bigraph(jump_bottom, c0, c1)) while (*point < length - 1) move_down(cursor, origin, screen, window, point, lines, line_count, length, desired);
//    else if (bigraph(jump_begin, c0, c1)) while (cursor->column) { (*point)--; move_left(cursor, origin, screen, window, lines, desired, true); }
//    else if (bigraph(jump_end, c0, c1)) while (cursor->column < lines[cursor->line].length) {  (*point)++; move_right(cursor, origin, screen, window, lines, line_count, length, desired, true); }
//}




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
        
        display(origin, cursor, lines, line_count);
        fflush(stdout);
        
        c = read_unicode(data[i++]);
            
        
        
        if (false) {}
        
        //        if (is(c, jump_key) && i < size) jump(data[i++], c, &cursor, &origin, &screen, &desired, window, &at, length, lines, line_count);
        
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


























