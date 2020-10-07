///
///        My Text Editor.
///
///   Created by: Daniel Rehman
///
///   Created on: 2005122.113101
///

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

// --------------------------------------
/// Parameters:
// --------------------------------------

static const long rename_color = 214L;
static const long confirm_color = 196L;
static const long edit_status_color = 234L;
static const long command_status_color = 239L;
static const long edited_flag_color = 130L;

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
    function_key = 'f',     option_key = 'p'
};

static const char
*left_exit = "wf",
*right_exit = "oj",
*edit_exit = "wq",
*jump_top = "ko",
*jump_bottom = "km",
*jump_begin = "kj",
*jump_end = "kl";

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

struct file {
    struct line* lines;
    unicode* text;
    size_t window_columns;
    size_t window_rows;
    size_t wrap_width;
    size_t tab_width;
    size_t length;
    size_t at;
    size_t line_count;
    enum editor_mode mode;
    bool saved;
    bool show_status;
    struct location origin;
    struct location cursor;
    struct location screen;
    struct location desired;
    char message[2048];
    char filename[4096];
};

unicode** global_text = NULL;
size_t *global_length = NULL;
struct termios terminal = {0};

static inline void get_datetime(char buffer[16]) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    strftime(buffer, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static void dump() {
    char tempname[4096] = {0};
    char datetime[16] = {0};
    get_datetime(datetime);
    strcpy(tempname, "temporary_savefile_");
    strcat(tempname, datetime);
    strcat(tempname, ".txt");
    FILE* tempfile = fopen(tempname, "w");
    if (!tempfile) tempfile = stdout;
    for (size_t i = 0; i < *global_length; i++)
        fputs((*global_text)[i], tempfile);
    fclose(tempfile);
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

static inline bool is(unicode u, char c) {
    return u && !strncmp(u, &c, 1);
}

static inline bool bigraph(const char* seq, unicode c0, unicode c1) {
    return is(c0, seq[0]) && is(c1, seq[1]);
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
    
    if (!file->at || !file->length)
        return;
    
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
        file->mode == edit_mode || file->mode == hard_edit_mode
            ? edit_status_color
            : command_status_color;
    
    printf(set_color "%s" reset_color, color, file->filename);
    printf(set_color "%s" reset_color, edited_flag_color, file->saved ? "" : " *");
    printf(set_color "  %s" reset_color, color, file->message);
}

static inline void display(struct file* file) {
    
    size_t screen_length = 0;
    struct location screen_cursor = {1, 1};
    char screen[file->window_columns * file->window_rows + 1];
    
    memset(screen, 0, sizeof screen);

    for (size_t line = file->origin.line; line < fmin(file->origin.line + file->window_rows - 1, file->line_count); line++) {
        for (size_t column = file->origin.column; column < fmin(file->origin.column + file->window_columns - 1, file->lines[line].length); column++) {
            
            unicode g = file->lines[line].line[column];
            
            if (line < file->cursor.line || (line == file->cursor.line && column < file->cursor.column)) {
                if (is(g, '\t'))
                    screen_cursor.column += file->tab_width;
                else
                    screen_cursor.column++;
            }
            
            if (is(g, '\t')) {
                for (size_t i = 0; i < file->tab_width; i++)
                    screen[screen_length++] = ' ';
            } else {
                for (size_t i = 0; i < strlen(g); i++)
                    screen[screen_length++] = g[i];
            }
        }
        
        if (line < file->cursor.line) {
            screen_cursor.line++;
            screen_cursor.column = 1;
        }
        
        screen[screen_length++] = '\n';
    }
    
    printf("%s%s", clear_screen, screen);
    
    if (file->show_status)
        print_status_bar(file);
    
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
            if (is(file->text[i], '\t')) length += file->tab_width; else length++;
            lines[file->line_count - 1].length++;
            if (file->wrap_width && length >= file->wrap_width) {
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
     while (file->cursor.column > column_target || file->cursor.line > line_target) {
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
    while (file->cursor.column < column_target || file->cursor.line < line_target) {
        move_right(file, false);
        file->at++;
    }
    if (file->cursor.line && file->lines[file->cursor.line - 1].continued && !column_target) {
        move_left(file, false);
        file->at--;
    }
}

static void jump(unicode c0, struct file* file) {
    unicode c1 = read_unicode();
    if (bigraph(jump_top, c0, c1)) { file->screen = file->cursor = file->origin = (struct location){0, 0}; file->at = 0; }
    else if (bigraph(jump_bottom, c0, c1)) while (file->at < file->length - 1) move_down(file);
    else if (bigraph(jump_begin, c0, c1)) while (file->cursor.column) { (file->at)--; move_left(file, true); }
    
    else if (bigraph(jump_end, c0, c1)) {
        while (file->cursor.column < file->lines[file->cursor.line].length) {
            file->at++;
            move_right(file, true);
        }
    }
}

static inline void open_file(struct file* buffer) {
    FILE* file = fopen(buffer->filename, "r");
    if (!file) {
        perror("fopen");
        exit(1);
    }
    int c = 0;
    while ((c = fgetc(file)) != EOF) {
        size_t u_length = 1;
        unicode u = malloc(sizeof(char)); u[0] = c;
        size_t count = 0;
        unsigned char cc = c;
        if ((cc >> 3) == 30) count = 3;
        else if ((c >> 4) == 14) count = 2;
        else if ((cc >> 5) == 6) count = 1;
        for (size_t i = 0; i < count; i++) {
            u = realloc(u, sizeof(char) * (u_length + 1));
            u[u_length++] = fgetc(file);
        }
        u = realloc(u, sizeof(char) * (u_length + 1));
        u[u_length++] = 0;
        buffer->text = realloc(buffer->text, sizeof(unicode) * (buffer->length + 1));
        buffer->text[buffer->length++] = u;
    }
    fclose(file);
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
        else printf("error: please type \"yes\" or \"no\".\n");
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
    fgets(response, max, stdin);
    configure_terminal();
    response[strlen(response) - 1] = '\0';
}

static inline void save(struct file* buffer) {
    bool prompted = false;
    if (!strlen(buffer->filename)) { prompt("filename", buffer->filename, 4096, rename_color); prompted = true; }
    
    if (!strlen(buffer->filename)) {
        sprintf(buffer->message, "aborted save.");
        return;
    } else if (prompted && access(buffer->filename, F_OK) != -1 && ! confirmed("file already exists, overwrite")) {
        strcpy(buffer->filename, "");
        sprintf(buffer->message, "aborted save.");
        return;
    }
    FILE* file = fopen(buffer->filename, "w+");
    if (!file) {
        sprintf(buffer->message, "save unsuccessful: %s", strerror(errno));
        return;
    } else {
        for (size_t i = 0; i < buffer->length; i++) fputs(buffer->text[i], file);
        if (ferror(file)) sprintf(buffer->message, "save unsuccessful: %s", strerror(errno));
        else sprintf(buffer->message, "saved.");
    }
    fclose(file);
    buffer->saved = true;
}

static inline void rename_file(char* old, char* message) {
    char new[4096]; prompt("filename", new, 4096, rename_color);
    if (!strlen(new)) sprintf(message, "aborted rename.");
    else if (access(new, F_OK) != -1 && !confirmed("file already exists, overwrite")) sprintf(message, "aborted rename.");
    else if (rename(old, new)) sprintf(message, "rename unsuccessful: %s", strerror(errno));
    else { strncpy(old, new, 4096); sprintf(message, "renamed."); }
}

static void read_option(struct file* file) {
    unicode c = read_unicode();
    if (is(c, '0')) strcpy(file->message, "");
    else if (is(c, '[')) file->show_status = !file->show_status;
    else if (is(c, 'p')) sprintf(file->message, "options: 0:clear [:togg p:help t:tab w:wrap l:get ");
    else if (is(c, 't')) {
        char tab_width_string[64];
        prompt("tab width", tab_width_string, 64, rename_color);
        size_t n = atoi(tab_width_string);
        file->tab_width = n ? n : 8;
        file->screen = file->cursor = file->origin = (struct location){0, 0}; file->at = 0;
    } else if (is(c, 'w')) {
        char wrap_width_string[64];
        prompt("wrap width", wrap_width_string, 64, rename_color);
        size_t n = atoi(wrap_width_string);
        file->wrap_width = n ? n : 128;
        file->screen = file->cursor = file->origin = (struct location){0, 0}; file->at = 0;
    } else if (is(c, 'l')) {
        sprintf(file->message, "ww = %lu | tw = %lu | ", file->wrap_width, file->tab_width);
    } else {
        sprintf(file->message, "error: unknown option argument. try ? for help.");
    }
}

static inline void destroy(struct file* file) {
    for (size_t i = 0; i < file->length; i++)
    free(file->text[i]);
    free(file->text);
    free(file->lines);
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
}

int main(const int argc, const char** argv) {
    
    struct file file = {
        .lines = NULL,
        .text = NULL,
        .window_columns = 0,
        .window_rows = 0,
        .wrap_width = 128,
        .tab_width = 8,
        .length = 0,
        .at = 0,
        .line_count = 0,
        .mode = command_mode,
        .saved = true,
        .show_status = true,
        .origin = {0, 0},
        .cursor = {0, 0},
        .screen = {0, 0},
        .desired = {0, 0},
        .message = {0},
        .filename = {0},
    };
    
    signal(SIGINT, signal_interrupt);
    printf("%s", save_screen);
    configure_terminal();
    
    if (argc > 1) {
        strncpy(file.filename, argv[1], 4096);
        open_file(&file);
    }
    
    file.lines = generate_line_view(&file);
    
    unicode c = 0, p = 0;
    
    while (file.mode != quit) {
        
        adjust_window_size(&file);
        display(&file);
        fflush(stdout);
        
        c = read_unicode();
        
        if (file.mode == command_mode) {
            if (is(c, 27)) {}
            else if (is(c, edit_key)) file.mode = edit_mode;
            else if (is(c, hard_edit_key)) file.mode = hard_edit_mode;
            else if (is(c, select_key)) file.mode = select_mode;
            else if (is(c, option_key)) read_option(&file);
            
            
            else if (is(c, word_up_key)) {
                for (int i = 0; i < 5; i++)
                move_up(&file);
            }
            else if (is(c, word_down_key)) {
                for (int i = 0; i < 5; i++)
                move_down(&file);
            }
            
            else if (is(c, up_key)) move_up(&file);
            else if (is(c, down_key)) move_down(&file);
            
            else if (is(c, right_key)) {
                if (file.at < file.length) {
                    file.at++;
                    move_right(&file, true);
                }
            }
            else if (is(c, left_key)) {
                if (file.at) {
                    file.at--;
                    move_left(&file, true);
                }
            }
            
            else if (is(c, jump_key)) jump(c, &file);
            
            else if (is(c, save_key)) save(&file);
            
            else if (is(c, rename_key)) rename_file(file.filename, file.message);
            
            else if (is(c, quit_key)) {
                if (file.saved)
                    file.mode = quit;
                
            } else if (is(c, force_quit_key)) {
                if (file.saved || confirmed("quit without saving"))
                    file.mode = quit;
                
            } else {
                sprintf(file.message, "error: unknown command %d", (int) c[0]);
            }
            
        } else if (file.mode == select_mode) {
            sprintf(file.message, "error: select mode not implemented.");
            file.mode = command_mode;
            
        } else {
            if (file.mode == edit_mode && bigraph(edit_exit, p, c)) {
                
                backspace(&file);
                file.mode = command_mode;
                save(&file);
                if (file.saved) file.mode = quit;
                
            } else if (file.mode == edit_mode &&
                       (bigraph(left_exit, p, c) ||
                        bigraph(right_exit, p, c))) {
                
                backspace(&file);
                file.mode = command_mode;
                
            } else if (is(c, 127)) {
                if (file.at && file.length) {
                    backspace(&file);
                    file.saved = false;
                }
                
            } else if (is(c, 27)) {
                
                unicode c = read_unicode();
                
                if (is(c, '[')) {
                    
                    unicode c = read_unicode();
                    
                    if (is(c, 'A')) move_up(&file);
                    else if (is(c, 'B')) move_down(&file);
                    else if (is(c, 'C')) {
                        if (file.at < file.length) {
                            file.at++;
                            move_right(&file, true);
                        }
                    } else if (is(c, 'D')) {
                        if (file.at) {
                            file.at--;
                            move_left(&file, true);
                        }
                    }
                } else if (is(c, 27))
                    file.mode = command_mode;
                
            } else {
                insert(c, &file);
                file.at++;
                free(file.lines);
                file.lines = generate_line_view(&file);
                move_right(&file, true);
                file.saved = false;
            }
        }
        p = c;
    }
    destroy(&file);
    restore_terminal();
    printf("%s", restore_screen);
}
