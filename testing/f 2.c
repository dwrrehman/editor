#include <iso646.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <assert.h>

const static size_t fuzz = 1;
//const static size_t debug = 1;

const static size_t wrap_width = 20; 
const static size_t tab_width = 8;
const static size_t scroll_speed = 4;

const static size_t negative_view_shift_margin = 5;
const static size_t negative_line_view_shift_margin = 2;
const static size_t positive_view_shift_margin = 1;

struct location {
    size_t line;
    size_t column;
};

struct logical_line {
    uint8_t* line;
    size_t capacity;
    size_t length;
};

struct render_line {
    uint8_t* line;
    size_t capacity;
    size_t length;
    size_t visual_length;
    
    size_t continued;
    size_t logical_index;
};

struct logical_lines {
    struct logical_line* lines;
    size_t count;
    size_t capacity;
};

struct render_lines {
    struct render_line* lines;
    size_t count;
    size_t capacity;
};

struct file {
    size_t window_columns;
    size_t window_rows;
    struct logical_lines logical;
    struct render_lines render;
    struct location cursor;
    struct location render_cursor;
    struct location visual_cursor;
    struct location visual_origin;
    struct location visual_screen;
    struct location visual_desired;
};

static struct termios terminal = {0};

static inline void restore_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049l", 8);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
}

static inline void configure_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049h", 8);
    
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        abort();
    }
    
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        abort();
    }
    
    write(STDOUT_FILENO, "\033[?1000h", 8);
}

static inline uint8_t read_byte() {
    uint8_t c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

static inline void render_line(struct file* file) {
        
    size_t line = file->render_cursor.line;
    
    file->render.lines[line].length = file->render_cursor.column;
    file->render.lines[line].visual_length = file->visual_cursor.column;
    
    for (size_t i = file->cursor.column; i < file->logical.lines[file->cursor.line].length; i++) {
        uint8_t c = file->logical.lines[file->cursor.line].line[i];
        
        if (file->render.lines[line].visual_length == wrap_width) {
            
            line++;
            if (line >= file->render.count or
                not file->render.lines[line].continued) {

                if (file->render.count + 1 >= file->render.capacity)
                    file->render.lines =
                    realloc(file->render.lines,
                            sizeof(struct render_line) *
                            (file->render.capacity = 2 * (file->render.capacity + 1)));

                memmove(file->render.lines + line + 1,
                        file->render.lines + line,
                        sizeof(struct render_line) *
                        (file->render.count - line));
                file->render.lines[line] = (struct render_line){.continued = true};
                file->render.count++;
            } else {
                file->render.lines[line].length = 0;
                file->render.lines[line].visual_length = 0;
            }
        }
        
        if (c == '\t') {
    
            size_t at = file->render.lines[line].visual_length;
            size_t count = 0;
            do {
                if (at >= wrap_width) break;
                at++; count++;
            } while (at % tab_width);
            if (file->render.lines[line].length + count >= file->render.lines[line].capacity)
                file->render.lines[line].line =
                realloc(file->render.lines[line].line,
                        file->render.lines[line].capacity =
                        2 * (file->render.lines[line].capacity + count));
            file->render.lines[line].line[file->render.lines[line].length++] = '\t';
            file->render.lines[line].visual_length++;
            for (size_t i = 1; i < count; i++) {
                file->render.lines[line].line[file->render.lines[line].length++] = '\n';
                file->render.lines[line].visual_length++;
            }
            
        } else {
            if (file->render.lines[line].length + 1 >= file->render.lines[line].capacity)
                file->render.lines[line].line = realloc(file->render.lines[line].line,
                                                        file->render.lines[line].capacity =
                                                        2 * (file->render.lines[line].capacity + 1));
            file->render.lines[line].line[file->render.lines[line].length++] = c;
            if ((c >> 6) != 2) file->render.lines[line].visual_length++;
        }
    }
    
    line++;
    size_t delete_to = line;
    while (delete_to < file->render.count and file->render.lines[delete_to].continued) delete_to++;

    memmove(file->render.lines + line, file->render.lines + delete_to,
            sizeof(struct render_line) * (file->render.count - delete_to));
    file->render.count -= (delete_to - line);
}

static inline void move_left(struct file* file, bool user) {

    if (not file->cursor.column) {
        if (not file->cursor.line) return;
        
        file->cursor.column = file->logical.lines[--file->cursor.line].length;
        file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
        file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
                
        if (file->visual_screen.line > negative_line_view_shift_margin) file->visual_screen.line--;
        else if (file->visual_origin.line) file->visual_origin.line--;
        else file->visual_screen.line--;
        if (file->visual_cursor.column > file->window_columns - negative_view_shift_margin) {
            file->visual_screen.column = file->window_columns - negative_view_shift_margin;
            file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
        } else {
            file->visual_screen.column = file->visual_cursor.column;
            file->visual_origin.column = 0;
        }
                        
        if (user) file->visual_desired = file->visual_cursor;
        return;
    }

    struct logical_line* line = file->logical.lines + file->cursor.line;
    
    while (file->cursor.column) {
        if ((line->line[--file->cursor.column] >> 6) != 2) break;
    }
    
    if (not file->render_cursor.column) {
        if (file->render_cursor.line) {
        
            file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
            file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
            
            if (file->visual_screen.line > negative_line_view_shift_margin) file->visual_screen.line--;
            else if (file->visual_origin.line) file->visual_origin.line--;
            else file->visual_screen.line--;
            if (file->visual_cursor.column > file->window_columns - negative_view_shift_margin) {
                file->visual_screen.column = file->window_columns - negative_view_shift_margin;
                file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
            } else {
                file->visual_screen.column = file->visual_cursor.column;
                file->visual_origin.column = 0;
            }
        }
    }
    
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
    
    size_t counter = 0, max_move_count = fmax(tab_width, 4);
    
    while (file->render_cursor.column) {
        uint8_t c = render_line->line[--file->render_cursor.column];

        if ((c >> 6) != 2) {
            file->visual_cursor.column--;
            if (file->visual_screen.column > negative_view_shift_margin) file->visual_screen.column--;
            else if (file->visual_origin.column) file->visual_origin.column--;
            else file->visual_screen.column--;
        }
        if (((c >> 6) != 2 and c != 10) or counter++ == max_move_count) break;
    }
    
    if (user) file->visual_desired = file->visual_cursor;
}

static inline void move_right(struct file* file, bool user) {
    
    if (file->cursor.line >= file->logical.count) return;
    
    if (file->cursor.line + 1 < file->logical.count and
        file->cursor.column == file->logical.lines[file->cursor.line].length) {
        file->cursor.column = 0;
        file->cursor.line++;
        file->render_cursor.column = 0;
        file->render_cursor.line++;
        file->visual_cursor.column = 0;
        file->visual_cursor.line++;
        
        if (file->visual_screen.line < file->window_rows - positive_view_shift_margin) file->visual_screen.line++;
        else file->visual_origin.line++;
        if (file->visual_cursor.column > file->window_columns - positive_view_shift_margin) {
            file->visual_screen.column = file->window_columns - positive_view_shift_margin;
            file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
        } else {
            file->visual_screen.column = file->visual_cursor.column;
            file->visual_origin.column = 0;
        }
        
        if (user) file->visual_desired = file->visual_cursor;
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
            
    while (file->cursor.column < line->length) {
        ++file->cursor.column;
        if (file->cursor.column >= line->length or
            (line->line[file->cursor.column] >> 6) != 2) break;
    }
    
    if (file->render_cursor.line + 1 < file->render.count and
        file->render_cursor.column == file->render.lines[file->render_cursor.line].length) {
        file->render_cursor.column = 0;
        file->render_cursor.line++;
        file->visual_cursor.column = 0;
        file->visual_cursor.line++;
        
        if (file->visual_screen.line < file->window_rows - positive_view_shift_margin) file->visual_screen.line++;
        else file->visual_origin.line++;
        if (file->visual_cursor.column > file->window_columns - positive_view_shift_margin) {
            file->visual_screen.column = file->window_columns - positive_view_shift_margin;
            file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
        } else {
            file->visual_screen.column = file->visual_cursor.column;
            file->visual_origin.column = 0;
        }
    }
    
    struct render_line* new_render_line = file->render.lines + file->render_cursor.line;
    
    size_t counter = 0, max_move_count = fmax(tab_width, 4);
    
    while (file->render_cursor.column < new_render_line->length) {
        if ((new_render_line->line[file->render_cursor.column] >> 6) != 2) {
            file->visual_cursor.column++;
            if (file->visual_screen.column < file->window_columns - positive_view_shift_margin) file->visual_screen.column++;
            else file->visual_origin.column++;
        }
        
        ++file->render_cursor.column;
        if (file->render_cursor.column == new_render_line->length) break;
        uint8_t c = new_render_line->line[file->render_cursor.column];
        if (((c >> 6) != 2 and c != 10) or counter++ == max_move_count) break;
    }

    if (user) file->visual_desired = file->visual_cursor;
}

static inline void move_up(struct file* file) {

    if (not file->visual_cursor.line) {
        file->render_cursor = (struct location){0, 0};
        file->visual_cursor = (struct location){0, 0};
        file->visual_screen = (struct location){0, 0};
        file->visual_origin = (struct location){0, 0};
        file->cursor = (struct location){0, 0};
        return;
    }
    
    size_t line_target = file->visual_cursor.line - 1;
    size_t column_target = fmin(file->render.lines[line_target].visual_length, file->visual_desired.column);
    
    while (file->visual_cursor.column > column_target or file->visual_cursor.line > line_target)
        move_left(file, false);
    
    if (file->visual_cursor.column > file->window_columns - negative_view_shift_margin) {
        file->visual_screen.column = file->window_columns - negative_view_shift_margin;
        file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
    } else {
        file->visual_screen.column = file->visual_cursor.column;
        file->visual_origin.column = 0;
    }
}

static inline void move_down(struct file* file) {

    if (file->visual_cursor.line == file->render.count - 1) {
        while (file->visual_cursor.column < file->render.lines[file->visual_cursor.line].visual_length)
            move_right(file, false);
        return;
    }
    
    size_t line_target = file->visual_cursor.line + 1;
    size_t column_target = fmin(file->render.lines[line_target].visual_length, file->visual_desired.column);
    
    while (file->visual_cursor.column < column_target or file->visual_cursor.line < line_target)
        move_right(file, false);
    
    if (file->render.lines[file->visual_cursor.line].continued and not column_target)
        move_left(file, false);
}

/*
 
 how to get a status bar:
  
 abAppend(ab, "\x1b[7m", 4);
   int len = 0;
   while (len < E.screencols) {
     abAppend(ab, " ", 1);
     len++;
   }
   abAppend(ab, "\x1b[m", 3);
 */
 
static inline void insert(uint8_t c, struct file* file) {
    
    if (c == '\r' || c == '\n') {
        
        struct logical_line* current = file->logical.lines + file->cursor.line;
        
        if (current->length == file->cursor.column) {
            
            size_t at = file->cursor.line + 1;
            size_t render_at = file->render_cursor.line + 1;
            
            if (file->logical.count + 1 >= file->logical.capacity) file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
            memmove(file->logical.lines + at + 1, file->logical.lines + at, sizeof(struct logical_line) * (file->logical.count - at));
            file->logical.lines[at] = (struct logical_line) {0};
            file->logical.count++;
            
            if (file->render.count + 1 >= file->render.capacity) file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (file->render.capacity = 2 * (file->render.capacity + 1)));
            memmove(file->render.lines + render_at + 1, file->render.lines + render_at, sizeof(struct render_line) * (file->render.count - render_at));
            file->render.lines[render_at] = (struct render_line) {0};
            file->render.count++;
            
            move_right(file, true);
            
        } else {
            
            size_t size = current->length - file->cursor.column;
            struct logical_line new = {malloc(size), size, size};
            
            memcpy(new.line, current->line + file->cursor.column, size);
            current->length = file->cursor.column;
            
            if (file->logical.count + 1 >= file->logical.capacity)
                file->logical.lines =
                realloc(file->logical.lines, sizeof(struct logical_line)
                        * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
            size_t at = file->cursor.line + 1;
            memmove(file->logical.lines + at + 1, file->logical.lines + at, sizeof(struct logical_line) * (file->logical.count - at));
            file->logical.lines[at] = new;
            file->logical.count++;
            
            if (file->render.count + 1 >= file->render.capacity)
                file->render.lines =
                realloc(file->render.lines, sizeof(struct render_line)
                        * (file->render.capacity = 2 * (file->render.capacity + 1)));
            
            size_t render_at = file->render_cursor.line + 1;
            memmove(file->render.lines + render_at + 1, file->render.lines + render_at, sizeof(struct render_line) * (file->render.count - render_at));
            file->render.lines[render_at] = (struct render_line) {0};
            file->render.count++;
            
            render_line(file);
            move_right(file, true);
            render_line(file);
        }
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    
    size_t at = file->cursor.column;
    if (line->length + 1 >= line->capacity) line->line = realloc(line->line, line->capacity = 2 * (line->capacity + 1));
    memmove(line->line + at + 1, line->line + at, line->length - at);
    ++line->length;
    line->line[at] = c;
    
    render_line(file);
    
    if (c < 128) move_right(file, true);
    else {
        file->cursor.column++;
        file->render_cursor.column++;
        
        if ((c >> 6) != 2) {
            file->visual_cursor.column++;
            if (file->visual_screen.column < file->window_columns - positive_view_shift_margin) file->visual_screen.column++;
            else file->visual_origin.column++;
        }
    }
}

static inline void delete(struct file* file) {
    if (not file->cursor.column) {
        if (not file->cursor.line) return;
                
        size_t at = file->cursor.line;
        size_t render_at = file->render_cursor.line;
        
        while (render_at and file->render.lines[render_at].continued) render_at--;
                                
        move_left(file, true);
        
        struct logical_line* new = file->logical.lines + at - 1;
        struct logical_line* old = file->logical.lines + at;
        
        if (new->length + old->length >= new->capacity)
            new->line = realloc(new->line, new->capacity = 2 * (new->capacity + old->length));
        
        if (old->length) {
            memcpy(new->line + new->length, old->line, old->length);
            new->length += old->length;
        }
        
        memmove(file->logical.lines + at, file->logical.lines + at + 1,
                sizeof(struct logical_line) * (file->logical.count - (at + 1)));
        file->logical.count--;
        
        memmove(file->render.lines + render_at, file->render.lines + render_at + 1,
                sizeof(struct render_line) * (file->render.count - (render_at + 1)));
        file->render.count--;
        
        render_line(file);
        
        return;
    }
    struct logical_line* line = file->logical.lines + file->cursor.line;
    size_t save = file->cursor.column;
    move_left(file, true);
    memmove(line->line + file->cursor.column, line->line + save, line->length - save);
    line->length -= save - file->cursor.column;
    render_line(file);
}

static inline void destroy_file(struct file* file) {
    for (size_t i = 0; i < file->logical.count; i++) free(file->logical.lines[i].line);
    free(file->logical.lines);
    for (size_t i = 0; i < file->render.count; i++) free(file->render.lines[i].line);
    free(file->render.lines);
}

static inline void create_file(struct file* file) {
    file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
    file->logical.lines[file->logical.count++] = (struct logical_line) {0};
    file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (file->render.capacity = 2 * (file->render.capacity + 1)));
    file->render.lines[file->render.count++] = (struct render_line) {0};
}

static inline void adjust_window_size(struct file* file, char** screen) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    file->window_columns = window.ws_col;
    file->window_rows = window.ws_row;
    *screen = realloc(*screen, file->window_columns * file->window_rows * 4);
}

static void display(struct file* file, char* screen) {
    
    size_t length = 7, screen_line = 1;
    memcpy(screen, "\033[H\033[2J", 7);
    
    size_t limit = fmin(file->visual_origin.line + file->window_rows, file->render.count);
    
    for (size_t line = file->visual_origin.line; line < limit; line++) {
        for (size_t column = 0, visual_column = 0; column < file->render.lines[line].length; column++) {
            uint8_t c = file->render.lines[line].line[column];
            if (visual_column >= file->visual_origin.column and
                visual_column < file->visual_origin.column + file->window_columns) {
                if (c == '\t' or c == '\n') screen[length++] = ' ';
                else screen[length++] = c;
            }
            if ((c >> 6) != 2) visual_column++;
        }
        
//        if (file->render.lines[line].continued) screen[length++] = '/';
//        else screen[length++] = '[';
        
        length += sprintf(screen + length, "\033[%lu;1H", ++screen_line);
    }
    length += sprintf(screen + length, "\033[%lu;%luH", file->visual_screen.line + 1, file->visual_screen.column + 1);
    
    if (not fuzz) write(STDOUT_FILENO, screen, length);
}

static void interpret_escape_code(struct file *file, char** screen, size_t* scroll_step, bool* quit) {
    uint8_t c = read_byte();
    if (c == '[') {
        uint8_t c = read_byte();
        if (c == 'A') move_up(file);
        else if (c == 'B') move_down(file);
        else if (c == 'C') move_right(file, true);
        else if (c == 'D') move_left(file, true);
        else if (c == 32) printf("clicking not implemented.\n");
        else if (c == 77) {
            uint8_t c = read_byte();
            if (c == 97) {
                read_byte();
                read_byte();
                
                ++*scroll_step;
                if (*scroll_step == scroll_speed) {
                    move_down(file);
                    *scroll_step = 0;
                }
            } else if (c == 96) {
                read_byte();
                read_byte();
                
                ++*scroll_step;
                if (*scroll_step == scroll_speed) {
                    move_up(file);
                    *scroll_step = 0;
                }
                
            } else { printf("c = %lu\n", (size_t) c);  }
        } else { printf("c = %lu\n", (size_t) c);  }
    }
    else if (c == 27) *quit = true;
    else if (c == '1') adjust_window_size(file, screen);
    else { printf("c = %lu\n", (size_t) c);  }
}

void editor(const uint8_t* input, size_t input_count) {
        
    if (not fuzz) configure_terminal();
    
    struct file file = {0};
    create_file(&file);
    
    char* screen = malloc(file.window_columns * file.window_rows * 4);
    
    if (not fuzz)
        adjust_window_size(&file, &screen);
    
    else {
        file.window_columns = 80;
        file.window_rows = 40;
        screen = realloc(screen, file.window_columns * file.window_rows * 4);
    }
    
    bool quit = false;
    size_t scroll_step = 0, input_index = 0;
    
    while ((fuzz and input_index < input_count) or (not quit and not fuzz)) {
        
        
        display(&file, screen);
        
        
//        if (not fuzz) printf("\033[H\033[2J");
//
//        size_t count = 0;
//        char buffer[8192] = {0};
//
//
//        if (debug and not fuzz) {
//
//            for (size_t line = 0; line < file.logical.count; line++) {
//
//                char string[4096] = {0};
//                sprintf(string, "[#%-5lu:(l=%-5lu,c=%-5lu,_=%-5lu,_=%-5lu)] ", line, file.logical.lines[line].length, file.logical.lines[line].capacity, 0L, 0L);
//
//                for (size_t i = 0; i < strlen(string); i++) {
//                    buffer[count++] = string[i];
//                }
//
//                for (size_t column = 0; column < file.logical.lines[line].length; column++) {
//                    if (column == file.cursor.column and line == file.cursor.line) {
//                        buffer[count++] = '_';
//                    } else {
//                        buffer[count++] = file.logical.lines[line].line[column];
//                    }
//                }
//                buffer[count++] = '\n';
//            }
//            buffer[count++] = 0;
//
//            printf("%s", buffer);
//            printf("\n:::\n");
//        }
//
//        if (not fuzz) {
//            count = 0;
//            memset(buffer, 0, sizeof buffer);
//
//            for (size_t line = 0; line < file.render.count; line++) {
//
//                if (debug) {
//                    char string[4096] = {0};
//                    sprintf(string, "[#%-5lu:(V=%-5lu,l=%-5lu,c=%-5lu,C=%-5lu)] ", line, file.render.lines[line].visual_length, file.render.lines[line].length, file.render.lines[line].capacity, file.render.lines[line].continued);
//
//                    for (size_t i = 0; i < strlen(string); i++) {
//                        buffer[count++] = string[i];
//                    }
//                }
//
//                for (size_t column = 0; column < file.render.lines[line].length; column++) {
//                    if (debug and column == file.render_cursor.column and line == file.render_cursor.line) {
//                        buffer[count++] = '_';
//                    } else {
//                        uint8_t c = file.render.lines[line].line[column];
//                        if (c == '\t' or c == 10)
//                            buffer[count++] = ' ';
//                        else
//                            buffer[count++] = c;
//                    }
//                }
//                buffer[count++] = '\n';
//            }
//            buffer[count++] = 0;
//
//            printf("%s", buffer);
//
//            if (debug) printf("\n\n\n:::[v=(%lu,%lu),r=(%lu,%lu),c=(%lu,%lu)]\n",
//                   file.visual_cursor.line,
//                   file.visual_cursor.column,
//                   file.render_cursor.line,
//                   file.render_cursor.column,
//                   file.cursor.line,
//                   file.cursor.column);
//
////            if (not debug)
////                printf(set_cursor, file.visual_cursor.line + 1, file.visual_cursor.column + 1);
//
//            fflush(stdout);
//        }

        
        if (not fuzz) {
            uint8_t c = read_byte();
            if (c == 27) interpret_escape_code(&file, &screen, &scroll_step, &quit);
            else if (c == 127) delete(&file);
            else insert(c, &file);
        } else {
            uint8_t c = input[input_index++];
            if (c == 127) delete(&file);
            else if (c == '1') move_left(&file, true);
            else if (c == '2') move_right(&file, true);
            else if (c == '3') move_down(&file);
            else if (c == '4') move_up(&file);
            else insert(c, &file);
        }
    }
    destroy_file(&file);
    if (not fuzz) restore_terminal();
}

//int main() { editor(0,0); }

extern int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    editor(data, size);
    return 0;
}
