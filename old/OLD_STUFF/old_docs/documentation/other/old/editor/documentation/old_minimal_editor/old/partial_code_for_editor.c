///
///        My Text Editor.
///
///   Created by: Daniel Rehman
///   Created on: 2005122.113101
///  Modified on: 2011157
///
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

struct location {
    i32 line;
    i32 column;
};

struct screen_location {
    i16 line;
    i16 column;
};

struct window {
    char* screen;
    i16 columns;
    i16 rows;
    i32 _padding;
};

struct logical_line {
    u8* line;
    i32 capacity;
    i32 length;
};

struct render_line {
    u8* line;
    i32 capacity;
    i32 length;
    i32 visual_length;
    i32 continued;
};

struct logical_lines {
    struct logical_line* lines;
    i32 count;
    i32 capacity;
};

struct render_lines {
    struct render_line* lines;
    i32 count;
    i32 capacity;
};

struct file {
    struct location begin;
    struct location cursor;
    struct location render_cursor;
    struct location visual_cursor;
    struct location visual_origin;
    struct logical_lines logical;
    struct render_lines render;
    struct screen_location visual_screen;
    i32 visual_desired;
    
    char filename[4096];
};

static struct window window = {0};
static struct termios terminal = {0};

static inline i32 min(i32 a, i32 b) {
    return a < b ? a : b;
}

static inline i8 file_exists(const char* filename) {
    return access(filename, F_OK) != -1;
}

static inline void restore_terminal() {
    
    write(1, "\033[?1049l", 8);
    write(1, "\033[?1000l", 8);
    
    if (tcsetattr(0, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, terminal))");
        abort();
    }
}

static inline void configure_terminal() {
    
    write(1, "\033[?1049h", 8);
    write(1, "\033[?1000h", 8);
    
    if (tcgetattr(0, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        abort();
    }
    
    atexit(restore_terminal);
            
    struct termios raw = terminal;
    raw.c_iflag &= ~((unsigned long)BRKINT | (unsigned long)ICRNL | (unsigned long)INPCK | (unsigned long)IXON);
    raw.c_oflag &= ~((unsigned long)OPOST);
    raw.c_lflag &= ~((unsigned long)ECHO | (unsigned long)ICANON | (unsigned long)IEXTEN);
        
    if (tcsetattr(0, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        abort();
    }
}

// static inline void render_line() {
        
//     struct file* file = buffers[active];
    
//     i32 line = file->render_cursor.line;
//     file->render.lines[line].length = file->render_cursor.column;
//     file->render.lines[line].visual_length = file->visual_cursor.column;
    
//     for (i32 column = file->cursor.column; column < file->logical.lines[file->cursor.line].length; column++) {
//         u8 c = file->logical.lines[file->cursor.line].line[column];
        
//         if (file->options.wrap_width and file->render.lines[line].visual_length == file->options.wrap_width) {
            
//             line++;
//             if (line >= file->render.count or
//                 not file->render.lines[line].continued) {

//                 if (file->render.count + 1 >= file->render.capacity)
//                     file->render.lines =
//                     realloc(file->render.lines,
//                             sizeof(struct render_line) *
//                             (size_t)(file->render.capacity = 2 * (file->render.capacity + 1)));

//                 memmove(file->render.lines + line + 1,
//                         file->render.lines + line,
//                         sizeof(struct render_line) *
//                         (size_t)(file->render.count - line));
//                 file->render.lines[line] = (struct render_line){.continued = 1};
//                 file->render.count++;
//             } else {
//                 file->render.lines[line].length = 0;
//                 file->render.lines[line].visual_length = 0;
//             }
//         }
        
//         if (c == '\t') {
    
//             i32 at = file->render.lines[line].visual_length;
//             i8 count = 0;
//             do {
//                 if (file->options.wrap_width and at >= file->options.wrap_width) break;
//                 at++; count++;
//             } while (at % file->options.tab_width);
//             if (file->render.lines[line].length + count >= file->render.lines[line].capacity)
//                 file->render.lines[line].line =
//                 realloc(file->render.lines[line].line,
//                         (size_t)(file->render.lines[line].capacity =
//                         2 * (file->render.lines[line].capacity + count)));
//             file->render.lines[line].line[file->render.lines[line].length++] = '\t';
//             file->render.lines[line].visual_length++;
//             for (i8 i = 1; i < count; i++) {
//                 file->render.lines[line].line[file->render.lines[line].length++] = '\n';
//                 file->render.lines[line].visual_length++;
//             }
            
//         } else {
//             if (file->render.lines[line].length + 1 >= file->render.lines[line].capacity)
//                 file->render.lines[line].line = realloc(file->render.lines[line].line,
//                                                         (size_t)(file->render.lines[line].capacity =
//                                                         2 * (file->render.lines[line].capacity + 1)));
//             file->render.lines[line].line[file->render.lines[line].length++] = c;
//             if ((c >> 6) != 2) file->render.lines[line].visual_length++;
//         }
//     }

//     line++;
//     i32 delete_to = line;
//     while (delete_to < file->render.count and file->render.lines[delete_to].continued) delete_to++;

//     memmove(file->render.lines + line, file->render.lines + delete_to,
//             sizeof(struct render_line) * (size_t)(file->render.count - delete_to));
//     file->render.count -= (delete_to - line);
// }

static inline void resize_window() {
            
    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    
    if (win.ws_col != window.columns or win.ws_row != window.rows) {
        window.columns = (i16) win.ws_col;
        window.rows = (i16) win.ws_row;
        window.screen = realloc(window.screen, (size_t)(window.columns * window.rows * 4));
        
        // struct file* file = buffers[active];
        
        // if (file->visual_cursor.line >= window.rows - 1 - (file->options.flags & show_status)) {
        //     file->visual_screen.line = window.rows - 1 - (file->options.flags & show_status);
        //     file->visual_origin.line = file->visual_cursor.line - file->visual_screen.line;
        // } else {
        //     file->visual_screen.line = (i16) file->visual_cursor.line;
        //     file->visual_origin.line = 0;
        // }
        
        // if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset) {
        //     file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset;
        //     file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
        // } else {
        //     file->visual_screen.column = (i16) file->visual_cursor.column;
        //     file->visual_origin.column = 0;
        // }
    }
}


// static inline void display() {
    
//     struct file* file = buffers[active];
    
//     i32 length = 9;
//     memcpy(window.screen,"\033[?25l\033[H", 9);
    
//     double f = floor(log10((double) file->logical.count));
//     file->line_number_width = (i8) f + 1;
//     file->line_number_cursor_offset = (file->options.flags & show_line_numbers) ? file->line_number_width + 2 : 0;

//     i32 line_number = 0;
//     for (i32 i = 0; i < file->visual_origin.line; i++)
//         if (not file->render.lines[i].continued)
//             line_number++;
    
//     for (i16 screen_line = 0; screen_line < window.rows - (file->options.flags & show_status); screen_line++) {
        
//         i32 line = (i32)screen_line + file->visual_origin.line;
        
//         if (line < file->render.count) {
        
//             if (file->options.flags & show_line_numbers) {
//                 if (not file->render.lines[line].continued) length += sprintf(window.screen + length, "\033[38;5;59m%*d\033[0m  ", file->line_number_width, ++line_number);
//                 else length += sprintf(window.screen + length, "%*s  " , file->line_number_width, " ");
//             }
            
//             i32 range = 0;
//             for (i32 column = 0, visual_column = 0; column < file->render.lines[line].length; column++) {
//                 u8 c = file->render.lines[line].line[column];
//                 if (visual_column >= file->visual_origin.column and
//                     visual_column < file->visual_origin.column + window.columns - file->line_number_cursor_offset) {
                    
//                     if (range < file->render.lines[line].coloring.count) {
//                         if (column == file->render.lines[line].coloring.ranges[range].start) {
//                             length += sprintf(window.screen + length, "\033[38;5;%hdm", file->render.lines[line].coloring.ranges[range].color);
//                         }
                            
//                         if (column == file->render.lines[line].coloring.ranges[range].end) {
//                             window.screen[length++] = '\033';
//                             window.screen[length++] = '[';
//                             window.screen[length++] = 'm';
//                         }
//                         if (column >= file->render.lines[line].coloring.ranges[range].end) range++;
//                     }
                    
//                     if (c == '\t' or c == '\n') window.screen[length++] = ' ';
//                     else window.screen[length++] = (char) c;
//                 }
//                 if ((c >> 6) != 2) visual_column++;
//             }
//         }
        
//         window.screen[length++] = '\033';
//         window.screen[length++] = '[';
//         window.screen[length++] = 'K';
//         if (screen_line < window.rows - 1) {
//             window.screen[length++] = '\r';
//             window.screen[length++] = '\n';
//         }
//     }
    
//     // if (file->options.flags & show_status) {
//     //     length += sprintf(window.screen + length, "\033[7m\033[38;5;246m");
//     //     char datetime[16] = {0};
//     //     get_datetime(datetime);
        
//     //     i32 status_length = sprintf
//     //     (window.screen + length, " %d %d %d %s %s %c  %s",
//     //      file->mode,
//     //      file->cursor.line + 1, file->cursor.column + 1,
//     //      datetime,
//     //      file->filename,
//     //      file->saved ? 's' : 'e',
//     //      file->message);
//     //     length += status_length;
        
//     //     for (i32 i = status_length; i < window.columns; i++)
//     //         window.screen[length++] = ' ';
        
//     //     window.screen[length++] = '\033';
//     //     window.screen[length++] = '[';
//     //     window.screen[length++] = 'm';
//     // }
    
//     length += sprintf(window.screen + length, "\033[%hd;%hdH",
//                       (i16) (file->visual_screen.line + 1),
//                       (i16) (file->visual_screen.column + 1
//                              + file->line_number_cursor_offset));
            
//     window.screen[length++] = '\033';
//     window.screen[length++] = '[';
//     window.screen[length++] = '?';
//     window.screen[length++] = '2';
//     window.screen[length++] = '5';
//     window.screen[length++] = 'h';
    
//     write(1, window.screen, (size_t) length);
// }

// static inline void move_left(i8 change_desired) {

//     struct file* file = buffers[active];
    
//     if (not file->cursor.column) {
//         if (not file->cursor.line) return;
        
//         file->cursor.column = file->logical.lines[--file->cursor.line].length;
//         file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
//         file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
                
//         if (file->visual_screen.line > file->options.negative_line_view_shift_margin) file->visual_screen.line--;
//         else if (file->visual_origin.line) file->visual_origin.line--;
//         else file->visual_screen.line--;
        
//         if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset) {
//             file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset;
//             file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
//         } else {
//             file->visual_screen.column = (i16) file->visual_cursor.column;
//             file->visual_origin.column = 0;
//         }
        
//         if (change_desired) file->visual_desired = file->visual_cursor.column;
//         return;
//     }

//     struct logical_line* line = file->logical.lines + file->cursor.line;
    
//     while (file->cursor.column) {
//         if ((line->line[--file->cursor.column] >> 6) != 2) break;
//     }
    
//     if (not file->visual_cursor.column) {
//         if (file->visual_cursor.line) {
        
//             file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
//             file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
            
//             if (file->visual_screen.line > file->options.negative_line_view_shift_margin) file->visual_screen.line--;
//             else if (file->visual_origin.line) file->visual_origin.line--;
//             else file->visual_screen.line--;
            
//             if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset) {
//                 file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset;
//                 file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
//             } else {
//                 file->visual_screen.column = (i16) file->visual_cursor.column;
//                 file->visual_origin.column = 0;
//             }
//         }
//     }
    
//     struct render_line* render_line = file->render.lines + file->render_cursor.line;
    
//     while (file->render_cursor.column) {
//         u8 c = render_line->line[--file->render_cursor.column];
        
//         if ((c >> 6) != 2) {
//             file->visual_cursor.column--;
//             if (file->visual_screen.column > file->options.negative_view_shift_margin) file->visual_screen.column--;
//             else if (file->visual_origin.column) file->visual_origin.column--;
//             else file->visual_screen.column--;
//         }
//         if ((c >> 6) != 2 and c != 10) break;
//     }
//     if (change_desired) file->visual_desired = file->visual_cursor.column;
// }

// static inline void move_right(i8 change_desired) {
    
//     struct file* file = buffers[active];
    
//     if (file->cursor.line >= file->logical.count) return;
    
//     if (file->cursor.line + 1 < file->logical.count and
//         file->cursor.column == file->logical.lines[file->cursor.line].length) {
//         file->cursor.column = 0;
//         file->cursor.line++;
//         file->render_cursor.column = 0;
//         file->render_cursor.line++;
//         file->visual_cursor.column = 0;
//         file->visual_cursor.line++;
        
//         if (file->visual_screen.line < window.rows - 1 - (file->options.flags & show_status))
//             file->visual_screen.line++;
//         else file->visual_origin.line++;
        
//         if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset) {
//             file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset;
//             file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
//         } else {
//             file->visual_screen.column = (i16) file->visual_cursor.column;
//             file->visual_origin.column = 0;
//         }
        
//         if (change_desired) file->visual_desired = file->visual_cursor.column;
//         return;
//     }
    
//     struct logical_line* line = file->logical.lines + file->cursor.line;
            
//     while (file->cursor.column < line->length) {
//         ++file->cursor.column;
//         if (file->cursor.column >= line->length or
//             (line->line[file->cursor.column] >> 6) != 2) break;
//     }
    
//     if (file->visual_cursor.line + 1 < file->render.count and
//         file->visual_cursor.column == file->render.lines[file->visual_cursor.line].visual_length) {
        
//         file->render_cursor.column = 0;
//         file->render_cursor.line++;
//         file->visual_cursor.column = 0;
//         file->visual_cursor.line++;
        
//         if (file->visual_screen.line < window.rows - 1 - (file->options.flags & show_status))
//             file->visual_screen.line++;
//         else file->visual_origin.line++;
        
//         if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset) {
//             file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset;
//             file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
//         } else {
//             file->visual_screen.column = (i16) file->visual_cursor.column;
//             file->visual_origin.column = 0;
//         }
//     }
    
//     struct render_line* new_render_line = file->render.lines + file->render_cursor.line;
    
//     while (file->render_cursor.column < new_render_line->length) {
//         if ((new_render_line->line[file->render_cursor.column] >> 6) != 2) {
//             file->visual_cursor.column++;
//             if (file->visual_screen.column < window.columns - 1 - file->line_number_cursor_offset)
//                 file->visual_screen.column++;
//             else file->visual_origin.column++;
//         }
        
//         ++file->render_cursor.column;
//         if (file->render_cursor.column == new_render_line->length) break;
//         u8 c = new_render_line->line[file->render_cursor.column];
//         if ((c >> 6) != 2 and c != 10) break;
//     }

//     if (change_desired) file->visual_desired = file->visual_cursor.column;
// }

// static inline void move_up() {

//     struct file* file = buffers[active];
    
//     if (not file->visual_cursor.line) {
//         file->render_cursor = (struct location){0, 0};
//         file->visual_cursor = (struct location){0, 0};
//         file->visual_screen = (struct screen_location){0, 0};
//         file->visual_origin = (struct location){0, 0};
//         file->cursor = (struct location){0, 0};
//         return;
//     }
    
//     i32 line_target = file->visual_cursor.line - 1;
//     i32 column_target = min(file->render.lines[line_target].visual_length, file->visual_desired);
    
//     while (file->visual_cursor.column > column_target or file->visual_cursor.line > line_target)
//         move_left(0);
    
//     if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset) {
//         file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset;
//         file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
//     } else {
//         file->visual_screen.column = (i16) file->visual_cursor.column;
//         file->visual_origin.column = 0;
//     }
// }

// static inline void move_down() {

//     struct file* file = buffers[active];
    
//     if (file->visual_cursor.line == file->render.count - 1) {
//         while (file->visual_cursor.column < file->render.lines[file->visual_cursor.line].visual_length)
//             move_right(0);
//         return;
//     }
    
//     i32 line_target = file->visual_cursor.line + 1;
//     i32 column_target = min(file->render.lines[line_target].visual_length, file->visual_desired);
    
//     while (file->visual_cursor.column < column_target or file->visual_cursor.line < line_target)
//         move_right(0);
    
//     if (file->render.lines[file->visual_cursor.line].continued and not column_target)
//         move_left(0);
// }

static inline void insert(u8 c, i8 record_action) {
    
    struct file* file = buffers[active];

    if (c == '\n') {
        
        struct logical_line* current = file->logical.lines + file->cursor.line;
        
        if (current->length == file->cursor.column) {
            
            i32 at = file->cursor.line + 1;
            i32 render_at = file->render_cursor.line + 1;
            
            if (file->logical.count + 1 >= file->logical.capacity) file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (size_t)(file->logical.capacity = 2 * (file->logical.capacity + 1)));
            memmove(file->logical.lines + at + 1, file->logical.lines + at, sizeof(struct logical_line) * (size_t)(file->logical.count - at));
            file->logical.lines[at] = (struct logical_line) {0};
            file->logical.count++;
            
            if (file->render.count + 1 >= file->render.capacity) file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (size_t)(file->render.capacity = 2 * (file->render.capacity + 1)));
            memmove(file->render.lines + render_at + 1, file->render.lines + render_at, sizeof(struct render_line) * (size_t)(file->render.count - render_at));
            file->render.lines[render_at] = (struct render_line) {0};
            file->render.count++;
            
            move_right(1);
            
        } else {
            
            i32 size = current->length - file->cursor.column;
            struct logical_line new = {malloc((size_t) size), size, size};
            
            memcpy(new.line, current->line + file->cursor.column, (size_t) size);
            current->length = file->cursor.column;
            
            if (file->logical.count + 1 >= file->logical.capacity)
                file->logical.lines =
                realloc(file->logical.lines, sizeof(struct logical_line)
                        * (size_t)(file->logical.capacity = 2 * (file->logical.capacity + 1)));
            i32 at = file->cursor.line + 1;
            memmove(file->logical.lines + at + 1, file->logical.lines + at, sizeof(struct logical_line) * (size_t)(file->logical.count - at));
            file->logical.lines[at] = new;
            file->logical.count++;
            
            if (file->render.count + 1 >= file->render.capacity)
                file->render.lines =
                realloc(file->render.lines, sizeof(struct render_line)
                        * (size_t)(file->render.capacity = 2 * (file->render.capacity + 1)));
            
            i32 render_at = file->render_cursor.line + 1;
            memmove(file->render.lines + render_at + 1, file->render.lines + render_at, sizeof(struct render_line) * (size_t)(file->render.count - render_at));
            file->render.lines[render_at] = (struct render_line) {0};
            file->render.count++;
            
            render_line();
            move_right(1);
            render_line();
        }
        goto final;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    
    i32 at = file->cursor.column;
    if (line->length + 1 >= line->capacity)
        line->line = realloc(line->line, (size_t)(line->capacity = 2 * (line->capacity + 1)));
    
    
    memmove(line->line + at + 1, line->line + at, (size_t)(line->length - at));
    ++line->length;
    line->line[at] = c;
    
    render_line();
    
    if (c < 128) move_right(1);
    else {
        
        if (not file) abort();
        
        file->cursor.column++;
        file->render_cursor.column++;
        
        if ((c >> 6) != 2) {
            file->visual_cursor.column++;
            if (file->visual_screen.column < window.columns - 1 - file->line_number_cursor_offset)
                file->visual_screen.column++;
            else file->visual_origin.column++;
        }
    }
        
}

static inline void backspace(i8 record_action) {
    
    struct file* file = buffers[active];
    
    
    if (not file->cursor.column) {
        if (not file->cursor.line) return;
        
        i32 at = file->cursor.line;
        i32 render_at = file->render_cursor.line;
        
        while (render_at and file->render.lines[render_at].continued) render_at--;
                                
        move_left(1);
        
        struct logical_line* new = file->logical.lines + at - 1;
        struct logical_line* old = file->logical.lines + at;
        
        if (new->length + old->length >= new->capacity)
            new->line = realloc(new->line, (size_t)(new->capacity = 2 * (new->capacity + old->length)));
        
        if (old->length) {
            memcpy(new->line + new->length, old->line, (size_t)old->length);
            new->length += old->length;
        }
        
        memmove(file->logical.lines + at, file->logical.lines + at + 1,
                sizeof(struct logical_line) * (size_t)(file->logical.count - (at + 1)));
        file->logical.count--;
        
        memmove(file->render.lines + render_at, file->render.lines + render_at + 1,
                sizeof(struct render_line) * (size_t)(file->render.count - (render_at + 1)));
        file->render.count--;
        render_line();
                
        
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    i32 save = file->cursor.column;
    move_left(1);
        
    i32 copy_deleted_length = save - file->cursor.column;
    u8* copy_deleted = NULL;
    if (record_action) {
        copy_deleted = malloc((size_t)copy_deleted_length);
        memcpy(copy_deleted, line->line + file->cursor.column, (size_t)copy_deleted_length);
    }

    memmove(line->line + file->cursor.column, line->line + save, (size_t)(line->length - save));
    line->length -= save - file->cursor.column;
    render_line();
       
}

static inline void destroy_buffer(struct file* file) {
    for (i32 i = 0; i < file->logical.count; i++)
        free(file->logical.lines[i].line);
    free(file->logical.lines);
    for (i32 i = 0; i < file->render.count; i++)
        free(file->render.lines[i].line);
    free(file->render.lines);
    free(file);
}

static inline void open_buffer(const char* given_filename) {
    if (not strlen(given_filename)) return;
    
    FILE* file = fopen(given_filename, "r");
    if (!file) {
        if (buffer_count) {
            sprintf(buffers[active]->message, "could not open %s: %s",
                    given_filename, strerror(errno));
        } else perror("fopen");
        return;
    }

    struct file* buffer = calloc(1, sizeof(struct file));
    
    strncpy(buffer->filename, given_filename, sizeof buffer->filename);
    
    buffer->tree = calloc(1, sizeof(struct action));
    buffer->head = buffer->tree;

    char* line = NULL;
    size_t line_capacity = 0;
    
    i32 line_length;
    while ((line_length = (i32)getline(&line, &line_capacity, file)) >= 0) {
        
        if (line_length and line[line_length - 1] == '\n') line_length--;
    
        i32 logical_line = buffer->logical.count;
        if (buffer->logical.count + 1 >= buffer->logical.capacity)
            buffer->logical.lines =
            realloc(buffer->logical.lines, sizeof(struct logical_line)
                    * (size_t)(buffer->logical.capacity = 2 * (buffer->logical.capacity + 1)));
        
        u8* line_copy = malloc((size_t)line_length);
        memcpy(line_copy, line, (size_t)line_length);
        buffer->logical.lines[buffer->logical.count++] = (struct logical_line) {line_copy, line_length, line_length};
    
        i32 this_line = buffer->render.count;
        if (buffer->render.count + 1 >= buffer->render.capacity)
            buffer->render.lines =
            realloc(buffer->render.lines, sizeof(struct render_line)
                    * (size_t)(buffer->render.capacity = 2 * (buffer->render.capacity + 1)));
        buffer->render.lines[buffer->render.count++] = (struct render_line) {0};

        for (i32 column = 0; column < buffer->logical.lines[logical_line].length; column++) {
            u8 c = buffer->logical.lines[logical_line].line[column];
            
            if (buffer->options.wrap_width and buffer->render.lines[this_line].visual_length == buffer->options.wrap_width) {
                if (buffer->render.count + 1 >= buffer->render.capacity)
                    buffer->render.lines =
                    realloc(buffer->render.lines,
                            sizeof(struct render_line) *
                            (size_t)(buffer->render.capacity = 2 * (buffer->render.capacity + 1)));
                buffer->render.lines[buffer->render.count++] = (struct render_line){.continued = 1};
                this_line++;
            }
            
            if (c == '\t') {
                i32 at = buffer->render.lines[this_line].visual_length;
                i8 count = 0;
                do {
                    if (buffer->options.wrap_width and at >= buffer->options.wrap_width) break;
                    at++; count++;
                } while (at % buffer->options.tab_width);
                if (buffer->render.lines[this_line].length + count >= buffer->render.lines[this_line].capacity)
                    buffer->render.lines[this_line].line =
                    realloc(buffer->render.lines[this_line].line,
                            (size_t)(buffer->render.lines[this_line].capacity =
                            2 * (buffer->render.lines[this_line].capacity + count)));
                buffer->render.lines[this_line].line[buffer->render.lines[this_line].length++] = '\t';
                buffer->render.lines[this_line].visual_length++;
                for (i8 i = 1; i < count; i++) {
                    buffer->render.lines[this_line].line[buffer->render.lines[this_line].length++] = '\n';
                    buffer->render.lines[this_line].visual_length++;
                }
                
            } else {
                if (buffer->render.lines[this_line].length + 1 >= buffer->render.lines[this_line].capacity)
                    buffer->render.lines[this_line].line = realloc(buffer->render.lines[this_line].line,
                                                                   (size_t)(buffer->render.lines[this_line].capacity = 2 * (buffer->render.lines[this_line].capacity + 1)));
                buffer->render.lines[this_line].line[buffer->render.lines[this_line].length++] = c;
                if ((c >> 6) != 2) buffer->render.lines[this_line].visual_length++;
            }
        }
    }
         
    if (not buffer->logical.count) {
        buffer->logical.lines = realloc(buffer->logical.lines, sizeof(struct logical_line));
        buffer->logical.lines[buffer->logical.count++] = (struct logical_line) {0};
        buffer->render.lines = realloc(buffer->render.lines, sizeof(struct render_line));
        buffer->render.lines[buffer->render.count++] = (struct render_line) {0};
    }
    
    fclose(file);
    free(line);
    active = buffer_count;
    buffers = realloc(buffers, sizeof(struct file*) * (size_t)(buffer_count + 1));
    buffers[buffer_count++] = buffer;
}



static inline void prompt(const char* message, i16 color, char* out, i16 out_size) {
    struct textbox* box = calloc(1, sizeof(struct textbox));
    box->tab_width = buffers[active]->options.tab_width;
    box->negative_view_shift_margin = buffers[active]->options.negative_view_shift_margin;
    box->prompt_length = (i16) strlen(message);
    while (1) {
        textbox_resize_window(box);
        textbox_display(box, message, color);
        u8 c = 0;
        read(0, &c, 1);
        if (c == '\r') break;
        else if (c == 27) {
            read(0, &c, 1);
            if (c == '[') {
                read(0, &c, 1);
                if (c == 'A') {}
                else if (c == 'B') {}
                else if (c == 'C') textbox_move_right(box);
                else if (c == 'D') textbox_move_left(box);
            } else if (c == 27) { box->logical_length = 0; break; }
        } else if (c == 127) textbox_backspace(box);
        else textbox_insert(c, box);
    }
    memset(out, 0, (size_t)out_size);
    memcpy(out, box->logical_line, (size_t)min(box->logical_length, out_size - 1));
    free(box->logical_line);
    free(box->render_line);
    free(box);
}

static inline void save() {

    struct file* buffer = buffers[active];

    if (not strlen(buffer->filename)) {
        
        prompt("save as: ", 214, buffer->filename, sizeof buffer->filename);

        if (not strlen(buffer->filename)) {
            sprintf(buffer->message, "aborted save");
            return;
        }
        
        if (not strrchr(buffer->filename, '.') and
            (buffer->options.flags & use_txt_extension_when_absent)) {
            strcat(buffer->filename, ".txt");
        }
        
        if (file_exists(buffer->filename) and
            not confirmed("file already exists, overwrite")) {
            strcpy(buffer->filename, "");
            sprintf(buffer->message, "aborted save");
            return;
        }
    }
    
    FILE* file = fopen(buffer->filename, "w+");
    if (not file) {
        sprintf(buffer->message, "error: %s", strerror(errno));
        strcpy(buffer->filename, "");
        return;

    } else {
        i64 bytes = 0;
        for (i32 i = 0; i < buffer->logical.count; i++) {
            bytes += fwrite(buffer->logical.lines[i].line, 1,
                            (size_t) buffer->logical.lines[i].length, file);
            if (i < buffer->logical.count - 1) {
                fputc('\n', file);
                bytes++;
            }
        }
        if (ferror(file)) {
            sprintf(buffer->message, "error: %s", strerror(errno));
            strcpy(buffer->filename, "");
            fclose(file);
            return;
            
        } else {
            fclose(file);
            sprintf(buffer->message, "wrote %lldb;%dl", bytes, buffer->logical.count);
            buffer->saved = 1;
        }
    }
}

static void interpret_escape_code() {
    struct file* file = buffers[active];
    u8 c = 0;
    read(0, &c, 1);
    if (c == '[') {
        read(0, &c, 1);
        if (c == 'A') move_up();
        else if (c == 'B') move_down();
        else if (c == 'C') move_right(1);
        else if (c == 'D') move_left(1);
        else if (c == 32) {
            read(0, &c, 1); read(0, &c, 1);
            sprintf(file->message, "error: clicking not implemented.");
        }
        else if (c == 77) {
            read(0, &c, 1);
            if (c == 97) {
                read(0, &c, 1); read(0, &c, 1);
                
                file->scroll_counter++;
                if (file->scroll_counter == file->options.scroll_speed) {
                    move_down();
                    file->scroll_counter = 0;
                }
            } else if (c == 96) {
                read(0, &c, 1); read(0, &c, 1);
                
                file->scroll_counter++;
                if (file->scroll_counter == file->options.scroll_speed) {
                    move_up();
                    file->scroll_counter = 0;
                }
            }
        }
    } else if (c == 27) file->mode = edit_mode;
}


int main(int argc, const char** argv) {
    
    configure_terminal();
    u8 c = 0, p = 0;
    
    while (c != ) {
        
        struct file* file = buffers[active];
       
        if (file->options.flags & should_autosave) autosave();
        
        read(0, &c, 1);
        
        if (file->mode == insert_mode) {
            
            if (c == 'f' and p == 'w' or c == 'j' and p == 'o') {
                undo(); file->mode = edit_mode;
            } else if (c == 27) interpret_escape_code();
            else if (c == 127) backspace(1);
            else if (c == 13) insert('\n', 1);
            else insert(c, 1);
            
        } else if (file->mode == edit_mode) {
            
            if (c == 'f') file->mode = insert_mode;
            if (c == 'a') file->mode = command_mode;
            else if (c == 27) interpret_escape_code();
            else if (c == 'o') move_up();
            else if (c == 'i') move_down();
            else if (c == 'j') move_left(1);
            else if (c == ';') move_right(1);
            else if (c == 'O') move_top_file();
            else if (c == 'I') move_bottom_file();
            else if (c == 'J') move_begin_line();
            else if (c == ':') move_end_line();
            else if (c == 'k') move_word_left();
            else if (c == 'l') move_word_right();
            else if (c == 'w') set_begin();
            // else if (c == 'c') copy_selection_to_clipboard();
            else if (c == 'v') insert_text(clipboard_text());
            else if (c == 'd') delete_selection();
            else if (c == 'u') undo();
            else if (c == 'r') redo();
            else if (c == 'U') alternate_up();
            else if (c == 'R') alternate_down();
        
        } else if (file->mode == command_mode) {
            if (c == 'e') file->mode = edit_mode;
	    else if (c == 27) interpret_escape_code();
            else if (c == 'w') save();
            else if (c == 'W') rename_file();

	    else if (c == 'r') {
		//char command_buffer[4096] = {0};
		//prompt("command: ", 33, command_buffer, sizeof command_buffer);
		//execute(command_buffer);
	    } 
            else if (c == 'o') open_using_prompt();
            else if (c == 'i') create_empty_buffer();
            else if (c == '{') sprintf(file->message, "buffer id = %x : \"%s\"", file->id, file->filename);
            else if (c == 'q') { if (file->saved) close_buffer(); else sprintf(file->message, "buffer has unsaved changes."); }
            else if (c == 'Q') { if (file->saved or confirmed("discard unsaved changes")) close_buffer(); }
            else if (c == 'j') { if (active) active--; }
            else if (c == ';') { if (active < buffer_count - 1) active++; }
            
            else if (c == '0') strcpy(file->message, "");
            
            else if (c == 'l') file->options.flags ^= show_line_numbers;
            else if (c == 's') file->options.flags ^= show_status;
            
            else if (c == 'c') {
                file->options.flags ^= use_c_syntax_highlighting;
                sprintf(file->message, "c syntax %s" , (file->options.flags & use_c_syntax_highlighting) ?  "on" : "off");
            } else if (c == 'b') {
                file->options.flags ^= should_autosave;
                sprintf(file->message, "autosave %s" , (file->options.flags & should_autosave) ?  "on" : "off");
            }
        } else {
            sprintf(file->message, "unknown mode: %d", file->mode);
            file->mode = edit_mode;
        }
                
        p = c;
    }
    // free(system_clipboard);
    restore_terminal();
}






















//static inline void syntax_highlight() {
    
//    struct file* file = buffers[active];
//
//    if (not (file->options.flags & use_c_syntax_highlighting))
//
//            return;

//    const char* keywords[] = {
//        "0", "1",  "(", ")",
//        "{", "}", "=", "+",
//
//        "exit", "puts", "MACRO",
//
//        "void", "int", "static", "inline",
//        "struct", "unsigned", "sizeof", "const",
//
//        "char", "for", "if", "else",
//        "while", "break", "continue", "long",
//
//        "float", "double", "short", "type",
//    0};
    
//    i16 colors[] = {
//        214, 214, 246, 246,
//        246, 246, 246, 246,
//
//        41, 41, 52,
//
//        33, 33, 33, 33,
//        33, 33, 33, 33,
//
//        33, 33, 33, 33,
//        33, 33, 33, 33,
//
//        33, 33, 33, 46,
//    };
    
//    for (i32 line = 0; line < file->render.count; line++) {
//
//        file->render.lines[line].coloring.count = 0;
//
//        for (i32 column = 0; column < file->render.lines[line].length; column++) {
//            for (i32 k = 0; keywords[k]; k++) {
//
//                if (file->render.lines[line].length - column >= (i32)strlen(keywords[k]) and
//                    not strncmp((char*)file->render.lines[line].line + column, keywords[k], strlen(keywords[k]))) {
//
//                    i32 start = column;
//                    i32 end = column + (i32)strlen(keywords[k]);
//
//                    if (file->render.lines[line].coloring.count + 1 >= file->render.lines[line].coloring.capacity)
//                        file->render.lines[line].coloring.ranges = realloc(file->render.lines[line].coloring.ranges, sizeof(struct colored_range) *
//                                                                           (size_t)(file->render.lines[line].coloring.capacity = 2 * (file->render.lines[line].coloring.capacity + 1)));
//
//                    file->render.lines[line].coloring.ranges[file->render.lines[line].coloring.count].start = start;
//                    file->render.lines[line].coloring.ranges[file->render.lines[line].coloring.count].end = end;
//                    file->render.lines[line].coloring.ranges[file->render.lines[line].coloring.count++].color = colors[k];
//                }
//            }
//        }
//    }
//}



//enum CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
//    CXString spelling = clang_getCursorSpelling(cursor);
//    CXString kind = clang_getCursorKindSpelling(clang_getCursorKind(cursor));
//
//    printf("Cursor '%s' of kind '%s'.\n", clang_getCString(spelling), clang_getCString(kind));
//
//    CXSourceRange range = clang_getCursorExtent(cursor);
//    CXSourceLocation begin = clang_getRangeStart(range);
//    CXSourceLocation end = clang_getRangeEnd(range);
//
//    unsigned int line, column, offset;
//    clang_getSpellingLocation(begin, NULL, &line, &column, &offset);
//    printf("begin: (%u,%u) : %u \n", line, column, offset);
//
//    clang_getSpellingLocation(end, NULL, &line, &column, &offset);
//    printf("end: (%u,%u) : %u \n", line, column, offset);
//
//    clang_disposeString(spelling);
//    clang_disposeString(kind);
//
//    return CXChildVisit_Recurse;
//}




//static inline char** segment(char* line) {
//    char** argv = NULL;
//    nat argc = 0;
//    char* c = strtok(line, " ");
//    while (c) {
//        argv = realloc(argv, sizeof(const char*) * (argc + 1));
//        argv[argc++] = c;
//        c = strtok(NULL, " ");
//    }
//    argv = realloc(argv, sizeof(const char*) * (argc + 1));
//    (argv)[argc++] = NULL;
//    return argv;
//}

//static inline void execute_shell_command(char* line) {
//
//    char** argv = segment(line);
//    if (!argv[0]) return;
//
//    bool succeeded = false, foreground = true;
//    if (line[strlen(line) - 1] == '&') {
//        line[strlen(line) - 1] = '\0';
//        foreground = false;
//    }
//
//    char* original = getenv("PATH");
//    char paths[strlen(original) + 4];
//    strcpy(paths, original);
//    strcat(paths, ":./");
//
//    char* path = strtok(paths, ":");
//    while (path) {
//        char command[strlen(path) + strlen(argv[0]) + 2];
//        sprintf(command, "%s/%s", path, argv[0]);
//        if (!access(command, X_OK)) {
//            if (!fork()) execv(command, argv);
//            if (foreground) wait(NULL);
//            succeeded = true;
//        }
//        path = strtok(NULL, ":");
//    }
//    if (!succeeded) sprintf(file->message, "%s: command not found", argv[0]);
//    free(argv);
//}


//static inline void shell() {
//
//    struct winsize window;
//    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
//    printf(set_cursor, window.ws_row, 0);
//    printf("%s", clear_line);
//    char prompt[128] = {0};
//    sprintf(prompt, set_color ": " reset_color, shell_prompt_color);
//    restore_terminal();
//    char* line = dummy_readline(prompt);
//    add_history(line);
//    execute_shell_command(line, file);
//    free(line);
//    configure_terminal();
//    if (file->options.pause_on_shell_command_output) read_unicode();
//}
