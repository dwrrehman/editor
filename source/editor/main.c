///
///        My Text Editor.
///
///   Created by: Daniel Rehman
///   Created on: 2005122.113101
///  Modified on: 2011157
///

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
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <clang-c/Index.h>
#include "libclipboard.h"

static const char* autosave_directory = "/Users/deniylreimn/Documents/documents/other/autosaves/";

enum keys {
    return_key = 13,
    escape_key = 27,
    backspace_key = 127,
};

enum editor_mode {
    insert_mode,
    edit_mode,
    command_mode,
};

struct location {
    size_t line;
    size_t column;
};

struct window {
    char* screen;
    size_t columns;
    size_t rows;
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

struct colored_range {
    long color;
    struct location start;
    struct location end;
};

struct coloring {
    struct colored_range* coloring;
    size_t coloring_count;
    size_t coloring_capacity;
};

struct options {
    size_t wrap_width;
    size_t tab_width;
    size_t scroll_speed;
    bool show_status;
    bool use_txt_extension_when_absent;
    bool use_c_syntax_highlighting;
    bool show_line_numbers;
    bool preserve_autosave_contents_on_save;
    unsigned int presses_until_autosave;
    bool pause_on_shell_command_output;
    size_t negative_view_shift_margin;
    size_t negative_line_view_shift_margin;
    size_t positive_view_shift_margin;
    size_t positive_line_view_shift_margin;
};

static const struct options default_options = {
    .wrap_width = 128,
    .tab_width = 8,
    .scroll_speed = 4,
    .show_status = true,
    .use_txt_extension_when_absent = true,
    .use_c_syntax_highlighting = false,
    .show_line_numbers = true,
    .preserve_autosave_contents_on_save = true,
    .presses_until_autosave = 20,
    .pause_on_shell_command_output = true,
    .negative_view_shift_margin = 5,
    .negative_line_view_shift_margin = 2,
    .positive_view_shift_margin = 0,
    .positive_line_view_shift_margin = 0,
};

struct textbox {
    uint8_t* logical_line;
    uint8_t* render_line;
    size_t logical_capacity;
    size_t render_capacity;
    size_t logical_length;
    size_t render_length;
    size_t visual_length;
    size_t cursor;
    size_t render_cursor;
    size_t visual_cursor;
    size_t visual_origin;
    size_t visual_screen;
    size_t prompt_length;
    
    size_t tab_width;
    size_t negative_view_shift_margin;
};

struct file {
    
    struct options options;
    struct logical_lines logical;
    struct render_lines render;
    
    struct location begin;
    struct location cursor;
    struct location render_cursor;
    struct location visual_cursor;
    struct location visual_origin;
    struct location visual_screen;
    struct location visual_desired;
    
    unsigned int autosave_counter;
    unsigned int scroll_step;
    unsigned int line_number_width;
    unsigned int line_number_cursor_offset;
    unsigned int mode;
    unsigned int id;
    bool saved;
    bool quit;
    char message[4096];
    char filename[4096];
    char autosave_name[4096];
};

static size_t buffer_count = 0;
static size_t active = 0;
static struct file** buffers = NULL;
static struct window window = {0};
static struct termios terminal = {0};

static inline bool file_exists(const char* filename) {
    return access(filename, F_OK) != -1;
}

static inline uint8_t read_byte() {
    uint8_t c = 0;
    read(STDIN_FILENO, &c, 1);
    return c;
}

static inline void get_datetime(char buffer[16]) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline void dump() {
    
    for (size_t b = 0; b < buffer_count; b++) {

        char tempname[4096] = {0};
        
        char datetime[16] = {0};
        get_datetime(datetime);
        
        struct file* buffer = buffers[b];
        
        strcpy(tempname, "temporary_savefile_");
        strcat(tempname, datetime);
        strcat(tempname, ".txt");
        
        printf("dump: dumping save file to: %s\n", tempname);
        
        FILE* tempfile = fopen(tempname, "w");
        if (!tempfile) tempfile = stdout;
        
        for (size_t i = 0; i < buffer->logical.count; i++) {
            fwrite(buffer->logical.lines[i].line, 1, buffer->logical.lines[i].length, tempfile);
            if (i < buffer->logical.count - 1) fputc('\n', tempfile);
        }
    
        fclose(tempfile);
    }
}

static inline void autosave() {
    if (++buffers[active]->autosave_counter == buffers[active]->options.presses_until_autosave) {

        for (size_t b = 0; b < buffer_count; b++) {

            char filename[4096] = {0};
            
            char datetime[16] = {0};
            get_datetime(datetime);

            struct file* buffer = buffers[b];

            strcpy(filename, buffer->autosave_name);
            strcat(filename, "autosave_");
            strcat(filename, datetime);
            strcat(filename, ".txt");

            mkdir(buffer->autosave_name, 0777);

            FILE* savefile = fopen(filename, "w");
            if (!savefile) {
                sprintf(buffer->message, "could not autosave: %s", strerror(errno));
            }
            
            for (size_t i = 0; i < buffer->logical.count; i++) {
                fwrite(buffer->logical.lines[i].line, 1, buffer->logical.lines[i].length, savefile);
                if (i < buffer->logical.count - 1) fputc('\n', savefile);
            }
        
            fclose(savefile);
        }
        buffers[active]->autosave_counter = 0;
    }
}

static inline void signal_interrupt(int unused) {
    printf("error: process interrupted, dump() & exit()'ing...\n");
    dump();
    exit(1);
}

static inline void restore_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049l", 8);
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
    
    write(STDOUT_FILENO, "\033[?1000l", 8);
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

static inline void render_line() {
        
    struct file* file = buffers[active];
    
    size_t line = file->render_cursor.line;
    file->render.lines[line].length = file->render_cursor.column;
    file->render.lines[line].visual_length = file->visual_cursor.column;
    
    for (size_t i = file->cursor.column; i < file->logical.lines[file->cursor.line].length; i++) {
        uint8_t c = file->logical.lines[file->cursor.line].line[i];
        
        if (file->render.lines[line].visual_length == file->options.wrap_width) {
            
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
                if (at >= file->options.wrap_width) break;
                at++; count++;
            } while (at % file->options.tab_width);
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


static inline void resize_window() {
            
    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
        
    if (win.ws_col != window.columns or win.ws_row != window.rows) {
        window.columns = win.ws_col;
        window.rows = win.ws_row;
        window.screen = realloc(window.screen, window.columns * window.rows * 4);
        
        struct file* file = buffers[active];
        
        if (file->visual_cursor.line >= window.rows - 1 - file->options.show_status - file->options.positive_line_view_shift_margin) {
            file->visual_screen.line = window.rows - 1 - file->options.show_status - file->options.positive_line_view_shift_margin;
            file->visual_origin.line = file->visual_cursor.line - file->visual_screen.line;
        } else {
            file->visual_screen.line = file->visual_cursor.line;
            file->visual_origin.line = 0;
        }
        
        if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) {
            file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin;
            file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
        } else {
            file->visual_screen.column = file->visual_cursor.column;
            file->visual_origin.column = 0;
        }
    }
}

static void display() {
    
    struct file* file = buffers[active];
    
    size_t length = 7, screen_line = 1;
    memcpy(window.screen, "\033[H\033[2J", 7);
    
    file->line_number_width = floor(log10(file->logical.count)) + 1;
    file->line_number_cursor_offset = file->options.show_line_numbers ? file->line_number_width + 2 : 0;
    
    size_t limit = fmin(file->visual_origin.line + window.rows - file->options.show_status, file->render.count);
    
    size_t line_number = 0;
    for (size_t i = 0; i < file->visual_origin.line; i++) {
        if (not file->render.lines[i].continued) line_number++;
    }
    
    for (size_t line = file->visual_origin.line; line < limit; line++) {
                
        if (file->options.show_line_numbers) {
            if (not file->render.lines[line].continued) length += sprintf(window.screen + length, "\033[38;5;%lum" "%*lu" "\033[0m"  "  " , 59UL, file->line_number_width, ++line_number);
            else length += sprintf(window.screen + length, "%*s  " , file->line_number_width, " ");
        }
        
        for (size_t column = 0, visual_column = 0; column < file->render.lines[line].length; column++) {
            uint8_t c = file->render.lines[line].line[column];
            if (visual_column >= file->visual_origin.column and
                visual_column < file->visual_origin.column + window.columns - file->line_number_cursor_offset) {
                if (c == '\t' or c == '\n') window.screen[length++] = ' ';
                else window.screen[length++] = c;
            }
            if ((c >> 6) != 2) visual_column++;
        }
        length += sprintf(window.screen + length, "\033[%lu;1H", ++screen_line);
    }
    
    if (file->options.show_status) {
        
        char datetime[16] = {0};
        get_datetime(datetime);
        
        size_t status_length = 0;
        char* status = malloc(window.columns * 4);
        
        status_length += sprintf(status,  " %u %s (%lu,%lu) %s%s - %s" ,
                                 file->mode,
                                 datetime,
                                 file->cursor.line + 1, file->cursor.column + 1,
                                 file->filename,
                                 file->saved ? " saved" : " edited",
                                 file->message);
        
        for (size_t i = status_length; i < window.columns; i++) status[i] = ' ';
        status[window.columns - 1] = '\0';
        
        length += sprintf(window.screen + length, "\033[%lu;1H", window.rows + 1);
        length += sprintf(window.screen + length, "\033[7m"  "\033[38;5;%lum" "%s" "\033[m", 252L, status);
    }
            
    length += sprintf(window.screen + length, "\033[%lu;%luH",
                      file->visual_screen.line + 1,
                      file->visual_screen.column + 1 + file->line_number_cursor_offset);
    
    write(STDOUT_FILENO, window.screen, length);
}

static inline void move_left(bool user) {

    struct file* file = buffers[active];
    
    if (not file->cursor.column) {
        if (not file->cursor.line) return;
        
        file->cursor.column = file->logical.lines[--file->cursor.line].length;
        file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
        file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
                
        if (file->visual_screen.line > file->options.negative_line_view_shift_margin) file->visual_screen.line--;
        else if (file->visual_origin.line) file->visual_origin.line--;
        else file->visual_screen.line--;
        
        if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) {
            file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin;
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
    
    if (not file->visual_cursor.column) {
        if (file->visual_cursor.line) {
        
            file->render_cursor.column = file->render.lines[--file->render_cursor.line].length;
            file->visual_cursor.column = file->render.lines[--file->visual_cursor.line].visual_length;
            
            if (file->visual_screen.line > file->options.negative_line_view_shift_margin) file->visual_screen.line--;
            else if (file->visual_origin.line) file->visual_origin.line--;
            else file->visual_screen.line--;
            
            if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) {
                file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin;
                file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
            } else {
                file->visual_screen.column = file->visual_cursor.column;
                file->visual_origin.column = 0;
            }
        }
    }
    
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
    
    while (file->render_cursor.column) {
        uint8_t c = render_line->line[--file->render_cursor.column];
        
        if ((c >> 6) != 2) {
            file->visual_cursor.column--;
            if (file->visual_screen.column > file->options.negative_view_shift_margin) file->visual_screen.column--;
            else if (file->visual_origin.column) file->visual_origin.column--;
            else file->visual_screen.column--;
        }
        if ((c >> 6) != 2 and c != 10) break;
    }
    
    if (user) file->visual_desired = file->visual_cursor;
}


static inline void move_right(bool user) {
    
    struct file* file = buffers[active];
    
    if (file->cursor.line >= file->logical.count) return;
    
    if (file->cursor.line + 1 < file->logical.count and
        file->cursor.column == file->logical.lines[file->cursor.line].length) {
        file->cursor.column = 0;
        file->cursor.line++;
        file->render_cursor.column = 0;
        file->render_cursor.line++;
        file->visual_cursor.column = 0;
        file->visual_cursor.line++;
        
        if (file->visual_screen.line < window.rows - 1 - file->options.show_status - file->options.positive_line_view_shift_margin) file->visual_screen.line++;
        else file->visual_origin.line++;
        
        if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) {
            file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin;
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
    
    if (file->visual_cursor.line + 1 < file->render.count and
        file->visual_cursor.column == file->render.lines[file->visual_cursor.line].visual_length) {
        
        file->render_cursor.column = 0;
        file->render_cursor.line++;
        file->visual_cursor.column = 0;
        file->visual_cursor.line++;
        
        if (file->visual_screen.line < window.rows - 1 - file->options.show_status - file->options.positive_line_view_shift_margin) file->visual_screen.line++;
        else file->visual_origin.line++;
        
        if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) {
            file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin;
            file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
        } else {
            file->visual_screen.column = file->visual_cursor.column;
            file->visual_origin.column = 0;
        }
    }
    
    struct render_line* new_render_line = file->render.lines + file->render_cursor.line;
    
    while (file->render_cursor.column < new_render_line->length) {
        if ((new_render_line->line[file->render_cursor.column] >> 6) != 2) {
            file->visual_cursor.column++;
            if (file->visual_screen.column < window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) file->visual_screen.column++;
            else file->visual_origin.column++;
        }
        
        ++file->render_cursor.column;
        if (file->render_cursor.column == new_render_line->length) break;
        uint8_t c = new_render_line->line[file->render_cursor.column];
        if ((c >> 6) != 2 and c != 10) break;
    }

    if (user) file->visual_desired = file->visual_cursor;
}

static inline void move_up() {

    struct file* file = buffers[active];
    
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
        move_left(false);
    
    if (file->visual_cursor.column >= window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) {
        file->visual_screen.column = window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin;
        file->visual_origin.column = file->visual_cursor.column - file->visual_screen.column;
    } else {
        file->visual_screen.column = file->visual_cursor.column;
        file->visual_origin.column = 0;
    }
}

static inline void move_down() {

    struct file* file = buffers[active];
    
    if (file->visual_cursor.line == file->render.count - 1) {
        while (file->visual_cursor.column < file->render.lines[file->visual_cursor.line].visual_length)
            move_right(false);
        return;
    }
    
    size_t line_target = file->visual_cursor.line + 1;
    size_t column_target = fmin(file->render.lines[line_target].visual_length, file->visual_desired.column);
    
    while (file->visual_cursor.column < column_target or file->visual_cursor.line < line_target)
        move_right(false);
    
    if (file->render.lines[file->visual_cursor.line].continued and not column_target)
        move_left(false);
}

static inline void insert(uint8_t c) {
    
    struct file* file = buffers[active];
    file->saved = false;
    
    if (c == '\n') {
        
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
            
            move_right(true);
            
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
            
            render_line();
            move_right(true);
            render_line();
        }
        return;
    }
    
    struct logical_line* line = file->logical.lines + file->cursor.line;
    
    size_t at = file->cursor.column;
    if (line->length + 1 >= line->capacity) line->line = realloc(line->line, line->capacity = 2 * (line->capacity + 1));
    memmove(line->line + at + 1, line->line + at, line->length - at);
    ++line->length;
    line->line[at] = c;
    
    render_line();
    
    if (c < 128) move_right(true);
    else {
        file->cursor.column++;
        file->render_cursor.column++;
        
        if ((c >> 6) != 2) {
            file->visual_cursor.column++;
            if (file->visual_screen.column < window.columns - 1 - file->line_number_cursor_offset - file->options.positive_view_shift_margin) file->visual_screen.column++;
            else file->visual_origin.column++;
        }
    }
}

static inline void backspace() {
    
    struct file* file = buffers[active];
    file->saved = false;
    
    if (not file->cursor.column) {
        if (not file->cursor.line) return;
                
        size_t at = file->cursor.line;
        size_t render_at = file->render_cursor.line;
        
        while (render_at and file->render.lines[render_at].continued) render_at--;
                                
        move_left(true);
        
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
        
        render_line();
        return;
    }
    struct logical_line* line = file->logical.lines + file->cursor.line;
    size_t save = file->cursor.column;
    move_left(true);
    memmove(line->line + file->cursor.column, line->line + save, line->length - save);
    line->length -= save - file->cursor.column;
    render_line();
}

static inline void destroy_buffer(struct file* file) {
    for (size_t i = 0; i < file->logical.count; i++)
        free(file->logical.lines[i].line);
    free(file->logical.lines);
    for (size_t i = 0; i < file->render.count; i++)
        free(file->render.lines[i].line);
    free(file->render.lines);
    free(file);
}

static inline void close_buffer() {
    struct file* swap = buffers[--buffer_count];
    buffers[buffer_count] = buffers[active];
    buffers[active] = swap;
    destroy_buffer(buffers[buffer_count]);
    active = buffer_count - 1;
}

static inline struct file* create_empty_buffer() {
    
    struct file* file = calloc(1, sizeof(struct file));
    file->logical.lines = realloc(file->logical.lines, sizeof(struct logical_line) * (file->logical.capacity = 2 * (file->logical.capacity + 1)));
    file->logical.lines[file->logical.count++] = (struct logical_line) {0};
    file->render.lines = realloc(file->render.lines, sizeof(struct render_line) * (file->render.capacity = 2 * (file->render.capacity + 1)));
    file->render.lines[file->render.count++] = (struct render_line) {0};
    file->options = default_options;
    file->saved = true;
    file->id = rand();
    sprintf(file->autosave_name, "%s/%x/", autosave_directory, file->id);
    
    active = buffer_count;
    buffers = realloc(buffers, sizeof(struct file*) * (buffer_count + 1));
    buffers[buffer_count++] = file;
    
    return file;
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
    buffer->id = rand();
    buffer->mode = edit_mode;
    buffer->saved = true;
    buffer->options = default_options;
    strncpy(buffer->filename, given_filename, sizeof buffer->filename);
    sprintf(buffer->autosave_name, "%s/%x/", autosave_directory, buffer->id);
    
    char* line = NULL;
    size_t line_capacity = 0;
    ssize_t line_length = 0;
    
    while ((line_length = getline(&line, &line_capacity, file)) >= 0) {
        
        if (line[line_length - 1] == '\n') line_length--;
    
        size_t logical_line = buffer->logical.count;
        if (buffer->logical.count + 1 >= buffer->logical.capacity)
            buffer->logical.lines =
            realloc(buffer->logical.lines, sizeof(struct logical_line)
                    * (buffer->logical.capacity = 2 * (buffer->logical.capacity + 1)));
        
        uint8_t* line_copy = malloc(line_length);
        memcpy(line_copy, line, line_length);
        buffer->logical.lines[buffer->logical.count++] = (struct logical_line) {line_copy, line_length, line_length};
    
        size_t line = buffer->render.count;
        if (buffer->render.count + 1 >= buffer->render.capacity)
            buffer->render.lines =
            realloc(buffer->render.lines, sizeof(struct render_line)
                    * (buffer->render.capacity = 2 * (buffer->render.capacity + 1)));
        buffer->render.lines[buffer->render.count++] = (struct render_line) {0};

        for (size_t i = 0; i < buffer->logical.lines[logical_line].length; i++) {
            uint8_t c = buffer->logical.lines[logical_line].line[i];
            
            if (buffer->render.lines[line].visual_length == buffer->options.wrap_width) {
                if (buffer->render.count + 1 >= buffer->render.capacity)
                    buffer->render.lines =
                    realloc(buffer->render.lines,
                            sizeof(struct render_line) *
                            (buffer->render.capacity = 2 * (buffer->render.capacity + 1)));
                buffer->render.lines[buffer->render.count++] = (struct render_line){.continued = true};
                line++;
            }
            
            if (c == '\t') {
                size_t at = buffer->render.lines[line].visual_length;
                size_t count = 0;
                do {
                    if (at >= buffer->options.wrap_width) break;
                    at++; count++;
                } while (at % buffer->options.tab_width);
                if (buffer->render.lines[line].length + count >= buffer->render.lines[line].capacity)
                    buffer->render.lines[line].line =
                    realloc(buffer->render.lines[line].line,
                            buffer->render.lines[line].capacity =
                            2 * (buffer->render.lines[line].capacity + count));
                buffer->render.lines[line].line[buffer->render.lines[line].length++] = '\t';
                buffer->render.lines[line].visual_length++;
                for (size_t i = 1; i < count; i++) {
                    buffer->render.lines[line].line[buffer->render.lines[line].length++] = '\n';
                    buffer->render.lines[line].visual_length++;
                }
                
            } else {
                if (buffer->render.lines[line].length + 1 >= buffer->render.lines[line].capacity)
                    buffer->render.lines[line].line = realloc(buffer->render.lines[line].line,
                                                              buffer->render.lines[line].capacity = 2 * (buffer->render.lines[line].capacity + 1));
                buffer->render.lines[line].line[buffer->render.lines[line].length++] = c;
                if ((c >> 6) != 2) buffer->render.lines[line].visual_length++;
            }
        }
    }
    fclose(file);
    free(line);
    active = buffer_count;
    buffers = realloc(buffers, sizeof(struct file*) * (buffer_count + 1));
    buffers[buffer_count++] = buffer;
}

static inline void textbox_render(struct textbox* box) {
    box->render_length = 0; box->visual_length = 0;
    
    for (size_t i = 0; i < box->logical_length; i++) {
        uint8_t c = box->logical_line[i];
                
        if (c == '\t') {
    
            size_t at = box->visual_length;
            size_t count = 0;
            do {
                at++; count++;
            } while (at % box->tab_width);
            if (box->render_length + count >= box->render_capacity)
                box->render_line =
                realloc(box->render_line,
                        box->render_capacity = 2 * (box->render_capacity + count));
            box->render_line[box->render_length++] = '\t';
            box->visual_length++;
            for (size_t i = 1; i < count; i++) {
                box->render_line[box->render_length++] = '\n';
                box->visual_length++;
            }
            
        } else {
            if (box->render_length + 1 >= box->render_capacity)
                box->render_line =
                realloc(box->render_line,
                        box->render_capacity = 2 * (box->render_capacity + 1));
            box->render_line[box->render_length++] = c;
            if ((c >> 6) != 2) box->visual_length++;
        }
    }
}

static inline void textbox_resize_window(struct textbox* box) {
    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
        
    if (win.ws_col != window.columns or win.ws_row != window.rows) {
        window.columns = win.ws_col;
        window.rows = win.ws_row;
        window.screen = realloc(window.screen, window.columns * 4);
        
        if (box->visual_cursor >= window.columns - 1 - box->prompt_length - 4) {
            box->visual_screen = window.columns - 1 - box->prompt_length - 4;
            box->visual_origin = box->visual_cursor - box->visual_screen;
        } else {
            box->visual_screen = box->visual_cursor;
            box->visual_origin = 0;
        }
    }
}

static void textbox_display(struct textbox* box, const char* prompt, long color) {
    size_t length = sprintf(window.screen, "\033[%lu;1H" "\033[38;5;%lum"  "%s" "\033[m", window.rows, color, prompt);
    size_t column = 0, visual_column = 0, characters = box->prompt_length;
    for (; column < box->render_length; column++) {
        uint8_t c = box->render_line[column];
        if (visual_column >= box->visual_origin and
            visual_column < box->visual_origin + window.columns - 4 - box->prompt_length) {
            if (c == '\t' or c == '\n') window.screen[length++] = ' ';
            else window.screen[length++] = c;
            characters++;
        }
        if ((c >> 6) != 2) visual_column++;
    }
    for (size_t i = characters; i < window.columns; i++) window.screen[length++] = ' ';
    length += sprintf(window.screen + length, "\033[%lu;%luH", window.rows, box->visual_screen + 1 + box->prompt_length);
    write(STDOUT_FILENO, window.screen, length);
}

static inline void textbox_move_left(struct textbox* box) {
    if (not box->cursor) return;
    while (box->cursor) {
        if ((box->logical_line[--box->cursor] >> 6) != 2) break;
    }
    while (box->render_cursor) {
        uint8_t c = box->render_line[--box->render_cursor];
        
        if ((c >> 6) != 2) {
            box->visual_cursor--;
            if (box->visual_screen > box->negative_view_shift_margin) box->visual_screen--;
            else if (box->visual_origin) box->visual_origin--;
            else box->visual_screen--;
        }
        if ((c >> 6) != 2 and c != 10) break;
    }
}

static inline void textbox_move_right(struct textbox* box) {
    if (box->cursor >= box->logical_length) return;
    while (box->cursor < box->logical_length) {
        ++box->cursor;
        if (box->cursor >= box->logical_length or (box->logical_line[box->cursor] >> 6) != 2) break;
    }
    while (box->render_cursor < box->render_length) {
        if ((box->render_line[box->render_cursor] >> 6) != 2) {
            box->visual_cursor++;
            if (box->visual_screen < window.columns - 1 - box->prompt_length - 4)
                box->visual_screen++;
            else box->visual_origin++;
        }
        ++box->render_cursor;
        if (box->render_cursor == box->render_length) break;
        uint8_t c = box->render_line[box->render_cursor];
        if ((c >> 6) != 2 and c != 10) break;
    }
}

static inline void textbox_insert(uint8_t c, struct textbox* box) {
    const size_t at = box->cursor;
    if (box->logical_length + 1 >= box->logical_capacity)
        box->logical_line = realloc(box->logical_line, box->logical_capacity = 2 * (box->logical_capacity + 1));
    memmove(box->logical_line + at + 1, box->logical_line + at, box->logical_length - at);
    ++box->logical_length;
    box->logical_line[at] = c;
    textbox_render(box);
    if (c < 128) textbox_move_right(box);
    else {
        box->cursor++;
        box->render_cursor++;
        if ((c >> 6) != 2) {
            box->visual_cursor++;
            if (box->visual_screen < window.columns - 1 - box->prompt_length - 4) box->visual_screen++;
            else box->visual_origin++;
        }
    }
}

static inline void textbox_backspace(struct textbox* box) {
    const size_t save = box->cursor;
    if (not save) return;
    textbox_move_left(box);
    memmove(box->logical_line + box->cursor, box->logical_line + save, box->logical_length - save);
    box->logical_length -= save - box->cursor;
    textbox_render(box);
}

static inline void prompt(const char* message, long color, char* out, size_t max_out_length) {
    struct textbox* box = calloc(1, sizeof(struct textbox));
    box->tab_width = buffers[active]->options.tab_width;
    box->negative_view_shift_margin = buffers[active]->options.negative_view_shift_margin;
    box->prompt_length = strlen(message);
    while (true) {
        textbox_resize_window(box);
        textbox_display(box, message, color);
        uint8_t c = read_byte();
        if (c == return_key) break;
        else if (c == escape_key) {
            uint8_t c = read_byte();
            if (c == '[') {
                uint8_t c = read_byte();
                if (c == 'A') {}
                else if (c == 'B') {}
                else if (c == 'C') textbox_move_right(box);
                else if (c == 'D') textbox_move_left(box);
            } else if (c == escape_key) { box->logical_length = 0; break; }
        } else if (c == backspace_key) textbox_backspace(box);
        else textbox_insert(c, box);
    }
    
    size_t index = 0;
    for (size_t i = 0; i < box->logical_length; i++) {
        if (index< max_out_length) {
            out[index++] = box->logical_line[i];
        } else break;
    }
    
    free(box->logical_line);
    free(box->render_line);
    free(box);
    return;
}

static void print_above_textbox(char *message, long color) {
    size_t length = sprintf(window.screen, "\033[%lu;1H" "\033[38;5;%lum"  "%s" "\033[m", window.rows - 1, color, message);
    write(STDOUT_FILENO, window.screen, length);
}

static inline bool confirmed(const char* question) {
    const long confirm_color = 196L;
    char message[4096] = {0};
    sprintf(message, "%s? (yes/no): ", question);
    
    while (true) {
        char response[10] = {0};
        prompt(message, confirm_color, response, sizeof response);
            
        if (!strncmp(response, "yes", 4)) return true;
        else if (!strncmp(response, "no", 3)) return false;
        else print_above_textbox("please type \"yes\" or \"no\".", 214L);
    }
}


//static inline char** segment(char* line) {
//    char** argv = NULL;
//    size_t argc = 0;
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
////    if (!succeeded) sprintf(file->message, "%s: command not found", argv[0]);
//    free(argv);
//}


//static inline void shell() {
//    ///TODO: make this use the text box that we made.
//    struct winsize window;
//    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
////    printf(set_cursor, window.ws_row, 0);
////    printf("%s", clear_line);
//    char prompt[128] = {0};
//    sprintf(prompt, set_color ": " reset_color, shell_prompt_color);
//    restore_terminal();
//    char* line = dummy_readline(prompt);
////    add_history(line);
////    execute_shell_command(line, file);
//    free(line);
//    configure_terminal();
////    if (file->options.pause_on_shell_command_output) read_unicode();
//}


static inline void save() {

    struct file* buffer = buffers[active];
    bool prompted = false;

    if (not strlen(buffer->filename)) {
        prompt("save as: ", 214L, buffer->filename, sizeof buffer->filename);

        if (not strlen(buffer->filename)) {
            sprintf(buffer->message, "aborted save.");
            return;
        }

        if (!strrchr(buffer->filename, '.') and
            buffer->options.use_txt_extension_when_absent) {
            strcat(buffer->filename, ".txt");
        }
        prompted = true;
    }

    if (prompted and file_exists(buffer->filename)
        and not confirmed("file already exists, overwrite")
        ) {
        strcpy(buffer->filename, "");
        sprintf(buffer->message, "aborted save.");
        return;
    }

    FILE* file = fopen(buffer->filename, "w+");
    if (!file) {
        sprintf(buffer->message, "save unsuccessful: %s", strerror(errno));
        strcpy(buffer->filename, "");
        return;

    } else {
        for (size_t i = 0; i < buffer->logical.count; i++) {
            fwrite(buffer->logical.lines[i].line, 1, buffer->logical.lines[i].length, file);
            if (i < buffer->logical.count - 1) fputc('\n', file);
        }
        if (ferror(file)) {
            sprintf(buffer->message, "write unsuccessful: %s", strerror(errno));
            strcpy(buffer->filename, "");
            fclose(file);
            return;
            
        } else {
            fclose(file);
            sprintf(buffer->message, "saved.");
            buffer->saved = true;
        }
    }
}

static inline void rename_file() {
    struct file* file = buffers[active];
    char new[4096] = {0};
    prompt("rename to: ", 214L, new, sizeof new);
    if (!strlen(new)) sprintf(file->message, "aborted rename.");
    else if (file_exists(new) and not confirmed("file already exists, overwrite"))
        sprintf(file->message, "aborted rename.");
    else if (rename(file->filename, new))
        sprintf(file->message, "rename unsuccessful: %s", strerror(errno));
    else {
        strncpy(file->filename, new, sizeof new);
        sprintf(file->message, "renamed to \"%s\"", file->filename);
    }
}

static inline void open_using_prompt() {
    struct file* file = buffers[active];
    char to_open[4096];
    prompt("open: ", 214L, to_open, sizeof to_open);
    if (not strlen(to_open)) {
        sprintf(file->message, "aborted open.");
        return;
    } else open_buffer(to_open);
}

static inline bool behind(struct location a, struct location b) {
    if (a.line < b.line) return true;
    if (a.line > b.line) return false;
    if (a.column < b.column) return true;
    else return false;
}

static inline void copy_selection_to_clipboard(clipboard_c* cb) {
    struct file* file = buffers[active];
    
    char* buffer = malloc(1024);
    size_t buffer_length = 0, buffer_capacity = 1024;
    
    struct location begin = file->begin, cursor = file->cursor;
    if (behind(file->cursor, file->begin)) { begin = file->cursor; cursor = file->begin; }
    
    for (size_t line = begin.line; line <= cursor.line; line++) {
        
        size_t
            begin_column = line == begin.line ? begin.column : 0,
            end_column = line == cursor.line ? cursor.column : file->logical.lines[line].length;
        
        if (buffer_length + (end_column - begin_column) > buffer_capacity)
            buffer = realloc(buffer, buffer_capacity = 2 * (buffer_capacity + (end_column - begin_column)));
        
        memcpy(buffer + buffer_length,
               file->logical.lines[line].line + begin_column,
               end_column - begin_column);
        buffer_length += end_column - begin_column;
        
        if (line != cursor.line) {
            
            if (buffer_length + 1 > buffer_capacity)
                buffer = realloc(buffer, buffer_capacity = 2 * (buffer_capacity + 1));
            buffer[buffer_length++] = '\n';
            
        }
    }
    
    if (buffer_length + 1 > buffer_capacity)
        buffer = realloc(buffer, buffer_capacity = 2 * (buffer_capacity + 1));
    
    buffer[buffer_length] = '\0';
        
    sprintf(file->message, "copied %lu bytes", buffer_length);
    
    clipboard_set_text(cb, buffer);
    free(buffer);
}


static inline void delete_selection() {
    struct file* file = buffers[active];
    
    if (behind(file->cursor, file->begin)) {
        struct location temp = file->cursor;
        file->cursor = file->begin;
        file->begin = temp;
    }
    
    while (file->begin.line < file->cursor.line or
           file->begin.column < file->cursor.column) backspace();
}

static inline void set_begin() {
    struct file* file = buffers[active];
    file->begin = file->cursor;
    sprintf(file->message, "set begin to (%lu,%lu)", file->begin.line, file->begin.column);
}

static inline void insert_text(const char* string) {
    for (size_t i = 0; string[i]; i++) insert(string[i]);
}

static void interpret_escape_code() {
    
    struct file* file = buffers[active];

    uint8_t c = read_byte();
    if (c == '[') {
        uint8_t c = read_byte();
        if (c == 'A') move_up();
        else if (c == 'B') move_down();
        else if (c == 'C') move_right(true);
        else if (c == 'D') move_left(true);
        else if (c == 32) {
            read_byte(); read_byte();
            sprintf(file->message, "error: clicking not implemented.");
        }
        else if (c == 77) {
            uint8_t c = read_byte();
            if (c == 97) {
                read_byte(); read_byte();
                
                file->scroll_step++;
                if (file->scroll_step == file->options.scroll_speed) {
                    move_down();
                    file->scroll_step = 0;
                }
            } else if (c == 96) {
                read_byte(); read_byte();
                
                file->scroll_step++;
                if (file->scroll_step == file->options.scroll_speed) {
                    move_up();
                    file->scroll_step = 0;
                }
            }
        }
    } else if (c == escape_key) file->mode = edit_mode;
}


int main(const int argc, const char** argv) {
    srand((unsigned) time(NULL));
    signal(SIGINT, signal_interrupt);
    if (argc <= 1) create_empty_buffer();
    else for (int i = argc; i-- > 1;) open_buffer(argv[i]);
    configure_terminal();
    clipboard_c* system_clipboard = clipboard_new(NULL);
    uint8_t p = 0;
    
    while (buffer_count) {
        
        struct file* file = buffers[active];
        resize_window(); display(); autosave();
        uint8_t c = read_byte();
        
        if (file->mode == insert_mode) {
            
            if (c == 'f' and p == 'w' or c == 'j' and p == 'o') { backspace(); file->mode = edit_mode; }
            else if (c == escape_key) interpret_escape_code();
            else if (c == backspace_key) backspace();
            else if (c == return_key) insert('\n');
            else insert(c);
            
        } else if (file->mode == edit_mode) {
            
            if (c == 'f') file->mode = insert_mode;
            if (c == 'a') file->mode = command_mode;
            else if (c == escape_key) interpret_escape_code();
            else if (c == 'o') move_up();
            else if (c == 'i') move_down();
            else if (c == ';') move_right(true);
            else if (c == 'j') move_left(true);
            else if (c == 'w') set_begin();
            else if (c == 'c') copy_selection_to_clipboard(system_clipboard);
            else if (c == 'v') insert_text(clipboard_text(system_clipboard));
            else if (c == 'd') delete_selection();
            
            // else if (c == 'u') undo();
            // else if (c == 'r') redo();
        
        } else if (file->mode == command_mode) {
            if (c == 'e') file->mode = edit_mode;
            else if (c == 'w') save();
            else if (c == 'W') rename_file();
            else if (c == 'o') open_using_prompt();
            else if (c == 'i') create_empty_buffer();
            else if (c == '{') sprintf(file->message, "buffer id = %x : \"%s\"", file->id, file->filename);
            else if (c == 'q') { if (file->saved) close_buffer(); else sprintf(file->message, "buffer has unsaved changes."); }
            else if (c == 'Q') { if (file->saved or confirmed("discard unsaved changes")) close_buffer(); }
            else if (c == 'j') { if (active) active--; }
            else if (c == ';') { if (active < buffer_count - 1) active++; }
            else if (c == '0') strcpy(file->message, "");
            else if (c == 'l') file->options.show_line_numbers = !file->options.show_line_numbers;
            else if (c == 's') file->options.show_status = !file->options.show_status;
            else if (c == 'c') file->options.use_c_syntax_highlighting = !file->options.use_c_syntax_highlighting;
                    
        } else {
            sprintf(file->message, "unknown mode: %d", file->mode);
            file->mode = edit_mode;
        }
        p = c;
    }
    clipboard_free(system_clipboard);
    restore_terminal();
}
