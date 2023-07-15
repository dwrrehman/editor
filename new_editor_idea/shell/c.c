#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iso646.h>
#include <sys/wait.h>

static void execute(char** args) {
	pid_t pid = fork();
	if (pid < 0) { perror("lsh"); exit(1); } 

	if (not pid) {
		if (execvp(args[0], args) == -1) { perror("execvp"); exit(1); }
	} else {
		int status = 0;
		do waitpid(pid, &status, WUNTRACED);
		while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}
}

int main(void) {
	printf("executing ls....\n");
	execute((char*[]){"ls", "..", NULL});
	execute((char*[]){"cat", NULL});
	printf("hello world from space!\n");
}
