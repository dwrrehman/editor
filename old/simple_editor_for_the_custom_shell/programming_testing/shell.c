// a shell which only requires the 
// tab and enter keys to use.
// written on 1202408117.173609 by dwrr. 

// added the shell functionality on 1202409253.003056 
// to allow this to be used for programming,
// running shell commands, and editing files, 
// using an external program for opening/writing to files.



/*


	current state of things:

		- we need to have a pipeset (0, 1, 2)  for each job,   a name/command
			and a  read-write  cli interface   and job index 
						for each job

				and we should be able to go inbetween them easily!

				note:   each job actually has its own   terminal. they don't share stdout. or stderr.  rather, they have their own seperate places where they can output data lol. 



								thus, outputting doesnt happen automatically. but rather, when we open the terminal or that job,  a read command is issued to grab all the ready output. 


		- we need to have a way of going btewen jobs, 
		we need to have a way of    issueing a read command, 

			we need a way of issueing a write command,   ie, submitting input. 


					a submit button, 


			and we need a way of scrolling up in the output,  

				ie, the output has an origin. it is a large string, which is printed out, from a particular position, for exactly a page worth of data. simple as that! nice. 


		- we also have to do path lookup, for the command. 

		- 	we need to have a job    be just   a   terminal window, 

			and we need to have  an option for creating a new terminal window.

			and closing a terminal window if theres a job running, or if theres not one running. 


		- we need a way of switching between jobs too. thats also important. 


		- also sending a particular signal to a job is important too. we'll recreate that eventually, i think. hmm wow. interesting. 



		- 







list of things to do:

	- store the job data in a struct 

	- make mulitple jobs, be able to switch between them, and give reads/writes good still. 

	- then parse the input string given from the two key system, into a series of strings, 
		via using   the escape character as the argument demarkater. i thinkkk...

						either that, or a dedicated symbol.

	- then do path look up on the first element of that array of strings, 
		trying to find the full executable name. 

	- then pipe that revised/modified list of strings    to the execute function, which will start a new job, and give that list to execve, and also fork and start up the parent-side code for interacting with that. 

			here is where we will implement the scroll back buffer with the job too. thats also important. so yeah. 







*/


#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <sys/wait.h> 
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <sys/wait.h> 
#include <stdint.h>
#include <signal.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iso646.h>

static struct winsize window = {0};
static struct termios terminal = {0};
extern char** environ;
typedef uint64_t nat;

static const char select_key = '\\';
static const char cycle_key = '`';

enum commands {
	root, back, in, delete, left, right, up, down, 
	space, newline, tab, escape, 
	read_output, submit, finishjob, sendeof, newjob, closejob, nextjob, quit, 

	ins_e,
	ins_t,
	ins_a,
	ins_o,
	ins_i,
	ins_dot,
	ins_comma,
	ins_open_paren,
	ins_close_paren,

	ins_n,
	ins_s,
	ins_r,
	ins_h,
	ins_d,
	ins_apostrophe,
	ins_question_mark,
	ins_open_square,
	ins_close_square,
	
	ins_l,
	ins_u,
	ins_c,
	ins_m,
	ins_f,
	ins_quote,
	ins_exclamation_mark,
	ins_open_brace,
	ins_close_brace,

	ins_y,
	ins_w,
	ins_g,
	ins_p,
	ins_b,
	ins_hyphen,
	ins_colon,
	ins_less,
	ins_greater,
	
	ins_v,
	ins_k,
	ins_x,
	ins_q,
	ins_j,
	ins_z,
	ins_underscore,
	ins_semicolon,
	ins_ampersand,
	ins_vertical,

	ins_0,
	ins_1,
	ins_2,
	ins_3,
	ins_4,
	ins_equals,
	ins_plus,
	ins_star,
	ins_pound,
	ins_at_sign,
	
	ins_5,
	ins_6,
	ins_7,
	ins_8,
	ins_9,
	ins_slash,
	ins_percent,
	ins_caret,
	ins_backslash,
	ins_grave,
	ins_tilde,
	
	command_count,
};

static const char* spelling[command_count] = {
	" ", "\\", "/", "!", "<", ">", "^", "v", 
	"sp", "nl", "tab", "esc", 
	"read", "write", "wait", "eof", "new", "close", "switch", "exit",

	"e","t","a","o","i",".",",","(",")",
	"n","s","r","h","d","'","?","[","]",
	"l","u","c","m","f","\"","!","{","}",
	"y","w","g","p","b","-",":","<",">",
	"v","k","x","q","j","z","_",";","&","|",
	"0","1","2","3","4","=","+","*","#","@",
	"5","6","7","8","9","/","%","^","\\","`","~",
};


struct job {
	char** arguments;
	char* output;
	nat status;
	nat origin;
	nat length;
	int fd[2];
	int rfd[2];
	int fdm[2];
	int padding;
	pid_t pid;
};


static void window_resized(int _) { if(_){} ioctl(0, TIOCGWINSZ, &window); } 

static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	tcsetattr(0, TCSANOW, &terminal);
	puts(""); 
	exit(0); 
}

static void change_directory(const char* d) {
	if (chdir(d) < 0) {
		printf("crp:err\""); printf("%s\n", d); printf("\"chdir\""); 
		printf("%s\n", strerror(errno)); 
		return;
	}
	printf("changed directories to %s\n", d);
	sleep(1);
}





/*

static void create_process(char** args) {
	pid_t pid = fork();
	if (pid < 0) { 
		printf("crp:*:fork\""); printf("%s\n", strerror(errno)); 
		return;
	}
	if (not pid) {
		if (execve(args[0], args, environ) < 0) { perror("execve"); exit(1); }
	} 
	int s = 0;
	if ((pid = wait(&s)) == -1) { 
		printf("crp:*:wait\""); printf("%s\n", strerror(errno)); 
		return;
	}
	char dt[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
	if (WIFEXITED(s)) 	printf("[%s:(%d) exited with code %d]\n", dt, pid, WEXITSTATUS(s));
	else if (WIFSIGNALED(s))printf("[%s:(%d) was terminated by signal %s]\n", dt, pid, strsignal(WTERMSIG(s)));
	else if (WIFSTOPPED(s)) printf("[%s:(%d) was stopped by signal %s]\n", 	dt, pid, strsignal(WSTOPSIG(s)));
	else 			printf("[%s:(%d) terminated for an unknown reason]\n", dt, pid);
	fflush(stdout);
	getchar();
}

static void execute_shell_command(char* command) {
	if (not strlen(command)) return;
	//save();
	const char delimiter = command[0];
	const char* string = command + 1;
	const size_t length = strlen(command + 1);

	char** arguments = NULL;
	size_t argument_count = 0;

	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (string[index] != delimiter) {
			if (not argument_length) start = index;
			argument_length++;

		} else if (string[index] == delimiter) {
		push:	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
			arguments[argument_count++] = strndup(string + start, argument_length);
			start = index;
			argument_length = 0; 
		}
	}
	if (argument_length) goto push;

	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
	arguments[argument_count] = NULL;

	write(1, "\033[?25h", 6);
	//tcsetattr(0, TCSANOW, &terminal);

	for (nat i = 0; i < (nat) (window.ws_row * 2); i++) puts("");
	printf("\033[H"); fflush(stdout);


	create_process(arguments);


	//struct termios terminal_copy = terminal; 
	//terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	//tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);
	free(arguments);
}

static void signal_handler_sigpipe(int event) {
	printf("WARNING: %d: program received a SIGPIPE signal.\n", event);
	puts("ignoring this signal.");
	fflush(stdout);
}*/




static char** parse_arguments(const char* string) {
	const char delimiter = 27;
	const size_t length = strlen(string);
	char** arguments = NULL;
	size_t argument_count = 0;
	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (string[index] != delimiter) {
			if (not argument_length) start = index;
			argument_length++;

		} else {
		push:	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
			arguments[argument_count++] = strndup(string + start, argument_length);
			start = index;
			argument_length = 0; 
		}
	}
	if (argument_length) goto push;
	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
	arguments[argument_count] = NULL;
	return arguments;
}


static char* path_lookup(const char* executable_name) {
	const nat executable_length = (nat) strlen(executable_name);
	if (not executable_length) return NULL;

	const char* path = getenv("PATH");
	const nat length = (nat) strlen(path);
	//puts(path);
	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (path[index] != ':') {
			if (not argument_length) start = index;
			argument_length++;
		} else {
		push:;	
			char* file = calloc(argument_length + 1 + executable_length + 1, 1);
			memcpy(file, path + start, argument_length);
			file[argument_length] = '/';
			memcpy(file + argument_length + 1, executable_name, executable_length);
			//printf("testing: %s  ---> ", file);
			if (not access(file, X_OK)) {
				//printf("\033[32mGOOD\033[0m");
				return file;
			} else {
				//printf("\033[31mERROR\033[0m");
				free(file);
			}
			//puts("");

			start = index;
			argument_length = 0; 
		}
	}
	if (argument_length) goto push;
	return NULL;
}


int main(void) {
	puts("a shell which only requires tab and enter to use.");

	signal(SIGPIPE, SIG_IGN);
	struct sigaction s_action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &s_action, NULL);
	struct sigaction s_action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &s_action2, NULL);
	//struct sigaction s_action3 = {.sa_handler = signal_handler_sigpipe}; 
	//sigaction(SIGPIPE, &s_action3, NULL);

	tcgetattr(0, &terminal);
	struct termios terminal_copy = terminal; 
	terminal_copy.c_cc[VMIN] = 0; 
	terminal_copy.c_cc[VTIME] = 1;
	terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	tcsetattr(0, TCSANOW, &terminal_copy);

	nat counts[128] = {0};
	nat array[128][128] = {0};

	array[0][0] = root;
	array[0][1] = in;
	array[0][2] = delete;
	array[0][3] = left;
	array[0][4] = right;
	array[0][5] = up;
	array[0][6] = down;
	array[0][7] = escape;
	array[0][8] = space;
	array[0][9] = newline;
	array[0][10] = tab;
	array[0][11] = read_output;
	array[0][12] = submit;
	array[0][13] = finishjob;
	array[0][14] = sendeof;
	counts[0] = 15;

	array[1][0] = back;
	array[1][1] = in;
	array[1][2] = ins_e;
	array[1][3] = ins_t;
	array[1][4] = ins_a;
	array[1][5] = ins_o;
	array[1][6] = ins_i;
	array[1][7] = ins_dot;
	array[1][8] = ins_comma;
	array[1][9] = ins_open_paren;
	array[1][10]= ins_close_paren;
	array[1][11] = newjob;
	array[1][12] = closejob;
	array[1][13] = nextjob;
	counts[1] = 14;

	array[2][0] = back;
	array[2][1] = in;
	array[2][2] = ins_n;
	array[2][3] = ins_s;
	array[2][4] = ins_r;
	array[2][5] = ins_h;
	array[2][6] = ins_d;
	array[2][7] = ins_apostrophe;
	array[2][8] = ins_question_mark;
	array[2][9] = ins_open_square;
	array[2][10]= ins_close_square;
	counts[2] = 11;

	array[3][0] = back;
	array[3][1] = in;
	array[3][2] = ins_l;
	array[3][3] = ins_u;
	array[3][4] = ins_c;
	array[3][5] = ins_m;
	array[3][6] = ins_f;
	array[3][7] = ins_quote;
	array[3][8] = ins_exclamation_mark;
	array[3][9] = ins_open_brace;
	array[3][10]= ins_close_brace;
	counts[3] = 11;

	array[4][0] = back;
	array[4][1] = in;
	array[4][2] = ins_y;
	array[4][3] = ins_w;
	array[4][4] = ins_g;
	array[4][5] = ins_p;
	array[4][6] = ins_b;
	array[4][7] = ins_hyphen;
	array[4][8] = ins_colon;
	array[4][9] = ins_less;
	array[4][10]= ins_greater;
	counts[4] = 11;

	array[5][0] = back;
	array[5][1] = in;
	array[5][2] = ins_v;
	array[5][3] = ins_k;
	array[5][4] = ins_x;
	array[5][5] = ins_q;
	array[5][6] = ins_j;
	array[5][7] = ins_z;
	array[5][7] = ins_underscore;
	array[5][8] = ins_semicolon;
	array[5][9] = ins_ampersand;
	array[5][10]= ins_vertical;
	counts[5] = 11;

	array[6][0] = back;
	array[6][1] = in;
	array[6][2] = ins_0;
	array[6][3] = ins_1;
	array[6][4] = ins_2;
	array[6][5] = ins_3;
	array[6][6] = ins_4;
	array[6][7] = ins_equals;
	array[6][8] = ins_plus;
	array[6][9] = ins_star;
	array[6][10]= ins_pound;
	array[6][11]= ins_at_sign;
	counts[6] = 12;

	array[7][0] = back;
	array[7][1] = in;
	array[7][2] = ins_5;
	array[7][3] = ins_6;
	array[7][4] = ins_7;
	array[7][5] = ins_8;
	array[7][6] = ins_9;
	array[7][7] = ins_slash;
	array[7][8] = ins_percent;
	array[7][9] = ins_caret;
	array[7][10] = ins_backslash;
	array[7][11]= ins_grave;;
	array[7][12]= ins_tilde;
	counts[7] = 13;

	array[8][0] = back;
	array[8][1] = quit;
	counts[8] = 2;

	nat last = 8;
	nat n = 0, m = 0;
	char message[2048] = {0};
	char* input = NULL;
	nat length = 0, cursor = 0;
	struct job* jobs = calloc(1, sizeof(struct job));
	nat job_count = 1;
	nat job_pointer = 0;

	write(1, "\033[?25l", 6);
loop:;	
	struct job* this = jobs + job_pointer;
	printf("\033[H\033[2J");
	for (nat i = 0; i < counts[n]; i++) {
		if (i == m) printf("\033[7m");
		printf(" %s ", spelling[array[n][i]]);
		if (i == m) printf("\033[0m");
	}
	printf("\njob %llu/%llu: message %s\n", job_pointer, job_count, message);
	if (job_count) {
		printf("status %llu: { ", this->status);
		for (nat i = 0; this->arguments and this->arguments[i]; i++) {
			printf("[%llu]:%s, ", i, this->arguments[i]);
		}
		printf("}\n");
	}

	for (nat i = 0; i < length; i++) {
		if (cursor == i) printf("\033[7m");
		if (input[i] == 27) printf(" , "); else putchar(input[i]); 
		if (cursor == i) printf("\033[0m");
	}
	if (cursor == length) printf("\033[7m \033[0m\n");
	puts("");


	for (nat i = this->origin; i < this->length; i++) {
		putchar(this->output[i]); 
	}
	puts("");


	char c = 0;
	const ssize_t nn = read(0, &c, 1);
	if (nn < 0) { perror("read"); fflush(stderr); }
	if (n == 0) {
		char buffer[65536] = {0};
		if (poll(&(struct pollfd){ .fd = this->fd[0], .events = POLLIN }, 1, 0) == 1) {		// we need to poll the stdout of the process to see if anything was outputted, of course lolllll
			ssize_t nbytes = read(this->fd[0], buffer, sizeof buffer);
			if ((nbytes <= 0) and (nbytes <= 0)) {
				printf("CHILD ERROR stdout read(). \n");
				printf("error: %s\n", strerror(errno));
			} else {
				this->output = realloc(this->output, this->length + (size_t) nbytes);
				memcpy(this->output + this->length, buffer, (size_t) nbytes);
				this->length += (size_t) nbytes;
			}
		}
		if (poll(&(struct pollfd){ .fd = this->fdm[0], .events = POLLIN }, 1, 0) == 1) {
			ssize_t nbytes = read(this->fdm[0], buffer, sizeof buffer);
			if (nbytes <= 0) {
				printf("CHILD ERROR stderr read(). \n");
				printf("error: %s\n", strerror(errno));
			} else {
				this->output = realloc(this->output, this->length + (size_t) nbytes);
				memcpy(this->output + this->length, buffer, (size_t) nbytes);
				this->length += (size_t) nbytes;
			}
		}

		if (job_count and this->status == 1) {
			int s = 0;
			pid_t pid = this->pid;
			int r = waitpid(pid, &s, WNOHANG);
			if (r == -1) { 
				printf("crp:*:wait\""); printf("%s\n", strerror(errno)); 
			}
			else if (r == 0) goto done_with_waiting;
			char dt[32] = {0};
			struct timeval t = {0};
			gettimeofday(&t, NULL);
			struct tm* tm = localtime(&t.tv_sec);
			strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
			if (WIFEXITED(s)) 
				snprintf(message, sizeof message, 
					"[%s:(%d) exited with code %d]\n", 
					dt, pid, WEXITSTATUS(s)
				);
			else if (WIFSIGNALED(s)) 
				snprintf(message, sizeof message, 
					"[%s:(%d) was terminated by signal %s]\n", 
					dt, pid, strsignal(WTERMSIG(s))
				);
			else if (WIFSTOPPED(s)) 
				snprintf(message, sizeof message, 
					"[%s:(%d) was stopped by signal %s]\n", 	
					dt, pid, strsignal(WSTOPSIG(s))
				);
			else snprintf(message, sizeof message, 
				"[%s:(%d) terminated for an unknown reason]\n", 
				dt, pid
			);
			this->status = 0;
			this->arguments = NULL;
		}
		done_with_waiting:;
	}


	if (c == 13) c = 10;

	if (c == cycle_key) { if (m == counts[n] - 1) m = 0; else m++; }
	else if (c == select_key) {
		const nat action = array[n][m];

		if (action == back) {}
		else if (action == in) {}
		else if (action == root) {}

		else if (action == read_output) {
			if (not job_count) goto next;
			
			char buffer[65536] = {0};
			if (poll(&(struct pollfd){ .fd = this->fd[0], .events = POLLIN }, 1, 0) == 1) {
				ssize_t nbytes = read(this->fd[0], buffer, sizeof buffer);
				if (nbytes <= 0) {
					printf("CHILD ERROR stdout read(). \n");
					printf("error: %s\n", strerror(errno));
				} else {
					this->output = realloc(this->output, this->length + (size_t) nbytes);
					memcpy(this->output + this->length, buffer, (size_t) nbytes);
					this->length += (size_t) nbytes;
				}
			}
			if (poll(&(struct pollfd){ .fd = this->fdm[0], .events = POLLIN }, 1, 0) == 1) {
				ssize_t nbytes = read(this->fdm[0], buffer, sizeof buffer);
				if (nbytes <= 0) {
					printf("CHILD ERROR stderr read(). \n");
					printf("error: %s\n", strerror(errno));
				} else {
					this->output = realloc(this->output, this->length + (size_t) nbytes);
					memcpy(this->output + this->length, buffer, (size_t) nbytes);
					this->length += (size_t) nbytes;
				}
			} 
		}

		else if (action == submit) {
			if (not job_count) goto next;
			if (not length) goto finish_submit;

			if (this->status == 0) {
				char* my_input = strndup(input, length);
				char** arguments = parse_arguments(my_input);
				if (not strcmp(arguments[0], "cd")) {
					const char* path = arguments[1] ? arguments[1] : getenv("HOME");
					change_directory(path);
					goto finish_submit;
				} 
				if (arguments[0][0] != '.' and arguments[0][0] != '/') {
					char* path = path_lookup(arguments[0]);
					if (not path) {
						snprintf(message, sizeof message, 
							"error: not found: \"%s\".", 
							arguments[0]
						);
						goto finish_submit;
					}
					free(arguments[0]);
					arguments[0] = path;
				}

				pipe(this->fd);
				pipe(this->rfd);
				pipe(this->fdm);

				const pid_t pid = fork();
				if (pid < 0) { 
					printf("crp:*:fork\""); printf("%s\n", strerror(errno)); 
					fflush(stdout);
					sleep(4);
				} else if (not pid) {
					close(this->fd[0]);
					dup2(this->fd[1], 1);
					close(this->fd[1]);
					close(this->fdm[0]);
					dup2(this->fdm[1], 2);
					close(this->fdm[1]);
					close(this->rfd[1]);
					dup2(this->rfd[0], 0);
					close(this->rfd[0]);
					execve(arguments[0], arguments, environ);
					printf("error: could not execute \"%s\": %s", 
						arguments[0], strerror(errno)
					);
					fflush(stdout);
					sleep(4);
					exit(1);
				} else {
					close(this->fd[1]);
					close(this->fdm[1]);
					close(this->rfd[0]);
					this->status = 1;
					this->arguments = arguments;
					this->pid = pid;
				}
			} else {
				ssize_t nbytes = write(this->rfd[1], input, length);
				if (nbytes <= 0) {
					printf("CHILD ERROR write(). \n");
					printf("error: %s\n", strerror(errno));
					fflush(stdout);
					sleep(4);
				}	
				this->output = realloc(this->output, this->length + (size_t) nbytes);
				memcpy(this->output + this->length, input, (size_t) nbytes);
				this->length += (size_t) nbytes;
			}			
		finish_submit:;
			length = 0; cursor = 0;
		}

		else if (action == nextjob) {
			if (not job_count) goto next;
			job_pointer = (job_pointer + 1) % job_count;
		}

		else if (action == newjob) {
			jobs = realloc(jobs, sizeof(struct job) * (job_count + 1));
			jobs[job_count] = (struct job) {0};
			job_pointer = job_count;
			job_count++;
		}


		else if (action == closejob) {
			if (not job_count) goto next;
			const struct job save = jobs[job_pointer];
			jobs[job_pointer] = jobs[job_count - 1];
			jobs[job_count - 1] = save;
			free(jobs[job_count - 1].output);
			free(jobs[job_count - 1].arguments);
			jobs[job_count - 1].length = 0;
			job_count--;
			if (job_count) job_pointer %= job_count; else job_pointer = 0;
		}

		else if (action == sendeof) {
			if (not job_count) goto next;
			if (this->status != 1) goto next;
			snprintf(message, sizeof message, 
				"sent eof: [%llu][%llu]: %s", 
				n, m, spelling[array[n][m]]
			);
			close(this->rfd[1]);
		}

		else if (action == finishjob) {
			if (not job_count) goto next;
			if (this->status != 1) goto next;
			close(this->fd[0]);
			close(this->rfd[1]);
			close(this->fdm[0]);
			int s = 0;
			pid_t pid = this->pid;
			if (waitpid(pid, &s, WNOHANG) == -1) {
				printf("crp:*:wait\""); printf("%s\n", strerror(errno)); 
			}
			char dt[32] = {0};
			struct timeval t = {0};
			gettimeofday(&t, NULL);
			struct tm* tm = localtime(&t.tv_sec);
			strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
			if (WIFEXITED(s)) 
				snprintf(message, sizeof message, 
					"[%s:(%d) exited with code %d]\n", 
					dt, pid, WEXITSTATUS(s)
				);
			else if (WIFSIGNALED(s)) 
				snprintf(message, sizeof message, 
					"[%s:(%d) was terminated by signal %s]\n", 
					dt, pid, strsignal(WTERMSIG(s))
				);
			else if (WIFSTOPPED(s)) 
				snprintf(message, sizeof message, 
					"[%s:(%d) was stopped by signal %s]\n", 	
					dt, pid, strsignal(WSTOPSIG(s))
				);
			else snprintf(message, sizeof message, 
				"[%s:(%d) terminated for an unknown reason]\n", 
				dt, pid
			);
			this->status = 0;
			this->arguments = NULL;
		}

		else if (action == up) {
			snprintf(message, sizeof message, 
				"scroll up: [%llu][%llu]: %s", 
				n, m, spelling[array[n][m]]
			);
			// modifies origin.
		}

		else if (action == down) {
			snprintf(message, sizeof message, 
				"scroll down: [%llu][%llu]: %s", 
				n, m, spelling[array[n][m]]
			);
			// modifies origin.
		}



		else if (action == left) {
			if (cursor) cursor--;


		} else if (action == right) {
			if (cursor < length) cursor++;



		} else if (action == delete) {
			if (cursor and length) {
				cursor--;
				length--;
				memmove(input + cursor, input + cursor + 1, length - cursor);
				input = realloc(input, length);
			}

		} else {
			char cc = spelling[action][0];

			     if (action == space) cc = ' ';
			else if (action == newline) cc = '\n';
			else if (action == tab) cc = '\t'; 
			else if (action == escape) cc = '\033';

			input = realloc(input, length + 1);
			memmove(input + cursor + 1, input + cursor, length - cursor);
			input[cursor++] = cc;
			length++;
		}

	next:
		if (m == 0) { 
			if (not n) goto loop;
			n--;
		}
		if (m == 1) {
			if (n >= last) goto done;
			n++;
		}
		m = 0;


	} else if (c == '<') {
		if (cursor) cursor--;

	} else if (c == '>') {
		if (cursor < length) cursor++;

	} else if (c == 127) {
		if (cursor and length) {
			cursor--;
			length--;
			memmove(input + cursor, input + cursor + 1, length - cursor);
			input = realloc(input, length);
		}

	} else if (c < 32 and not isspace(c) and c != 27) { 
	} else {
		input = realloc(input, length + 1);
		memmove(input + cursor + 1, input + cursor, length - cursor);
		input[cursor++] = c;
		length++;
	}
	goto loop;

done:;	write(1, "\033[?25h", 6);
}











			//for (nat i = 0; arguments[i]; i++) {
			//	printf("arguments[%llu] = %s\n", i, arguments[i]);
			//}




		//snprintf(message, sizeof message, 
		//	"[%llu][%llu]: \n", n, m
		//);


		//	snprintf(message, sizeof message, "read_output: [%llu][%llu]: %s", n, m, spelling[array[n][m]]);









//static void execute(const char* input) {
	//printf("execute(): executing: %s....\n", input); 
//}








/*			
		if (command == newjob) {

			// do argument parsing. 
			// if (arg[0] == "cd") change_directory();
			// if (arg[0] == "exit") terminate_job();
			// do path lookup.

			struct job new = {0};
			pipe(new.fd);
			pipe(new.rfd);
			pipe(new.fdm);

			if (fork() == 0) {
				close(new.fd[0]);
				dup2(new.fd[1], 1);
				close(new.fd[1]);
				close(new.fdm[0]);
				dup2(new.fdm[1], 2);
				close(new.fdm[1]);
				close(new.rfd[1]);
				dup2(new.rfd[0], 0);
				close(new.rfd[0]);
				// execve
				exit(0);
			} else {
				close(new.fd[1]);
				close(new.fdm[1]);
				close(new.rfd[0]);
			}
*/



















































/*



static char status[4096] = {0};
static char filename[4096] = {0};
static char autosavename[4096] = {0};
static volatile struct winsize window = {0};
static struct termios terminal = {0};

extern char** environ;
typedef uint64_t nat;


static void window_resized(int _) {if(_){} }//ioctl(0, TIOCGWINSZ, &window); }
static noreturn void interrupted(int _) {if(_){} 
	write(1, "\033[?25h", 6);
	//tcsetattr(0, TCSANOW, &terminal);
	//save(); 
	puts(""); 
	exit(0); 
}



static void change_directory(const char* d) {
	if (chdir(d) < 0) {
		printf("crp:err\""); printf("%s\n", d); printf("\"chdir\""); 
		printf("%s\n", strerror(errno)); 
		return;
	}
	//print("changed directories\n");
}

static void create_process(char** args) {
	pid_t pid = fork();
	if (pid < 0) { 
		printf("crp:*:fork\""); printf("%s\n", strerror(errno)); 
		return;
	}
	if (not pid) {
		if (execve(args[0], args, environ) < 0) { perror("execve"); exit(1); }
	} 
	int s = 0;
	if ((pid = wait(&s)) == -1) { 
		printf("crp:*:wait\""); printf("%s\n", strerror(errno)); 
		return;
	}
	char dt[32] = {0};
	struct timeval t = {0};
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	strftime(dt, 32, "1%Y%m%d%u.%H%M%S", tm);
	if (WIFEXITED(s)) 	printf("[%s:(%d) exited with code %d]\n", dt, pid, WEXITSTATUS(s));
	else if (WIFSIGNALED(s))printf("[%s:(%d) was terminated by signal %s]\n", dt, pid, strsignal(WTERMSIG(s)));
	else if (WIFSTOPPED(s)) printf("[%s:(%d) was stopped by signal %s]\n", 	dt, pid, strsignal(WSTOPSIG(s)));
	else 			printf("[%s:(%d) terminated for an unknown reason]\n", dt, pid);
	fflush(stdout);
	getchar();
}

static void execute(char* command) {
	if (not strlen(command)) return;
	//save();
	const char delimiter = command[0];
	const char* string = command + 1;
	const size_t length = strlen(command + 1);

	char** arguments = NULL;
	size_t argument_count = 0;

	size_t start = 0, argument_length = 0;
	for (size_t index = 0; index < length; index++) {
		if (string[index] != delimiter) {
			if (not argument_length) start = index;
			argument_length++;

		} else if (string[index] == delimiter) {
		push:	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
			arguments[argument_count++] = strndup(string + start, argument_length);
			start = index;
			argument_length = 0; 
		}
	}
	if (argument_length) goto push;

	arguments = realloc(arguments, sizeof(char*) * (argument_count + 1));
	arguments[argument_count] = NULL;

	write(1, "\033[?25h", 6);
	//tcsetattr(0, TCSANOW, &terminal);

	for (nat i = 0; i < (nat) (window.ws_row * 2); i++) puts("");
	printf("\033[H"); fflush(stdout);

	create_process(arguments);
	//struct termios terminal_copy = terminal; 
	//terminal_copy.c_lflag &= ~((size_t) ECHO | ICANON);
	//tcsetattr(0, TCSANOW, &terminal_copy);
	write(1, "\033[?25l", 6);
	free(arguments);
}

static void signal_handler_sigpipe(int event) {
	printf("WARNING: %d: program received a SIGPIPE signal.\n", event);
	puts("ignoring this signal.");
	fflush(stdout);
}


struct job {
	const char* name;
	char* output;
	char* input;
	nat status;
	nat origin;
	nat cursor;
	nat output_length;
	nat input_length;
	int fd[2];
	int rfd[2];
	int fdm[2];
};


int main(void) {
	signal(SIGPIPE, SIG_IGN);
	struct sigaction action = {.sa_handler = window_resized}; 
	sigaction(SIGWINCH, &action, NULL);
	struct sigaction action2 = {.sa_handler = interrupted}; 
	sigaction(SIGINT, &action2, NULL);
	//struct sigaction action3 = {.sa_handler = signal_handler_sigpipe}; 
	//sigaction(SIGPIPE, &action3, NULL);



	nat running = 1;
	while (running) {


		...input...



		if (command == newjob) {

			// do argument parsing. 
			// if (arg[0] == "cd") change_directory();
			// if (arg[0] == "exit") terminate_job();
			// do path lookup.

			struct job new = {0};
			pipe(new.fd);
			pipe(new.rfd);
			pipe(new.fdm);

			if (fork() == 0) {
				close(new.fd[0]);
				dup2(new.fd[1], 1);
				close(new.fd[1]);
				close(new.fdm[0]);
				dup2(new.fdm[1], 2);
				close(new.fdm[1]);
				close(new.rfd[1]);
				dup2(new.rfd[0], 0);
				close(new.rfd[0]);
				if (execve(args[0], args, environ) < 0) { printf("could not start up job\n"); perror("execve"); exit(1); }
				exit(0);
			} else {
				close(new.fd[1]);
				close(new.fdm[1]);
				close(new.rfd[0]);
			}





		} else if (command == flush) { 


			char buffer[12800] = {0};
			if (poll(&(struct pollfd){ .fd = this.fd[0], .events = POLLIN }, 1, 0) == 1) {
				ssize_t nbytes = read(this.fd[0], buffer, sizeof buffer);
				if (nbytes <= 0) {
					printf("CHILD ERROR stdout read(). \n");
					printf("error: %s\n", strerror(errno));
				} else fwrite(buffer, 1, (size_t) nbytes, stdout);
				
			} 
			if (poll(&(struct pollfd){ .fd = this.fdm[0], .events = POLLIN }, 1, 0) == 1) {
				ssize_t nbytes = read(this.fdm[0], buffer, sizeof buffer);
				if (nbytes <= 0) {
					printf("CHILD ERROR stderr read(). \n");
					printf("error: %s\n", strerror(errno));
				} else fwrite(buffer, 1, (size_t) nbytes, stdout);
			} 
			fflush(stdout);


			// this command should write it to the terminal_scrollback_buffer.  ie,       .output and  .output_length






		} else if (command == submit) {

			ssize_t nbytes = write(this.rfd[1], input, n);				
			if (nbytes <= 0) {
				printf("CHILD ERROR write(). \n");
				printf("error: %s\n", strerror(errno));
			}
			fflush(stdout);



		} else if (command == terminate_job) { 
			close(this.fd[0]);
			close(this.rfd[1]);
			close(this.fdm[0]);
		}

 


	}// while
}























			//puts("reading from process..");



				//printf("n = %ld\n", nbytes);
    // this is just the main input loop.  


			/////////////////// this code wouldnt exist ///////////////
			//char input[4096] = {0};
			//printf(":: ");
			//fflush(stdout);
			//ssize_t n = read(0, input, sizeof input);
			//if (n <= 0) {
			//	quit = 1;
			//	puts("ERROR: read(0) could not return, command mode failed.");
			//	printf("error: %s\n", strerror(errno));
			//}
			///////////////////////////////////////////////////////////





				//printf("n = %ld\n", nbytes);


		//puts("reading from process..");



//else {
				//printf("nothing to read at the moment.\n");
			//}





					//puts("\"");
					//printf("child stdout says: \"");








	//printf("child stderr: \"");
					//puts("\"");


//else {
			//printf("nothing to read from stderr at the moment.\n");
			//}

				///////////////////////////unused code///////////////////////////////
				//char input[4096] = {0};
				//printf(":writedata: ");
				//fflush(stdout);
				//ssize_t n = read(0, input, sizeof input);
				//if (n <= 0) {
				//	puts("ERROR: read(0) could not return, command mode failed.");
				//	printf("error: %s\n", strerror(errno));
				//}
				////////////////////////////////////////////////////////////



				//puts("sending DATA to child...");
//printf("n = %ld\n", nbytes);
 //else {
				//printf("write: successfully sent %ld bytes.\n", n);
				//printf("wrote: <<<%.*s>>>\n", (int) n, input);
			//}


	//} // if (fork) {...} else {..} 



			//puts("exiting from job terminal...");    // not possible. use switchjob instead. 
			//running = 0;


		//else {
			//printf("error: command not found: %d...\n", input[0]);
		//}
		//usleep(10000);


	//puts("closing file descriptors now.");
	//fflush(stdout);




		// replace this with the execve call. 
		//execlp("../program/list_stuff", "../program/list_stuff", 0);

		// print error about how exeve failed lol. 


*/





































			/*	char buffer[1280] = {0};


					ssize_t nbytes = read(fd[0], buffer, sizeof buffer);
					printf("n = %ld\n", nbytes);
					if (nbytes <= 0) {
						printf("CHILD ERROR read(). \n");
						printf("error: %s\n", strerror(errno));
					} else {
						printf("child says: \"");
						fwrite(buffer, 1, (size_t) nbytes, stdout);
						puts("\"");
					}

					fflush(stdout);


				sleep(4);



				puts("sending ACK to child...");
				nbytes = write(rfd[1], "ACK\n", 4);
				printf("n = %ld\n", nbytes);
				if (nbytes <= 0) {
					printf("CHILD ERROR read(). \n");
					printf("error: %s\n", strerror(errno));
				} else {
					puts("successfully sent ack.");
				}

				fflush(stdout);



				sleep(4);


					nbytes = read(fd[0], buffer, sizeof buffer);
					printf("n = %ld\n", nbytes);
					if (nbytes <= 0) {
						printf("CHILD ERROR read(). \n");
						printf("error: %s\n", strerror(errno));
					} else {
						printf("child says: \"");
						fwrite(buffer, 1, (size_t) nbytes, stdout);
						puts("\"");
					}
					fflush(stdout);
*/










/*
int main(void) {

	int fd[2];
	pipe(fd);

	int id = fork();

	if (not id) {
		close(fd[0]);
		int x = 0;
		printf("input from child: ");
		scanf("%d", &x);
		write(fd[1], &x, sizeof(int));
		close(fd[1]);

	} else {
		close(fd[1]);
		int y = 0;
		read(fd[0], &y, sizeof(int));
		close(fd[0]);	
		printf("got from child processo: %d\n", y);
	}
}
*/
