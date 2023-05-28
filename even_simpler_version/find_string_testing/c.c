
#include <stdio.h>
#include <string.h>

int main(void) {
	const char* text = "TutorialsPoint is a string. cool beans.";
	const char* tofind = "Point";
	const char* r = strstr(text, tofind);
	printf("The substring is: #%s# %ld.\n", r, r - text);
}


\
\const char* offset = strrstr(text, input + 1);
			if (not offset) printf("absent %s\n", input + 1);
			else {
				cursor = (size_t) (offset - text);
				printf("%lu\n", cursor);
			}


