///    i am legit so happy with my editor right now, its not even funny. i love this so much. i mean, there still are like a ton of little things that i have to fix, but honestly, this is working so well, its crazy. i am just so happy wih how far i have come, and how amazingly the editor actually works... its phenomeonal.... i love it... yayyy!!!
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

static const long
    rename_color = 214L,
    confirm_color = 196L,

    insert_status_color = 214L,
    edit_status_color = 234L,
    command_status_color = 239L,

    edited_flag_color = 130L,
    shell_prompt_color = 33L;

enum key_bindings {
    insert_key = 'f',       hard_insert_key = 'F',
    command_key = 'a',
    
    up_key = 'o',           down_key = 'i',
    left_key = 'j',         right_key = ';',
    
    word_left_key = 'J',    word_right_key = ':',
    
    escape_key = 27,        backspace_key = 127,
};

static const char *left_exit = "wf", *right_exit = "oj";


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

struct clipboard {
    char* text;
    size_t length;
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
    unsigned int movements_until_inactive_autosave;
    unsigned int edits_until_active_autosave;
    bool pause_on_shell_command_output;
    
    size_t negative_view_shift_margin;
    size_t negative_line_view_shift_margin;
    
    size_t positive_view_shift_margin;
    size_t positive_line_view_shift_margin;
};

static const struct options default_options = {
    .wrap_width = 80,
    .tab_width = 8,
    .scroll_speed = 4,
    .show_status = true,
    .use_txt_extension_when_absent = true,
    .use_c_syntax_highlighting = false,
    .show_line_numbers = true,
    .preserve_autosave_contents_on_save = true,
    .movements_until_inactive_autosave = 30 * 1000,
    .edits_until_active_autosave = 20,
    .pause_on_shell_command_output = true,
    .negative_view_shift_margin = 5,
    .negative_line_view_shift_margin = 2,
    .positive_view_shift_margin = 0,
    .positive_line_view_shift_margin = 0,
};

struct file {
    struct logical_lines logical;
    struct render_lines render;
    struct options options;
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
    if (++buffers[active]->autosave_counter == buffers[active]->options.edits_until_active_autosave) {

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

static inline void dump_and_panic() {
    printf("panic: internal error: dump() & abort()'ing...\n");
    dump();
    abort();
}

static inline void signal_interrupt(int unused) {
    printf("error: process interrupted, dump() & exit()'ing...\n");
    dump();
    exit(1);
}


static inline void restore_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049l", 8); // restore screen.
        
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        abort();
    }
    
    write(STDOUT_FILENO, "\033[?1000l", 8); // disable terminal mouse reporting.
}

static inline void configure_terminal() {
    
    write(STDOUT_FILENO, "\033[?1049h", 8); // save screen.
    
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
    
    write(STDOUT_FILENO, "\033[?1000h", 8); // enable terminal mouse reporting.
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


static inline void adjust_window_size() {
    struct winsize win;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
    window.columns = win.ws_col;
    window.rows = win.ws_row;
    window.screen = realloc(window.screen, window.columns * window.rows * 4);
}

static void display() {
    
    struct file* file = buffers[active];
    
    size_t length = 7, screen_line = 1;
    memcpy(window.screen, "\033[H\033[2J", 7);
    
    file->line_number_width = floor(log10(file->logical.count)) + 1;
    file->line_number_cursor_offset = file->options.show_line_numbers ? file->line_number_width + 2 : 0;
    
    size_t limit = fmin(file->visual_origin.line + window.rows - file->options.show_status, file->render.count);
    
    for (size_t line = file->visual_origin.line; line < limit; line++) {
        
        ///TODO: PRINT LOGICAL LINE NUMBERS: only if "!line.continued".
        if (file->options.show_line_numbers)
            length += sprintf(window.screen + length, "\033[38;5;%lum" "%*lu" "\033[0m"  "  " , 59UL, file->line_number_width, line + 1);
        
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
        //    printf(set_cursor, file->window.rows, 1);
        //    printf("%s", clear_line);
            
        const long color =
            file->mode == insert_mode ? insert_status_color :
            file->mode == edit_mode ? edit_status_color :
            file->mode == edit_mode ? command_status_color : 73;
                    
        /// "\033[38;5;%lum" : "\033[0m"
        
        
        size_t status_length = 0;
        char* status = malloc(window.columns * 4);
    
        status_length += sprintf(status,  "(%3lu,%3lu) %s%s  %s" ,
                file->cursor.line, file->cursor.column,
                file->filename,
                file->saved ? " saved" : " *",
                file->message);
    
        for (size_t i = status_length; i < window.columns; i++) status[i] = ' ';
        status[window.columns - 1] = '\0';
        
        length += sprintf(window.screen + length, "\033[%lu;1H", window.rows + 1);
        length += sprintf(window.screen + length, "\033[7m"  "\033[38;5;%lum" "%s" "\033[m", color, status);
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
        if (file->visual_cursor.column >= window.columns - file->line_number_cursor_offset) {
            file->visual_screen.column = window.columns - file->line_number_cursor_offset;
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
            if (file->visual_cursor.column >= window.columns - file->line_number_cursor_offset) {
                file->visual_screen.column = window.columns - file->line_number_cursor_offset;
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
        if (file->visual_cursor.column >= window.columns - file->line_number_cursor_offset) {
            file->visual_screen.column = window.columns - file->line_number_cursor_offset;
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
    
    if (file->visual_cursor.column >= window.columns - file->line_number_cursor_offset) {
        file->visual_screen.column = window.columns - file->line_number_cursor_offset;
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
    
    if (c == '\r') {
        
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

static inline void delete() {
    
    struct file* file = buffers[active];
        
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
    struct file* swap = buffers[buffer_count - 1];
    buffers[buffer_count - 1] = buffers[active];
    buffers[active] = swap;
    destroy_buffer(buffers[--buffer_count]);
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
    free(line);
        
    active = buffer_count;
    buffers = realloc(buffers, sizeof(struct file*) * (buffer_count + 1));
    buffers[buffer_count++] = buffer;
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

//static inline char* strip(char* str) {
//    while (isspace(*str)) str++;
//    if (!*str) return str;
//    char* end = str + strlen(str) - 1;
//    while (end > str && isspace(*end)) end--;
//    end[1] = '\0';
//    return str;
//}

//static inline bool confirmed(const char* question) {
//    ///TODO: make this use the text box that we made.
//    while (true) {
//        struct winsize window;
//        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
////        printf(set_cursor, window.ws_row, 1);
////        printf("%s", clear_line);
//        printf(set_color "%s? (yes/no): " reset_color, confirm_color, question);
//        fflush(stdout);
//        char response[8] = {0};
//        restore_terminal();
//        fgets(response, 8, stdin);
//        configure_terminal();
//        if (!strncmp(response, "yes\n", 4)) return true;
//        else if (!strncmp(response, "no\n", 3)) return false;
//        else printf("please type \"yes\" or \"no\".\n");
//    }
    
//    return false;
//}


//static inline void prompt(const char* message, char* response, int max, long color) {
//    ///TODO: make this use the text box that we made.
//    struct winsize window;
//    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
////    printf(set_cursor, window.ws_row, 0);
////    printf("%s", clear_line);
//    printf(set_color "%s: " reset_color, color, message);
//    memset(response, 0, sizeof(char) * (max));
//    restore_terminal();
//    char* line = dummy_readline("");
//    // add_history(line);
//    strncpy(response, line, max - 1);
//    free(line);
//    configure_terminal();
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
//        prompt("filename", buffer->filename, 4096, rename_color);

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
//        and not confirmed("file already exists, overwrite")
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

static inline void rename_file(char* old, char* message) {
    char new[4096];
//    prompt("filename", new, sizeof new, rename_color);

    if (!strlen(new))
        sprintf(message, "aborted rename.");

    else if (file_exists(new)
//             and not confirmed("file already exists, overwrite")
             )
        sprintf(message, "aborted rename.");

    else if (rename(old, new))
        sprintf(message, "rename unsuccessful: %s", strerror(errno));

    else {
        strncpy(old, new, sizeof new);
        sprintf(message, "renamed.");
    }
}

//static inline void read_option() {
//
//    struct file* file = buffers[active];
//
//    uint8_t c = read_byte();
//    if (c == '0') strcpy(file->message, "");
//    else if (c == 'l') file->options.show_line_numbers = !file->options.show_line_numbers;
//    else if (c == '[') file->options.show_status = !file->options.show_status;
//    else if (c == 'c') file->options.use_c_syntax_highlighting = !file->options.use_c_syntax_highlighting;
//    else if (c == 'p') sprintf(file->message, "0:sb_clear [:sb_toggle l:numbers c:csyntax o:open_buf f:func ");
//    else if (c == 'i') {
////        create_new_buffer();
//    }
//    else if (c == 'o') {
//            char new[4096];
//            prompt("open", new, 4096, rename_color);
////            open_file(strip(new), );
//
//    } else if (c == 'f') {
//        char option_command[128] = {0};
//        prompt("option", option_command, sizeof option_command, rename_color);
//        sprintf(file->message, "error: programmatic options are currently unimplemented.");
//    } else {
//        sprintf(file->message, "error: unknown option argument. try pp for help.");
//    }
//}



static void interpret_escape_code() {
    
    struct file* file = buffers[active];

    uint8_t c = read_byte();
    if (c == '0') adjust_window_size();
    else if (c == '[') {
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
    }
}

int main(const int argc, const char** argv) {
    
    srand((unsigned) time(0));
    
    if (argc <= 1) create_empty_buffer();
    else for (int i = argc; i-- > 1;) open_buffer(argv[i]);
    
    if (not buffer_count) exit(1);
    
    signal(SIGINT, signal_interrupt);
    
    configure_terminal();
    adjust_window_size();
    
    uint8_t p = 0;
    
    while (buffer_count) {
        
        unsigned int mode = buffers[active]->mode;
        display();
        uint8_t c = read_byte();
        
        if (mode == edit_mode) {
            
            if (c == insert_key) buffers[active]->mode = insert_mode;
            if (c == command_key) buffers[active]->mode = command_mode;
            else if (c == up_key) move_up();
            else if (c == down_key) move_down();
            else if (c == right_key) move_right(true);
            else if (c == left_key) move_left(true);
            else {
                sprintf(buffers[active]->message, "unknown command: %d", c);
            }
            
        } else if (mode == insert_mode) {
            if (c == 'f' and p == 'w' or c == 'j' and p == 'o') {
                delete(); buffers[active]->mode = edit_mode;
            }
            else if (c == escape_key) interpret_escape_code();
            else if (c == backspace_key) delete();
            else insert(c);
            autosave();
            
        } else if (mode == command_mode) {
            if (c == insert_key) buffers[active]->mode = insert_mode;
            if (c == command_key) buffers[active]->mode = edit_mode;
            else if (c == 'W') save();
            else if (c == 'q') close_buffer();
            else {
                sprintf(buffers[active]->message, "unknown command: %d", c);
            }
            
        } else {
            sprintf(buffers[active]->message, "unknown mode: %d", mode);
            buffers[active]->mode = command_mode;
        }
        p = c;
    }
    restore_terminal();
}