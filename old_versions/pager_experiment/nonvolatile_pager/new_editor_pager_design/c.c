#include <stdio.h>        // nonvolatile ed-like editor,
#include <stdlib.h>       // written on 202401103.023834 by dwrr
#include <string.h>       // meant for tb's mainly. not writing code really.
#include <iso646.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>     // rewrite to make this editor use this editor's display system, but a string ds internally, and have autosaving. 
#include <time.h>        // and also make it less like a pager, and more an editor in some ways. 
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h> // 202402036.032044:   rewrite of editor to not make it perfectly nonvolatile, but still not require saving. 
#include <sys/wait.h> //  every 16 insertions,  we will do a full file autosave, i think. so yeah.   "xxxxxxxxxxxxxxxx" that many chars. so yeah.
#include <stdint.h>   // we might be able to get away with only writing a portion of the document too, not sure though. ill think about it. 
#include <signal.h>   // string ds is used!!!    printing will be very different from the original main editor. 
#include <copyfile.h>

typedef uint64_t nat;

static nat cursor = 0, count = 0, anchor = 0;
static char* text = NULL;

static void insert(char c) {
	text = realloc(text, count + 1);
	memmove(text + cursor + 1, text + cursor, count - cursor);
	text[cursor] = c; cursor++; count++;
}

static char delete(void) {
	count--; cursor--; char c = text[cursor];
	memmove(text + cursor, text + cursor + 1, count - cursor);
	text = realloc(text, count);
	return c;
}

static void save(char* filename, nat sizeof_filename) {
	int flags = O_WRONLY | O_TRUNC;
	mode_t permission = 0;
	if (not *filename) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof_filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	}
	int file = open(filename, flags, permission);
	if (file < 0) { perror("load: read open file"); puts(filename); getchar(); }
	write(file, text, count);
	close(file);
	printf("\033[7mwrote %llu\033[0m\n", count);
}

int main(int argc, const char** argv) {
	char filename[4096] = {0};
	if (argc < 2) goto new_file; 
	strlcpy(filename, argv[1], sizeof filename);
	int df = open(filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = open(filename, O_RDONLY);
	if (file < 0) { read_error: perror("load: read open file"); exit(1); }
	struct stat s; fstat(file, &s);
	count = (nat) s.st_size;
	text = malloc(count);
	read(file, text, count);
	close(file);
	new_file: cursor = 0; anchor = 0;
	struct termios terminal = {0};
	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_lflag &= ~((size_t) ICANON);
	tcsetattr(0, TCSAFLUSH, &terminal_copy);
	write(1, text, count);

	const nat view_size = 512;

	nat selection_begin = 0;
	nat selection_count = 0;

loop:;	char c = 0;
	read(0, &c, 1);
	if (0) {}

	else if (c == 1)  {
		printf("\033[7m%llu\033[0m\n", cursor);
		anchor = cursor; 	//A  -   anchor()
	}

	else if (c == 4)  { 			//D  -   find()

		if (anchor > cursor) { puts("\033[7merror\033[0m"); goto loop; }

		char* tofind = strndup(text + anchor, cursor - anchor);
		const nat tofind_count = cursor - anchor;
		for (nat i = 0; i < tofind_count; i++) delete();

		bool looped = false;

		again:;

		nat t = 0;
		for (nat i = anchor; i < count; i++) {
			if (t == tofind_count) {
				printf("\033[7m%llu:%llu\033[0m\n", i - tofind_count, i);
				selection_count = tofind_count;
				selection_begin = i; 

				cursor = i; anchor = i;

				const nat begin = (int64_t)i - (int64_t)tofind_count - (int64_t)view_size < (int64_t)0 ? 0 : i - view_size;
				const nat end = i + view_size >= count ? count : i + view_size;
				
				write(1, text + begin, (i - tofind_count) - begin);
				//write(1, "\033[7m", 4);
				write(1, text + (i - tofind_count), tofind_count);

				////write(1, "/\033[0m", 5);
				//write(1, text + i, end - i);

				goto loop;
			}

			if (text[i] == tofind[t]) t++; else t = 0;
		}
		anchor = 0;
		if (looped) puts("\033[7m?\033[0m"); else { looped = true; goto again; }
		free(tofind);
	}

	else if (c == 18) { 			//R  -   replace()

		if (anchor > cursor) { puts("\033[7merror\033[0m"); goto loop; }

		char* toreplace = strndup(text + anchor, cursor - anchor);
		const nat toreplace_count = cursor - anchor;
		for (nat i = 0; i < toreplace_count; i++) delete();

		cursor = selection_begin;
		for (nat i = 0; i < selection_count; i++) delete();    // delete the current selection from a find call.
		
		// insert a series of characters equal to the current string. 

		for (nat i = 0; i < toreplace_count; i++) {
			insert(toreplace[i]);
		}

		printf("\033[7m%llu: -%llu +%llu\033[0m\n", selection_begin, selection_count, toreplace_count);
		selection_count = 0;
		free(toreplace);
	}
	else if (c == 14) save(filename, sizeof filename);   //N - temporary:  save the file.       (make this automatic!)
	else if (c == 27) goto done;
	else if (c == 127) { if (cursor) { delete(); write(1, "\b\b\b   \b\b\b", 9); } else write(1, "\b\b  \b\b", 6); }
	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert(c);
	goto loop;

done:	save(filename, sizeof filename);
	tcsetattr(0, TCSAFLUSH, &terminal);
	write(1, "\n", 1);
}











// add backwards searching,   control-h   i think 










// todo:

//	goto done;  // quit editor

//	write(1, text + origin, display_count);
	







/*

	the basic way of text editing, is going to be to 


		insert a bunch of text, just by typing. 

			you can't backspace on a line though. which is kind of rough. but we'll figure that out later. 
					thats a small problem honestly 


	

		setting the current cursor position, to be the anchor.   ie, anchoring 



		getting the text in between the anchor and cursor, and saving it in a string,    also called the current selection. 

		getting the text the current selection, and storing it in a seperate string, 


		doing a find and replace, which searches for the first occurence of that first string, and replaces it with the second one. 
		this can be repeated with the same strings, without having to set them again and again. 

			note, that the text around the selection is actually printed out, (a bunch of characters before, and after) when we do this operation.



		i think i want to make it so that when you set the first string, the text is printed. ie, searching for strings is also the way you navigate the cursor, and visualize the document. pretty neat. 




so yeah, the typical usage would be 




	<anchor> type some text into the document <cut_and_obtain_selection_for_string1> 


								which also deletes what you had just typed too. anchor is unchanged. 
								but cursor = anchor. 

then 


	type some text to replace that string <cut_and_obtain_selection_for_string2>
							which would delete the string that was found in the document matching string1, 
							and will insert string2 in its place. so yeah. pretty simple. 


note, when you actually type the keyboard shortcut for <cut_and_obtain_selection_for_string1>
the display would have printed out a ton of text surrounding the thing you are searching for.
yeah, i think i want the <find_command> to be inreplace of <cut_and_obtain_selection_for_string1> 
it does the same thing, but it also updates the screen right when you do it, i think. for immediate feedback. that would work. 


	





note 

i think i want the string-search to have modulo behavior...     but its also like more effort to implement it like that lololol 
	hmm 
	idk 
				ill think about it 

						i defintiely don't want two different keybinds for forwards and backwards, i feel like thats just kinda wasteful 

					we only have so many keybinds that we can actually use with this thing lol 

						i want to keep it to like                AT MAX         5 key binds. 


								thats like if we do things really messy,      we'll be at 5 keybinds 


										ideallyyyyy 3 


								or 2 



									but probably 3 






	so yeah 



so we need at least one 



	for the anchor... and for string finding, and for replaceing... hmmm...



so here are the commands so far:  


	(in addition to ESC for quitting the editor, backspace for deleting characters, and other characters for inserting text, lol)









	control-A		set anchor 




	control-D		find-string




	control-R		replace-string 

















*/






























		                // note:  we don't update the display each key press. 
				// for the most part, this is handled by the terminal's ECHO mode. 
				// however, for scrolling the file, and navigation, we update the screen ourselves, 
				// after the user presses the associated key! very efficient! yay.  impervious to display bugs lol. 



















//	     if (c == 1/*A*/) sendc();
//	else if (c == 2/*B*/) {} 
//	// C
//	else if (c == 4/*D*/) sendc();
//	else if (c == 5/*E*/) {}
//	else if (c == 6/*F*/) {}
//	else if (c == 7/*G*/) {}
//	else if (c == 8/*H*/) copy();
//	// I
//	// J
//	else if (c == 11/*K*/) {}
//	else if (c == 12/*L*/) {}
//	// M
//	else if (c == 14/*N*/) {}
//	// O
//	else if (c == 16/*P*/) {/* redo(); */}
//	// Q
//	else if (c == 18/*R*/) paste();
//	// S
//	else if (c == 20/*T*/) {}
//	else if (c == 21/*U*/) undo();
//	// V
//	else if (c == 23/*W*/) {}
//	else if (c == 24/*X*/) { copy(); cut(); }
//	// Y
//	// Z
//	
//	//     d r
//	//   a   h
	//      x     

	//     u p 
	//
	//   


/*
	{ copy(); cut(); } // X
	copy(); // H
	paste(); // R

	sendc(); // D
	sendc(); // A

	undo(); // U
	redo(); // P	

*/



//	else if (c == 27) interpret_arrow_key();
//	else if (c == 127) delete_cut();
//
//	else if ((unsigned char) c >= 32 or c == 10 or c == 9) insert_replace(&c, 1);
//


/*



struct action {
	off_t parent;
	off_t pre;
	off_t post;
	off_t length;
};



// char filename[4096] = {0};

	//int flags = O_RDONLY;
	//mode_t permission = 0;

		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;




static void save() {            // simplify / collapse this function.

	int flags = O_WRONLY;
	mode_t permission = 0;

	if (argc < 2) {
		srand((unsigned)time(0)); rand();
		char datetime[32] = {0};
		struct timeval t = {0};
		gettimeofday(&t, NULL);
		struct tm* tm = localtime(&t.tv_sec);
		strftime(datetime, 32, "1%Y%m%d%u.%H%M%S", tm);
		snprintf(filename, sizeof filename, "%s_%08x%08x.txt", datetime, rand(), rand());
		flags |= O_CREAT | O_EXCL;
		permission = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	} else strlcpy(filename, argv[1], sizeof filename);

	int dir = open(directory, O_RDONLY | O_DIRECTORY, 0);
	if (dir < 0) { perror("read open directory"); exit(1); }
	int df = openat(dir, filename, O_RDONLY | O_DIRECTORY);
	if (df >= 0) { close(df); errno = EISDIR; goto read_error; }
	int file = openat(dir, filename, flags, permission);
	if (file < 0) { read_error: perror("read openat file"); exit(1); }

	write(file, text, count);
	close(file);
	close(dir);

}

// have a create_new() function,   a  load_existing()     and save()     function.    then do a save() on quit.




*/












/////////////////////    /////////////////////     /////////////////////    /////////////////////     ///////////////////// 

// how do we display the text?  does backspace work properly?   how do we display where we are in the document?....
// do we use a page size like the pager?     lets implement find-forwards next. thats important.
//   what keybind?  

/////////////////////    /////////////////////     /////////////////////    /////////////////////     ///////////////////// 




/*











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



















// write(1, "\033[7m/\033[0m", 9);



















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


