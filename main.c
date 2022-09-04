//  Daniel Warren Riaz Rehman's minimalist 
//        terminal-based text editor 
//
//     Designed with reliability, minimalism, 
//     simplicity, and ergonimics in mind.
//
//          tentatively named:   "t".
//
// -------------------------------------------
//
//    change log:
//          written on 2101177.005105
//           edited on 2111114.172631
//           edited on 2112116.194022
//         debugged on 2201252.173237
// 	   debugged on 2208022.211844
// 	   debugged on 2208151.002947

// 	     edited on 2209036.110503
// 	   

#include <iso646.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <termios.h>
#include <sys/time.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define is_fuzz_testing		0

#define memory_safe 	1
#define fuzz 		is_fuzz_testing
#define use_main    	not is_fuzz_testing

typedef ssize_t nat;

// todo: make these part of the config file / parameters.

static const char* autosave_directory = "/Users/dwrr/Documents/personal/autosaves/";

static const nat autosave_frequency = 8;     // in seconds

enum action_type {
	no_action,
	insert_action,
	delete_action,
	paste_text_action,
	cut_action,
	anchor_action,
};

struct line { 
	char* data; 
	nat count, capacity; 
};

struct buffer {
	struct line* lines;
	struct action* actions;

	nat     saved, mode, autosaved, 
		selecting, ww_fix, count, capacity, line_number_width,

		lcl, lcc, 	vcl, vcc,  	vol, voc, 
		vsl, vsc, 	vdc,    	lal, lac,

		wrap_width, tab_width, 
		show_status, show_line_numbers, 
		use_txt_extension_when_absent,
		line_number_color, status_bar_color, alert_prompt_color, 
		info_prompt_color, default_prompt_color,
		action_count, head
	;

	char filename[4096];
	char location[4096];
	char message[4096];
};

struct logical_state {
	nat     saved, autosaved, selecting, 
		ww_fix, wrap_width, tab_width,
		lcl, lcc, 	vcl, vcc,  	vol, voc, 
		vsl, vsc, 	vdc,    	lal, lac;
};

struct action { 
	nat* children; 
	char* text;
	nat parent;
	nat type;
	nat choice;
	nat count;
	nat length;
	struct logical_state pre;
	struct logical_state post;
};

static size_t fuzz_input_index = 0;       
static size_t fuzz_input_count = 0;       
static const uint8_t* fuzz_input = NULL;  
static nat window_rows = 0, window_columns = 0;
static nat buffer_count = 0, active_index = 0; 
static struct buffer* buffers = NULL;
static struct buffer this = {0}, textbox = {0}; 
static char selected_file[4096] = {0};
static char cwd[4096] = {0};
static pthread_mutex_t mutex;
static struct line* lines;
static struct action* actions;
static char* screen = NULL;
static nat 
	count, capacity, 
	action_count, head, 
	wrap_width, tab_width,
	lcl, lcc, 	vcl, vcc,  	vol, voc, 
	vsl, vsc, 	vdc,    	lal, lac
;


	
1. store registers to buffer
2. set active buffer to "after last buffer"
3. perform a context switch   (load active buffer into registers)
4. now the last buffer (the textbox) is in the registers! now use move left, and move right as normal, but prevent moving before, or at the end of a line! up-arrow and down-arrow change the history for the command buffer, 


actually


			do we just rip out the textbox system?... like literally...
				we dont even atually need it 


					like why exactly do we need it...!?


	
	

			currently, we are using it for command mode, 

				the ability to paste the current selection into the text buffer...

					a displaying which doesnt wrap the lines... which i dont think matters too much... 

						and then...

										ummm


						hmm


							actually 

	
					i feel like the ability to have the textbox NOT take up the entire screen is actually useful..

					at laest, when you want to be able to see both the textbox, and the screen at the same time. 

						so yeah.... i think we HAVE to keep it,  it is useful, at least visuallyyyyy



					butttt

					i think that we can totallyyyyy just make it use (ALMOST) ALLLLLL the buffer machinery, and literally act like just another buffer. 


				in fact, i actually dont see any reason why, we cant just...      wait no, we cant give control flow to the main loop, thatd be bad


					we use this function     a textbox,      for getting a string of course, 

						which.... 


							i mean, 



						.....if you think about it... 



							why cant you just type in the current buffer, in order to do that?

									like literally....

							hmmm



						okay, maybe we dont need the textbox?....  hmmmmmm



								well, i mean, if we get rid of it being a seperate place,    then like, 

								i just dont think its what we want, in alot of screnatious,   we also loose the whole history stufff.... so like, i would kinda prefer to not use the main loop   for printing buffers, and stuff
							(and of course, the idea would be that we could just have an if statement, checking if we are visualizing the textbox or just a regular buffer... 
					because the textbox would have index "buffer_count"   basically 


							that part, i knnow for sure. i want to store the textbox stuff there. 


						then, when we are saving, i guess i can just 



									save the textbox like i would any other file?


								or maybe it automatically saves, when you leave the textbox... actually, yeah i like that one more. 



								wait, how does it get smaller?  umm... i guess it doesnt lol. 




						hmm


								maybe we just delete the beginning portion of it, every now and then.. lol


					i mean, it isnt too hard to set a cap on the number of lines. lol. anyways 





					

	

			so, yeah, i am definitely using a   buffer   to store the textbox info, 


				and i am also definitelyyyyy settling on having a seperate main loop for it, one that has the custom commands, i think.... hmm... yeah i think we needddddd to 


							just because, with the history,  it behaves so differently from the other buffers. 

								



				i am technically still considering   making the current buffer itself, with all of its content, just be the textbox that we actully use to type in.... but i dont think thats a good idea.... because like, 


						what if we dont have edit permissions for the file?   hmmm



							or, i guess... what if we want to have a history?  we would totally loose our nice history stuffff    if we made the current file the textbox. even though, that would make things very easy, technically...


							i think a textbox is actually important enough, that we need to have a seperate thing for it. i just think we do. i think we need it to be seperate from the file itself. yeah. idk why,  but i really feel like we need to. 


	
						the textbox is actually part of confirming some mission critical duties, and so, i feel like we shouldnt be using "the current line" for that. hmmmmmmm


						i mean, if it was in a buffer all on its own, then itd be fine?


						i think?



							its just.. now we cant see the file lol. 


							i guess ideally, we could display two files at once.  thatd be ideal, i guess..



					although, that gets super complicated, and i dont reallyyy want to support that at all. 


							so yeah. i just want to have the textbox i think. its alittle bit more code, but its not that bad, i think. 



							



			oh wait



					actually 



					having a horizontal split, at least, is actually really simple, i think...?

				hmmmmmmmm


						its just... a tiny bit odd... 


						ill think about it... 






				i mean, it would be super cool to have the textbox just be a buffer.


	
	
					buttt!!!! what would be the prompt?




						like, how would we actually do anything?... wait



				this sounds like it doesnt exactly work?




			or, wait, i guesss we would just use the status... hmmm



				


			and i mean



			actually 

	
	
					technically 
	

	
		wait




		lol


			isnt the status bar, technically just a buffer, which holds that stufff lol



		its literally the other buffer 


				right there


			its the status buffer 



					hahaha


	
					hmm


									okay





							that could actually work




						and i guess, the thing is, this buffer is given the values from your current buffer. 


						and of course, 




			or 




		i guess 


					its more of just like... a scratch buffer, ,...?   thats interestinggg



				very very very very interestinggg



		



	

		if you use a horizontal split,  


			and just have it enabled alwaysss


						(like,you know the    show_status_bar var?   that is literally the split height, of the scratch buffer!!!!)




						so yeah,  that actually mightttt work 



								ohhh and you can set the split height to zero, of course,   


									which simply just sets the 




ohh


		yikes


			nopeee



							this is not good



						you would have to like... errr



										you literally couldnt use the split window thingy

														for yourrr files


											you couldnt 


											you could only use it for the scratch buffer




									which is used for... i guess textbox entry,  


									and also the status bar


			so yeah


						i mean, that genuinely could work actually


	
	
	
			it is definitely a huge redesign of that system 

					hmmm



	
	
	
		i meann, i kinda like it 

	
	

				so there is one buffer which is always displayed, no matter what.


						and thats the  scratch bufffer. 


	
						it issssss the status bar


						     ANDDDDD the textbox



					and all we need to make it work,   is the ability to display two files, 


							one on the top half of the screen,   and one on the bottom halft 


	
								superrrr simple, we already made our code, so we can do it easily, i think.. 

				hmm


	
							wow

	

				quite interesting, huh!


	

	
			of course,   when we display the scratch, we never pring the cursor inside it,   until we actually want to use it as a textbox!!!

						very very very very very cool 



				i think i am going to dedfinitely take this approach. 
	
	
				i think its super cool, actually 





					it also allows us to copy from the status message,   its just so unifying actually 

	
	
					



		so how does the scratch buffer work...?

	
				like, what is the layout of stuff?

	

	
	
		well

	
				the status stuff is at the top of the file.   it is constantly edited, as you use the editor, and the values are updated. they are kinda like displaying the registers of the editor, actually, which is super cool. 

				

							oh my god this is actaully beautiful., 




									OF COURSE,,,,    THE SCRATCH REGISTER LIVES AT BUFFER_COUNT!!!


												so beautiful 



						yessssssssssss


									i love this so much


											you can call it       "*n"  kinda


									yeah


											hahaha i love that so much



									its just like the ua theory 

	

				ohhh and    show_status_bar    is renamed to    scratch_lines_display_count   or something 


						and its a     nat     that you can incr, and decr.

								if it goes to zero, thats literlalyy     show_status = false


										and if you ever trigger a textbox,  


										then active_buffer = buffer_count




									which amazing and beautiful!!!!




				yayyyyyyyyyy



					i love this alot!!!


					yayyyyyyyyyyy


								so happy i found this 




		so anyways,  


			the textbox always appends to the end of the *n buffer. always. 

			


				it is automatically saved, upon exiting the textbox utility. i think. pretty sure. 

	
	
					andddd

								you cannnn actually move into it,   while NOT using the textbox utility. 


										so, you can literally edit it as a simple buffer. 


	
									ANDDDDDD it is always at the bottom where it is. 

										yeah. cool. that is literally so cool. 


												yayyyyy



										okay, so, if so, we literally can just update our display function, and then everything is simple!!!


						wow!!!



								what a simplification. 



								then, the textbox utility gets the current line, as a string, of course. 


										very simple. 


									i love this so much!!! so beautiful. 


									very very very very very very very cool 

							oh, also, the textbox utility actually inserts a string, on the line you have to give the answer. 



					ohhh, one thing


							you cant color things


						so like 


					yeah


								the status bar,   the prompt colors



							those all have to go 



							but thats fine haha      i didnt really need them anyways


											yeah, i like just black and white anyways



	
				



			okay, this is totally going to work, i think 


				i love this so much 


	
			


			the human is the comparator circuit, "<",    comparing   *i   (the current buffer)  and *n   the scratch buffer


							this literally could not be more correct, and beautiful. 


	
				like, i am totally certain, we found the way that we are going to write this editor, 


						and deal with split windows,   and the textbox,   and the status bar


								all of those odd things,     have all been unified under this single idea 

											of a scratch buffer 





							so amazing 


									so good 

							yayyyyyy



						i love it 



	
				should simply things, while allowing for more emergent features! i think 




				
						








	wow this is literally amazing          i am so happy i found this

			


				so


					note:      we are not storing the    "registers we are displaying in the status bar"


						we are not storingggggg those valuse in the scratch buffer, (aka the *n buffer!!)


							we are just           printtttinggggg   those values   into the first two lines of the scratch buffer,  because its useful to show them there. 



							very very veryuseful. 




		infact,    any register in the editor,  or for a buffer, 



					that we want to visualize, 



						we will actually   print    inside the scratch buffer!




						oh!!!



									including the meta data / registers    of   *n itself!!!



											that is also found inside the scatch buffer!



										i think 


										somewheer






						so yeah




				this is reallyyyy       like actually 



							the righe way to do things 




	
			












static inline bool zero_width(char c) { return (((unsigned char)c) >> 6) == 2;  }
static inline bool visual(char c) { return not zero_width(c); }
static inline bool file_exists(const char* f) { return access(f, F_OK) != -1; }

static inline nat unicode_strlen(const char* string) {
	nat i = 0, length = 0;
	while (string[i]) {
		if (visual(string[i])) length++;
		i++;
	}
	return length;
}

static inline char read_stdin() {
	char c = 0;
	if (fuzz) {
		if (fuzz_input_index >= fuzz_input_count) return 0;
		fuzz_input_index++;
		return (char) fuzz_input[fuzz_input_index - 1];
	}
	read(0, &c, 1);
	return c;
}

static inline bool equals(const char* s1, const char* s2) {
	if (strlen(s1) != strlen(s2)) return false;
	else return not strcmp(s1, s2);
}

static inline bool is_directory(const char *path) {
	struct stat s;
	if (stat(path, &s)) return false;
	return S_ISDIR(s.st_mode);
}

static inline void get_datetime(char datetime[16]) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm* tm_info = localtime(&tv.tv_sec);
	strftime(datetime, 15, "%y%m%d%u.%H%M%S", tm_info);
}

static inline bool stdin_is_empty() {
	if (fuzz) return fuzz_input_index >= fuzz_input_count;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);
	struct timeval timeout = {0};
	return select(1, &readfds, NULL, NULL, &timeout) != 1;
}

static inline struct termios configure_terminal() {
	struct termios save = {0};
	tcgetattr(0, &save);
	struct termios raw = save;
	raw.c_oflag &= ~( (unsigned long)OPOST );
	raw.c_iflag &= ~( (unsigned long)BRKINT 
			| (unsigned long)ICRNL 
			| (unsigned long)INPCK 
			| (unsigned long)IXON );	
	raw.c_lflag &= ~( (unsigned long)ECHO 
			| (unsigned long)ICANON 
			| (unsigned long)IEXTEN );
	tcsetattr(0, TCSAFLUSH, &raw);
	return save;
}

static inline nat compute_vcc() {
	nat v = 0;
	for (nat c = 0; c < lcc; c++) {
		char k = lines[lcl].data[c];
		if (k == '\t') { 
			if (v + tab_width - v % tab_width <= wrap_width) 
				do v++;
				while (v % tab_width);  
			else v = 0;
		} else if (visual(k)) {
			if (v < wrap_width) v++; else v = 0;
		}
	}
	return v;
}

static inline void move_left() {
	if (not lcc) {
		if (not lcl) return;
		lcl--;
		lcc = lines[lcl].count;
 line_up: 	vcl--;
		if (vsl) vsl--;
		else if (vol) vol--;
		vcc = compute_vcc();
		if (vcc >= window_columns - 1 - this.line_number_width) { 
			vsc = window_columns - 1 - this.line_number_width;  voc = vcc - vsc; 
		} else { vsc = vcc; voc = 0; }
	} else {
		do lcc--; while (lcc and zero_width(lines[lcl].data[lcc]));
		if (lines[lcl].data[lcc] == '\t') {
			const nat diff = tab_width - compute_vcc() % tab_width;
			if (vcc < diff) goto line_up;
			vcc -= diff;
			if (vsc >= diff) vsc -= diff;
			else if (voc >= diff - vsc) { voc -= diff - vsc; vsc = 0; }
		} else {
			if (not vcc) goto line_up;
			vcc--;
			if (vsc) vsc--; else if (voc) voc--;
		}
	}
}

static inline void move_right() {

	if (lcl >= count) return;
	if (lcc >= lines[lcl].count) {
		if (lcl + 1 >= count) return;
		lcl++; lcc = 0; 
line_down:	vcl++; vcc = 0; voc = 0; vsc = 0;
		if (vsl + 1 < window_rows - this.show_status) vsl++; 
		else vol++;
	} else {
		if (lines[lcl].data[lcc] == '\t') {
			do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
			if (vcc + tab_width - vcc % tab_width > wrap_width) goto line_down;
			do {
				vcc++; 
				if (vsc + 1 < window_columns - this.line_number_width) vsc++;
				else voc++;
			} while (vcc % tab_width);
			
		} else {
			do lcc++; while (lcc < lines[lcl].count and zero_width(lines[lcl].data[lcc]));
			if (vcc >= wrap_width) goto line_down;
			vcc++; 
			if (vsc + 1 < window_columns - this.line_number_width) vsc++; 
			else voc++;
		}
	}
}

static inline void move_up() {
	if (not vcl) {
		lcl = 0; lcc = 0;
		vcl = 0; vcc = 0;
		vol = 0; voc = 0;
		vsl = 0; vsc = 0;
		return;
	}
	nat line_target = vcl - 1;
	while (vcc and vcl > line_target) move_left(); 
	do move_left(); while (vcc > vdc and vcl == line_target);
	if (vcc > window_columns - this.line_number_width) { vsc = window_columns - this.line_number_width; voc = vcc - vsc; } 
	else { vsc = vcc; voc = 0; }
}

static inline void move_down() {
	nat line_target = vcl + 1;
	while (vcl < line_target) { 
		if (lcl == count - 1 and lcc == lines[lcl].count) return;
		move_right();
	}
	while (vcc < vdc and lcc < lines[lcl].count) {
		if (lines[lcl].data[lcc] == '\t' and vcc + (tab_width - (vcc % tab_width)) > vdc) return; // TODO: ^ WHAT IS THIS!?!?!?
		move_right();
	}
}

static inline void jump_line(nat line) {
	while (lcl < line and lcl < count) move_down();
	while (lcl > line and lcl) move_up();
}

static inline void jump_column(nat column) {
	while (lcc < column and lcc < lines[lcl].count) move_right();
	while (lcc > column and lcc) move_left();
	vdc = vcc;
}

static inline void jump_to(nat line, nat column) {
	jump_line(line);
	jump_column(column);
}

static inline void move_begin() {
	while (vcc) move_left();
	vdc = vcc;
}

static inline void move_end() {
	while (lcc < lines[lcl].count and vcc < wrap_width) move_right(); 
	vdc = vcc;
}

static inline void move_top() {
	lcl = 0; lcc = 0;
	vcl = 0; vcc = 0;
	vol = 0; voc = 0;
	vsl = 0; vsc = 0;
	vdc = 0;
}

static inline void move_bottom() {
	while (lcl < count - 1 or lcc < lines[lcl].count) move_down(); 
	vdc = vcc;
}

static inline void move_word_left() {
	do move_left();
	while (not(
		(not lcl and not lcc) or (
			(lcc < lines[lcl].count and isalnum(lines[lcl].data[lcc]))  and 
			(not lcc or not isalnum(lines[lcl].data[lcc - 1]))
		)
	));
	vdc = vcc;
}

static inline void move_word_right() {
	do move_right();
	while (not(
		(lcl >= count - 1 and lcc >= lines[lcl].count) or (
			(lcc >= lines[lcl].count or not isalnum(lines[lcl].data[lcc]))  and 
			(lcc and isalnum(lines[lcl].data[lcc - 1]))
		)
	));
	vdc = vcc;
}

static inline void record_logical_state(struct logical_state* pcond_out) {
	struct logical_state* p = pcond_out; 

	p->saved = this.saved;
	p->autosaved = this.autosaved;
	p->ww_fix = this.ww_fix;
	p->selecting = this.selecting;

	p->lcl = lcl;  p->lcc = lcc; 	
	p->vcl = vcl;  p->vcc = vcc;
  	p->vol = vol;  p->voc = voc;
	p->vsl = vsl;  p->vsc = vsc; 
	p->vdc = vdc;  p->lal = lal;
	p->lac = lac; 

	p->wrap_width = wrap_width;
	p->tab_width = tab_width;
}

static inline void require_logical_state(struct logical_state* pcond_in) {  
	struct logical_state* p = pcond_in;

	this.saved = p->saved;
	this.autosaved = p->autosaved;
	this.ww_fix = p->ww_fix;
	this.selecting = p->selecting;

	lcl = p->lcl;  lcc = p->lcc; 	
	vcl = p->vcl;  vcc = p->vcc;
  	vol = p->vol;  voc = p->voc;
	vsl = p->vsl;  vsc = p->vsc; 
	vdc = p->vdc;  lal = p->lal;
	lac = p->lac;

	wrap_width = p->wrap_width;
	tab_width = p->tab_width;
}

static inline void create_action(struct action new) {
	new.parent = head;
	actions[head].children = realloc(actions[head].children, sizeof(nat) * (size_t) (actions[head].count + 1));
	actions[head].choice = actions[head].count;
	actions[head].children[actions[head].count++] = action_count;
	
	actions = realloc(actions, sizeof(struct action) * (size_t)(action_count + 1));
	head = action_count;
	actions[action_count++] = new;
}

static inline void insert(char c, bool should_record) { 

	if (should_record and zero_width(c) 
		and not (
			actions[head].type == insert_action and 
			actions[head].text[0] != '\n' and
			actions[head].post.lcl == lcl and 
			actions[head].post.lcc == lcc and 
			actions[head].count == 0
		)) return; 

	struct action new_action = {0};
	if (should_record and visual(c)) record_logical_state(&new_action.pre);

	struct line* here = lines + lcl;
	if (c == '\n') {
		nat rest = here->count - lcc;
		here->count = lcc;
		struct line new = {malloc((size_t) rest), rest, rest};
		if (rest) memcpy(new.data, here->data + lcc, (size_t) rest);

		if (not memory_safe) {
			if (count + 1 >= capacity) 
				lines = realloc(lines, sizeof(struct line) * (size_t)(capacity = 8 * (capacity + 1)));
		} else {
			lines = realloc(lines, sizeof(struct line) * (size_t)(count + 1));
		}

		memmove(lines + lcl + 2, lines + lcl + 1, sizeof(struct line) * (size_t)(count - (lcl + 1)));
		lines[lcl + 1] = new;
		count++;

	} else {
		if (not memory_safe) {
			if (here->count + 1 >= here->capacity) 
				here->data = realloc(here->data, (size_t)(here->capacity = 8 * (here->capacity + 1)));
		} else {
			here->data = realloc(here->data, (size_t)(here->count + 1));
		}

		memmove(here->data + lcc + 1, here->data + lcc, (size_t) (here->count - lcc));
		here->data[lcc] = c;
		here->count++;
	}

	if (zero_width(c)) lcc++; 
	else { move_right(); vdc = vcc; }

	this.saved = false;
	this.autosaved = false;
	if (not should_record) return;
	lac = lcc; lal = lcl;

	if (zero_width(c)) {
		actions[head].text = realloc(actions[head].text, (size_t) actions[head].length + 1);
		actions[head].text[actions[head].length++] = c;
		record_logical_state(&actions[head].post);
		return;
	}

	record_logical_state(&new_action.post);
	new_action.type = insert_action;
	new_action.text = malloc(1);
	new_action.text[0] = c;
	new_action.length = 1;
	create_action(new_action);
}

static inline void delete(bool should_record) {

	struct action new_action = {0};
	if (should_record) record_logical_state(&new_action.pre);

	char* deleted_string = NULL;
	nat deleted_length = 0;
	struct line* here = lines + lcl;

	if (not lcc) {
		if (not lcl) return;
		move_left(); vdc = vcc;
		struct line* new = lines + lcl;

		if (not memory_safe) {
			if (new->count + here->count >= new->capacity)
				new->data = realloc(new->data, (size_t)(new->capacity = 8 * (new->capacity + here->count)));
		} else {
			new->data = realloc(new->data, (size_t)(new->count + here->count));
		}

		if (here->count) memcpy(new->data + new->count, here->data, (size_t) here->count);
		free(here->data);

		new->count += here->count;
		memmove(lines + lcl + 1, lines + lcl + 2, 
			sizeof(struct line) * (size_t)(count - (lcl + 2)));
		count--;

		if (memory_safe) lines = realloc(lines, sizeof(struct line) * (size_t)count);

		if (should_record) {
			deleted_length = 1;
			deleted_string = malloc(1);
			deleted_string[0] = '\n';
		}

	} else {
		nat save = lcc;
		move_left();
		vdc = vcc;
		
		if (should_record) {
			deleted_length = save - lcc;
			deleted_string = malloc((size_t) deleted_length);
			memcpy(deleted_string, here->data + lcc, (size_t) deleted_length);
		}

		memmove(here->data + lcc, here->data + save, (size_t)(here->count - save));
		here->count -= save - lcc;

		if (memory_safe) here->data = realloc(here->data, (size_t)(here->count));
	}

	this.saved = false;
	this.autosaved = false;
	if (not should_record) return;
	lac = lcc; lal = lcl;

	record_logical_state(&new_action.post);
	new_action.type = delete_action;
	new_action.text = deleted_string;
	new_action.length = deleted_length;
	create_action(new_action);
}

static void insert_string(const char* string, nat length) {
	struct action new = {0};
	record_logical_state(&new.pre);

	for (nat i = 0; i < length; i++) 
		insert((char) string[i], 0);
	
	record_logical_state(&new.post);
	new.type = paste_text_action;
	new.text = strndup(string, (size_t) length);
	new.length = length;
	create_action(new);
}

static inline void insertdt() {
	char datetime[16] = {0};
	get_datetime(datetime);
	insert_string(datetime, 14);
}

static inline void display() {

	static char* screen = NULL;
	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 15; window.ws_col = 50; }
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen = realloc(screen, (size_t) (window_rows * window_columns * 4));
	}

	const nat w = (window_columns - 1) - (this.line_number_width);
	if (this.ww_fix and wrap_width != w) wrap_width = w;

	nat length = 9; 
	memcpy(screen, "\033[?25l\033[H", 9);
	
	nat sl = 0, sc = 0; 
	nat vl = vol, vc = voc; 

	if (fuzz) {
		if (lcl < 0) abort();
		if (lcc < 0) abort();
		if (vol < 0) abort();
		if (voc < 0) abort();
		if (vcl < 0) abort();
		if (vcc < 0) abort();
		if (vsl < 0) abort();
		if (vsc < 0) abort();
	}

	struct logical_state state = {0};
	record_logical_state(&state);
	while (1) { 
		if (vcl <= 0 and vcc <= 0) break;
		if (vcl <= state.vol and vcc <= state.voc) break;
		move_left();
	}

 	nat line = lcl, col = lcc; 
	require_logical_state(&state); 

	double f = floor(log10((double) count)) + 1;
	int line_number_digits = (int)f;
	this.line_number_width = this.show_line_numbers * (line_number_digits + 2);

	do {
		if (line >= count) goto next_visual_line;

		if (this.show_line_numbers and vl >= vol and vl < vol + window_rows - this.show_status) {
			if (not col or (not sc and not sl)) 
				length += sprintf(screen + length, 
					"\033[38;5;%ldm%*ld\033[0m  ", 
					this.line_number_color + (line == lcl ? 5 : 0), 
					line_number_digits, line
				);
			else length += sprintf(screen + length, "%*s  " , line_number_digits, " ");
		}

		do {
			if (col >= lines[line].count) goto next_logical_line;  
			
			char k = lines[line].data[col++];

			if (k == '\t') {

				if (vc + (tab_width - vc % tab_width) > wrap_width) goto next_visual_line;

				do { 
					if (	vc >= voc and vc < voc + window_columns - this.line_number_width
					and 	vl >= vol and vl < vol + window_rows - this.show_status
					) {
						screen[length++] = ' '; sc++;
					}
					vc++;
				} while (vc % tab_width);

			} else {
				if (	vc >= voc and vc < voc + window_columns - this.line_number_width
				and 	vl >= vol and vl < vol + window_rows - this.show_status 
				and 	(sc or visual(k))
				) { 
					screen[length++] = k;
					if (visual(k)) sc++;	
				}
				if (visual(k)) {
					if (vc >= wrap_width) goto next_visual_line; 
					vc++; 
				} 
			}

		} while (sc < window_columns - this.line_number_width or col < lines[line].count);

	next_logical_line:
		line++; col = 0;

	next_visual_line:
		if (vl >= vol and vl < vol + window_rows - this.show_status) {
			screen[length++] = '\033';
			screen[length++] = '[';	
			screen[length++] = 'K';
			if (sl < window_rows - 1) {
				screen[length++] = '\r';
				screen[length++] = '\n';
			}
			sl++; sc = 0;
		}

		vl++; vc = 0; 

	} while (sl < window_rows - this.show_status);

	if (this.show_status) {

		nat status_length = sprintf(screen + length, " %ld %ld %ld %ld %ld %s %c%c %s",
			this.mode, 
			active_index, buffer_count,
			lcl, lcc, 
			this.filename, 
			this.saved ? 's' : 'e', 
			this.autosaved ? ' ' : '*',
			this.message
		);
		length += status_length;

		for (nat i = status_length; i < window_columns; i++)
			screen[length++] = ' ';

		screen[length++] = '\033';
		screen[length++] = '[';
		screen[length++] = 'm';
	}
    
	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", vsl + 1, vsc + 1 + this.line_number_width);

	if (not fuzz)  
		write(1, screen, (size_t) length);
}


static inline void textbox_display(const char* prompt, nat prompt_color) {

	struct winsize window = {0};
	ioctl(1, TIOCGWINSZ, &window);
	if (window.ws_row == 0 or window.ws_col == 0) { window.ws_row = 15; window.ws_col = 50; }
	if (window.ws_row != window_rows or window.ws_col != window_columns) {
		window_rows = window.ws_row;
		window_columns = window.ws_col - 1; 
		screen = realloc(screen, (size_t) (window_columns * 4));
	}

	nat length = sprintf(screen, "\033[?25l\033[%ld;1H\033[38;5;%ldm%s\033[m", window_rows, prompt_color, prompt);
	nat col = 0, vc = 0, sc = textbox.line_number_width;
	while (sc < window_columns and col < textbox.lines[lcl].count) {
		char k = textbox.lines[lcl].data[col];
		if (vc >= textbox.voc and vc < textbox.voc + window_columns - textbox.line_number_width and (sc or visual(k))) {
			screen[length++] = k;
			if (visual(k)) { sc++; }
		}
		if (visual(k)) vc++; 
		col++;
	} 

	for (nat i = sc; i < window_columns; i++) 
		screen[length++] = ' ';

	length += sprintf(screen + length, "\033[%ld;%ldH\033[?25h", window_rows, textbox.vsc + 1 + textbox.line_number_width);

	if (not fuzz) 
		write(1, screen, (size_t) length);
}

static inline void print_above_textbox(char* write_message, nat color) {
	nat length = sprintf(screen, "\033[%ld;1H\033[K\033[38;5;%ldm%s\033[m", window_rows - 1, color, write_message);
	if (not fuzz) 
		write(1, screen, (size_t) length);
}

static inline void clear_above_textbox() {
	nat length = sprintf(screen, "\033[%ld;1H\033[K", window_rows - 1);
	if (not fuzz) 
		write(1, screen, (size_t) length);
}

static inline void prompt(const char* prompt_message, nat color, char* out, nat out_size) {

	tb.prompt_length = unicode_strlen(prompt_message);
	do {
		adjust_window_size();
		textbox_display(prompt_message, color);

		char c = read_stdin();

		if (c == '\r' or c == '\n') break;
		else if (c == '\t') {  
			
			const nat path_length = (nat) strlen(user_selection);
			for (nat i = 0; i < path_length; i++) 
				textbox_insert(user_selection[i]);
		}
		else if (c == 27 and stdin_is_empty()) { tb.count = 0; break; }
		else if (c == 27) {
			c = read_stdin();

			if (c == '[') {
				c = read_stdin();

				if (c == 'A') {}
				else if (c == 'B') {}
				else if (c == 'C') textbox_move_right();
				else if (c == 'D') textbox_move_left();
			}
		} else if (c == 127) textbox_delete();
		else textbox_insert(c);

		if (fuzz) { 
			if (fuzz_input_index >= fuzz_input_count) break;
		}

	} while (1);
	if (tb.count > out_size) tb.count = out_size;
	memcpy(out, tb.data, (size_t) tb.count);
	memset(out + tb.count, 0, (size_t) out_size - (size_t) tb.count);
	out[out_size - 1] = 0;
	free(tb.data);
	tb = (struct textbox){0};
}

static inline bool confirmed(const char* question, const char* yes_action, const char* no_action) {

	char prompt_message[4096] = {0}, invalid_response[4096] = {0};
	sprintf(prompt_message, "%s? (%s/%s): ", question, yes_action, no_action);
	sprintf(invalid_response, "please type \"%s\" or \"%s\".", yes_action, no_action);
	
	while (1) {
		char response[4096] = {0};
		prompt(prompt_message, buffer.alert_prompt_color, response, sizeof response);
		clear_above_textbox();

		if (equals(response, yes_action)) return true;
		else if (equals(response, no_action)) return false;
		else if (equals(response, "")) return false;
		else print_above_textbox(invalid_response, buffer.default_prompt_color);

		if (fuzz) return true;
	}
}

static inline void store_current_data_to_buffer() {
	if (not buffer_count) return;

	const nat b = active_index;
	buffers[b] = this;

	buffers[b].wrap_width = wrap_width;
	buffers[b].tab_width = tab_width;
	buffers[b].capacity = capacity;
	buffers[b].count = count;
	buffers[b].head = head;
	buffers[b].action_count = action_count;
	buffers[b].actions = actions;
	buffers[b].lines = lines;

	buffers[b].lcl = lcl;  buffers[b].lcc = lcc; 
	buffers[b].vcl = vcl;  buffers[b].vcc = vcc; 
	buffers[b].vol = vol;  buffers[b].voc = voc; 
	buffers[b].vsl = vsl;  buffers[b].vsc = vsc; 
	buffers[b].vdc = vdc;  buffers[b].lal = lal;
	buffers[b].lac = lac; 
}

static inline void load_buffer_data_into_registers() {
	if (not buffer_count) return;

	struct buffer ba = buffers[active_index];
	this = ba;

	capacity = ba.capacity;
	count = ba.count;
	wrap_width = ba.wrap_width;
	tab_width = ba.tab_width;
	head = ba.head;
	action_count = ba.action_count;
	lines = ba.lines;
	actions = ba.actions;

	lcl = ba.lcl;  lcc = ba.lcc;
	vcl = ba.vcl;  vcc = ba.vcc;
	vol = ba.vol;  voc = ba.voc; 
	vsl = ba.vsl;  vsc = ba.vsc; 
	vdc = ba.vdc;  lal = ba.lal;
	lac = ba.lac;
}

static inline void zero_registers() {    // does this need to exist?... 

	wrap_width = 0;
	tab_width = 0;

	capacity = 0;
	count = 0;

	head = 0;
	action_count = 0;

	lines = NULL;
	actions = NULL;

	lcl = 0; lcc = 0; vcl = 0; vcc = 0; vol = 0; 
	voc = 0; vsl = 0; vsc = 0; vdc = 0; lal = 0; lac = 0;

	this = (struct buffer){0};
	textbox = (struct buffer){0};
	buffers = NULL;
	buffer_count = 0;
	active_index = 0;
}

static inline void initialize_registers() {
	
	wrap_width = 0;
	tab_width = 8; 
	capacity = 1;
	count = 1;
	head = 0;
	action_count = 1;
	lines = calloc(1, sizeof(struct line));
	actions = calloc(1, sizeof(struct action));

	lcl = 0; lcc = 0; vcl = 0; vcc = 0; vol = 0; 
	voc = 0; vsl = 0; vsc = 0; vdc = 0; lal = 0; lac = 0;

	this.show_status = 1;
	this.show_line_numbers = 1; 
	this.line_number_width = 0;
	this.saved = true;
	this.autosaved = true;
	this.mode = 0;
	this.alert_prompt_color = 196; 
	this.info_prompt_color = 45; 
	this.default_prompt_color = 214; 
	this.line_number_color = 236; 
	this.status_bar_color = 245; 
	this.use_txt_extension_when_absent = 1;

	memset(this.message, 0, sizeof message);
	memset(this.filename, 0, sizeof filename);
}

static inline void create_empty_buffer() {
	store_current_data_to_buffer();
	buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count + 1));
	buffers[buffer_count] = (struct buffer) {0};
	initialize_registers();
	active_index = buffer_count;
	buffer_count++;
	store_current_data_to_buffer();
}

static inline void close_active_buffer() {
	store_current_data_to_buffer();
	for (nat line = 0; line < buffers[active_index].count; line++) 
		free(buffers[active_index].lines[line].data);
	free(buffers[active_index].lines);

	for (nat a = 0; a < buffers[active_index].action_count; a++) {
		free(buffers[active_index].actions[a].text);
		free(buffers[active_index].actions[a].children);
	}
	free(buffers[active_index].actions);

	buffer_count--;
	memmove(buffers + active_index, buffers + active_index + 1, 
		sizeof(struct buffer) * (size_t)(buffer_count - active_index));
	if (active_index >= buffer_count) active_index = buffer_count - 1;
	buffers = realloc(buffers, sizeof(struct buffer) * (size_t)(buffer_count));
	load_buffer_data_into_registers();
}

static inline void move_to_next_buffer() {
	store_current_data_to_buffer(); 
	if (active_index) active_index--; 
	load_buffer_data_into_registers();
}

static inline void move_to_previous_buffer() {
	store_current_data_to_buffer(); 
	if (active_index < buffer_count - 1) active_index++; 
	load_buffer_data_into_registers();
}

static inline void open_file(const char* given_filename) {
	if (fuzz) return;

	if (not strlen(given_filename)) return;
	
	FILE* file = fopen(given_filename, "r");
	if (not file) {
		if (buffer_count) {
			sprintf(message, "error: fopen: %s", strerror(errno));
			return;
		}
		perror("fopen"); exit(1);
	}

	fseek(file, 0, SEEK_END);        
        size_t length = (size_t) ftell(file);
	char* text = malloc(sizeof(char) * length);
        fseek(file, 0, SEEK_SET);
        fread(text, sizeof(char), length, file);

	create_empty_buffer();
	adjust_window_size();
	for (size_t i = 0; i < length; i++) 
		insert(text[i], 0);

	free(text); 
	fclose(file);
	buffer.saved = true; 
	buffer.autosaved = true; 
	buffer.mode = 1; 
	move_top();
	strcpy(filename, given_filename);
	store_current_data_to_buffer();

	sprintf(message, "read %lub", length);
}

static inline void emergency_save_to_file() {
	
	if (fuzz) return;

	char dt[16] = {0};
	get_datetime(dt);

	char id[32] = {0};
	sprintf(id, "%08x%08x", rand(), rand());
	
	char local_filename[4096] = {0};
	sprintf(local_filename, "EMERGENCY_FILE_SAVE__%s__%s__.txt", dt, id);

	FILE* file = fopen(local_filename, "w+");
	if (not file) {
		printf("emergency error: %s\n", strerror(errno));
		return;
	}
	
	unsigned long long bytes = 0;
	for (nat i = 0; i < count; i++) {
		bytes += fwrite(lines[i].data, sizeof(char), (size_t) lines[i].count, file);
		if (i < count - 1) {
			fputc('\n', file);
			bytes++;
		}
	}

	if (ferror(file)) {
		printf("emergency error: %s\n", strerror(errno));
		fclose(file);
		return;
	}

	fclose(file);

	printf("interrupt: emergency wrote %lldb;%ldl to %s\n\r", bytes, count, local_filename);
}

static inline void emergency_save_all_buffers() {

	store_current_data_to_buffer();

	nat saved_active_index = active_index;
	for (int i = 0; i < buffer_count; i++) {
		
		active_index = i;
		load_buffer_data_into_registers();
		emergency_save_to_file(); 

		sleep(1);
	}
	
	active_index = saved_active_index;
	load_buffer_data_into_registers();
}

static inline void autosave() {

	if (fuzz) return;

	char dt[16] = {0};
	get_datetime(dt);

	char local_filename[4096] = {0};
	sprintf(local_filename, "%sautosave_%s_.txt", autosave_directory, dt);


	FILE* file = fopen(local_filename, "w+");
	if (not file) {
		printf("emergency error: %s\n", strerror(errno));
		return;
	}
	
	unsigned long long bytes = 0;
	for (nat i = 0; i < count; i++) {
		bytes += fwrite(lines[i].data, sizeof(char), (size_t) lines[i].count, file);
		if (i < count - 1) {
			fputc('\n', file);
			bytes++;
		}
	}

	if (ferror(file)) {
		printf("emergency error: %s\n", strerror(errno));
		fclose(file);
		return;
	}

	fclose(file);

	buffer.autosaved = true;
}

static void handle_signal_interrupt(int code) {

	if (fuzz) exit(1);

	printf(	"interrupt: caught signal SIGINT(%d), "
		"emergency saving...\n\r", 
		code);

	srand((unsigned)time(NULL));
	emergency_save_all_buffers();

	printf("press '.' to continue running process\n\r");
	int c = getchar(); 
	if (c != '.') exit(1);
}


static inline void save() {
	if (fuzz) return;

	if (not strlen(filename)) {

	prompt_filename:

		prompt("save as: ", buffer.default_prompt_color, filename, sizeof filename);

		if (not strlen(filename)) { sprintf(message, "aborted save"); return; }

		if (not strrchr(filename, '.') and buffer.use_txt_extension_when_absent) strcat(filename, ".txt");

		if (file_exists(filename) and not confirmed("file already exists, overwrite", "overwrite", "no")) {
			strcpy(filename, ""); goto prompt_filename;
		}

	}

	FILE* file = fopen(filename, "w+");
	if (not file) {
		sprintf(message, "error: %s", strerror(errno));
		strcpy(filename, "");
		return;
	}
	
	unsigned long long bytes = 0;
	for (nat i = 0; i < count; i++) {
		bytes += fwrite(lines[i].data, sizeof(char), (size_t) lines[i].count, file);
		if (i < count - 1) {
			fputc('\n', file);
			bytes++;
		}
	}

	if (ferror(file)) {
		sprintf(message, "error: %s", strerror(errno));
		strcpy(filename, "");
		fclose(file);
		return;
	}

	fclose(file);
	sprintf(message, "wrote %lldb;%ldl", bytes, count);
	buffer.saved = true;
}

static inline void rename_file() {
	if (fuzz) return;

	char new[4096] = {0};
	prompt_filename:
	prompt("rename to: ", buffer.default_prompt_color, new, sizeof new);
	if (not strlen(new)) { sprintf(message, "aborted rename"); return; }

	if (file_exists(new) and not confirmed("file already exists, overwrite", "overwrite", "no")) {
		strcpy(new, ""); goto prompt_filename;
	}

	if (rename(filename, new)) sprintf(message, "error: %s", strerror(errno));
	else {
		strncpy(filename, new, sizeof new);
		sprintf(message, "renamed to \"%s\"", filename);
	}
}

static inline void interpret_escape_code() {

	static nat scroll_counter = 0;

	char c = read_stdin();
	
	if (c == '[') {

		c = read_stdin();

		if (c == 'A') move_up();
		else if (c == 'B') move_down();
		else if (c == 'C') { move_right(); vdc = vcc; }
		else if (c == 'D') { move_left(); vdc = vcc; }

		//  TODO:   i need to completely redo the way that we scroll the screen, using mouse/trackpad scrolling. 

		else if (c == 'M') {

			read_stdin();

			if (c == 97) {

				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "scroll reporting: [%d:%d:%d]", c, str[0], str[1]);

				if (not scroll_counter) {
					move_down();
				}
				scroll_counter = (scroll_counter + 1) % 4;
				

			} else if (c == 96) {

				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y
				//sprintf(message, "scroll reporting: [%d:%d:%d]", c, str[0], str[1]);

	
				if (not scroll_counter) {
					move_up();
				}
				scroll_counter = (scroll_counter + 1) % 4;
				
			} else {
				char str[3] = {0};
				read(0, str + 0, 1); // x
				read(0, str + 1, 1); // y

				//sprintf(message, "mouse reporting: [%d:%d:%d].", c, str[0], str[1]);
			}
		}
	
	
	
	} 
}

static inline void prompt_open() {
	char new_filename[4096] = {0};
	prompt("open: ", buffer.default_prompt_color, new_filename, sizeof new_filename);
	if (not strlen(new_filename)) { sprintf(message, "aborted open"); return; }
	open_file(new_filename);
}

static inline void prompt_jump_line() {
	char string_number[128] = {0};
	prompt("line: ", buffer.default_prompt_color, string_number, sizeof string_number);
	nat line = atoi(string_number);
	if (line >= count) line = count - 1;
	jump_line(line);
	sprintf(message, "jumped to %ld %ld", lcl, lcc);
}

static inline void prompt_jump_column() {
	char string_number[128] = {0};
	prompt("column: ", buffer.default_prompt_color, string_number, sizeof string_number);
	nat column = atoi(string_number);
	if (column > lines[lcl].count) column = lines[lcl].count;
	jump_column(column);
	sprintf(message, "jumped to %ld %ld", lcl, lcc);
}

static char* get_sel(nat* out_length, nat first_line, nat first_column, nat last_line, nat last_column) {
	
	char* string = malloc(256);
	nat length = 0;

	nat s_capacity = 256;

	nat line = first_line, column = first_column;

	while (line < last_line) {
		
		if (not memory_safe) {
			if (length + lines[line].count - column + 1 >= s_capacity) 
				string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + lines[line].count - column + 1)));
		} else {
			string = realloc(string, (size_t) (length + lines[line].count - column));
		}

		if (lines[line].count - column) 
			memcpy(string + length, lines[line].data + column, (size_t)(lines[line].count - column));

		length += lines[line].count - column;

		if (not memory_safe) {
			// do nothing. 
		} else {
			string = realloc(string, (size_t) (length + 1));
		}

		string[length++] = '\n';
		line++;
		column = 0;
	}

	if (not memory_safe) {
		if (length + last_column - column >= s_capacity) 
		 	string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + last_column - column)));
	} else {
		string = realloc(string, (size_t) (length + last_column - column));	
	}

	if (last_column - column) memcpy(string + length, lines[line].data + column, (size_t)(last_column - column));
	length += last_column - column;
	*out_length = length;
	return string;
}

static inline char* get_selection(nat* out) {
	if (lal < lcl) return get_sel(out, lal, lac, lcl, lcc);
	if (lcl < lal) return get_sel(out, lcl, lcc, lal, lac);
	if (lac < lcc) return get_sel(out, lal, lac, lcl, lcc);
	if (lcc < lac) return get_sel(out, lcl, lcc, lal, lac);
	*out = 0;
	return NULL;
}

static inline void paste() {

 if (not fuzz) {

	FILE* file = popen("pbpaste", "r");
	if (not file) { sprintf(message, "error: paste: popen(): %s", strerror(errno)); return; }
	struct action new = {0};
	record_logical_state(&new.pre);
	char* string = malloc(256);
	nat s_capacity = 256, length = 0, c = 0;

	lac = lcc; lal = lcl; // todo: why are we doing this here!?

	while ((c = fgetc(file)) != EOF) {

		if (not memory_safe) {
			if (length + 1 >= s_capacity) 
				string = realloc(string, (size_t) (s_capacity = 8 * (s_capacity + length + 1)));
		} else {
			string = realloc(string, (size_t) (length + 1));
		}

		string[length++] = (char) c;
		insert((char)c, 0);
	}
	pclose(file);
	sprintf(message, "pasted %ldb", length);
	record_logical_state(&new.post);
	new.type = paste_text_action;
	new.text = string;
	new.length = length;
	create_action(new);


 } else {
 	struct action new = {0};
 	record_logical_state(&new.pre);
 	char* string = malloc(256);
 	nat length = 0;
 	lac = lcc; lal = lcl;
 	string = realloc(string, (size_t) (length + 1));
 	string[length++] = (char) 'A';
 	insert((char)'A', 0);
 	sprintf(message, "pasted %ldb", length);
 	record_logical_state(&new.post);
 	new.type = paste_text_action;
 	new.text = string;
 	new.length = length;
 	create_action(new);
 }
}

static inline void cut_selection() {
	if (lal < lcl) goto anchor_first;
	if (lcl < lal) goto cursor_first;
	if (lac < lcc) goto anchor_first;
	if (lcc < lac) goto cursor_first;
	return;
cursor_first:;
	nat line = lcl, column = lcc;
	while (lcl < lal or lcc < lac) move_right();
	lal = line; lac = column;
anchor_first:
	while (lal < lcl or lac < lcc) delete(0);
}

static inline void cut() { 
	struct action new = {0};
	record_logical_state(&new.pre);
	nat deleted_length = 0;
	char* deleted_string = get_selection(&deleted_length);
	cut_selection();
	sprintf(message, "deleted %ldb", deleted_length);
	record_logical_state(&new.post);
	new.type = cut_text_action;
	new.text = deleted_string;
	new.length = deleted_length;
	create_action(new);
}

static inline void copy() {
	if (fuzz) return;

	FILE* file = popen("pbcopy", "w");
	if (not file) { sprintf(message, "error: copy: popen(): %s", strerror(errno)); return; }

	nat length = 0;
	char* selection = get_selection(&length);
	fwrite(selection, 1, (size_t)length, file);
	pclose(file);
	free(selection);
	sprintf(message, "copied %ldb", length);
}

static inline void run_shell_command(const char* command) {
	FILE* f = popen(command, "r");
	if (not f) {
		sprintf(message, "error: could not run command \"%s\": %s\n", command, strerror(errno));
		return;
	}

	char line[4096] = {0};
	nat length = 0;
	char* output = NULL;

	while (fgets(line, sizeof line, f)) {
		const nat line_length = (nat) strlen(line);
		output = realloc(output, (size_t) length + 1 + (size_t) line_length); 
		sprintf(output + length, "%s", line);
		length += line_length;
	}
	pclose(f);
	lal = lcl; lac = lcc;
	sprintf(message, "output %ldb", length);
	insert_string(output, length);
}


static inline void prompt_run() {
	char command[4096] = {0};
	prompt("run(2>&1): ", 85, command, sizeof command);
	if (not strlen(command)) { sprintf(message, "aborted run"); return; }
	sprintf(message, "running: %s", command);
	run_shell_command(command);
}

static inline void replay_action(struct action a) {
	require_logical_state(&a.pre);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action or a.type == paste_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else if (a.type == delete_action) delete(0); 
	else if (a.type == cut_text_action) cut_selection();
	else sprintf(message, "error: unknown action");
	require_logical_state(&a.post); 
}

static inline void reverse_action(struct action a) {
	require_logical_state(&a.post);
	if (a.type == no_action or a.type == anchor_action) {}
	else if (a.type == insert_action) delete(0);
	else if (a.type == paste_text_action) {
		while (lcc > a.pre.lcc or lcl > a.pre.lcl) delete(0);
	} else if (a.type == delete_action or a.type == cut_text_action) {
		for (nat i = 0; i < a.length; i++) insert(a.text[i], 0);
	} else sprintf(message, "error: unknown action");
	require_logical_state(&a.pre);
}

static inline void undo() {
	if (not head) return;
	reverse_action(actions[head]);

	sprintf(message, "undoing %ld ", actions[head].type);
	if (actions[head].count != 1) 
		sprintf(message + strlen(message), "%ld %ld", actions[head].choice, actions[head].count);

	head = actions[head].parent;
}

static inline void undo_silent() {
	if (not head) return;
	reverse_action(actions[head]);
	head = actions[head].parent;
}

static inline void redo() {
	if (not actions[head].count) return;
	head = actions[head].children[actions[head].choice];
	sprintf(message, "redoing %ld ", actions[head].type);
	if (actions[head].count != 1) 
		sprintf(message + strlen(message), "%ld %ld", actions[head].choice, actions[head].count);
	replay_action(actions[head]);
}

static inline void alternate_incr() {
	if (actions[head].choice + 1 < actions[head].count) actions[head].choice++;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void alternate_decr() {
	if (actions[head].choice) actions[head].choice--;
	sprintf(message, "switched %ld %ld", actions[head].choice, actions[head].count);
}

static inline void recalculate_position() {
	nat save_lcl = lcl, save_lcc = lcc;
	move_top();
	adjust_window_size();
	jump_to(save_lcl, save_lcc);
}

static inline void open_directory() {
	
	DIR* directory = opendir(current_path);
	if (not directory) { 
		sprintf(message, "couldnt open path=%s, reason=%s", current_path, strerror(errno));	
		return;
	}

	struct dirent *e = NULL;

	nat length = (nat)strlen(current_path) + 2;
	char* menu = calloc((size_t) length + 1, sizeof(char));
	sprintf(menu, ":%s\n", current_path);
	
	while ((e = readdir(directory))) {

		char path[4096] = {0};
		strlcpy(path, current_path, sizeof path);
		strlcat(path, e->d_name, sizeof path);

		if (is_directory(path)) {
			menu = realloc(menu, sizeof(char) * ((size_t) length + 3 + strlen(e->d_name))); 
			sprintf(menu + length, "%s/\n", e->d_name);
			length += 2 + strlen(e->d_name);
		} else {
			menu = realloc(menu, sizeof(char) * ((size_t) length + 2 + strlen(e->d_name))); 
			sprintf(menu + length, "%s\n", e->d_name);
			length += 1 + strlen(e->d_name);
		}
	}

	closedir(directory);

	lal = lcl; lac = lcc;
	insert_string(menu, length);
	jump_to(lal, lac);	
}

static inline void change_directory() {

	char* selection = strndup(lines[lcl].data, (size_t) lines[lcl].count);

	if (equals(selection, "../") ) {
		if (not equals(current_path, "/")) {
			current_path[strlen(current_path) - 1] = 0;
			*(1+strrchr(current_path, '/')) = 0;
		} else {
			sprintf(message, "error: at root /");
		}
		
	} else if (equals(selection, "./") ) {
		// do nothing.

	} else {
		char path[4096] = {0};
		strlcpy(path, current_path, sizeof path);
		strlcat(path, selection, sizeof path);

		if (is_directory(path)) {
			strlcat(current_path, selection, sizeof current_path);
		} else {
			sprintf(message, "error: not a directory");
		}
	}
}

static inline void file_select() {
	char* line = strndup(lines[lcl].data, (size_t) lines[lcl].count);
	strlcpy(user_selection, current_path, sizeof user_selection);
	strlcat(user_selection, line, sizeof user_selection);
	free(line);
	sprintf(message, "selected: %s", user_selection);
}

static inline char** split(char* string, char delim, int* array_count) {

	int a_count = 0;
	char** array = NULL;
	int start = 0, i = 0;
	const int length = (int)strlen(string);

	for (; i < length; i++) {
		if (string[i] == delim) {
			string[i] = 0;
			if (strlen(string + start)) {
				array = realloc(array, sizeof(char*) * (size_t)(a_count + 1));
				array[a_count++] = strdup(string + start);
			}
			start = i + 1;
		}
	}

	if (strlen(string + start)) {
		array = realloc(array, sizeof(char*) * (size_t)(a_count + 1));
		array[a_count++] = strdup(string + start);
	}

	*array_count = a_count;
	return array;
}

static inline void execute(char c, char p) {

	if (buffer.mode == 0) {

		if (c == 'c' and p == 'h') { undo(); buffer.mode = 1; }
		else if (c == 27 and stdin_is_empty()) buffer.mode = 1;
		else if (c == 27) interpret_escape_code();
		else if (c == 127) delete(1);
		else if (c == '\r') insert('\n', 1);
		else insert(c, 1);

	} else if (buffer.mode == 1) {

	

		if (true/*STATE__should_recent_anchor*/) { lal = lcl; lac = lcc; }      // where do we do this?
	

		

	
	// all anchor() does now, is just sets the anchor (?... no..?),   and then    sets  STATE_should_recent_anchor to be false.  and then when you do a cut or paste, or anything that uses anchor, it sets it to be 1 again. 
				

	
		if (c == ' ');

		// else if (c == 'l' and p == 'e') prompt_jump_line();         // unbind this?... yeah...
		// else if (c == 'k' and p == 'e') prompt_jump_column();       // unbind this?... hmm...
		// else if (c == 'd' and p == 'h') prompt_open();
		// else if (c == 'f' and p == 'h') create_empty_buffer();


		else if (c == 'l' and p == 'e') {}
		else if (c == 'k' and p == 'e') {}
		else if (c == 'i' and p == 'e') move_bottom();
		else if (c == 'p' and p == 'e') move_top();
		else if (c == 'n' and p == 'e') move_begin();
		else if (c == 'o' and p == 'e') move_end();

		else if (c == 'u' and p == 'e') alternate_decr();
		else if (c == 'r' and p == 'h') alternate_incr();

		else if (c == 'a' and p == 'h') move_to_previous_buffer();
		else if (c == 's' and p == 'h') move_to_next_buffer();

		else if (c == 'm' and p == 'h') copy();
		else if (c == 'c' and p == 'h') paste(); 
		
		else if (c == 't' and p == 'h') anchor();
		else if (c == 'd' and p == 'h') {}
		

		else if (c == 'a') {}
		else if (c == 'd') delete(1);
		else if (c == 'r') cut();

		else if (c == 't') buffer.mode = 0;
		else if (c == 'm') { buffer.mode = 2; goto command_mode; }

		else if (c == 'c') undo();
		else if (c == 'k') redo();

		else if (c == 'o') move_word_right(); 
		else if (c == 'l') move_word_left(); 

		else if (c == 'p') move_up();
		else if (c == 'u') move_down();
		else if (c == 'i') { move_right(); vdc = vcc; }
		else if (c == 'n') { move_left(); vdc = vcc; }

		else if (c == 's') save();

		else if (c == 'q') {
			if (buffer.saved or confirmed("discard unsaved changes", "discard", "no")) 
				close_active_buffer(); 
		}

		else if (c == 27 and stdin_is_empty()) {}
		else if (c == 27) interpret_escape_code();

	} else if (buffer.mode == 2) {

		command_mode: buffer.mode = 1;

		char string[4096] = {0};
		prompt(" •• ", 82, string, sizeof string);
		
		nat length = (nat) strlen(string);
		char* d = calloc((size_t) length + 1, sizeof(char));
		nat d_length = 0;

		for (nat i = 0; i < length; i++) {
			if ((unsigned char) string[i] < 33) continue;
			d[d_length++] = string[i];
		}

		d[d_length] = 0;
		int command_count = 0;
		char** commands = split(d, '.', &command_count);

		for (int i = 0; i < command_count; i++) {
			const char* command = commands[i];

			     if (equals(command, "donothing")) {}
			else if (equals(command, "datetime")) insertdt();
			else if (equals(command, "run")) prompt_run();
			else if (equals(command, "run")) prompt_run();
			else if (equals(command, "open")) prompt_open();
			else if (equals(command, "rename")) rename_file();
			else if (equals(command, "save")) save();
			else if (equals(command, "duplicate")) {/* duplicate(); */}
			else if (equals(command, "autosave")) autosave();
			else if (equals(command, "in")) { change_directory(); undo_silent(); open_directory(); }
			else if (equals(command, "changedirectory")) change_directory();
			else if (equals(command, "opendirectory")) open_directory();
			else if (equals(command, "selectfile")) file_select();
			else if (equals(command, "openfile")) { file_select(); open_file(user_selection); }
			else if (equals(command, "selection")) sprintf(message, "%s", user_selection);
			else if (equals(command, "where")) sprintf(message, "@ %s", current_path);
			else if (equals(command, "home")) { getcwd(current_path, sizeof current_path); 
								strlcat(current_path, "/", sizeof current_path); }
			else if (equals(command, "clearmessage")) memset(message, 0, sizeof message);
			else if (equals(command, "numbers")) show_line_numbers = not show_line_numbers;
			else if (equals(command, "status")) show_status = not show_status;
			else if (equals(command, "cut")) cut();
			else if (equals(command, "delete")) delete(1);
			else if (equals(command, "anchor")) anchor();
			else if (equals(command, "paste")) paste();
			else if (equals(command, "copy")) copy();
			else if (equals(command, "undo")) undo();
			else if (equals(command, "redo")) redo();
			else if (equals(command, "alternateincr")) alternate_incr();
			else if (equals(command, "alternatedecr")) alternate_decr();
			else if (equals(command, "moveright")) { move_right(); vdc = vcc; }
			else if (equals(command, "moveleft")) { move_left(); vdc = vcc; }
			else if (equals(command, "moveup")) move_up();
			else if (equals(command, "movedown")) move_down();
			else if (equals(command, "movewordright")) move_word_right();
			else if (equals(command, "movewordleft")) move_word_left();
			else if (equals(command, "movebegin")) move_begin();
			else if (equals(command, "moveend")) move_end();
			else if (equals(command, "movetop")) move_top();
			else if (equals(command, "movebottom")) move_bottom();
			else if (equals(command, "jumpline")) prompt_jump_line();
			else if (equals(command, "jumpcolumn")) prompt_jump_column();
			else if (equals(command, "new")) create_empty_buffer();
			else if (equals(command, "nextbuffer")) move_to_next_buffer();
			else if (equals(command, "previousbuffer")) move_to_previous_buffer();			
			else if (equals(command, "mode0")) buffer.mode = 0;
			else if (equals(command, "mode1")) buffer.mode = 1;
			else if (equals(command, "mode2")) buffer.mode = 2;

			else if (equals(command, "goto")) {}
			else if (equals(command, "branch")) {}
			else if (equals(command, "incr")) {}
			else if (equals(command, "setzero")) {}

			else if (equals(command, "commandcount")) sprintf(message, "command count = %d", command_count);

		
			else if (equals(command, "wrapresizetemp")) { wrap_width = 0; recalculate_position(); }
			else if (equals(command, "quit")) {
				if (buffer.saved or confirmed("discard unsaved changes", "discard", "no")) 
					close_active_buffer(); 

			} else {
				sprintf(message, "command not recognized: %s", command);
				break;
			}
		}

	} else buffer.mode = 1;
}



static void* autosaver(void* unused) {

	while (1) {
		sleep(autosave_frequency);
		pthread_mutex_lock(&mutex);
		if (not buffer_count) break;
		if (not buffer.autosaved) autosave();
		pthread_mutex_unlock(&mutex);
	}

	return NULL;
}

static inline void editor() {

	struct termios terminal;
	static pthread_t autosave_thread;

	if (not fuzz) {
		terminal = configure_terminal();
		write(1, "\033[?1049h\033[?1000h\033[7l", 20);
		buffer.needs_display_update = 1;
		pthread_mutex_init(&mutex, NULL);
		pthread_mutex_lock(&mutex);
		pthread_create(&autosave_thread, NULL, &autosaver, NULL);

	} 

	char p = 0, c = 0;

    	getcwd(current_path, sizeof current_path);
	strlcat(current_path, "/", sizeof current_path);
loop:
	// if (buffer.needs_display_update) 
	display();

	pthread_mutex_unlock(&mutex);
	c = read_stdin(); 
	pthread_mutex_lock(&mutex);
	if (fuzz and not c) goto done;

	//buffer.needs_display_update = 1;
	execute(c, p);
	p = c;
	if (buffer_count) goto loop;



done:
	while (buffer_count) close_active_buffer();
	zero_registers();
	free(screen);
	screen = NULL;
	window_rows = 0;
	window_columns = 0;

	if (not fuzz) {
		write(1, "\033[?1049l\033[?1000l\033[7h", 20);	
		tcsetattr(0, TCSAFLUSH, &terminal);
		pthread_mutex_unlock(&mutex);
		pthread_detach(autosave_thread);
		pthread_mutex_destroy(&mutex);
	}
}


#if fuzz && !use_main

int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *input, size_t size) {
	create_empty_buffer();

	fuzz_input_index = 0; 
	fuzz_input = _input;
	fuzz_input_count = _input_count;

	editor();

	return 0;
}

#else

int main(const int argc, const char** argv) {
	if (argc <= 1) create_empty_buffer();
	else for (int i = 1; i < argc; i++) open_file(argv[i]);
	signal(SIGINT, handle_signal_interrupt);
	editor(NULL, 0);
}

#endif


// ---------------------------------------------------------------------------------------------------



































/*

	char buffer[4096] = {0};
	printf("input: ");
	fgets(buffer, sizeof buffer, stdin);
	buffer[strlen(buffer) - 1] = 0;


int array_count = 0;
	char* string = buffer;

	nat length = (nat) strlen(string);
	char* d = calloc((size_t) length + 1, sizeof(char));
	nat d_length = 0;

	for (nat i = 0; i < length; i++) {
		if ((unsigned char) string[i] < 33) continue;
		d[d_length++] = string[i];
	}

	d[d_length] = 0;

	char** array = split(d, '.', &array_count);

	printf("(%d)[ ", array_count);
	for (int i = 0; i < array_count; i++) {
		printf("\"%s\" ", array[i]);
	}

	puts("]\ndone!");




	char buffer[4096] = {0};
	printf("input: ");
	fgets(buffer, sizeof buffer, stdin);
	buffer[strlen(buffer) - 1] = 0;

	char* d = string;
	char* s = string;

	do while(isspace(*s)) s++; while(*d++ = *s++);



	int array_count = 0;
	char* string = buffer;
	char** array = split(d, ' ', &array_count);

	printf("(%d)[ ", array_count);
	for (int i = 0; i < array_count; i++) {
		printf("\"%s\" ", array[i]);
	}

	puts("]\ndone!");






	exit(1);













//while (i < length and string[i] == delim) ++i; 
//printf("B start=%d, i=%d\n", start, i);
//if (i < length) i++;
//printf("A start=%d, i=%d\n", start, i);
//printf("Q start=%d, i=%d\n", start, i);
//while (i < length and string[i] == delim) ++i; 
//string[i] = 0;
//	if (i < length) i++;
//	start = i;

//printf("B start=%d, i=%d\n", start, i);

*/




	// -----------------------------------------
	


	// keybingings to change still:

		//else if (c == 'E') show_status = not show_status;            // unbind this.  make this a written out command.
		//else if (c == 'N') show_line_numbers = not show_line_numbers;    // unbind this.  make this a written out command.

		//else if (c == 'F') prompt_open();      		// both bind, and  make this a written out command.
		//else if (c == 'S') rename_file();     				// unbind this. make this a written-out command. 

		//else if (c == '_') memset(message, 0, sizeof message);    	// unbind this. make this a written out command
		//else if (c == '\\') { wrap_width = 0; recalculate_position(); }  // make this part of the set command. 
	
		// im not sure what to do about these....

		//else if (c == '\r') menu_select();         // make this a written out command...
		//else if (c == '\"') menu_change();         // make this a written out command...
		//else if (c == ';')  menu_display();        // make this a written out command...

		//else if (c == '\'') { menu_change(); undo_silent(); menu_display(); }      // make this a written out command...

		// else if (c == '1') {insertdt();}        // make this a written out command...
		









			// todo:  implement a global history for the prompt textbox!!!!
			/*
					very important. 

							also, save it to the config file..? not sure... hmm.. yeah... 

							we should have a directory that we save all of our config files to. 

				*/	















/*
static void insert_printable_string(const char* string, nat length) {
	struct action new = {0};
	record_logical_state(&new.pre);

	for (nat i = 0; i < length; i++) {
		if (isprint((char) string[i])) insert((char) string[i], 0);
	}
	
	record_logical_state(&new.post);
	new.type = paste_text_action;
	new.text = strndup(string, (size_t) length);
	new.length = length;
	create_action(new);
}
*/

/*
static void insert_cstring(const char* string) { insert_string(string, (nat) strlen(string)); }
*/








/*

static inline bool is_regular_file(const char *path) {
	struct stat s;
	stat(path, &s);
	return S_ISREG(s.st_mode);
}

*/







/*



static inline void textbox_move_left() {
	if (not tb.c) return;
	do tb.c--; while (tb.c and zero_width(tb.data[tb.c]));
	tb.vc--; 
	if (tb.vs) tb.vs--; else if (tb.vo) tb.vo--;
}

static inline void textbox_move_right() {
	if (tb.c >= tb.count) return;
	tb.vc++; 
	if (tb.vs < window_columns - tb.prompt_length) tb.vs++; else tb.vo++;
	do tb.c++; while (tb.c < tb.count and zero_width(tb.data[tb.c]));
}

static inline void textbox_insert(char c) {

	if (not memory_safe) {
		if (tb.count + 1 >= tb.capacity) 
			tb.data = realloc(tb.data, (size_t)(tb.capacity = 8 * (tb.capacity + 1)));
	} else {
		tb.data = realloc(tb.data, (size_t)(tb.count + 1));
	}

	memmove(tb.data + tb.c + 1, tb.data + tb.c, (size_t) (tb.count - tb.c));
	tb.data[tb.c] = c;
	tb.count++;
	if (zero_width(c)) tb.c++; else textbox_move_right();
}

static inline void textbox_delete() {
	if (not tb.c) return;
	nat save = tb.c;
	textbox_move_left();
	memmove(tb.data + tb.c, tb.data + save, (size_t)(tb.count - save));
	tb.count -= save - tb.c;

	if (not memory_safe) {
		// do nothing.
	} else {
		tb.data = realloc(tb.data, (size_t)(tb.count));
	}
}



*/
















					
//static struct textbox tb = {0};      // this should be just a particular buffer, actually. 
				     // its just simpler that way. but it will be rendered differently.







/*
static inline nat compute_custom_vcc(nat given_lcc) {
	nat v = 0;
	for (nat c = 0; c < given_lcc; c++) {
		char k = lines[lcl].data[c];
		if (k == '\t') {
			if (v + tab_width - v % tab_width <= wrap_width)
				do v++; 
				while (v % tab_width);
			else v = 0;
		} else if (visual(k)) {
			if (v < wrap_width) v++; else v = 0;
		}
	}
	return v;
}
/// consolidate these two functions:


static inline char read_stdin() {
	char c = 0;
	if (fuzz) {
		if (fuzz_input_index >= fuzz_input_count) return;
		c = (char) fuzz_input[fuzz_input_index++];	
	} else {
		read(0, &c, 1);
	}
	return c;
}




*/
/*




static inline void anchor() {

	// struct action new = {.type = anchor_action};

	// record_logical_state(&new.pre);

	// all this should do is just set   STATE_should_recent_anchor = false;      thats it. 

	// lal = lcl; lac = lcc;    // it shouldnt do this. i think...?


	// sprintf(message, "anchor %ld %ld", lal, lac);

	// record_logical_state(&new.post);
	// create_action(new);
}


*/



	/*
		after every single movement command, i think. yeah. i think it needs to be done like that too.   yikes. 

			i think we just want to do it inside the actual movement commands themselves, actually. yeah. i think so. 

				okay, cool. 






	
*/






/*
struct textbox {
	char* data;
	nat 
		count, capacity, prompt_length, 
		c, vc, vs, vo
	;
};

static struct line* lines = NULL;

static struct action* actions = NULL;

static nat 
	count = 0, capacity = 0, 
	line_number_width = 0, tab_width = 0, wrap_width = 0, 
	show_status = 0, 
	lcl = 0, lcc = 0,  lal = 0, lac = 0,
	vcl = 0, vcc = 0,  vol = 0, voc = 0,  vsl = 0, vsc = 0, 
	vdc = 0,
	head, action_count;

static char message[4096] = {0};

static char filename[4096] = {0};

*/







/*
	this.saved = ba.saved;
	this.autosaved = ba.autosaved;
	this.mode = ba.mode;

	this.line_number_width = ba.line_number_width;
	this.needs_display_update = ba.needs_display_update;
	this.alert_prompt_color = ba.alert_prompt_color;
	this.info_prompt_color = ba.info_prompt_color;
	this.default_prompt_color = ba.default_prompt_color;
	this.line_number_color = ba.line_number_color;
	this.status_bar_color = ba.status_bar_color;

	this.show_status = ba.show_status;
	this.show_line_numbers = ba.show_line_numbers;
	this.use_txt_extension_when_absent = ba.use_txt_extension_when_absent;

	memcpy(this.message, ba.message, sizeof this.message);
	memcpy(this.filename, ba.filename, sizeof this.filename);
*/






/*	
	buffers[b].saved = buffer.saved;
	buffers[b].autosaved = buffer.autosaved;
	buffers[b].mode = buffer.mode;

	buffers[b].line_number_width = line_number_width;
	buffers[b].needs_display_update = buffer.needs_display_update;
	buffers[b].show_status = show_status;
	buffers[b].show_line_numbers = show_line_numbers;

	buffers[b].alert_prompt_color = buffer.alert_prompt_color;
	buffers[b].info_prompt_color = buffer.info_prompt_color;
	buffers[b].default_prompt_color = buffer.default_prompt_color;
	buffers[b].line_number_color = buffer.line_number_color;
	buffers[b].status_bar_color = buffer.status_bar_color;


	buffers[b].use_txt_extension_when_absent = buffer.use_txt_extension_when_absent;

	memcpy(buffers[b].message, this.message, sizeof message);
	memcpy(buffers[b].filename, this.filename, sizeof filename);
*/



