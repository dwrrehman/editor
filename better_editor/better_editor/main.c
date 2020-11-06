#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
//#include <stdbool.h>
//#include <ctype.h>
//#include <math.h>
//#include <time.h>
//#include <ftw.h>
//#include <errno.h>
//#include <pthread.h>
//#include <sys/time.h>
//#include <sys/ioctl.h>
//#include <sys/stat.h>
//#include <sys/wait.h>
//#include <sys/types.h>

const size_t debug = 0;
const size_t fuzz = 0;

const size_t tab_width = 8;

static const char
    *set_cursor = "\033[%lu;%luH",
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

static inline void render_line(struct file* file, size_t logical_line, size_t render_line) {
    
    struct logical_line* logical = file->logical.lines + logical_line;
    struct render_line* render = file->render.lines + render_line;
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
                render->line[render->length++] = 10;
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

    if (not file->cursor.column) {
        if (not file->cursor.line) return;
        file->cursor.column = file->logical.lines[--file->cursor.line].length;
        file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
        file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
        return;
    }

    struct logical_line* line = file->logical.lines + file->cursor.line;
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
    
    while (file->cursor.column) {
        if ((line->line[--file->cursor.column] >> 6) != 2) break;
    }
    
    while (file->render_cursor.column) {
        uint8_t c = render_line->line[--file->render_cursor.column];
        if ((c >> 6) != 2) file->visual_cursor.column--;
        if ((c >> 6) != 2 and c != 10) break;
    }
}

static inline void move_right(struct file* file) {
    
    if (file->cursor.line >= file->logical.count) return; // unimpl.
    
    if (file->cursor.line + 1 < file->logical.count and
        file->cursor.column == file->logical.lines[file->cursor.line].length) {
        file->cursor.column = 0;
        file->cursor.line++;
        file->render_cursor.column = 0;
        file->render_cursor.line++;
        file->visual_cursor.column = 0;
        file->visual_cursor.line++;
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
        
    while (file->cursor.column < line->length) {
        ++file->cursor.column;
        if (file->cursor.column >= line->length or
            (line->line[file->cursor.column] >> 6) != 2) break;
    }
    
    while (file->render_cursor.column < render_line->length) {
        if ((render_line->line[file->render_cursor.column] >> 6) != 2) file->visual_cursor.column++;
        uint8_t c = render_line->line[++file->render_cursor.column];
        if ((c >> 6) != 2 and c != 10) break;
    }
}
 
static inline void insert(uint8_t c, struct file* file) {
    
    if (c == '\n') {
    
        struct logical_line* current = file->logical.lines + file->cursor.line;
        
        if (current->length == file->cursor.column) {
            
            size_t at = file->cursor.line + 1;
            if (file->logical.count + 1 >= file->logical.capacity) file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
            memmove(file->logical.lines + at + 1, file->logical.lines + at, sizeof(struct logical_line) * (file->logical.count - at));
            file->logical.lines[at] = (struct logical_line) {0};
            file->logical.count++;
            
            ///TODO: BUT WAIT: what if logical line is longer than wrap width!?!
            size_t render_at = file->render_cursor.line + 1;
            if (file->render.count + 1 >= file->render.capacity) file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (file->render.capacity = 2 * (file->render.capacity + 1)));
            memmove(file->render.lines + render_at + 1, file->render.lines + render_at, sizeof(struct render_line) * (file->logical.count - render_at));
            file->render.lines[render_at] = (struct render_line) {0};
            file->render.count++;
            
            move_right(file);
                        
        } else {
            uint8_t* rest = current->line + file->cursor.column;
            size_t size = current->length - file->cursor.column;
                                    
            struct logical_line new = {malloc(size), size, size};
            struct render_line render_new = {malloc(size), size, size};
            memcpy(new.line, rest, size);
            
            current->length = file->cursor.column;
            render_line(file, file->cursor.line, file->render_cursor.line);
                    
            size_t at = file->cursor.line + 1;
            if (file->logical.count + 1 >= file->logical.capacity) file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
            memmove(file->logical.lines + at + 1, file->logical.lines + at, sizeof(struct logical_line) * (file->logical.count - at));
            file->logical.lines[at] = new;
            file->logical.count++;
            
            ///TODO: BUT WAIT: what if logical line is longer than wrap width!?!
            size_t render_at = file->render_cursor.line + 1;
            if (file->render.count + 1 >= file->render.capacity) file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (file->render.capacity = 2 * (file->render.capacity + 1)));
            memmove(file->render.lines + render_at + 1, file->render.lines + render_at, sizeof(struct render_line) * (file->logical.count - render_at));
            file->render.lines[render_at] = render_new;
            file->render.count++;
            
            move_right(file);
            render_line(file, file->cursor.line, file->render_cursor.line);
        }
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    size_t at = file->cursor.column;
    if (line->length + 1 >= line->capacity) line->line = realloc(line->line, line->capacity = 2 * (line->capacity + 1));
    memmove(line->line + at + 1, line->line + at, line->length - at);
    ++line->length;
    line->line[at] = c;
    render_line(file, file->cursor.line, file->render_cursor.line);
    
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
        
        size_t at = file->cursor.line;
        size_t render_at = file->render_cursor.line;
    
        move_left(file);

        struct logical_line* new = file->logical.lines + at - 1;
        struct logical_line* old = file->logical.lines + at;
    
        // concat old onto the end of new:
        if (new->length + old->length >= new->capacity) new->line = realloc(new->line, new->capacity = 2 * (new->capacity + old->length));
        memcpy(new->line + new->length, old->line, old->length);
        new->length += old->length;
        
        // re-render the line that was appended to.
        render_line(file, file->cursor.line, file->render_cursor.line);
                
        // delete the old line:
        memmove(file->logical.lines + at, file->logical.lines + at + 1, sizeof(struct logical_line) * (file->logical.count - (at + 1)));
        file->logical.count--;
        
        // remove the render line too:   ///TODO: BUT WAIT: what if the logical line is longer than wrap width, and we need to remove multiple lines? wait! but we never have to do that, right?
        memmove(file->render.lines + render_at, file->render.lines + render_at + 1, sizeof(struct render_line) * (file->render.count - (render_at + 1)));
        file->render.count--;
        
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    size_t save = file->cursor.column;
    move_left(file);
    memmove(line->line + file->cursor.column, line->line + save, line->length - save);
    line->length -= save - file->cursor.column;
    render_line(file, file->cursor.line, file->render_cursor.line);
}

void editor(const uint8_t* input, size_t count) {
    
    if (not fuzz) configure_terminal();
    struct file file = {0};
                
    file.logical.lines = realloc(file.logical.lines, sizeof(struct logical_line) * (file.logical.capacity = 2 * (file.logical.capacity + 1)));
    file.logical.lines[file.logical.count++] = (struct logical_line) {0};
                    
    file.render.lines = realloc(file.render.lines, sizeof(struct render_line) * (file.render.capacity = 2 * (file.render.capacity + 1)));
    file.render.lines[file.render.count++] = (struct render_line) {0};
    
    size_t i = 0;
    
    while (not fuzz or i < count) {
        if (not fuzz) printf("%s", clear_screen);

        size_t count = 0;
        char buffer[8192] = {0};

        if (debug and not fuzz) {
        
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
        }
        
        if (not fuzz) {
            count = 0;
            memset(buffer, 0, sizeof buffer);

            for (size_t line = 0; line < file.render.count; line++) {

                if (debug) {
                    char string[4096] = {0};
                    sprintf(string, "[#%-5lu:(V=%-5lu,l=%-5lu,c=%-5lu)] ", line, file.render.lines[line].visual_length, file.render.lines[line].length, file.render.lines[line].capacity );

                    for (size_t i = 0; i < strlen(string); i++) {
                        buffer[count++] = string[i];
                    }
                }

                for (size_t column = 0; column < file.render.lines[line].length; column++) {
                    if (debug and column == file.render_cursor.column and line == file.render_cursor.line) {
                        buffer[count++] = '_';
                    } else {
                        uint8_t c = file.render.lines[line].line[column];
                        if (c == '\t' or c == 10)
                            buffer[count++] = ' ';
                        else
                            buffer[count++] = c;
                    }
                }
                buffer[count++] = '\n';
            }
            buffer[count++] = 0;

            printf("%s", buffer);

            if (debug) printf("\n\n\n:::[v=(%lu,%lu),r=(%lu,%lu),c=(%lu,%lu)]\n",
                   file.visual_cursor.line,
                   file.visual_cursor.column,
                   file.render_cursor.line,
                   file.render_cursor.column,
                   file.cursor.line,
                   file.cursor.column);

            if (not debug)
                printf(set_cursor, file.visual_cursor.line + 1, file.visual_cursor.column + 1);
                
            fflush(stdout);
        }
        
        uint8_t c = 0;
        if (fuzz) c = input[i++];
        else c = read_byte_from_stdin();
        
        if (not fuzz) {
            if (c == 27) {
                uint8_t c = read_byte_from_stdin();
                if (c == '[') {
                    uint8_t c = read_byte_from_stdin();
                    if (c == 'A') abort();
                    if (c == 'B') abort();
                    if (c == 'C') move_right(&file);
                    if (c == 'D') move_left(&file);
                } else if (c == 27) abort();
            }
            else if (c == 'q') break;
            else if (c == 127) delete(&file);
            else if (c == '1') move_left(&file);
            else if (c == '2') move_right(&file);
            else insert(c, &file);
        } else {
            if (c == 127) delete(&file);
            else if (c == '1') move_left(&file);
            else if (c == '2') move_right(&file);
            else insert(c, &file);
        }

    }
    
    for (size_t i = 0; i < file.logical.count; i++) {
        free(file.logical.lines[i].line);
    }
    free(file.logical.lines);
    
    for (size_t i = 0; i < file.render.count; i++) {
        free(file.render.lines[i].line);
    }
    free(file.render.lines);
        
    if (not fuzz) restore_terminal();
}


int main() { editor(NULL, 0); }

//extern int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
//    editor(data, size);
//    return 0;
//}
