#include <stdio.h>        // nonvolatile ed-like editor,
#include <stdlib.h>       // written on 202401103.023834 by dwrr
#include <string.h>       // meant for tb's mainly. not writing code really.
#include <iso646.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <stdio.h>  
#include <stdlib.h> 
#include <string.h> 
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <termios.h>
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
#include <copyfile.h>

typedef uint64_t nat;

struct action {
	off_t parent;
	off_t pre;
	off_t post;
	off_t length;
};

static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";
static const nat autosave_frequency = 24;
extern char** environ;
static struct termios terminal = {0};
static char filename[4096] = {0}, directory[4096] = {0};
static int file = -1, directory_fd = -1;
static int history = -1;
static off_t head = 0;
static nat autosave_counter = 0;

static void autosave(void) {

	char autosave_filename[4096] = {0};

	char datetime[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);

	snprintf(autosave_filename, sizeof autosave_filename, "%s_%08x%08x.txt", datetime, rand(), rand());
	int flags = O_RDWR | O_CREAT | O_EXCL;
	mode_t permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	const int autosave_dir = open(autosave_directory, O_RDONLY | O_DIRECTORY, 0);
	if (autosave_dir < 0) { perror("read open autosave_directory"); exit(1); }
	int df = openat(autosave_dir, autosave_filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int autosave_file = openat(autosave_dir, autosave_filename, flags, permission);
	if (autosave_file < 0) { read_error: perror("read openat autosave_filename"); exit(1); }

	close(file);
	close(directory_fd);
	
	flags = O_RDWR;
	permission = 0;

	directory_fd = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (directory_fd < 0) { perror("read open directory"); exit(1); }
	df = openat(directory_fd, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error2; }
	file = openat(directory_fd, filename, flags, permission);
	if (file < 0) { read_error2: perror("read openat file"); exit(1); }

	if (fcopyfile(file, autosave_file, NULL, COPYFILE_ALL)) {
		perror("fcopyfile autosave COPYFILE_ALL");
		getchar();
	}

	close(autosave_file);
	close(autosave_dir);
	autosave_counter = 0; 
}

static void finish_action(struct action node, const char* string, nat length, off_t cursor) {
	node.post = cursor;
	head = lseek(history, 0, SEEK_END);
	write(history, &node, sizeof node);
	write(history, string, length);
	lseek(history, 0, SEEK_SET);
	write(history, &head, sizeof head);
	fsync(history);
}

static void insert(char c) {
	if (++autosave_counter >= autosave_frequency) autosave();
	lseek(history, 0, SEEK_SET);
	read(history, &head, sizeof head);
	
	off_t cursor = lseek(file, 0, SEEK_CUR);
	struct action node = { .parent = head, .pre = cursor, .length = 1 };
	off_t count  = lseek(file, 0, SEEK_END);
	size_t size = (size_t) (count - cursor);
	char* rest = malloc(size + 1); 
	memcpy(rest, &c, 1);
	lseek(file, cursor, SEEK_SET);
	read(file, rest + 1, size);
	lseek(file, cursor, SEEK_SET);
	write(file, rest, size + 1);
	lseek(file, cursor + 1, SEEK_SET);
	fsync(file);
	finish_action(node, &c, 1, cursor);
	free(rest);
}

static void delete(void) {
	if (++autosave_counter >= autosave_frequency) autosave();
	off_t cursor = lseek(file, 0, SEEK_CUR);
	if (not cursor) return;
	lseek(history, 0, SEEK_SET);
	read(history, &head, sizeof head);
	struct action node = { .parent = head, .pre = cursor, .length = -1 };
	off_t count = lseek(file, 0, SEEK_END);
	size_t size = (size_t) (count - cursor);
	char* rest = malloc(size + 1);
	lseek(file, cursor - 1, SEEK_SET);
	read(file, rest, size + 1);
	lseek(file, cursor - 1, SEEK_SET);
	write(file, rest + 1, size);
	lseek(file, cursor - 1, SEEK_SET);
	ftruncate(file, count - 1);
	fsync(file);
	finish_action(node, rest, 1, cursor);
	free(rest);
}

int main(int argc, const char** argv) {
	srand((unsigned)time(0)); rand();
	getcwd(directory, sizeof directory);

	int flags = O_RDWR;
	mode_t permission = 0;

	if (argc < 2) {
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	} else strlcpy(filename, argv[1], sizeof filename);

	directory_fd = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (directory_fd < 0) { perror("read open directory"); exit(1); }
	int df = openat(directory_fd, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	file = openat(directory_fd, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }

	const char* history_directory = "/Users/dwrr/Documents/personal/histories/";  
	char history_filename[4096] = {0};

	flags = O_RDWR;
	permission = 0;

	if (argc < 3) {
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(history_filename, sizeof history_filename, "%s_%08x%08x.history", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	} else if (argc == 3) strlcpy(history_filename, argv[2], sizeof history_filename);
	else exit(puts("usage error"));

	const int hdir = open(history_directory, O_RDONLY | O_DIRECTORY, 0);
	if (hdir < 0) { perror("read open history directory"); exit(1); }
	df = openat(hdir, history_filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto hread_error; }
	history = openat(hdir, history_filename, flags, permission);
	if (history < 0) { hread_error: perror("read openat history file"); exit(1); }

	if (argc < 3) 
		write(history, &head, sizeof head); 
	else 	 read(history, &head, sizeof head);

	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_lflag &= ~((size_t) ICANON);
	tcsetattr(0, TCSAFLUSH, &terminal_copy);
	






/////////////////////    /////////////////////     /////////////////////    /////////////////////     ///////////////////// 

// how do we display the text?  does backspace work properly?   how do we display where we are in the document?....
// do we use a page size like the pager?     lets implement find-forwards next. thats important.
//   what keybind?  

/////////////////////    /////////////////////     /////////////////////    /////////////////////     ///////////////////// 






loop1:;	
	char c = 0;
	read(0, &c, 1);

	if (c == 1) {
		if (not length) {
			char* page = malloc(page_size);			
			ssize_t n = read(file, page, page_size);
			if (n < 0) write(1, "read error\n", 11);
			else { 
				lseek(file, -n, SEEK_CUR);
				write(1, page, (size_t) n); 
				write(1, "\033[7m/\033[0m", 9);
			} free(page); goto loop;
		}
		string[--length] = 0;
		size_t i = 0;
		search:; char c = 0;
		if (not read(file, &c, 1)) { write(1, "?\n", 2); selection = 0; goto clear; } 
		if (c == string[i]) i++; else i = 0; 
		if (i != length) goto search;
		selection = length;
		printf("c%lld s%llu\n", lseek(file, 0, SEEK_CUR), selection); 
		clear: memset(string, 0, sizeof string); 
		length = 0; goto loop;
	}
	


	}
	else if (c == 4) goto done;
	else if (c == 127) { delete(); write(1, "\b\b\b   \b\b\b", 9); }
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c);
	goto loop1;

done:	close(file);
	close(directory_fd);
	close(history);
	tcsetattr(0, TCSAFLUSH, &terminal);
}

/*


	okay, so i think we kinda have a problem lol. 

	so with the previous ed like editor,   we could use the current input buffer as the mechanism for getting a string that we want to search for. 

		not so with this version of the text editor. 

				all characters that we type that are text, are immediately inserted, actually.  so yeahhhh


						and the only mechanism we have of deleting text, is the backspace key, i think. 


		wellll, actuall, 


						i still think we are going to provide a way of doing cutting actaully        of the current selection, only, though. 



			and actually, i feel like i want to make the search        delete it too?   no 



		bad idea 


									i don't know how to implement the search bar, basically. thats the problem. 

						its not going to be a seperate search bar-     its going to be within the document itself, 



				its just, then that means, in order to do that, 





						we need to have some way of cutting the text that the user literally typed into the document just to be the text was searched for 


							so like 


	the document looks like this 




						hello there from space
						
						hello



the user typed hello in order to search for the "hello" string, 

	

					and we have to delete it some how, 


						and         we have to have marked the spot in the text where the selection begins. thats important too!

								so yeah. i think we need at least three things-



									1. a cut command, which deletes the current selection   (start + length)


									2. a search command, which finds a given string, starting from start, of length  cursor - start

									3.  a set-start command, which drops an anchor at the current cursor position.  ie, 
												start = cursor;



	this way the typical usage would be 



				<set-start>  hello 




	OH WAIT 

			we can just use a clipboard. 


					OH wow thats actually so good. lets just do that. 


			 a clipboard     


						thats the solution. 



								ie, when you type some text, 



							


		wait


	okay

	so 

	heres the sequence


							<set-start> hello <cut> <search>



		thats the sequence 



						when you cut it ends up on your clipboard, 



							and search just picks up on the clipboard. 




								




			so yeah, literally identical to the current editor, kinda



						except, 


			wait, what if when you backspaced, it ended up on the thingy 



					so we didnt even need to have a seperate cut command!?!?!


						like, when you backspaced, character by character, 



						it always put it into the current buffer, 

				i guess it has to do it backwards LOL       but thats fine



								but yeah, 










		i think regardless       we definitely need to have a        <set-start>       key binding 



							probably control-A        if i had to guess.  idk. 




						


			and then, maybe control-T for searching. that would work. 




			



		wait, so why do we even need to do control-A?



							ohhh, i guess to clear the buffer, actually!




								yeah, we are going to actaully buffer,    at all times, what the user deleted. i think. 


									and that backwards-clipboard is used       is used for doing searching. 




			so the usage is, 



											control-A    (which clears the clipboard)

		wait, no, we should just make it so that control-A starts   or enables the recording of data to the clipboard. we shouldnt do it always. so yeah. 



				okay, that couldddd work


									 








not quite happy with this... hm... 

	i feel like there HAS to be a simpler solution. 



like, literally the rest of this editor is so incredibly simple, and perfect so far 



















*/




// write(1, "\033[7m/\033[0m", 9);

/*








static void get_count(void) {
	struct stat s;
	fstat(file, &s);
	count = s.st_size;
}

static char next(void) {
	char c = 0;
	ssize_t n = read(file, &c, 1);
	if (n == 0) return 0;
	if (n != 1) { perror("next read"); exit(1); }
	return c;
}

static char at(off_t a) {
	lseek(file, a, SEEK_SET);
	return next();
}

static void at_string(off_t a, nat byte_count, char* destination) {
	lseek(file, a, SEEK_SET);
	ssize_t n = read(file, destination, byte_count);
	if (n == 0) return;
	if (n < 0) { perror("next read"); exit(1); }
}








#include <stdio.h>     // revised editor, based on less, more, and ed. 
#include <stdlib.h>    // only uses three commands, find-select, replace-selection, and set-numeric
#include <unistd.h>    // written by dwrr on 202312177.223938
#include <string.h>
#include <iso646.h>
#include <fcntl.h>
#include <termios.h>

int main(int argc, const char** argv) {
	typedef uint64_t nat;
	if (argc > 2) exit(puts("usage ./editor <file>"));

	int file = open(argv[1], O_RDWR, 0);
	if (file < 0) { 
		perror("read open file"); 
		exit(1); 
	}

	struct termios original = {0};
	tcgetattr(0, &original);
	struct termios terminal = original; 
	terminal.c_cc[VKILL]   = _POSIX_VDISABLE;
	terminal.c_cc[VWERASE] = _POSIX_VDISABLE;
	tcsetattr(0, TCSAFLUSH, &terminal);

	nat page_size = 1024, length = 0, selection = 0;
	char string[4096] = {0};
		
	loop:; char line[4096] = {0};
	read(0, line, sizeof line);

	if (not strcmp(line, "n\n")) {
		if (not length) { 
			printf("c%lld s%llu p%llu\n", lseek(file, 0, SEEK_CUR), selection, page_size); 
			goto loop; 
		}

		string[--length] = 0;
		char c = *string;
		const nat n = strtoull(string + 1, NULL, 10); 

		if (c == 'q') goto done;
		else if (c == 'o') { for (int i = 0; i < 100; i++) puts(""); puts("\033[1;1H"); }
		else if (c == 'p') page_size = n;
		else if (c == 's') selection = n;
		else if (c == 'l') lseek(file, (off_t) n, SEEK_SET);
		else if (c == '+') lseek(file, (off_t) n, SEEK_CUR);
		else if (c == '-') lseek(file, (off_t)-n, SEEK_CUR);
		else puts("?");
		goto clear;
	
	} else if (not strcmp(line, "u\n")) {

		// todo: make a backup of the file, after every single edit. 

		if (length) string[--length] = 0;
		const off_t cursor = lseek(file, (off_t) -selection, SEEK_CUR);
		const off_t count = lseek(file, 0, SEEK_END);
		const nat rest = (nat) (count - cursor - (off_t) selection);
		char* buffer = malloc(length + rest);
		memcpy(buffer, string, length);
		lseek(file, cursor + (off_t) selection, SEEK_SET);
		read(file, buffer + length, rest);
		lseek(file, cursor, SEEK_SET);
		write(file, buffer, length + rest);
		lseek(file, cursor, SEEK_SET);
		ftruncate(file, count + (off_t) length - (off_t) selection);
		fsync(file); free(buffer);
		printf("+%llu -%llu\n", length, selection);
		goto clear;
 	
	} else if (not strcmp(line, "t\n")) {
		if (not length) {
			char* page = malloc(page_size);			
			ssize_t n = read(file, page, page_size);
			if (n < 0) write(1, "read error\n", 11);
			else { 
				lseek(file, -n, SEEK_CUR);
				write(1, page, (size_t) n); 
				write(1, "\033[7m/\033[0m", 9);
			} free(page); goto loop;
		}
		string[--length] = 0;
		size_t i = 0;
		search:; char c = 0;
		if (not read(file, &c, 1)) { write(1, "?\n", 2); selection = 0; goto clear; } 
		if (c == string[i]) i++; else i = 0; 
		if (i != length) goto search;
		selection = length;
		printf("c%lld s%llu\n", lseek(file, 0, SEEK_CUR), selection); 
		clear: memset(string, 0, sizeof string); 
		length = 0; goto loop;
	} else length = strlcat(string, line, sizeof string);
	goto loop; done: close(file);
}







*/


