//
//  comments.h
//  editor
//
//  Created by Daniel Rehman on 2009207.
//

#ifndef comments_h
#define comments_h





/*
 
 
 
 
 
 
 
 
 
 
 
 
 static const float cursor_blink_speed = 0.2;
 
 
 
 
 
 
 
 
 static float time = 0.0;
 float luminosity = 1.0;
 if (blink) {
     luminosity = fabs(sin(time));
     time += cursor_blink_speed;
 }
 
 if (line_numbers) {
     sprintf(screen, "%4lu ", line);
     screen_length += 5;
     cursor_x += char_width * 5;
 }
 
 
 
 
 
 
 
//        char buffer[3000] = {0};
//        int c = 0;
//        for (int i = 0; i < length; i++) {
//            for (int j = 0; text[i][j]; j++) {
//                buffer[c++] = text[i][j];
//            }
//        }
//
//        if (length) {
//            SDL_Surface* surface1 = TTF_RenderUTF8_Solid(font, buffer, white);
//            if (!surface1) {
//                fprintf(stderr,"error: could not create surface: %s\n", SDL_GetError());
//            }
//            SDL_Texture* texture1 = SDL_CreateTextureFromSurface(renderer, surface1);
//            if (!texture1) {
//                fprintf(stderr,"error: could not create texture: %s\n", SDL_GetError());
//            }
//            SDL_Rect rect1 = {0};
//            rect1.x = 0;
//            rect1.y = 0;
//            rect1.w = surface1->w;
//            rect1.h = surface1->h;
//            SDL_FreeSurface(surface1);
//
//
//            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
//            SDL_RenderClear(renderer);
//
//            SDL_RenderCopy(renderer, texture1, NULL, &rect1);
//
//            SDL_RenderPresent(renderer);
//
//            SDL_DestroyTexture(texture1);
//
//        } else {
//            SDL_Surface* surface1 = TTF_RenderUTF8_Solid(font, " ", white);
//            if (!surface1) {
//                fprintf(stderr,"error: could not create surface: %s\n", SDL_GetError());
//            }
//            SDL_Texture* texture1 = SDL_CreateTextureFromSurface(renderer, surface1);
//            if (!texture1) {
//                fprintf(stderr,"error: could not create texture: %s\n", SDL_GetError());
//            }
//            SDL_Rect rect1 = {0};
//            rect1.x = 0;
//            rect1.y = 0;
//            rect1.w = surface1->w;
//            rect1.h = surface1->h;
//            SDL_FreeSurface(surface1);
//
//
//            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
//            SDL_RenderClear(renderer);
//
//            SDL_RenderCopy(renderer, texture1, NULL, &rect1);
//
//            SDL_RenderPresent(renderer);
//
//            SDL_DestroyTexture(texture1);
//
//        }
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 ///
 ///        My Text Editor.
 ///
 ///   Created by: Daniel Rehman
 ///
 ///   Created on: 2005122.113101
 ///
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

 static size_t wrap_width = 128;
 static size_t tab_width = 8;

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

 // --------------------------------------



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

 static const char
     *set_cursor = "\033[%lu;%luH",
     *clear_screen = "\033[1;1H\033[2J",
     *clear_line = "\033[2K",
     *save_screen = "\033[?1049h",
     *restore_screen = "\033[?1049l";

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
     for (size_t i = 0; i < *global_length; i++) fputs((*global_text)[i], tempfile);
     fclose(tempfile);
 }

 static inline void dump_and_panic() { printf("panic: internal error: dump() & abort()'ing...\n"); dump(); abort(); }
 static inline void signal_interrupt(int unused) { printf("error: process interrupted, dump() & exit()'ing...\n"); dump(); exit(1); }

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

 static inline bool is(unicode u, char c) { return u && !strncmp(u, &c, 1); }
 static inline bool bigraph(const char* seq, unicode c0, unicode c1) { return is(c0, seq[0]) && is(c1, seq[1]); }

 static inline unicode read_unicode() {
     unicode bytes = 0; size_t total = 0, count = 0;
     unsigned char c = read_byte_from_stdin();
     bytes = realloc(bytes, sizeof(char) * (total + 1));
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

 static inline void insert(unicode c, size_t at, unicode** text, size_t* length) {
     if (at > *length) { dump_and_panic(); return; }
     *text = realloc(*text, sizeof(unicode) * (*length + 1));
     memmove(*text + at + 1, *text + at, sizeof(unicode) * (*length - at));
     ++*length; (*text)[at] = c;
 }

 static inline void delete(size_t at, unicode** text, size_t* length) {
     if (at > *length) { dump_and_panic(); return; }
     if (!at || !*length) return;
     free((*text)[at - 1]);
     memmove((*text) + at - 1, (*text) + at, sizeof(unicode) * (*length - at));
     *text = realloc(*text, sizeof(unicode) * (--*length));
 }

 static inline void print_status_bar(enum editor_mode mode, bool saved, char* message, struct winsize window, char* filename) {
     printf(set_cursor, window.ws_row, 1);
     printf("%s", clear_line);
     const long color = mode == edit_mode || mode == hard_edit_mode ? edit_status_color : command_status_color;
     printf(set_color "%s" reset_color, color, filename);
     printf(set_color "%s" reset_color, edited_flag_color, saved ? "" : " *");
     printf(set_color "  %s" reset_color, color, message);
 }

 static inline void display
  (enum editor_mode mode, struct location origin, struct location cursor, struct winsize window,
   struct line* lines, size_t line_count, char* message, bool saved, bool show_status,
   char* filename) {
     size_t screen_length = 0;
     char screen[window.ws_col * window.ws_row + 1];
     memset(screen, 0, sizeof screen);
     struct location screen_cursor = {1,1};
     for (size_t line = origin.line; line < fmin(origin.line + window.ws_row - 1, line_count); line++) {
         for (size_t column = origin.column; column < fmin(origin.column + window.ws_col - 1, lines[line].length); column++) {
             unicode g = lines[line].line[column];
             if (line < cursor.line || (line == cursor.line && column < cursor.column)) {
                 if (is(g, '\t')) screen_cursor.column += tab_width;
                 else screen_cursor.column++;
             } if (is(g, '\t')) for (size_t i = 0; i < tab_width; i++) screen[screen_length++] = ' ';
             else for (size_t i = 0; i < strlen(g); i++) screen[screen_length++] = g[i];
         } if (line < cursor.line) {
             screen_cursor.line++;
             screen_cursor.column = 1;
         } screen[screen_length++] = '\n';
     }
     printf("%s%s", clear_screen, screen);
     if (show_status) print_status_bar(mode, saved, message, window, filename);
     printf(set_cursor, screen_cursor.line, screen_cursor.column);
 }

 static inline struct line* generate_line_view(unicode* text, size_t text_length, size_t* count) {
     size_t length = 0; bool continued = false;
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
             if (is(text[i], '\t')) length += tab_width; else length++;  ///FIXME: bug is right here. tabs must  increase line length by tabwidth too! i think.
             lines[*count - 1].length++;
         }
         if (wrap_width && length >= wrap_width && !is(text[i],'\n')) {
             lines[*count - 1].continued = true;
             length = 0;
         }
         if (!length) {
             lines = realloc(lines, sizeof(struct line) * (*count + 1));
             lines[*count].line = text + i + 1;
             lines[*count].continued = continued;
             lines[(*count)++].length = 0;
         }
     } return lines;
 }

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
     } fflush(stdout);
 }

 static inline void move_left
  (struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
   struct line* lines, struct location* desired, bool user) {
     if (cursor->column) {
         cursor->column--;
         if (screen->column) screen->column--; else if (origin->column) origin->column--;
     } else if (cursor->line) {
         cursor->line--;
         cursor->column = lines[cursor->line].length - lines[cursor->line].continued;
         if (screen->line) screen->line--; else if (origin->line) origin->line--;
         if (cursor->column > window.ws_col - 1) {
             screen->column = window.ws_col - 1;
             origin->column = cursor->column - screen->column;
         } else {
             screen->column = cursor->column;
             origin->column = 0;
         }
     } if (user) *desired = *cursor;
 }

 static inline void move_right
  (struct location* cursor, struct location* origin, struct location* screen, struct winsize window,
   struct line* lines, size_t line_count, size_t length, struct location* desired, bool user) {
     if (cursor->column < lines[cursor->line].length) {
         cursor->column++;
         if (screen->column < window.ws_col - 1) screen->column++; else origin->column++;
     } else if (cursor->line < line_count - 1) {
         cursor->column = lines[cursor->line++].continued;
         if (screen->line < window.ws_row - 2) screen->line++; else origin->line++;
         if (cursor->column > window.ws_col - 1) {
             screen->column = window.ws_col - 1;
             origin->column = cursor->column - screen->column;
         } else {
             screen->column = cursor->column;
             origin->column = 0;
         }
     } if (user) *desired = *cursor;
 }

 static inline void move_up
 (struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
  size_t* point, struct line* lines, struct location* desired) {
     if (!cursor->line) {
         *screen = *cursor = *origin = (struct location){0, 0};
         *point = 0; return;
     }
     const size_t column_target = fmin(lines[cursor->line - 1].length, desired->column);
     const size_t line_target = cursor->line - 1;
     while (cursor->column > column_target || cursor->line > line_target) {
         move_left(cursor, origin, screen, window, lines, desired, false);
         (*point)--;
     }
     if (cursor->column > window.ws_col - 1) {
         screen->column = window.ws_col - 1;
         origin->column = cursor->column - screen->column;
     } else {
         screen->column = cursor->column;
         origin->column = 0;
     }
 }

 static inline void move_down
  (struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
   size_t* point, struct line* lines, size_t line_count, size_t length, struct location* desired) {
     if (cursor->line >= line_count - 1) {
         while (*point < length) {
             move_right(cursor, origin, screen, window, lines, line_count, length, desired, false);
             (*point)++;
         } return;
     }
     const size_t column_target = fmin(lines[cursor->line + 1].length, desired->column);
     const size_t line_target = cursor->line + 1;
     while (cursor->column < column_target || cursor->line < line_target) {
         move_right(cursor, origin, screen, window, lines, line_count, length, desired, false);
         (*point)++;
     }
     if (cursor->line && lines[cursor->line - 1].continued && !column_target) {
         move_left(cursor, origin, screen, window, lines, desired, false);
         (*point)--;
     }
 }

 static void jump(unicode c0,
   struct location *cursor, struct location *origin, struct location *screen, struct location *desired,
   struct winsize window, size_t* point, size_t length, struct line* lines, size_t line_count) {
     unicode c1 = read_unicode();
     if (bigraph(jump_top, c0, c1)) { *screen = *cursor = *origin = (struct location){0, 0}; *point = 0; }
     else if (bigraph(jump_bottom, c0, c1)) while (*point < length - 1) move_down(cursor, origin, screen, window, point, lines, line_count, length, desired);
     else if (bigraph(jump_begin, c0, c1)) while (cursor->column) { (*point)--; move_left(cursor, origin, screen, window, lines, desired, true); }
     else if (bigraph(jump_end, c0, c1)) while (cursor->column < lines[cursor->line].length) {  (*point)++; move_right(cursor, origin, screen, window, lines, line_count, length, desired, true); }
 }

 static inline void open_file(const char* filename, unicode** text, size_t* length) {
     FILE* file = fopen(filename, "r");
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
         *text = realloc(*text, sizeof(unicode) * (*length + 1));
         (*text)[(*length)++] = u;
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

 static inline void save(unicode* text, size_t length, char* filename, bool* saved, char* message) {
     bool prompted = false;
     if (!strlen(filename)) { prompt("filename", filename, 4096, rename_color); prompted = true; }
     
     if (!strlen(filename)) {
         sprintf(message, "aborted save.");
         return;
     } else if (prompted && access(filename, F_OK) != -1 && ! confirmed("file already exists, overwrite")) {
         strcpy(filename, "");
         sprintf(message, "aborted save.");
         return;
     }
     FILE* file = fopen(filename, "w+");
     if (!file) {
         sprintf(message, "save unsuccessful: %s", strerror(errno));
         return;
     } else {
         for (size_t i = 0; i < length; i++) fputs(text[i], file);
         if (ferror(file)) sprintf(message, "save unsuccessful: %s", strerror(errno));
         else sprintf(message, "saved.");
     }
     fclose(file);
     *saved = true;
 }

 static inline void rename_file(char* old, char* message) {
     char new[4096]; prompt("filename", new, 4096, rename_color);
     if (!strlen(new)) sprintf(message, "aborted rename.");
     else if (access(new, F_OK) != -1 && !confirmed("file already exists, overwrite")) sprintf(message, "aborted rename.");
     else if (rename(old, new)) sprintf(message, "rename unsuccessful: %s", strerror(errno));
     else { strncpy(old, new, 4096); sprintf(message, "renamed."); }
 }

 static void read_option(size_t* at,
                         struct location* cursor,
                         char* message,
                         struct location* origin,
                         struct location* screen,
                         bool* show_status) {
     unicode c = read_unicode();
     if (is(c, '0')) strcpy(message, "");
     else if (is(c, '[')) *show_status = !*show_status;
     else if (is(c, 'p')) sprintf(message, "options: 0:clear [:togg p:help t:tab w:wrap l:get ");
     else if (is(c, 't')) {
         char tab_width_string[64];
         prompt("tab width", tab_width_string, 64, rename_color);
         size_t n = atoi(tab_width_string);
         tab_width = n ? n : 8;
         *screen = *cursor = *origin = (struct location){0, 0}; *at = 0;
     } else if (is(c, 'w')) {
         char wrap_width_string[64];
         prompt("wrap width", wrap_width_string, 64, rename_color);
         size_t n = atoi(wrap_width_string);
         wrap_width = n ? n : 128;
         *screen = *cursor = *origin = (struct location){0, 0}; *at = 0;
     } else if (is(c, 'l')) {
         sprintf(message, "ww = %lu | tw = %lu | ", wrap_width, tab_width);
     } else {
         sprintf(message, "error: unknown option argument. try ? for help.");
     }
 }

 int main(const int argc, const char** argv) {
     enum editor_mode mode = command_mode;
     bool saved = true, show_status = true;
     char message[2048] = {0}, filename[4096] = {0};
     size_t length = 0, at = 0, line_count = 0;
     unicode* text = NULL, c = 0, p = 0;
     global_text = &text; global_length = &length; struct winsize window;
     struct location origin = {0,0}, cursor = {0,0}, screen = {0,0}, desired = {0,0};
     if (argc != 1) { open_file(argv[1], &text, &length); strncpy(filename, argv[1], 4096); }
     signal(SIGINT, signal_interrupt);
     printf("%s", save_screen);
     configure_terminal();
     struct line* lines = generate_line_view(text, length, &line_count);
     while (mode != quit) {
         
         ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
         display(mode, origin, cursor, window, lines, line_count, message, saved, show_status, filename);
         fflush(stdout);
         c = read_unicode();
         
         if (mode == command_mode) {
             if (is(c, 27)) {}
             else if (is(c, edit_key)) mode = edit_mode;
             else if (is(c, hard_edit_key)) mode = hard_edit_mode;
             else if (is(c, select_key)) mode = select_mode;
             else if (is(c, option_key)) read_option(&at, &cursor, message, &origin, &screen, &show_status);
             else if (is(c, up_key)) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
             
             else if (is(c, word_up_key)) for (int i = 0; i < 5; i++) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
             else if (is(c, word_down_key)) for (int i = 0; i < 5; i++) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
                         
             else if (is(c, down_key)) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
             else if (is(c, right_key)) { if (at < length) { at++; move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true); } }
             else if (is(c, left_key)) { if (at) { at--; move_left(&cursor, &origin, &screen, window, lines, &desired, true); } }
             else if (is(c, jump_key)) jump(c, &cursor, &origin, &screen, &desired, window, &at, length, lines, line_count);
             else if (is(c, save_key)) save(text, length, filename, &saved, message);
             else if (is(c, rename_key)) rename_file(filename, message);
             else if (is(c, quit_key) || is(c, force_quit_key)) { if (saved || (is(c, force_quit_key) && confirmed("quit without saving"))) mode = quit; }
             else { sprintf(message, "error: unknown command %d", (int) c[0]); }
             
         } else if (mode == select_mode) {
             sprintf(message, "error: select mode not implemented.");
             mode = command_mode;
             
         } else {
             if (mode == edit_mode && bigraph(edit_exit, p, c)) {
                 delete(at--, &text, &length);
                 move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                 mode = command_mode;
                 save(text, length, filename, &saved, message);
                 if (saved) mode = quit;
                 
             } else if (mode == edit_mode && (bigraph(left_exit, p, c) || bigraph(right_exit, p, c))) {
                 delete(at--, &text, &length);
                 move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                 free(lines); lines = generate_line_view(text, length, &line_count);
                 mode = command_mode;
                 
             } else if (is(c, 127)) {
                 if (at && length) {
                     delete(at--, &text, &length);
                     move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                     free(lines); lines = generate_line_view(text, length, &line_count);
                     saved = false;
                 }
             } else if (is(c, 27)) {
                 unicode c = read_unicode();
                 if (is(c, '[')) {
                     unicode c = read_unicode();
                          if (is(c, 'A')) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
                     else if (is(c, 'B')) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
                     else if (is(c, 'C') && at < length) { at++; move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true); }
                     else if (is(c, 'D') && at) { at--; move_left(&cursor, &origin, &screen, window, lines, &desired, true); }
                 } else if (is(c, 27)) mode = command_mode;
                 
             } else {
                 insert(c, at++, &text, &length);
                 free(lines); lines = generate_line_view(text, length, &line_count);
                 move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true);
                 saved = false;
             }
         }
         p = c;
     }
     for (size_t i = 0; i < length; i++) free(text[i]);
     free(text);
     free(lines);
     restore_terminal();
     printf("%s", restore_screen);
 }

 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 {
     
     static bool escape = false;
     static bool tab = false;
     static bool is_fullscreen = false;
     static bool rotation_mode = 0;
     
     const Uint8* key = SDL_GetKeyboardState(nullptr);
     
     const float power = (delta * camera_acceleration);
     rotation_mode = !!key[SDL_SCANCODE_C];
     if (key[SDL_SCANCODE_SPACE]) printf("0\n");
     if (key[SDL_SCANCODE_LSHIFT]) printf("1\n");
     if (key[SDL_SCANCODE_W]) printf("w\n");
     if (key[SDL_SCANCODE_S]) printf("s\n");
     if (key[SDL_SCANCODE_A]) printf("a\n");
     if (key[SDL_SCANCODE_D]) printf("d\n");
     
     SDL_Event e;
     while (SDL_PollEvent(&e)) {
         
         if (e.window.type == SDL_WINDOWEVENT_RESIZED) window_changed(camera, window);
         const Uint8* key = SDL_GetKeyboardState(nullptr);
         if (e.type == SDL_QUIT) gamemode = 0;
         
         if (e.type == SDL_MOUSEBUTTONDOWN) {
             printf("mouse event! [button: %d]\n", e.button.button);
         }
         
         if (e.type == SDL_MOUSEMOTION) {
             if (move_camera) rotate_camera(camera, {e.motion.xrel, e.motion.yrel}, rotation_mode);
         }
         if (e.type == SDL_KEYDOWN) {
             if (key[SDL_SCANCODE_GRAVE]) {
                 if (escape) {
                     disable_mouse_rotation();
                 } else if (tab) {
                     gamemode = 0;
                 }
             } else if (key[SDL_SCANCODE_1]) {
                 if (escape) {
                     rendermode = GL_FILL;
                     enable_mouse_rotation();
                 } else if (tab) {
                     gamemode = 1;
                 }
             } else if (key[SDL_SCANCODE_2]) {
                 if (escape) {
                     rendermode = GL_LINES;
                 } else if (tab) {
                     gamemode = 2;
                 }
             } else if (key[SDL_SCANCODE_3]) {
                 if (escape) {
                     debugmode = !debugmode;
                 } else if (tab) {
                     gamemode = 3;
                 }
             } else if (key[SDL_SCANCODE_4]) {
                 if (escape) {
                     debugmode = 2;
                 } else if (tab) {
                     gamemode = 4;
                 }
             }
             escape = false;
             tab = false;
             if (key[SDL_SCANCODE_RETURN]) {
                 is_fullscreen = !is_fullscreen;
                 SDL_SetWindowFullscreen(window, is_fullscreen);
             }
             if (key[SDL_SCANCODE_TAB]) tab = true;
             if (key[SDL_SCANCODE_ESCAPE]) escape = true;
             if (key[SDL_SCANCODE_X]) camera.upward = glm::vec3(0,1,0);
         }
     }
 }
 

 
 
 
 
 SDL_Window* window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
 if (!window) return fprintf(stderr,"error: could not create window: %s\n", SDL_GetError());
 
 SDL_WarpMouseInWindow(window, window_width / 2.0, window_height / 2.0);
 
 SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
 TTF_Font* font = TTF_OpenFont("/Library/Fonts/Arial Unicode.ttf", 24);
 if (!font) return fprintf(stderr,"error: could not get font: %s\n", TTF_GetError());
 SDL_Color color_white = { 255, 255, 255 };
               
 SDL_GLContext context = SDL_


 
 ////
 ////  main.c
 ////  editor
 ////
 ////  Created by Daniel Rehman on 2009196.
 ////
 //
 //#include <SDL2/SDL_ttf.h>
 //
 //#include <stdbool.h>
 //#include <stdlib.h>
 //#include <string.h>
 //#include <math.h>
 //#include <time.h>
 //#include <errno.h>
 //#include <unistd.h>
 //#include <sys/time.h>
 //#include <sys/ioctl.h>
 //#include <termios.h>
 //#include <errno.h>
 //#include <ctype.h>
 //
 //static unsigned window_width = 2000;
 //static unsigned window_height = 1500;
 //
 //int main() {
 //
 //    if (SDL_Init(SDL_INIT_EVERYTHING) < 0 || TTF_Init() < 0) fprintf(stderr, "error: could not initialize SDL2: %s\n", SDL_GetError());
 //
 //    SDL_Window* window = SDL_CreateWindow("window_name", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
 //                                          window_width, window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
 //    if (!window) fprintf(stderr,"error: could not create window: %s\n", SDL_GetError());
 //
 //    printf("Hello, World!\n");
 //    sleep(1);
 //    printf("Hello, World!\n");
 //    sleep(1);
 //
 //    SDL_CloseAudio();
 //    SDL_DestroyWindow(window);
 //    SDL_Quit();
 //    TTF_Quit();
 //
 //}
 
 
 
 
 
 
 if (key[SDL_SCANCODE_RETURN]) SDL_SetWindowFullscreen(window, true);

 
 //                printf("e.text.text = %d, %d, %d\n", e.key.keysym.scancode, e.key.keysym.sym, e.key.keysym.mod);
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 


 static size_t wrap_width = 128;
 static size_t tab_width = 8;

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



 typedef char* unicode;

 enum editor_mode {
     quit,
     command_mode,
     edit_mode,
     hard_edit_mode,
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


 static inline void insert(unicode c, size_t at, unicode** text, size_t* length) {
     if (at > *length) { dump_and_panic(); return; }
     *text = realloc(*text, sizeof(unicode) * (*length + 1));
     memmove(*text + at + 1, *text + at, sizeof(unicode) * (*length - at));
     ++*length; (*text)[at] = c;
 }

 static inline void delete(size_t at, unicode** text, size_t* length) {
     if (at > *length) { dump_and_panic(); return; }
     if (!at || !*length) return;
     free((*text)[at - 1]);
     memmove((*text) + at - 1, (*text) + at, sizeof(unicode) * (*length - at));
     *text = realloc(*text, sizeof(unicode) * (--*length));
 }

 static inline void display
  (enum editor_mode mode, struct location origin, struct location cursor, struct winsize window,
   struct line* lines, size_t line_count, char* message, bool saved, bool show_status,
   char* filename) {
     size_t screen_length = 0;
     char screen[window.ws_col * window.ws_row + 1];
     memset(screen, 0, sizeof screen);
     struct location screen_cursor = {1,1};
     for (size_t line = origin.line; line < fmin(origin.line + window.ws_row - 1, line_count); line++) {
         for (size_t column = origin.column; column < fmin(origin.column + window.ws_col - 1, lines[line].length); column++) {
             unicode g = lines[line].line[column];
             if (line < cursor.line || (line == cursor.line && column < cursor.column)) {
                 if (is(g, '\t')) screen_cursor.column += tab_width;
                 else screen_cursor.column++;
             } if (is(g, '\t')) for (size_t i = 0; i < tab_width; i++) screen[screen_length++] = ' ';
             else for (size_t i = 0; i < strlen(g); i++) screen[screen_length++] = g[i];
         } if (line < cursor.line) {
             screen_cursor.line++;
             screen_cursor.column = 1;
         } screen[screen_length++] = '\n';
     }
     printf("%s%s", clear_screen, screen);
     if (show_status) print_status_bar(mode, saved, message, window, filename);
     printf(set_cursor, screen_cursor.line, screen_cursor.column);
 }

 static inline struct line* generate_line_view(unicode* text, size_t text_length, size_t* count) {
     size_t length = 0; bool continued = false;
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
             if (is(text[i], '\t')) length += tab_width; else length++;  ///FIXME: bug is right here. tabs must  increase line length by tabwidth too! i think.
             lines[*count - 1].length++;
         }
         if (wrap_width && length >= wrap_width && !is(text[i],'\n')) {
             lines[*count - 1].continued = true;
             length = 0;
         }
         if (!length) {
             lines = realloc(lines, sizeof(struct line) * (*count + 1));
             lines[*count].line = text + i + 1;
             lines[*count].continued = continued;
             lines[(*count)++].length = 0;
         }
     } return lines;
 }

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
     } fflush(stdout);
 }

 static inline void move_left
  (struct location *cursor, struct location *origin, struct location *screen, struct winsize window,
   struct line* lines, struct location* desired, bool user) {
     if (cursor->column) {
         cursor->column--;
         if (screen->column) screen->column--; else if (origin->column) origin->column--;
     } else if (cursor->line) {
         cursor->line--;
         cursor->column = lines[cursor->line].length - lines[cursor->line].continued;
         if (screen->line) screen->line--; else if (origin->line) origin->line--;
         if (cursor->column > window.ws_col - 1) {
             screen->column = window.ws_col - 1;
             origin->column = cursor->column - screen->column;
         } else {
             screen->column = cursor->column;
             origin->column = 0;
         }
     } if (user) *desired = *cursor;
 }

 static inline void move_right
  (struct location* cursor, struct location* origin, struct location* screen, struct winsize window,
   struct line* lines, size_t line_count, size_t length, struct location* desired, bool user) {
     if (cursor->column < lines[cursor->line].length) {
         cursor->column++;
         if (screen->column < window.ws_col - 1) screen->column++; else origin->column++;
     } else if (cursor->line < line_count - 1) {
         cursor->column = lines[cursor->line++].continued;
         if (screen->line < window.ws_row - 2) screen->line++; else origin->line++;
         if (cursor->column > window.ws_col - 1) {
             screen->column = window.ws_col - 1;
             origin->column = cursor->column - screen->column;
         } else {
             screen->column = cursor->column;
             origin->column = 0;
         }
     } if (user) *desired = *cursor;
 }




 int main(const int argc, const char** argv) {
 
     enum editor_mode mode = command_mode;
 
     bool saved = true, show_status = true;
      
     size_t length = 0, at = 0, line_count = 0;
 
     unicode* text = NULL, c = 0, p = 0;
 
 
     struct location origin = {0,0}, cursor = {0,0}, screen = {0,0}, desired = {0,0};
 
     struct line* lines = generate_line_view(text, length, &line_count);
 
     while (mode != quit) {
                  
         display(mode, origin, cursor, window, lines, line_count, message, saved, show_status, filename);
 
 
 
         c = read_unicode();
 
         
         if (mode == command_mode) {
          
            if (is(c, edit_key)) mode = edit_mode;
 
             else if (is(c, hard_edit_key)) mode = hard_edit_mode;
 
             else if (is(c, select_key)) mode = select_mode;
 
             else if (is(c, option_key)) read_option(&at, &cursor, message, &origin, &screen, &show_status);
 
             else if (is(c, up_key)) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
             else if (is(c, down_key)) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
             else if (is(c, right_key)) { if (at < length) { at++; move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true); } }
             else if (is(c, left_key)) { if (at) { at--; move_left(&cursor, &origin, &screen, window, lines, &desired, true); } }
             else if (is(c, jump_key)) jump(c, &cursor, &origin, &screen, &desired, window, &at, length, lines, line_count);
             else if (is(c, save_key)) save(text, length, filename, &saved, message);
             
             else if (is(c, quit_key) || is(c, force_quit_key)) { if (saved || (is(c, force_quit_key) && confirmed("quit without saving"))) mode = quit; }
                                   
             
         } else {
             if (mode == edit_mode && bigraph(edit_exit, p, c)) {
                 delete(at--, &text, &length);
                 move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                 mode = command_mode;
                 save(text, length, filename, &saved, message);
                 if (saved) mode = quit;
                 
             } else if (mode == edit_mode && (bigraph(left_exit, p, c) || bigraph(right_exit, p, c))) {
                 delete(at--, &text, &length);
                 move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                 free(lines); lines = generate_line_view(text, length, &line_count);
                 mode = command_mode;
                 
             } else if (is(c, 127)) {
                 if (at && length) {
                     delete(at--, &text, &length);
                     move_left(&cursor, &origin, &screen, window, lines, &desired, true);
                     free(lines); lines = generate_line_view(text, length, &line_count);
                     saved = false;
                 }
             } else if (is(c, 27)) {
                 unicode c = read_unicode();
                 if (is(c, '[')) {
                     unicode c = read_unicode();
                          if (is(c, 'A')) move_up(&cursor, &origin, &screen, window, &at, lines, &desired);
                     else if (is(c, 'B')) move_down(&cursor, &origin, &screen, window, &at, lines, line_count, length, &desired);
                     else if (is(c, 'C') && at < length) { at++; move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true); }
                     else if (is(c, 'D') && at) { at--; move_left(&cursor, &origin, &screen, window, lines, &desired, true); }
                 } else if (is(c, 27)) mode = command_mode;
                 
             } else {
                 insert(c, at++, &text, &length);
                 free(lines); lines = generate_line_view(text, length, &line_count);
                 move_right(&cursor, &origin, &screen, window, lines, line_count, length, &desired, true);
                 saved = false;
             }
         }
         p = c;
     }
 
     for (size_t i = 0; i < length; i++) free(text[i]);
     free(text);
     free(lines);
     
     
 }

 const char* cursor_string = "|";
 
 */


#endif /* comments_h */
