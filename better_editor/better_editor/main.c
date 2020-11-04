#include <iso646.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <ftw.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

const size_t tab_width = 8;

static const char
//    *set_cursor = "\033[%lu;%luH",
    *clear_screen = "\033[1;1H\033[2J";
//    *clear_line = "\033[2K",
//    *save_screen = "\033[?1049h",
//    *restore_screen = "\033[?1049l";

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

struct termios terminal = {0};

static inline void restore_terminal() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
}

static inline void configure_terminal() {
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        abort();
    }
    
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        abort();
    }
}

static inline uint8_t read_byte_from_stdin() {
    
    uint8_t c = 0;
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    
    if (n < 0) {
        printf("n < 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        abort();
        
    } else if (n == 0) {
        printf("n == 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        abort();
    }
    
    return c;
}

static inline void render_line(struct file* file) {

    if (file->render_cursor.line == file->render.count) {
        if (file->render.count + 1 >= file->render.capacity)
            file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (file->render.capacity = 2 * (file->render.capacity + 1)));
        file->render.lines[file->render.count++] = (struct render_line) {0};
    }

    struct logical_line* logical = file->logical.lines + file->cursor.line;
    struct render_line* render = file->render.lines + file->render_cursor.line;
    render->length = 0;
    render->visual_length = 0;

    for (size_t i = 0; i < logical->length; i++) {
        uint8_t c = logical->line[i];

        if (c == '\t') {
            size_t at = render->visual_length;
            size_t count = 0;
            do { at++; count++; } while (at % tab_width);
            if (render->length + count >= render->capacity)
                render->line = realloc(render->line, render->capacity = 2 * (render->capacity + count));
            render->line[render->length++] = '\t';
            render->visual_length++;
            for (size_t i = 1; i < count; i++) {
                render->line[render->length++] = '\n';
                render->visual_length++;
            }

        } else {
            if (render->length + 1 >= render->capacity)
                render->line = realloc(render->line, render->capacity = 2 * (render->capacity + 1));
            render->line[render->length++] = c;
            if ((c >> 6) != 2) render->visual_length++;
        }
    }
}

static inline void move_left(struct file* file) {

    struct logical_line* line = file->logical.lines + file->cursor.line;
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
    
    while (file->cursor.column) {
        if ((line->line[--file->cursor.column] >> 6) != 2) break;
    }
    
    while (file->render_cursor.column) {
        uint8_t c = render_line->line[--file->render_cursor.column];
        if ((c >> 6) != 2) file->visual_cursor.column--;
        if ((c >> 6) != 2 and c != '\n') break;
    }
}

static inline void move_right(struct file* file) {
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
        
    while (file->cursor.column < line->length) {
        if ((line->line[++file->cursor.column] >> 6) != 2) break;
    }
    
    while (file->render_cursor.column < render_line->length) {
        if ((render_line->line[file->render_cursor.column] >> 6) != 2) file->visual_cursor.column++;
        uint8_t c = render_line->line[++file->render_cursor.column];
        if ((c >> 6) != 2 and c != '\n') break;
    }
}
 
static inline void insert(uint8_t c, struct file* file) {
    if (c == '\n') {
        return;
    }
    if (file->cursor.line == file->logical.count) {
        if (file->logical.count + 1 >= file->logical.capacity)
            file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
        file->logical.lines[file->logical.count++] = (struct logical_line) {0};
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    size_t at = file->cursor.column;
    if (line->length + 1 >= line->capacity) line->line = realloc(line->line, line->capacity = 2 * (line->capacity + 1));
    memmove(line->line + at + 1, line->line + at, line->length - at);
    ++line->length;
    line->line[at] = c;
    render_line(file);
    if (c < 128) move_right(file);
    else {
        file->cursor.column++;
        file->render_cursor.column++;
        if ((c >> 6) != 2) file->visual_cursor.column++;
    }
}

static inline void delete(struct file* file) {
    if (not file->cursor.column) {
        if (not file->cursor.line) return;
    }
    struct logical_line* line = file->logical.lines + file->cursor.line;
    size_t save = file->cursor.column;
    move_left(file);
    memmove(line->line + file->cursor.column, line->line + save, line->length - save);
    line->length -= save - file->cursor.column;
    render_line(file);
}

int main(int argc, const char** argv) {
    
    configure_terminal();

    struct file file = {0};
    size_t count = 0;
    char buffer[4096] = {0};
    uint8_t c = 0;
        
    while (c != 'q') {
                
        printf("%s", clear_screen);
                
        count = 0;
        memset(buffer, 0, sizeof buffer);

        for (size_t line = 0; line < file.logical.count; line++) {
            
            char string[4096] = {0};
            sprintf(string, "[#%-5lu:(l=%-5lu,c=%-5lu,_=%-5lu)] ", line, file.logical.lines[line].length, file.logical.lines[line].capacity, file.logical.lines[line].capacity);
            
            for (size_t i = 0; i < strlen(string); i++) {
                buffer[count++] = string[i];
            }
            
            for (size_t column = 0; column < file.logical.lines[line].length; column++) {
                if (column == file.cursor.column and line == file.cursor.line) {
                    buffer[count++] = '_';
                } else {
                    buffer[count++] = file.logical.lines[line].line[column];
                }
            }
            buffer[count++] = '\n';
        }
        buffer[count++] = 0;

        printf("%s", buffer);

        printf("\n:::\n");
        
        count = 0;
        memset(buffer, 0, sizeof buffer);

        for (size_t line = 0; line < file.render.count; line++) {
            
            char string[4096] = {0};
            sprintf(string, "[#%-5lu:(V=%-5lu,l=%-5lu,c=%-5lu)] ", line, file.render.lines[line].visual_length, file.render.lines[line].length, file.render.lines[line].capacity );
            
            for (size_t i = 0; i < strlen(string); i++) {
                buffer[count++] = string[i];
            }
            
            for (size_t column = 0; column < file.render.lines[line].length; column++) {
                if (column == file.render_cursor.column and line == file.render_cursor.line) {
                    buffer[count++] = '_';
                } else {
                    uint8_t c = file.render.lines[line].line[column];
                    if (c == '\t' or c == '\n')
                        buffer[count++] = ' ';
                    else
                        buffer[count++] = c;
                }
            }
            buffer[count++] = '\n';
        }
        buffer[count++] = 0;
        
        printf("%s", buffer);
        
        printf("\n:::[v=(%lu,%lu),r=(%lu,%lu),c=(%lu,%lu)]\n",
               file.visual_cursor.line,
               file.visual_cursor.column,
               
               file.render_cursor.line,
               file.render_cursor.column,
               
               file.cursor.line,
               file.cursor.column);
        
        fflush(stdout);
        c = read_byte_from_stdin();
        
        if (c == 127) delete(&file);
        else if (c == '1') move_left(&file);
        else if (c == '2') move_right(&file);
        else insert(c, &file);
    }
    
    restore_terminal();
}
