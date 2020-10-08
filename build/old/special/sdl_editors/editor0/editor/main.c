///
///        My Text Editor.
///
///   Created by: Daniel Rehman
///
///   Created on: 2009207.011700
///

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef char* unicode;

const char* font_path = "/Users/deniylreimn/Library/Fonts/Meslo LG L Regular for Powerline.ttf";

static const unsigned int
    max_font_size = 300,
    command_key = 2048;

static unsigned int
    window_width = 1000,
    window_height = 700,
    font_size = 60,
    wrap_width = 100,
    window_rows = 0,
    window_cols = 0;

static int
    char_width = 0,
    char_height = 0;

struct location {
    size_t line;
    size_t column;
};

struct line {
    unicode* line;
    size_t length;
    bool continued;
};

static inline bool is(unicode u, char c) { return u && !strncmp(u, &c, 1); }

static inline struct line* generate_line_view(unicode* text, size_t text_length, size_t* count) {
    size_t length = 0;
    bool continued = false;
    
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
            if (is(text[i], '\t')) length += 8;
            else length++;
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
    }
    return lines;
}

static inline void insert(unicode c, size_t at, unicode** text, size_t* length) {
    if (at > *length) { abort(); return; }
    *text = realloc(*text, sizeof(unicode) * (*length + 1));
    memmove(*text + at + 1, *text + at, sizeof(unicode) * (*length - at));
    ++*length; (*text)[at] = c;
}

static inline void delete(size_t at, unicode** text, size_t* length) {
    if (at > *length) { abort(); return; }
    if (!at || !*length) return;
    free((*text)[at - 1]);
    memmove((*text) + at - 1, (*text) + at, sizeof(unicode) * (*length - at));
    *text = realloc(*text, sizeof(unicode) * (--*length));
}

static inline void move_left(struct location *cursor, struct location *origin, struct location *screen, struct line* lines, struct location* desired, bool user) {
    if (cursor->column) {
        cursor->column--;
        if (screen->column > 5) screen->column--; else if (origin->column) origin->column--; else screen->column--;
    } else if (cursor->line) {
        cursor->line--;
        cursor->column = lines[cursor->line].length - lines[cursor->line].continued;
        if (screen->line > 5) screen->line--; else if (origin->line) origin->line--; else screen->line--;
        if (cursor->column > window_cols - 5) {
            screen->column = window_cols - 5;
            origin->column = cursor->column - screen->column;
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
    } if (user) *desired = *cursor;
}

static inline void move_right(struct location* cursor, struct location* origin, struct location* screen, struct line* lines, size_t line_count, size_t length, struct location* desired, bool user) {
    if (cursor->column < lines[cursor->line].length) {
        cursor->column++;
        if (screen->column < window_cols - 2) screen->column++; else origin->column++;
    } else if (cursor->line < line_count - 1) {
        cursor->column = lines[cursor->line++].continued;
        if (screen->line < window_rows - 2) screen->line++; else origin->line++;
        if (cursor->column > window_cols - 2) {
            screen->column = window_cols - 2;
            origin->column = cursor->column - screen->column;
        } else {
            screen->column = cursor->column;
            origin->column = 0;
        }
    } if (user) *desired = *cursor;
}

static inline void move_up(struct location *cursor, struct location *origin, struct location *screen, size_t* point, struct line* lines, struct location* desired) {
    if (!cursor->line) {
        *screen = *cursor = *origin = (struct location){0, 0};
        *point = 0;
        return;
    }
    const size_t column_target = fmin(lines[cursor->line - 1].length, desired->column);
    const size_t line_target = cursor->line - 1;
    while (cursor->column > column_target || cursor->line > line_target) {
        move_left(cursor, origin, screen, lines, desired, false);
        (*point)--;
    }
    if (cursor->column > window_cols - 5) {
        screen->column = window_cols - 5;
        origin->column = cursor->column - screen->column;
    } else {
        screen->column = cursor->column;
        origin->column = 0;
    }
}

static inline void move_down(struct location *cursor, struct location *origin, struct location *screen, size_t* point, struct line* lines, size_t line_count, size_t length, struct location* desired) {
    if (cursor->line >= line_count - 1) {
        while (*point < length) {
            move_right(cursor, origin, screen, lines, line_count, length, desired, false);
            (*point)++;
        }
        return;
    }
    const size_t column_target = fmin(lines[cursor->line + 1].length, desired->column);
    const size_t line_target = cursor->line + 1;
    while (cursor->column < column_target || cursor->line < line_target) {
        move_right(cursor, origin, screen, lines, line_count, length, desired, false);
        (*point)++;
    }
    if (cursor->line && lines[cursor->line - 1].continued && !column_target) {
        move_left(cursor, origin, screen, lines, desired, false);
        (*point)--;
    }
}

static inline unsigned int render_text(char* text, SDL_Color cursor_color,
                        unsigned int x, unsigned int y,
                        TTF_Font* font, SDL_Renderer* renderer) {
    SDL_Surface* surface = TTF_RenderUTF8_Solid(font, text, cursor_color);
    if (!surface) fprintf(stderr,"error: could not create surface: %s\n", SDL_GetError());
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) fprintf(stderr,"error: could not create texture: %s\n", SDL_GetError());
    
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    return rect.h;
}

static inline void display(SDL_Renderer* renderer, TTF_Font* font, struct location origin, struct location cursor, struct line* lines, size_t line_count) {
    
    size_t screen_length = 0;
    char screen[window_cols + 1];
    unsigned int cursor_x = 0, cursor_y = 0, vertical_position = 0;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    
    for (size_t line = origin.line; line < fmin(origin.line + window_rows, line_count); line++) {
        for (size_t column = origin.column; column < fmin(origin.column + window_cols, lines[line].length); column++) {
            
            unicode g = lines[line].line[column];
            if (line < cursor.line || (line == cursor.line && column < cursor.column)) {
                if (is(g, '\t')) cursor_x += char_width * 8; else cursor_x += char_width;
            }
            if (is(g, '\t')) for (size_t i = 0; i < 8; i++) screen[screen_length++] = ' ';
            else for (size_t i = 0; i < strlen(g); i++) screen[screen_length++] = g[i];
        }
        if (line < cursor.line) {
            cursor_y += char_height;
            cursor_x = 0;
        }
        
        SDL_Color white = {255, 255, 255, 0};
        screen[screen_length] = '\0';
        vertical_position += render_text(screen_length ? screen : " ", white, 0, vertical_position, font, renderer);
        screen_length = 0;
    }
    SDL_Color cursor_color = {102, 255, 102};
    render_text("_", cursor_color, cursor_x, cursor_y, font, renderer);
    SDL_RenderPresent(renderer);
}

static inline void resize_font(TTF_Font** font) {
    *font = TTF_OpenFont(font_path, font_size);
    if (!*font) fprintf(stderr, "error: font not found\n");
    TTF_SizeUTF8(*font, " ", &char_width, &char_height);
    window_cols = 2 * window_width / char_width;
    window_rows = 2 * window_height / char_height;
}

static inline void insert_char(char* c, unicode** text, size_t *at, size_t* length, size_t* line_count, struct line** lines,
                        struct location* cursor, struct location* origin,
                        struct location* screen, struct location* desired) {
    insert(c, (*at)++, text, length);
    free(*lines); *lines = generate_line_view(*text, *length, line_count);
    move_right(cursor, origin, screen, *lines, *line_count, *length, desired, true);
}

static inline void delete_char(size_t *at, struct location *cursor, struct location *desired, size_t *length, size_t *line_count,
                        struct line **lines, struct location *origin,
                        struct location *screen, unicode **text) {
    delete((*at)--, text, length);
    move_left(cursor, origin, screen, *lines, desired, true);
    free(*lines); *lines = generate_line_view(*text, *length, line_count);
}

int main(const int argc, const char** argv) {

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0 || TTF_Init() < 0) fprintf(stderr, "error: could not initialize SDL2: %s\n", SDL_GetError());
    
    SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) fprintf(stderr,"error: could not create window: %s\n", SDL_GetError());
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) fprintf(stderr,"error: could not create renderer: %s\n", SDL_GetError());
    
    TTF_Font* font = TTF_OpenFont(font_path, font_size);
    if (!font) fprintf(stderr, "error: font not found\n");
    
    TTF_SizeUTF8(font, " ", &char_width, &char_height);
    window_cols = 2 * window_width / char_width;
    window_rows = 2 * window_height / char_height;
    
    unicode* text = NULL;
    size_t length = 0, at = 0, line_count = 0;
    
    struct location
        origin = {0,0},
        cursor = {0,0},
        screen = {0,0},
        desired = {0,0};
    
    struct line* lines = generate_line_view(text, length, &line_count);
    
    bool quit = false;
    while (!quit) {
        
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            const Uint8* key = SDL_GetKeyboardState(NULL);
            
            if (e.type == SDL_QUIT) quit = true;
            
            if (e.type == SDL_KEYDOWN) {
                                                                        
                const int modifier = e.key.keysym.mod;
                
                if ((key[SDL_SCANCODE_Q] && modifier == command_key) ||
                    (key[SDL_SCANCODE_W] && modifier == command_key)) quit = true;
                
                if (key[SDL_SCANCODE_BACKSPACE]) {
                    if (at && length) delete_char(&at, &cursor, &desired, &length, &line_count, &lines, &origin, &screen, &text);
                }
                if (key[SDL_SCANCODE_RETURN]) insert_char(strdup("\n"), &text, &at, &length, &line_count, &lines, &cursor, &origin, &screen, &desired);
                if (key[SDL_SCANCODE_TAB])    insert_char(strdup("\t"), &text, &at, &length, &line_count, &lines, &cursor, &origin, &screen, &desired);
                
                if (key[SDL_SCANCODE_LEFT]) {
                    if (at) { at--; move_left(&cursor, &origin, &screen, lines, &desired, true); }
                }
                if (key[SDL_SCANCODE_RIGHT]) {
                    if (at < length) { at++; move_right(&cursor, &origin, &screen, lines, line_count, length, &desired, true); }
                }
                if (key[SDL_SCANCODE_UP]) move_up(&cursor, &origin, &screen, &at, lines, &desired);
                if (key[SDL_SCANCODE_DOWN]) move_down(&cursor, &origin, &screen, &at, lines, line_count, length, &desired);
                                                
                if (key[SDL_SCANCODE_EQUALS] && modifier == command_key) {
                    if (font_size < max_font_size) { font_size++; resize_font(&font); }
                }
                if (key[SDL_SCANCODE_MINUS] && modifier == command_key) {
                    if (font_size) { font_size--; resize_font(&font); }
                }
                usleep(5000);
            }
            if (e.type == SDL_TEXTINPUT) insert_char(strdup(e.text.text), &text, &at, &length, &line_count, &lines, &cursor, &origin, &screen, &desired);
        }
        display(renderer, font, origin, cursor, lines, line_count);
        usleep(20000);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit(); SDL_Quit();
}
































/**
 ---------------------- issues: ---------------------
 
 - BUG: our view is getting misaligned, when we have tabs,
    redo how we are working with tabs!!
 
 - FEATURE: add copy paste support.
        /// SDL_SetClipboardText();
        /// SDL_GetClipboardText();

 - FEATURE: add jump commands back in.
 
 - FEATURE: add the various modes.
 
 x: - DEV: wayyyy too many booleans.;
 
 x: - DEV: clean up the entire codebase. its disgusting.;
 
 - FEATURE: add ability to work with files...? later.
 
 - FEATURE: completely rework the view render to allow
    for syntax highlighting, by char.
 
 - FEATURE: add back in the status bar.
 
 - FEATURE: add back in the autosave/panic-dump system.
 
 x: - BUG: redo how the line numbers look. they look weird.;
 
 - BUG: make tabs work properly: make them tab over until a tabstop point, which may not be 8 always.
 
 - FEATURE: add multiple buffers.
 
 x: - DEV: simply the entire codebase.;

 - CRASHING BUG: when typing the character (unicode)         option-key + "-"   you can crash the editor.
        bad. very bad.
 
 
 - FEATURE: add in smooth scrolling! in both axises.


 - CRASHING BUG: make the editor literally never crash.
 
 - FEATURE: figure out how to make a vertical cursor like     |   like the one that xcode has.
 
 - FEATURE: figure out how to make the whole editor more power efficient, maybe using WaitEvent, and not blinking the cursor, might be a good idea.
        also, note, the cursor doesnt blink while typing is occuring. and also, it blinks rather slowly. this should all be copied.
        however, also, i am okay with not blinking the cursor at all. this might lead to a more efficient system.
 
 - FEATURE: think about how we are going to make this an application with an icon...?, and not a commandline utility...? should this be rewritten as a mac application?
  
 - FEATURE: think about how we are going to make this editor programmable!
 
 
 - BUG: when resizing the font, the cursor comes out of view, alot, and the editor is buggy, and basically breaks.
 
 - BUG: remove bools: show_cursor, show_line_numbers, blink.       ie, remove all bools.
 
 - FEATURE: allow for full screen using command + option + "/".
 
 - FEATURE: allow for quiting using command W.
 
 - DEV: Stop using so many globals in this code.
 
 - BUG: Fix all memory leaks, and properly track all ownership within the application, properly.
 
 
 
 - TODO: Basically, start from scratch, and make the editor again, using good developmental practices.
        this try was just me trying to get something working. not efficient. not beautiful. not bug free.
 
                we need it to be organized, bug-free, and beautiful. and imperative.
 
 
 
 
 
 
 -------------- next changes ill be doing: --------------
 
    - simplify, yet, clean up code
 
    - make cursor not blink anymore.
 
    - make cursor vertical.
 
    - add in jumps.
 
    - allow fullscreen using our shortcut.
 
 
 */
