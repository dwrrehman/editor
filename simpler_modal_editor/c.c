#include <stdio.h>   // 202410266.140132: dwrr
#include <stdlib.h>  
#include <string.h>  
#include <iso646.h>  
#include <unistd.h>  
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <sys/wait.h> 
#include <stdint.h>
#include <signal.h>
#include <stdnoreturn.h>

typedef uint64_t nat;





int main(int argc, const char** argv) {
	struct sigaction action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);



	int df = open(filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = open(filename, O_RDONLY);
	if (file < 0) { read_error: printf("could not open \"%s\"\n", filename); perror("editor: open"); exit(1); }
	struct stat ss; fstat(file, &ss);
	count = (nat) ss.st_size;
	text = malloc(count);
	read(file, text, count);
	close(file);

new: 	cursor = 0; anchor = disabled;
	finish_action((struct action) {0}, NULL, (int64_t) 0);
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 1; 
	terminal_copy.c_cc[VTIME] = 0;
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);




loop:	ioctl(0, TIOCGWINSZ, &window);
	display();
	char c = 0;
	ssize_t n = read(0, &c, 1);c = remap(c);
	if (n < 0) { perror("read"); fflush(stderr); }
	if (mode == insert_mode) {

	} else {
		if (c == 27 or c == ' ') {}
		else if (c == 't') mode = insert_mode;

	}
	goto loop;
done:	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	save(); exit(0);
}









/*



		
		else if (c == 't') mode = insert_mode;

		else if (c == 'd') mode = search_mode;

		else if (c == 'r') delete(1, 1);


		else if (c == 'a') anchor = anchor == disabled ? cursor : disabled;
		else if (c == 'w') paste();
		else if (c == 'c') copy_local();

		else if (c == 'q') goto do_c;
		else if (c == 'y') save();
		
		else if (c == 'i') right();
		else if (c == 'n') left();
		else if (c == 'p') up();
		else if (c == 'u') down();

		else if (c == 'h') half_page_up();
		else if (c == 'm') half_page_down();

		







else if (c == 'b') { task_to_clip(); goto do_c; }
else if (c == 'v') { task2_to_clip(); goto do_c; }

else if (c == 'x') redo();
else if (c == 'z') undo();

else if (c == 'g') paste_global(); 
else if (c == 'f') copy_global();


else if (c == 'e') word_left();
else if (c == 'o') word_right();


else if (c == 's') insert_char();


else if (c == 127) { if (anchor == disabled) delete(1,1); else delete_selection(); }






	static const char* help_string = 
	"q	quit, must be saved first.\n"                              q
	"z	this help string.\n"					  ...
	"s	save the current file contents.\n"                         y
	"<sp>	exit insert mode.\n"                                       drtpun
	"<sp>	move up one line.\n"                                      ...
	"<nl>	move down one line.\n"                                    ...
	"a	anchor at cursor.\n"                                       a
	"p	display cursor/file information.\n"                       ...
	"t	go into insert mode.\n"                                    t
	"u<N>	cursor += N.\n"                                           i u m
	"n<N>	cursor -= N.\n"                                           n p h
	"c<N>	cursor = N.\n"                                             d
	"d[N]	display page of text starting from cursor.\n"             ...
	"w[N]	display page of text starting from anchor.\n"             ...
	"m<S>	search forwards from cursor for string S.\n"               d
	"h<S>	search backwards from cursor for string S.\n"             ...
	"o	copy current selection to clipboard.\n"                    c
	"k	inserts current datetime at cursor and advances.\n"       ...
	"e<S>	inserts S at cursor and advances.\n"                      ...
	"i	inserts clipboard at cursor, and advances.\n"              w
	"r	removes current selection. requires confirming.\n";        r 









*/








