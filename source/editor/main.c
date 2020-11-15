///
///        My Text Editor.
///
///   Created by: Daniel Rehman
///   Created on: 2005122.113101
///  Modified on: 2010154.220506
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
    edit_status_color = 234L,
    command_status_color = 239L,
    edited_flag_color = 130L,
    shell_prompt_color = 33L;

enum key_bindings {
    insert_key = 'f',         hard_insert_key = 'F',
    command_key = 'a',
    
    up_key = 'o',           down_key = 'i',
    left_key = 'j',         right_key = ';',
    
    word_left_key = 'J',    word_right_key = ':',
};

static const char *left_exit = "wf", *right_exit = "oj";

static const char
    *set_cursor = "\033[%lu;%luH",
    *clear_screen = "\033[1;1H\033[2J",
    *clear_line = "\033[2K",
    *save_screen = "\033[?1049h",
    *restore_screen = "\033[?1049l";

#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

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
    unsigned int ms_until_inactive_autosave;
    unsigned int edits_until_active_autosave;
    bool pause_on_shell_command_output;
    size_t negative_view_shift_margin;
    size_t negative_line_view_shift_margin;
    size_t positive_view_shift_margin;
};

struct file {
    struct options options;
    struct logical_lines logical;
    struct render_lines render;
    struct location cursor;
    struct location render_cursor;
    struct location visual_cursor;
    struct location visual_origin;
    struct location visual_screen;
    struct location visual_desired;
    struct window window;
    unsigned int line_number_width;
    unsigned int mode;
    unsigned int id;
    bool saved;
    char message[4096];
    char filename[4096];
    char autosave_name[4096];
};

struct file empty_buffer = {
    .saved = true,
    .options = {
        .wrap_width = 128,
        .tab_width = 8,
        .scroll_speed = 4,
        .show_status = false,
        .use_txt_extension_when_absent = true,
        .use_c_syntax_highlighting = false,
        .show_line_numbers = false,
        .preserve_autosave_contents_on_save = false,
        .ms_until_inactive_autosave = 30 * 1000,
        .edits_until_active_autosave = 20,
        .pause_on_shell_command_output = true,
        .negative_view_shift_margin = 5,
        .negative_line_view_shift_margin = 2,
        .positive_view_shift_margin = 1,
    }
};

static struct termios terminal = {0};

static inline void get_datetime(char buffer[16]) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline void dump() {
    char tempname[4096] = {0};
    
    char datetime[16] = {0};
    get_datetime(datetime);
    
    strcpy(tempname, "temporary_savefile_");
    strcat(tempname, datetime);
    strcat(tempname, ".txt");
    
    printf("dump: dumping save file to: %s\n", tempname);
    
    FILE* tempfile = fopen(tempname, "w");
    if (!tempfile) tempfile = stdout;
    
    for (size_t i = 0; i < buffers[active]->length; i++)
        fputs((buffers[active]->text)[i], tempfile);
    
    fclose(tempfile);
}

static inline void autosave(struct file* buffer) {
            
    char filename[4096] = {0};
    
    char datetime[16] = {0};
    get_datetime(datetime);
    
    strcpy(filename, buffer->autosave_name);
    strcat(filename, "autosave_");
    strcat(filename, datetime);
    strcat(filename, ".txt");
    
    mkdir(buffer->autosave_name, 0777);
    
    FILE* savefile = fopen(filename, "w");
    if (!savefile) {
        sprintf(buffer->message, "could not autosave: %s", strerror(errno));
    }
    for (size_t i = 0; i < buffer->length; i++)
        fputs(buffer->text[i], savefile);
    
    fclose(savefile);
}

//void* autosaver(void* _buffer) {
//    struct file* buffer = _buffer;
//    while (buffer->mode != quit) {
//
//        for (unsigned int i = 0; i < buffer->options.ms_until_inactive_autosave; i++) {
//            if (buffer->mode == quit) return NULL;
//            usleep(1000);
//        }
//        char saved_message[4096] = {0};
//        strcpy(saved_message, buffer->message);
//        sprintf(buffer->message, "autosaving..");
//
//        for (unsigned int i = 0; i < 500; i++) {
//            if (buffer->mode == quit) return NULL;
//            usleep(1000);
//        }
//
//        autosave(buffer);
//        strcpy(buffer->message, saved_message);
//    }
//    return NULL;
//}

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal))");
        dump_and_panic();
    }
}

static inline void configure_terminal() {
    if (tcgetattr(STDIN_FILENO, &terminal) < 0) {
        perror("tcgetattr(STDIN_FILENO, &terminal)");
        dump_and_panic();
    }
    
    atexit(restore_terminal);
    struct termios raw = terminal;
    raw.c_lflag &= ~(ECHO | ICANON);
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        perror("tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)");
        dump_and_panic();
    }
}

static inline char read_byte_from_stdin() {
    char c = 0;
    
    const ssize_t n = read(STDIN_FILENO, &c, 1);
    
    if (n < 0) {
        printf("n < 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        dump_and_panic();
        
    } else if (n == 0) {
        printf("n == 0 : ");
        perror("read(STDIN_FILENO, &c, 1) syscall");
        dump_and_panic();
    }
    return c;
}

static inline int traverse_and_remove(const char* file, const struct stat* _0, int _1, struct FTW* _2) {
    return remove(file);
}

static inline int remove_directory_and_all_contents(const char* path) {
    return nftw(path, traverse_and_remove, 64, FTW_DEPTH | FTW_PHYS);
}

//static inline bool bigraph(const char* seq, uint8_t c0, uint8_t c1) {
//    return c0 == seq[0] and c1 == seq[1];
//}

//static inline unicode read_unicode() {
//    unsigned char c = read_byte_from_stdin();
//    size_t length = 1, count = 0;
//    if ((c >> 3) == 30) count = 3;
//    else if ((c >> 4) == 14) count = 2;
//    else if ((c >> 5) == 6) count = 1;
//    unicode bytes = malloc(sizeof(char) * (count + 2));
//    bytes[0] = c;
//    for (size_t i = 0; i < count; i++)
//        bytes[length++] = read_byte_from_stdin();
//    bytes[length++] = 0;
//    return bytes;
//}

//static inline void insert(unicode c, struct file* file) {
//    if (file->at > file->length) {
//        dump_and_panic();
//        return;
//    }
//
//    file->text = realloc(file->text, sizeof(unicode) * (file->length + 1));
//
//    memmove(file->text + file->at + 1,
//            file->text + file->at,
//            sizeof(unicode) * (file->length - file->at));
//
//    file->length++;
//    file->text[file->at] = c;
//}

//static inline void delete(struct file* file) {
//    if (file->at > file->length) {
//        dump_and_panic();
//        return;
//    }
//
//    if (!file->at or !file->length) return;
//
//    free(file->text[file->at - 1]);
//
//    memmove(file->text + file->at - 1,
//            file->text + file->at,
//            sizeof(unicode) * (file->length - file->at));
//
//    file->text = realloc(file->text, sizeof(unicode) * --file->length);
//}





static inline void print_status_bar(struct file* file) {
    
    printf(set_cursor, file->window_rows, 1);
    printf("%s", clear_line);
    
    const long color =
        file->mode == edit_mode or file->mode == hard_edit_mode
            ? edit_status_color
            : command_status_color;
    
    printf(set_color "%s" reset_color, color, file->filename);
    printf(set_color "%s" reset_color, edited_flag_color, file->saved ? "" : " *");
    printf(set_color "  %s" reset_color, color, file->message);
}

//static inline bool unicode_string_equals_literal(unicode* s1, const char* s2, size_t n) {
//    while (n and is(*s1, *s2)) {
//        ++s1;
//        ++s2;
//        --n;
//    }
//    return !n;
//}

//static inline void syntax_highlight(struct file* file) {
//
//    if (!file->options.use_c_syntax_highlighting) return;
//
//    return;
//
//    static size_t active = 0;
//    static size_t buffer_count = 0;
//    static struct file** buffers = NULL;
//
//
//
//    ///TODO: do this only or the screen.
//    ///figure out the size of the screen,
//    ///and do this only for the visible text!
//
//    const int keyword_count = 31;
//
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
//        "float", "double", "short", "type"
//    };
//
//    const long colors[] = {
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
//
////    for (size_t line = file->origin.line; line < fmin(file->origin.line + file->window_rows - 1, file->line_count); line++) {
////
////        for (size_t column = file->origin.column; column < fmin(file->origin.column + file->window_columns - (file->line_number_width + 2), file->lines[line].length); column++) {
////
////            for (int k = 0; k < keyword_count; k++) {
////                if (file->lines[line].length - column >= strlen(keywords[k]) and
////                    unicode_string_equals_literal(file->lines[line].line + column,
////                                                  keywords[k], strlen(keywords[k]))) {
////
////                    struct location start = {line, column};
////                    struct location end = {line, column + strlen(keywords[k])};
////                    struct colored_range node = {colors[k], start, end};
////
////                    file->coloring = realloc(file->coloring, sizeof(struct colored_range)
////                                             * (file->coloring_count + 1));
////                    file->coloring[file->coloring_count++] = node;
////                }
////            }
////        }
////    }
//}

static inline void display(struct file* file) {
    if (not file->window.columns and
        not file->window.rows) return;
    
    char* screen = NULL;
    size_t screen_length = 0;
    struct location screen_cursor = {1, 1};
    
    file->line_number_width = floor(log10(file->line_count)) + 1;
    
    for (size_t line = file->origin.line; line < fmin(file->origin.line + file->window_rows - 1, file->line_count); line++) {
        
        size_t line_length = 0;
        
        if (file->options.show_line_numbers) {
            if (line <= file->cursor.line)
                screen_cursor.column += file->line_number_width + 2;
            char line_number_buffer[128] = {0};
            sprintf(line_number_buffer, set_color "%*lu" reset_color set_color "  " reset_color,
                    59UL, file->line_number_width, line + 1, 62L);
            
            for (int i = 0; i < strlen(line_number_buffer); i++) {
                screen = realloc(screen, sizeof(char) * (screen_length + 1));
                screen[screen_length++] = line_number_buffer[i];
            }
        }
        
        for (size_t column = file->origin.column; column < fmin(file->origin.column + file->window_columns - (file->line_number_width + 2), file->lines[line].length); column++) {
            
            unicode g = file->lines[line].line[column];
            
            for (size_t range = 0; range < file->coloring_count; range++) {
                
                if (line == file->coloring[range].start.line and
                    column == file->coloring[range].start.column) {
                    char highlight_buffer[128] = {0};
                    sprintf(highlight_buffer, set_color, file->coloring[range].color);
                    for (int i = 0; i < strlen(highlight_buffer); i++) {
                        screen = realloc(screen, sizeof(char) * (screen_length + 1));
                        screen[screen_length++] = highlight_buffer[i];
                    }
                }
                
                if (line == file->coloring[range].end.line and
                    column == file->coloring[range].end.column) {
                    char highlight_buffer[128] = {0};
                    sprintf(highlight_buffer, reset_color);
                    for (int i = 0; i < strlen(highlight_buffer); i++) {
                        screen = realloc(screen, sizeof(char) * (screen_length + 1));
                        screen[screen_length++] = highlight_buffer[i];
                    }
                }
            }
                
            if (line < file->cursor.line or (line == file->cursor.line and column < file->cursor.column)) {
                if (is(g, '\t'))
                    screen_cursor.column += file->options.tab_width;
                else
                    screen_cursor.column++;
            }
            
            size_t space_count_for_tab = 0;
            if (is(g, '\t')) {
                do {
                    screen = realloc(screen, sizeof(char) * (screen_length + 1));
                    screen[screen_length++] = ' ';
                    line_length++;
                    space_count_for_tab++;
                } while (line_length % file->options.tab_width);
            } else {
                for (size_t i = 0; i < strlen(g); i++) {
                    screen = realloc(screen, sizeof(char) * (screen_length + 1));
                    screen[screen_length++] = g[i];
                    line_length++;
                }
            }
        }
        
        if (line < file->cursor.line) {
            screen_cursor.line++;
            screen_cursor.column = 1;
        }
        
        screen = realloc(screen, sizeof(char) * (screen_length + 1));
        screen[screen_length++] = '\n';
    }
    
    screen = realloc(screen, sizeof(char) * (screen_length + 1));
    screen[screen_length++] = '\0';
    
    printf("%s%s", clear_screen, screen);
    
    if (file->options.show_status) print_status_bar(file);
    
    printf(set_cursor, screen_cursor.line, screen_cursor.column);
}

//static inline struct line* generate_line_view(struct file* file) {
//    size_t length = 0;
//    struct line* lines = malloc(sizeof(struct line));
//    lines[0].line = file->text;
//    lines[0].continued = 0;
//    lines[0].length = 0;
//    lines[0].render_length = 0;
//    file->line_count = 1;
//
//    const size_t start = 0, end = file->length;   ///TODO: determine the right end points for just the screen rendering.
//
//    for (size_t i = start; i < end; i++) {
//
//        if (is(file->text[i], '\n')) {
//            lines[file->line_count - 1].continued = false;
//            length = 0;
//        } else {
//
//            if (is(file->text[i], '\t')) {
//                length += file->options.tab_width;
//                lines[file->line_count - 1].render_length += file->options.tab_width;
//            } else {
//                length++;
//                lines[file->line_count - 1].render_length++;
//            }
//
//            lines[file->line_count - 1].length++;
//
//            if (file->options.wrap_width and length >= file->options.wrap_width) {
//                lines[file->line_count - 1].continued = true;
//                length = 0;
//            }
//        }
//        if (!length) {
//            lines = realloc(lines, sizeof(struct line) * (file->line_count + 1));
//            lines[file->line_count].line = file->text + i + 1;
//            lines[file->line_count].continued = false;
//            lines[file->line_count].render_length = 0;
//            lines[file->line_count++].length = 0;
//        }
//    }
//    return lines;
//}

//static inline void move_left(struct file* file, bool user) {
//
//    if (file->cursor.column) {
//
//        file->cursor.column--;
//
//        if (file->screen.column > 5)
//            file->screen.column--;
//        else if (file->origin.column)
//            file->origin.column--;
//        else
//            file->screen.column--;
//
//    } else if (file->cursor.line) {
//
//        file->cursor.line--;
//
//        file->cursor.column = file->lines[file->cursor.line].length - file->lines[file->cursor.line].continued;
//
//        if (file->screen.line > 5)
//            file->screen.line--;
//        else if (file->origin.line)
//            file->origin.line--;
//        else
//            file->screen.line--;
//
//        if (file->cursor.column > file->window_columns - 5) {
//            file->screen.column = file->window_columns - 5;
//            file->origin.column = file->cursor.column - file->screen.column;
//        } else {
//            file->screen.column = file->cursor.column;
//            file->origin.column = 0;
//        }
//    }
//    if (user)
//        file->desired = file->cursor;
//}
//
//static inline void move_right(struct file* file, bool user) {
//
//    if (file->cursor.column < file->lines[file->cursor.line].length) {
//
//        file->cursor.column++;
//
//        if (file->screen.column < file->window_columns - 3)
//            file->screen.column++;
//        else
//            file->origin.column++;
//
//    } else if (file->cursor.line < file->line_count - 1) {
//
//        file->cursor.column = file->lines[file->cursor.line++].continued;
//
//        if (file->screen.line < file->window_rows - 3)
//            file->screen.line++;
//        else
//            file->origin.line++;
//
//        if (file->cursor.column > file->window_columns - 3) {
//            file->screen.column = file->window_columns - 3;
//            file->origin.column = file->cursor.column - file->screen.column;
//        } else {
//            file->screen.column = file->cursor.column;
//            file->origin.column = 0;
//        }
//    }
//    if (user) file->desired = file->cursor;
//}
//
//static inline void move_up(struct file* file) {
//
//     if (!file->cursor.line) {
//         file->screen = file->cursor = file->origin = (struct location){0, 0};
//         file->at = 0;
//         return;
//     }
//     const size_t column_target = fmin(file->lines[file->cursor.line - 1].length, file->desired.column);
//     const size_t line_target = file->cursor.line - 1;
//     while (file->cursor.column > column_target or file->cursor.line > line_target) {
//         move_left(file, false);
//         file->at--;
//     }
//     if (file->cursor.column > file->window_columns - 5) {
//         file->screen.column = file->window_columns - 5;
//         file->origin.column = file->cursor.column - file->screen.column;
//     } else {
//         file->screen.column = file->cursor.column;
//         file->origin.column = 0;
//     }
// }
//
//static inline void move_down(struct file* file) {
//
//    if (file->cursor.line >= file->line_count - 1) {
//        while (file->at < file->length) {
//            move_right(file, false);
//            file->at++;
//        }
//        return;
//    }
//
//    const size_t column_target = fmin(file->lines[file->cursor.line + 1].length, file->desired.column);
//    const size_t line_target = file->cursor.line + 1;
//
//    while (file->cursor.column < column_target or file->cursor.line < line_target) {
//        move_right(file, false);
//        file->at++;
//    }
//
//    if (file->cursor.line and file->lines[file->cursor.line - 1].continued and !column_target) {
//        move_left(file, false);
//        file->at--;
//    }
//}

//static inline void jump(unicode c0, struct file* file) {
//    unicode c1 = read_unicode();
//    if (bigraph(jump_top, c0, c1)) { file->screen = file->cursor = file->origin = (struct location){0, 0}; file->at = 0; }
//    else if (bigraph(jump_bottom, c0, c1)) while (file->at < file->length) move_down(file);
//    else if (bigraph(jump_begin, c0, c1)) while (file->cursor.column) { (file->at)--; move_left(file, true); }
//
//    else if (bigraph(jump_end, c0, c1)) {
//        while (file->cursor.column < file->lines[file->cursor.line].length) {
//            file->at++;
//            move_right(file, true);
//        }
//    }
//}

static inline struct file* create_new_buffer() {
    buffers = realloc(buffers, sizeof(struct file*) * (buffer_count + 1));
    buffers[buffer_count] = malloc(sizeof(struct file));
    
    struct file* buffer = buffers[buffer_count];
    *(buffers[buffer_count]) = empty_buffer;
    
    active = buffer_count;
    buffer_count++;
    
    buffer->id = rand();
    sprintf(buffer->autosave_name, "%s/%x/", autosave_directory, buffer->id);
        
    syntax_highlight(buffer);
    
    return buffer;
}

static inline void destroy(struct file* file) {
    for (size_t i = 0; i < file->length; i++) {
        free(file->text[i]);
    }
    free(file->text);
    free(file->lines);
    free(file->coloring);
    free(file);
}

static inline void close_buffer() {
    
    buffers[active]->mode = quit;
    pthread_join(buffers[active]->autosave_thread, NULL);
    
    struct file* swap = buffers[buffer_count - 1];
    buffers[buffer_count - 1] = buffers[active];
    buffers[active] = swap;
    destroy(buffers[--buffer_count]);
    active = buffer_count - 1;
}

static inline int open_file(const char* given_filename) {
    
    if (!strlen(given_filename)) return 1;
    
    FILE* file = fopen(given_filename, "r");
    if (!file) {
        if (buffer_count) {
            sprintf(buffers[active]->message, "could not open %s: %s",
                    given_filename, strerror(errno));
        } else perror("fopen");
        return 1;
    }
    
    struct file* buffer = create_new_buffer();
    
    fseek(file, 0, SEEK_END);
    const size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    buffer->text = realloc(buffer->text, sizeof(unicode) * (file_size));
    
//    int character = 0;
//    while ((character = fgetc(file)) != EOF) {
//
//        unsigned char c = character;
//        size_t length = 1, count = 0;
//        if ((c >> 3) == 30) count = 3;
//        else if ((c >> 4) == 14) count = 2;
//        else if ((c >> 5) == 6) count = 1;
//
//        unicode bytes = malloc(sizeof(char) * (count + 2));
//        bytes[0] = c;
//        for (size_t i = 0; i < count; i++)
//            bytes[length++] = fgetc(file);
//        bytes[length++] = 0;
//
//        buffer->text[buffer->length++] = bytes;
//    }
    
//    buffer->text = realloc(buffer->text, sizeof(unicode) * (buffer->length + 1));

    fclose(file);
    strncpy(buffer->filename, given_filename, 4096);
    
//    buffer->lines = generate_line_view(buffer);
//    syntax_highlight(buffer);
    return 0;
}


static inline char** segment(char* line) {
    char** argv = NULL;
    size_t argc = 0;
    char* c = strtok(line, " ");
    while (c) {
        argv = realloc(argv, sizeof(const char*) * (argc + 1));
        argv[argc++] = c;
        c = strtok(NULL, " ");
    }
    argv = realloc(argv, sizeof(const char*) * (argc + 1));
    (argv)[argc++] = NULL;
    return argv;
}

static inline void execute_shell_command(char* line, struct file* file) {
    
    char** argv = segment(line);
    if (!argv[0]) return;
    
    bool succeeded = false, foreground = true;
    if (line[strlen(line) - 1] == '&') {
        line[strlen(line) - 1] = '\0';
        foreground = false;
    }
    
    char* original = getenv("PATH");
    char paths[strlen(original) + 4];
    strcpy(paths, original);
    strcat(paths, ":./");
    
    char* path = strtok(paths, ":");
    while (path) {
        char command[strlen(path) + strlen(argv[0]) + 2];
        sprintf(command, "%s/%s", path, argv[0]);
        if (!access(command, X_OK)) {
            if (!fork()) execv(command, argv);
            if (foreground) wait(NULL);
            succeeded = true;
        }
        path = strtok(NULL, ":");
    }
    if (!succeeded) sprintf(file->message, "%s: command not found", argv[0]);
    free(argv);
}

static inline char* strip(char* str) {
    while (isspace(*str)) str++;
    if (!*str) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    end[1] = '\0';
    return str;
}

static inline bool confirmed(const char* question) {
    while (true) {
        struct winsize window;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
        printf(set_cursor, window.ws_row, 1);
        printf("%s", clear_line);
        printf(set_color "%s? (yes/no): " reset_color, confirm_color, question);
        fflush(stdout);
        char response[8] = {0};
        restore_terminal();
        fgets(response, 8, stdin);
        configure_terminal();
        if (!strncmp(response, "yes\n", 4)) return true;
        else if (!strncmp(response, "no\n", 3)) return false;
        else printf("please type \"yes\" or \"no\".\n");
    }
}

char* dummy_readline(char* p) {return NULL;}

static inline void prompt(const char* message, char* response, int max, long color) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    printf(set_color "%s: " reset_color, color, message);
    memset(response, 0, sizeof(char) * (max));
    restore_terminal();
    char* line = dummy_readline("");
    // add_history(line);
    strncpy(response, line, max - 1);
    free(line);
    configure_terminal();
}

static inline void shell(struct file* file) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    char prompt[128] = {0};
    sprintf(prompt, set_color ": " reset_color, shell_prompt_color);
    restore_terminal();
    char* line = dummy_readline(prompt);
//    add_history(line);
    execute_shell_command(line, file);
    free(line);
    configure_terminal();
    if (file->options.pause_on_shell_command_output) read_unicode();
}

static inline bool file_exists(const char* filename) {
    return access(filename, F_OK) != -1;
}

static inline void save(struct file* buffer) {
    bool prompted = false;
    
    if (!strlen(buffer->filename)) {
        prompt("filename", buffer->filename, 4096, rename_color);
        
        if (!strlen(buffer->filename)) {
            sprintf(buffer->message, "aborted save.");
            return;
        }
        
        if (!strrchr(buffer->filename, '.') and
            buffer->options.use_txt_extension_when_absent) {
            strcat(buffer->filename, ".txt");
        }
        
        prompted = true;
    }
    
    if (prompted and file_exists(buffer->filename) and
        !confirmed("file already exists, overwrite")) {
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
        for (size_t i = 0; i < buffer->length; i++)
            fputs(buffer->text[i], file);
        if (ferror(file)) {
            sprintf(buffer->message, "write unsuccessful: %s", strerror(errno));
            strcpy(buffer->filename, "");
            fclose(file);
            return;
        } else {
            fclose(file);
            sprintf(buffer->message, "saved.");
            buffer->saved = true;
            if (!buffer->options.preserve_autosave_contents_on_save)
                remove_directory_and_all_contents(buffer->autosave_name);
        }
    }
}

static inline void rename_file(char* old, char* message) {
    char new[4096];
    prompt("filename", new, 4096, rename_color);
    
    if (!strlen(new))
        sprintf(message, "aborted rename.");
    
    else if (access(new, F_OK) != -1 and
             !confirmed("file already exists, overwrite"))
        sprintf(message, "aborted rename.");
    
    else if (rename(old, new))
        sprintf(message, "rename unsuccessful: %s", strerror(errno));
    
    else {
        strncpy(old, new, 4096);
        sprintf(message, "renamed.");
    }
}

static inline void read_option(struct file* file) {
    unicode c = read_unicode();
    if (is(c, '0')) strcpy(file->message, "");
    else if (is(c, 'l')) file->options.show_line_numbers = !file->options.show_line_numbers;
    else if (is(c, '[')) file->options.show_status = !file->options.show_status;
    else if (is(c, 'c')) file->options.use_c_syntax_highlighting = !file->options.use_c_syntax_highlighting;
    else if (is(c, 'p')) sprintf(file->message, "0:sb_clear [:sb_toggle l:numbers c:csyntax o:open_buf f:func ");
    else if (is(c, 'i')) create_new_buffer();
    else if (is(c, 'o')) {
            char new[4096];
            prompt("open", new, 4096, rename_color);
            open_file(strip(new));
        
    } else if (is(c, 'f')) {
        char option_command[128] = {0};
        prompt("option", option_command, 128, rename_color);
        sprintf(file->message, "error: programmatic options are currently unimplemented.");
    } else {
        sprintf(file->message, "error: unknown option argument. try pp for help.");
    }
}

static inline void adjust_window_size(struct file* file) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    file->window_columns = window.ws_col;
    file->window_rows = window.ws_row;
}

//static inline void backspace(struct file* file) {
//    delete(file);
//    file->at--;
//    move_left(file, true);
//    free(file->lines);
//    file->lines = generate_line_view(file);
//    syntax_highlight(file);
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

/// example code for cliboard stuff:

//    clipboard_c* cb = clipboard_new(NULL);
//    const char* string = clipboard_text(cb);
//    puts(string);
//    clipboard_set_text(cb, "hello world");
//    clipboard_free(cb);
//    exit(0);

int main(const int argc, const char** argv) {

    srand((unsigned) time(0));
//    using_history();
    if (argc <= 1) create_new_buffer();
    else for (int i = argc - 1; i; i--) open_file(argv[i]);
    if (!buffer_count) exit(1);
    
    printf("%s", save_screen);
    configure_terminal();
    signal(SIGINT, signal_interrupt);
    
    // for my purposes:
    if (argc == 1) buffers[active]->mode = edit_mode;

    unicode c = 0, p = 0;
    size_t autosave_counter = 0;

    while (buffer_count) {
        
        struct file* this = buffers[active];
        adjust_window_size(this);
        display(this);
        fflush(stdout);
        
        c = read_unicode();
        
        if (this->mode == command_mode) {
            
            if (bigraph(shell_command_key, p, c)) shell(this);
            else if (is(c, 27)) {}
            else if (is(c, edit_key)) this->mode = edit_mode;
            else if (is(c, hard_edit_key)) this->mode = hard_edit_mode;
            else if (is(c, select_key)) this->mode = select_mode;
            else if (is(c, option_key)) read_option(this);
            
            else if (is(c, word_up_key)) {
                for (int i = 0; i < 10; i++)
                move_up(this);
            }
            else if (is(c, word_down_key)) {
                for (int i = 0; i < 10; i++)
                move_down(this);
            }
            
            else if (is(c, up_key)) move_up(this);
            else if (is(c, down_key)) move_down(this);
            
            else if (is(c, right_key)) {
                if (this->at < this->length) {
                    this->at++;
                    move_right(this, true);
                }
            }
            else if (is(c, left_key)) {
                if (this->at) {
                    this->at--;
                    move_left(this, true);
                }
            }
            
            else if (is(c, jump_key)) jump(c, this);
            
            else if (is(c, save_key)) save(this);
                                    
            else if (is(c, rename_key)) rename_file(this->filename, this->message);
            
            else if (is(c, quit_key)) {
                if (this->saved) close_buffer();
                
            } else if (is(c, force_quit_key)) {
                if (this->saved or confirmed("quit without saving")) close_buffer();
                
            } else if (is(c, next_buffer_key)) {
                if (active < buffer_count - 1) {
                    active++;
                    sprintf(buffers[active]->message, "set active buffer [%lu]", active);
                }
                
            } else if (is(c, previous_buffer_key)) {
                if (active) {
                    active--;
                    sprintf(buffers[active]->message, "set active buffer [%lu]", active);
                }
                
            } else if (is(c, function_key)) {
                sprintf(this->message, "{function}");
        
            } else sprintf(this->message, "unknown command %c(%d)", *c, (int)*c);
                
        } else if (this->mode == select_mode) {
            sprintf(this->message, "error: select mode not implemented.");
            this->mode = command_mode;
            
        } else if (this->mode == edit_mode) {
            
            if (bigraph(edit_exit, p, c)) {
                
                backspace(this);
                this->mode = command_mode;
                
                save(this);
                if (this->saved) close_buffer();
                
            } else if (bigraph(left_exit, p, c) or bigraph(right_exit, p, c)) {
                backspace(this);
                this->mode = command_mode;
                
            } else if (is(c, 127)) {
                if (this->at and this->length) {
                    backspace(this);
                    this->saved = false;
                }
                
            } else if (is(c, 27)) {
                unicode c = read_unicode();
                
                if (is(c, '[')) {
                    unicode c = read_unicode();
                    
                    if (is(c, 'A')) move_up(this);
                    else if (is(c, 'B')) move_down(this);
                    else if (is(c, 'C')) {
                        if (this->at < this->length) {
                            this->at++;
                            move_right(this, true);
                        }
                    } else if (is(c, 'D')) {
                        if (this->at) {
                            this->at--;
                            move_left(this, true);
                        }
                    }
                } else if (is(c, 27))
                    this->mode = command_mode;
                
            } else {
                insert(c, this);
                this->at++;
                free(this->lines);
                this->lines = generate_line_view(this);
                syntax_highlight(this);
                move_right(this, true);
                
                this->saved = false;
                if (++autosave_counter == this->options.edits_until_active_autosave) {
                    autosave_counter = 0;
                    autosave(this);
                }
            }
            
        } else {
            sprintf(this->message, "unknown mode: %d", this->mode);
            this->mode = command_mode;
        }
        p = c;
    }
//    clear_history();
    restore_terminal();
    printf("%s", restore_screen);
}









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
//            line++;
//            if (line >= file->render.count or
//                not file->render.lines[line].continued) {
//
//                if (file->render.count + 1 >= file->render.capacity)
//                    file->render.lines =
//                    realloc(file->render.lines,
//                            sizeof(struct render_line) *
//                            (file->render.capacity = 2 * (file->render.capacity + 1)));
//
//                memmove(file->render.lines + line + 1,
//                        file->render.lines + line,
//                        sizeof(struct render_line) *
//                        (file->render.count - line));
//                file->render.lines[line] = (struct render_line){.continued = true};
//                file->render.count++;
//            } else {
//                file->render.lines[line].length = 0;
//                file->render.lines[line].visual_length = 0;
//            }
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
    
//    line++;
//    size_t delete_to = line;
//    while (delete_to < file->render.count and file->render.lines[delete_to].continued) delete_to++;
//
//    memmove(file->render.lines + line, file->render.lines + delete_to,
//            sizeof(struct render_line) * (file->render.count - delete_to));
//    file->render.count -= (delete_to - line);
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
    
    struct render_line* render_line = file->render.lines + file->render_cursor.line;
    
    while (file->render_cursor.column) {
        uint8_t c = render_line->line[--file->render_cursor.column];

        if ((c >> 6) != 2) {
            file->visual_cursor.column--;
            if (file->visual_screen.column > negative_view_shift_margin) file->visual_screen.column--;
            else if (file->visual_origin.column) file->visual_origin.column--;
            else file->visual_screen.column--;
        }
        if ((c >> 6) != 2 and c != 10) break;
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

    struct render_line* new_render_line = file->render.lines + file->render_cursor.line;
    
    while (file->render_cursor.column < new_render_line->length) {
        if ((new_render_line->line[file->render_cursor.column] >> 6) != 2) {
            file->visual_cursor.column++;
            if (file->visual_screen.column < file->window_columns - positive_view_shift_margin) file->visual_screen.column++;
            else file->visual_origin.column++;
        }
        
        ++file->render_cursor.column;
        if (file->render_cursor.column == new_render_line->length) break;
        uint8_t c = new_render_line->line[file->render_cursor.column];
        if ((c >> 6) != 2 and c != 10) break;
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
        
        move_left(file, true);
        
//        size_t old_render_at = render_at;
//        size_t new_render_at = render_at;
//
//        do new_render_at++;
//        while (new_render_at < file->render.count and
//               file->render.lines[new_render_at].continued);
        
        struct logical_line* new = file->logical.lines + at - 1;
        struct logical_line* old = file->logical.lines + at;
        
        if (new->length + old->length >= new->capacity)
            new->line = realloc(new->line, new->capacity = 2 * (new->capacity + old->length));
        
        if (old->length) {
            memcpy(new->line + new->length, old->line, old->length);
            new->length += old->length;
        }
        
        memmove(file->logical.lines + at,
                file->logical.lines + at + 1,
                sizeof(struct logical_line)
                * (file->logical.count - (at + 1)));
        file->logical.count--;
        
//        memmove(file->render.lines + old_render_at, file->render.lines + new_render_at,
//                sizeof(struct render_line) * (file->render.count - new_render_at));
//        file->render.count -= (new_render_at - old_render_at);
        
        memmove(file->render.lines + render_at,
                file->render.lines + render_at + 1,
                sizeof(struct render_line)
                * (file->render.count - (render_at + 1)));
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
                
            } else { printf("c = %lu\n", (size_t) c); abort(); }
        } else { printf("c = %lu\n", (size_t) c); abort(); }
    }
    else if (c == 27) *quit = true;
    else if (c == '1') adjust_window_size(file, screen);
    else { printf("c = %lu\n", (size_t) c); abort(); }
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
    
    while ((fuzz and input_index < input_count) or not quit) {
        display(&file, screen);
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

int main() { editor(0,0); }















