
#include <stdio.h>
#include <string.h>

static char* find(char* text, char* tofind, int count, int length, int cursor) {
	for (int i = 0, t = 0; i < count;) {
		if (text[i] == tofind[t]) { 
			t++; i++; 
			if (t == length) return text + i - length;
		} else { t = 0; i++; }
	}
	return NULL;
}

static char* find_reverse(char* text, char* tofind, int count, int length, int cursor) {
	for (int i = cursor, t = 0; i > 0;) {
		if (text[i] == tofind[t]) { 
			t++; i++; 
			if (t == length) return text + i - length;
		} else { t = 0; i--; }
	}
	return NULL;
}

int main(void) {

	char* text = strdup("hello this is my bubbles and beans. my favorite bubbles is perhaps the bubble and beans.");
	int count = (int) strlen(text);

	char* tofind = strdup("bubbles");
	int length = (int) strlen(tofind);

	int cursor = 0;
	
	char* r = find(text + cursor, tofind, count, length);
	long location = r - text;
	printf("MINE: The substring is: #%s# %ld.\n", text + location, location);

	r = strstr(text, tofind);
	location = r - text;
	printf("THEM: The substring is: #%s# %ld.\n", text + location, location);
}









