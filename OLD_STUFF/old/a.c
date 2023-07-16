#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <iso646.h>
#include <stdint.h>
#include <stdbool.h>



// approach:      the simplest most efficient and robust way of doing input, simply using fgets and terminal in canonical mode. 
//                has bugs:  tabs across the wrap width is buggy alot. and lines can only be like 2048 characters long before the input mech becomes unresponsive and no characters appear anymore, and are ignored. 
// 		  VERY power efficient though... at least for the program itself. the terminal has to work slightly harder though. 

int main(void) {
	char* inserted = NULL; 
	size_t inserted_length = 0;
	char input[512] = {0};




loop: 	fgets(input, sizeof input, stdin);

	if (not strcmp(input, ".\n")) { if (inserted_length) inserted_length--; } 
	else {
		const size_t length = strlen(input);
		inserted = realloc(inserted, inserted_length + length);
		memcpy(inserted + inserted_length, input, length);
		inserted_length += length;
		printf("appending... (%lu)\n", inserted_length);
		goto loop;
	}




	printf("\n\trecieved inserted(%lu): \n\n\t\t\"", inserted_length);
	fwrite(inserted, inserted_length, 1, stdout); 
	printf("\"\n\n\n\n");

	if (inserted_length == 4 and not strncmp(inserted, "quit", 4)) goto done;

	inserted_length = 0;
	goto loop;
done:
	puts("quitting...");
}


