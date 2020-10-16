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
    jump = 'k',
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

//static const char
//    *left_exit = "wf",
//    *right_exit = "oj",
//    *edit_exit = "wq",
//    *jump_top = "ko",
//    *jump_bottom = "km",
//    *jump_begin = "kj",
//    *jump_end = "kl";

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
        
//        if (is(c, jump_key) && i < size) jump(data[i++], c, &cursor, &origin, &screen, &desired, window, &at, length, lines, line_count);
        
        if (false) {}
        
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





























/*


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
#include <pthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <readline/history.h>
#include <readline/readline.h>

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
    edit_key = 'e',         hard_edit_key = 'E',
    select_key = 's',
    
    up_key = 'o',           down_key = 'i',
    left_key = 'j',         right_key = ';',
    
    word_up_key = 'O',      word_down_key = 'I',
    word_left_key = 'J',    word_right_key = ':',
    
    jump_key = 'k',         find_key = 'l',
    cut_key = 'd',          paste_key = 'a',
    
    rename_key = 'W',       save_key = 'w',
    force_quit_key = 'Q',   quit_key = 'q',
    
    redo_key = 'r',         undo_key = 'u',
    function_key = 'f',     option_key = 'p',
    
    next_buffer_key = 'n',  previous_buffer_key = 'v',
};

static const char
    *left_exit = "wf",
    *right_exit = "oj",
    *edit_exit = "wq",
    *jump_top = "ko",
    *jump_bottom = "km",
    *jump_begin = "kj",
    *jump_end = "kl",
    *shell_command_key = "fd";

static const char
    *set_cursor = "\033[%lu;%luH",
    *clear_screen = "\033[1;1H\033[2J",
    *clear_line = "\033[2K",
    *save_screen = "\033[?1049h",
    *restore_screen = "\033[?1049l";

#define set_color "\033[38;5;%lum"
#define reset_color "\033[0m"

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

struct clipboard {
    char* text;
    size_t length;
};

struct options {
    size_t wrap_width;
    size_t tab_width;
    bool show_status;
    bool use_txt_extension_when_absent;
    bool use_c_syntax_highlighting;
    bool show_line_numbers;
    bool preserve_autosave_contents_on_save;
    unsigned int ms_until_inactive_autosave;
    unsigned int edits_until_active_autosave;
    bool pause_on_shell_command_output;
};

struct colored_range {
    long color;
    struct location start;
    struct location end;
};

struct file {
    struct line* lines;
    unicode* text;
    struct colored_range* coloring;
    size_t coloring_count;
    size_t window_columns;
    size_t window_rows;
    size_t length;
    size_t at;
    size_t line_count;
    bool saved;
    unsigned int id;
    enum editor_mode mode;
    struct options options;
    struct location origin;
    struct location cursor;
    struct location screen;
    struct location desired;
    char message[4096];
    char filename[4096];
    char autosave_name[4096];
    pthread_t autosave_thread;
};


static volatile size_t active = 0;
static volatile size_t buffer_count = 0;
static struct file** buffers = NULL;

struct file empty_buffer = {
    .lines = NULL,
    .text = NULL,
    .coloring = NULL,
    .coloring_count = 0,
    .window_columns = 0,
    .window_rows = 0,
    .length = 0,
    .at = 0,
    .line_count = 0,
    .id = 0,
    .mode = command_mode,
    .saved = true,
    .options = {
        .wrap_width = 128,
        .tab_width = 8,
        .show_status = false,
        .use_txt_extension_when_absent = true,
        .use_c_syntax_highlighting = false,
        .show_line_numbers = false,
        .preserve_autosave_contents_on_save = false,
        .ms_until_inactive_autosave = 30 * 1000,
        .edits_until_active_autosave = 30,
        .pause_on_shell_command_output = true,
    },
    .origin = {0, 0},
    .cursor = {0, 0},
    .screen = {0, 0},
    .desired = {0, 0},
    .message = {0},
    .filename = {0},
    .autosave_name = {0},
    .autosave_thread = 0,
};

struct termios terminal = {0};

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

void* autosaver(void* _buffer) {
    struct file* buffer = _buffer;
    while (buffer->mode != quit) {
        
        for (unsigned int i = 0; i < buffer->options.ms_until_inactive_autosave; i++) {
            if (buffer->mode == quit) return NULL;
            usleep(1000);
        }
        char saved_message[4096] = {0};
        strcpy(saved_message, buffer->message);
        sprintf(buffer->message, "autosaving..");
        
        for (unsigned int i = 0; i < 500; i++) {
            if (buffer->mode == quit) return NULL;
            usleep(1000);
        }
        
        autosave(buffer);
        strcpy(buffer->message, saved_message);
    }
    return NULL;
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

static inline bool is(unicode u, char c) {
    return u and !strncmp(u, &c, 1);
}

static inline bool bigraph(const char* seq, unicode c0, unicode c1) {
    return is(c0, seq[0]) and is(c1, seq[1]);
}

static inline unicode read_unicode() {
    
    unsigned char c = read_byte_from_stdin();
    size_t total = 0, count = 0;
    unicode bytes = malloc(sizeof(char));
    bytes[total++] = c;
    
    if ((c >> 3) == 30) count = 3;
    else if ((c >> 4) == 14) count = 2;
    else if ((c >> 5) == 6) count = 1;
    for (size_t i = 0; i < count; i++) {
        bytes = realloc(bytes, sizeof(char) * (total + 1));
        bytes[total++] = read_byte_from_stdin();
    }
    bytes = realloc(bytes, sizeof(char) * (total + 1));
    bytes[total++] = '\0';
    return bytes;
}

static inline void insert(unicode c, struct file* file) {
    if (file->at > file->length) {
        dump_and_panic();
        return;
    }
    
    file->text = realloc(file->text, sizeof(unicode) * (file->length + 1));
    
    memmove(file->text + file->at + 1,
            file->text + file->at,
            sizeof(unicode) * (file->length - file->at));
    
    file->length++;
    file->text[file->at] = c;
}

static inline void delete(struct file* file) {
    if (file->at > file->length) {
        dump_and_panic();
        return;
    }
    
    if (!file->at or !file->length) return;
    
    free(file->text[file->at - 1]);
    
    memmove(file->text + file->at - 1,
            file->text + file->at,
            sizeof(unicode) * (file->length - file->at));
    
    file->text = realloc(file->text, sizeof(unicode) * --file->length);
}

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

static inline bool unicode_string_equals_literal(unicode* s1, const char* s2, size_t n) {
    while (n and is(*s1, *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    return !n;
}

static inline void syntax_highlight(struct file* file) {
    file->coloring_count = 0;
    
    if (!file->options.use_c_syntax_highlighting) return;
            
    const int keyword_count = 31;
    
    const char* keywords[] = {
        "0", "1",  "(", ")",
        "{", "}", "=", "+",
        
        "exit", "puts", "MACRO",
        
        "void", "int", "static", "inline",
        "struct", "unsigned", "sizeof", "const",
        
        "char", "for", "if", "else",
        "while", "break", "continue", "long",
        
        "float", "double", "short", "type"
    };
    
    const long colors[] = {
        214, 214, 246, 246,
        246, 246, 246, 246,
        
        41, 41, 52,
        
        33, 33, 33, 33,
        33, 33, 33, 33,
        
        33, 33, 33, 33,
        33, 33, 33, 33,
        
        33, 33, 33, 46,
    };
    
    for (size_t line = 0; line < file->line_count; line++) {
        for (size_t column = 0; column < file->lines[line].length; column++) {
            for (int k = 0; k < keyword_count; k++) {
                if (file->lines[line].length - column >= strlen(keywords[k]) and
                    unicode_string_equals_literal(file->lines[line].line + column,
                                                  keywords[k], strlen(keywords[k]))) {
                    
                    struct location start = {line, column};
                    struct location end = {line, column + strlen(keywords[k])};
                    struct colored_range node = {colors[k], start, end};
                    
                    file->coloring = realloc(file->coloring, sizeof(struct colored_range)
                                             * (file->coloring_count + 1));
                    file->coloring[file->coloring_count++] = node;
                }
            }
        }
    }
}

static inline void display(struct file* file) {
    if (!file->window_columns and !file->window_rows) return;
    
    char* screen = NULL;
    size_t screen_length = 0;
    struct location screen_cursor = {1, 1};
    
    unsigned int line_number_width = floor(log10(file->line_count)) + 1;
    
    for (size_t line = file->origin.line; line < fmin(file->origin.line + file->window_rows - 1, file->line_count); line++) {
        
        size_t line_length = 0;
        
        if (file->options.show_line_numbers) {
            if (line <= file->cursor.line)
                screen_cursor.column += line_number_width + 2;
            char line_number_buffer[128] = {0};
            sprintf(line_number_buffer, set_color "%*lu" reset_color set_color "  " reset_color,
                    59L, line_number_width, line + 1 (because people are stupid), 62L);
            
            for (int i = 0; i < strlen(line_number_buffer); i++) {
                screen = realloc(screen, sizeof(char) * (screen_length + 1));
                screen[screen_length++] = line_number_buffer[i];
            }
        }
        
        for (size_t column = file->origin.column; column < fmin(file->origin.column + file->window_columns - (line_number_width + 2), file->lines[line].length); column++) {
            
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

static inline struct line* generate_line_view(struct file* file) {
    size_t length = 0;
    struct line* lines = malloc(sizeof(struct line));
    lines[0].line = file->text;
    lines[0].continued = 0;
    lines[0].length = 0;
    file->line_count = 1;
    for (size_t i = 0; i < file->length; i++) {
        if (is(file->text[i], '\n')) {
            lines[file->line_count - 1].continued = false;
            length = 0;
        } else {
            if (is(file->text[i], '\t')) length += file->options.tab_width; else length++;
            lines[file->line_count - 1].length++;
            if (file->options.wrap_width and length >= file->options.wrap_width) {
                lines[file->line_count - 1].continued = true;
                length = 0;
            }
        }
        if (!length) {
            lines = realloc(lines, sizeof(struct line) * (file->line_count + 1));
            lines[file->line_count].line = file->text + i + 1;
            lines[file->line_count].continued = false;
            lines[file->line_count++].length = 0;
        }
    } return lines;
}

static inline void move_left(struct file* file, bool user) {
    
    if (file->cursor.column) {
        
        file->cursor.column--;
        
        if (file->screen.column > 5)
            file->screen.column--;
        else if (file->origin.column)
            file->origin.column--;
        else
            file->screen.column--;
        
    } else if (file->cursor.line) {
        
        file->cursor.line--;
        
        file->cursor.column = file->lines[file->cursor.line].length - file->lines[file->cursor.line].continued;
        
        if (file->screen.line > 5)
            file->screen.line--;
        else if (file->origin.line)
            file->origin.line--;
        else
            file->screen.line--;
        
        if (file->cursor.column > file->window_columns - 5) {
            file->screen.column = file->window_columns - 5;
            file->origin.column = file->cursor.column - file->screen.column;
        } else {
            file->screen.column = file->cursor.column;
            file->origin.column = 0;
        }
    }
    if (user)
        file->desired = file->cursor;
}

static inline void move_right(struct file* file, bool user) {
    
    if (file->cursor.column < file->lines[file->cursor.line].length) {
        
        file->cursor.column++;
        
        if (file->screen.column < file->window_columns - 3)
            file->screen.column++;
        else
            file->origin.column++;
        
    } else if (file->cursor.line < file->line_count - 1) {
        
        file->cursor.column = file->lines[file->cursor.line++].continued;
        
        if (file->screen.line < file->window_rows - 3)
            file->screen.line++;
        else
            file->origin.line++;
        
        if (file->cursor.column > file->window_columns - 3) {
            file->screen.column = file->window_columns - 3;
            file->origin.column = file->cursor.column - file->screen.column;
        } else {
            file->screen.column = file->cursor.column;
            file->origin.column = 0;
        }
    }
    if (user) file->desired = file->cursor;
}

static inline void move_up(struct file* file) {
    
     if (!file->cursor.line) {
         file->screen = file->cursor = file->origin = (struct location){0, 0};
         file->at = 0;
         return;
     }
     const size_t column_target = fmin(file->lines[file->cursor.line - 1].length, file->desired.column);
     const size_t line_target = file->cursor.line - 1;
     while (file->cursor.column > column_target or file->cursor.line > line_target) {
         move_left(file, false);
         file->at--;
     }
     if (file->cursor.column > file->window_columns - 5) {
         file->screen.column = file->window_columns - 5;
         file->origin.column = file->cursor.column - file->screen.column;
     } else {
         file->screen.column = file->cursor.column;
         file->origin.column = 0;
     }
 }

static inline void move_down(struct file* file) {
    
    if (file->cursor.line >= file->line_count - 1) {
        while (file->at < file->length) {
            move_right(file, false);
            file->at++;
        }
        return;
    }
    
    const size_t column_target = fmin(file->lines[file->cursor.line + 1].length, file->desired.column);
    const size_t line_target = file->cursor.line + 1;
    
    while (file->cursor.column < column_target or file->cursor.line < line_target) {
        move_right(file, false);
        file->at++;
    }
    
    if (file->cursor.line and file->lines[file->cursor.line - 1].continued and !column_target) {
        move_left(file, false);
        file->at--;
    }
}

static inline void jump(unicode c0, struct file* file) {
    unicode c1 = read_unicode();
    if (bigraph(jump_top, c0, c1)) { file->screen = file->cursor = file->origin = (struct location){0, 0}; file->at = 0; }
    else if (bigraph(jump_bottom, c0, c1)) while (file->at < file->length) move_down(file);
    else if (bigraph(jump_begin, c0, c1)) while (file->cursor.column) { (file->at)--; move_left(file, true); }
    
    else if (bigraph(jump_end, c0, c1)) {
        while (file->cursor.column < file->lines[file->cursor.line].length) {
            file->at++;
            move_right(file, true);
        }
    }
}

static inline struct file* create_new_buffer() {
    buffers = realloc(buffers, sizeof(struct file*) * (buffer_count + 1));
    buffers[buffer_count] = malloc(sizeof(struct file));
    
    struct file* buffer = buffers[buffer_count];
    *(buffers[buffer_count]) = empty_buffer;
    
    active = buffer_count;
    buffer_count++;
    
    buffer->id = rand();
    sprintf(buffer->autosave_name, "%s/%x/", autosave_directory, buffer->id);
    
    buffer->lines = generate_line_view(buffer);
    syntax_highlight(buffer);
        
    pthread_create(&buffer->autosave_thread, NULL, autosaver, buffer);
    
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

    int character = 0;
    while ((character = fgetc(file)) != EOF) {
        unsigned char c = character;
        size_t length = 0, count = 0;
        
        unicode bytes = malloc(sizeof(char));
        bytes[length++] = c;
        
        if ((c >> 3) == 30) count = 3;
        else if ((c >> 4) == 14) count = 2;
        else if ((c >> 5) == 6) count = 1;
        
        for (size_t i = 0; i < count; i++) {
            bytes = realloc(bytes, sizeof(char) * (length + 1));
            bytes[length++] = fgetc(file);
        }
        
        bytes = realloc(bytes, sizeof(char) * (length + 1));
        bytes[length++] = '\0';
        
        buffer->text = realloc(buffer->text, sizeof(unicode) * (buffer->length + 1));
        buffer->text[buffer->length++] = bytes;
    }
    fclose(file);
    strncpy(buffer->filename, given_filename, 4096);
    
    buffer->lines = generate_line_view(buffer);
    syntax_highlight(buffer);
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

static inline void prompt(const char* message, char* response, int max, long color) {
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    printf(set_cursor, window.ws_row, 0);
    printf("%s", clear_line);
    printf(set_color "%s: " reset_color, color, message);
    memset(response, 0, sizeof(char) * (max));
    restore_terminal();
    char* line = readline("");
    add_history(line);
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
    char* line = readline(prompt);
    add_history(line);
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

static inline void backspace(struct file* file) {
    delete(file);
    file->at--;
    move_left(file, true);
    free(file->lines);
    file->lines = generate_line_view(file);
    syntax_highlight(file);
}

enum CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    CXString spelling = clang_getCursorSpelling(cursor);
    CXString kind = clang_getCursorKindSpelling(clang_getCursorKind(cursor));
    
    printf("Cursor '%s' of kind '%s'.\n", clang_getCString(spelling), clang_getCString(kind));
    
    CXSourceRange range = clang_getCursorExtent(cursor);
    CXSourceLocation begin = clang_getRangeStart(range);
    CXSourceLocation end = clang_getRangeEnd(range);
        
    unsigned int line, column, offset;
    clang_getSpellingLocation(begin, NULL, &line, &column, &offset);
    printf("begin: (%u,%u) : %u \n", line, column, offset);
    
    clang_getSpellingLocation(end, NULL, &line, &column, &offset);
    printf("end: (%u,%u) : %u \n", line, column, offset);
    
    clang_disposeString(spelling);
    clang_disposeString(kind);
    
    return CXChildVisit_Recurse;
}

/// example code for cliboard stuff:

//    clipboard_c* cb = clipboard_new(NULL);
//    const char* string = clipboard_text(cb);
//    puts(string);
//    clipboard_set_text(cb, "hello world");
//    clipboard_free(cb);
//    exit(0);

int main(const int argc, const char** argv) {

    srand((unsigned) time(0));
    using_history();
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
                if (this->saved)
                    close_buffer();
                
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
    clear_history();
    restore_terminal();
    printf("%s", restore_screen);
}

*/
