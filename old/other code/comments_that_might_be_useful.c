





// note: we only have to redraw the entire display if the .origin changed. else, we can just replace the line that was edited.
	// also, we have to redraw the entire display, if we are going things like find and replace, i think? or like, multuple cursors...? hmm.. 


// From cursor to end of line	ESC [ K or ESC [ 0 K
// From beginning of line to cursor	ESC [ 1 K
// Entire line containing cursor	ESC [ 2 K

// From cursor to end of screen	ESC [ J or ESC [ 0 J
// From beginning of screen to cursor	ESC [ 1 J
// Entire screen	ESC [ 2 J

// set cursor position ESC [ Pl ; Pc H

// } else if (c == 'I') write(1, "\033[B", 3);

		// else if (c == 'O') write(1, "\033[A", 3);




// static inline void render(	nat column, nat line, 
// 				struct location cursor, 
// 				struct line* lines) {

// 	char screen[4096] = {0};
// 	nat length = 9;
//     	memcpy(screen,"\033[?25l\033[K", 9);

// 	struct line* this = lines + line;
	
// 	for (nat col = column; col < this->length; col++) {
// 		screen[length++] = (char) this->line[col];
// 	}
	
// 	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", 
// 			cursor.line + 1, cursor.column + 1);
	
// 	write(1, screen, (size_t) length);
// }

//if (cursor.column < window.ws_col - 1) {


 


/*



  struct file* file = buffers[active];
    
    i32 length = 9;
    memcpy(window.screen,"\033[?25l\033[H", 9);
    
    double f = floor(log10((double) file->logical.count));
    file->line_number_width = (i8) f + 1;
    file->line_number_cursor_offset = (file->options.flags & show_line_numbers) ? file->line_number_width + 2 : 0;

    i32 line_number = 0;
    for (i32 i = 0; i < file->visual_origin.line; i++)
        if (not file->render.lines[i].continued)
            line_number++;
    
    for (i16 screen_line = 0; screen_line < window.rows - (file->options.flags & show_status); screen_line++) {
        
        i32 line = (i32)screen_line + file->visual_origin.line;
        
        if (line < file->render.count) {
        
            if (file->options.flags & show_line_numbers) {
                if (not file->render.lines[line].continued) length += sprintf(window.screen + length, "\033[38;5;59m%*d\033[0m  ", file->line_number_width, ++line_number);
                else length += sprintf(window.screen + length, "%*s  " , file->line_number_width, " ");
            }
            
            i32 range = 0;
            for (i32 column = 0, visual_column = 0; column < file->render.lines[line].length; column++) {
                u8 c = file->render.lines[line].line[column];
                if (visual_column >= file->visual_origin.column and
                    visual_column < file->visual_origin.column + window.columns - file->line_number_cursor_offset) {
                    if (c == '\t' or c == '\n') window.screen[length++] = ' ';
                    else window.screen[length++] = (char) c;
                }
                if ((c >> 6) != 2) visual_column++;
            }
        }
        
        window.screen[length++] = '\033';
        window.screen[length++] = '[';
        window.screen[length++] = 'K';
        if (screen_line < window.rows - 1) {
            window.screen[length++] = '\r';
            window.screen[length++] = '\n';
        }
    }











	// for (nat line = 0; line < file->count; line++) {
	// 	for (nat column = 0; column < file->lines[line].length; column++) {
	// 		uint8_t c = file->lines[line].line[column];
	// 		screen[length++] = (char) c;
	// 	}
		
	// 	screen[length++] = '\033';
	// 	screen[length++] = '[';
	// 	screen[length++] = 'K';

	// 	if (screen_line < window_rows - 2) {
	// 		screen[length++] = '\r';
	// 		screen[length++] = '\n';
	// 	}

	// 	screen_line++; 
	// 	screen_column = 0;
	// }

















/// super minimal version of my text editor.
#include <iso646.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

typedef uint8_t byte;
typedef int32_t nat;

struct line {
	uint8_t* line;
	nat length;
	nat capacity;
}

struct file {
	struct line* logical;
	nat count;
	nat capacity;
	nat cl;
	nat cc;
	nat sl;
	nat sc;
	nat ol;
	nat oc;
	nat
};

static inline void render(	nat column, nat line, 
				struct location cursor, 
				struct line* lines) {

	char screen[4096] = {0};
	nat length = 9;
    	memcpy(screen,"\033[?25l\033[K", 9);

	struct line* this = lines + line;
	
	for (nat col = column; col < this->length; col++) {
		screen[length++] = (char) this->line[col];
	}
	
	length += sprintf(screen + length, "\033[%d;%dH\033[?25h", 
			cursor.line + 1, cursor.column + 1);
	
	write(1, screen, (size_t) length);
}

int main() {

	struct termios terminal = {0};
	tcgetattr(0, &terminal);
	struct termios raw = terminal;

	raw.c_oflag &= ~((unsigned long)OPOST);
	raw.c_iflag &= ~((unsigned long)BRKINT | (unsigned long)ICRNL 
			| (unsigned long)INPCK | (unsigned long)IXON);	
	raw.c_lflag &= ~((unsigned long)ECHO | (unsigned long)ICANON | (unsigned long)IEXTEN);
	tcsetattr(0, TCSAFLUSH, &raw);
	write(1, "\033[?1049h\033[?1000h", 16);

	struct winsize window;
	ioctl(1, TIOCGWINSZ, &window);
	
	struct location render_origin = {0};
	struct location render_screen = {0};
	
	struct location cursor = {0};

	// nat line_count = 1;
	struct line* lines = calloc(1, sizeof(struct line));

	byte c = 0;
	while (c != 'q') {
		read(0, &c, 1);

		if (c == 'J') {
			if (cursor.column) {
				cursor.column--;
				write(1, "\033[D", 3);
				/// this doesnt work for tabs....? or i guess it could...
			}

		} else if (c == ':') {
			
			if (cursor.column < lines[cursor.line].length) { 
				// cursor.column < window.ws_col - 1
				cursor.column++;
				write(1, "\033[C", 3);
			}

		} else if (c == 127) {

			if (cursor.column) {				

				struct line* this = lines + cursor.line;

				this->line = realloc(this->line, sizeof(struct line) 
						* (size_t) (this->length - 1));

				memmove(this->line + cursor.column - 1, 
					this->line + cursor.column, 
					sizeof(struct line) * (size_t) 
					(this->length - cursor.column));
				
				this->length--;
				cursor.column--;
				write(1, "\033[D", 3);
				render(cursor.column, cursor.line, cursor, lines);
			}

		} else if (c == 13) {

			// if (cursor.line < window.ws_row - 1) {
			// 	write(1, "\n\r", 2);
			// 	cursor.line++;
			// 	cursor.column = 0;
			// }
			
		} else {
			if (cursor.column < window.ws_col - 1) {
				
				struct line* this = lines + cursor.line;
				
				this->line = realloc(this->line, sizeof(struct line) 
						* (size_t) (this->length + 1));

				memmove(this->line + cursor.column + 1, 
					this->line + cursor.column, 
					sizeof(struct line) * (size_t) 
					(this->length - cursor.column));


				this->line[cursor.column] = c;
				this->length++;
				cursor.column++;
				render(cursor.column - 1, cursor.line, cursor, lines);
			} 
		}
	}
	write(1, "\033[?1049l\033[?1000l", 16);	
	tcsetattr(0, TCSAFLUSH, &terminal);

	printf("cursor = (%d, %d)\n", cursor.line, cursor.column);

	exit(0);
}


		



// note: we only have to redraw the entire display if the .origin changed. else, we can just replace the line that was edited.
	// also, we have to redraw the entire display, if we are going things like find and replace, i think? or like, multuple cursors...? hmm.. 


// From cursor to end of line	ESC [ K or ESC [ 0 K
// From beginning of line to cursor	ESC [ 1 K
// Entire line containing cursor	ESC [ 2 K

// From cursor to end of screen	ESC [ J or ESC [ 0 J
// From beginning of screen to cursor	ESC [ 1 J
// Entire screen	ESC [ 2 J

// set cursor position ESC [ Pl ; Pc H

// } else if (c == 'I') write(1, "\033[B", 3);

		// else if (c == 'O') write(1, "\033[A", 3);






*/


















// if (c == '\t') {
				
			// 	screen[length++] = ' ';
			// }

// do {
//                 if (file->options.wrap_width and at >= file->options.wrap_width) break;
//                 at++; count++;
//             } while (at % file->options.tab_width);


                    	// else 






    // for (i16 screen_line = 0; screen_line < ; screen_line++) {
        
    //     i32 line = (i32)screen_line + file->visual_origin.line;
        
    //     if (line < file->render.count) {
        
            // if (file->options.flags & show_line_numbers) {
            //     if (not file->render.lines[line].continued) length += sprintf(window.screen + length, "\033[38;5;59m%*d\033[0m  ", file->line_number_width, ++line_number);
            //     else length += sprintf(window.screen + length, "%*s  " , file->line_number_width, " ");
            // }
            
            
        //     for (i32 column = 0, visual_column = 0; column < file->render.lines[line].length; column++) {
        //         u8 c = file->render.lines[line].line[column];

        //         if (visual_column >= file->visual_origin.column and
        //             visual_column < file->visual_origin.column + window.columns - file->line_number_cursor_offset) {
        //             if (c == '\t' or c == '\n') window.screen[length++] = ' ';
        //             else window.screen[length++] = (char) c;
        //         }
        //         if ((c >> 6) != 2) visual_column++;
        //     }
        // }
        
     
    // }




