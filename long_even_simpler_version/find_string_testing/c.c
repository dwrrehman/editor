#include <iso646.h>
#include <stdio.h>
#include <string.h>

#define red   "\x1B[31m"
#define green   "\x1B[32m"
//#define yellow   "\x1B[33m"
//#define blue   "\x1B[34m"
//#define magenta   "\x1B[35m"
//#define cyan   "\x1B[36m"
#define reset "\x1B[0m"



static char* find(char* text, char* tofind, int count, int length, int cursor) {
	int i = cursor, t = 0;
loop:	if (t == length) return text + i - length;
	if (i >= count) return NULL;
	if (text[i] == tofind[t]) t++; else t = 0;
	i++; goto loop;
}

static char* find_reverse(char* text, char* tofind, int count, int length, int cursor) {
	int i = cursor, t = length;
loop:	if (not t) return text + i;
	if (not i) return NULL;
	i--; t--; if (text[i] != tofind[t]) t = length;
	goto loop;
}


static void print_location(char* text, int count, int location, int cursor) {
	int i = 0;
	for (; i < count; i++) {
		if (i == cursor) printf(red "@" reset);
		if (i == location) printf(green "#" reset);
		putchar(text[i]);
	}
	if (i == cursor) printf(red "@" reset);
	if (i == location) printf(green "#" reset);
}

static void test_find(const char* given_text, const char* given_tofind, const int cursor) {
	char* text = strdup(given_text);
	const int count = (int) strlen(text);
	char* tofind = strdup(given_tofind);
	const int length = (int) strlen(tofind);

	char* r = find(text, tofind, count, length, cursor);
	long location = r - text;
	printf(	"find(\"%s\", %d): \n"
		"(%ld) => @result: \"",
		tofind, cursor, location
	);

	print_location(text, count, (int) location, cursor);

	puts("\"\n");
}

static void test_find_reverse(const char* given_text, const char* given_tofind, const int cursor) {
	char* text = strdup(given_text);
	const int count = (int) strlen(text);
	char* tofind = strdup(given_tofind);
	const int length = (int) strlen(tofind);

	char* r = find_reverse(text, tofind, count, length, cursor);
	long location = r - text;
	printf(	"find_reverse(\"%s\", %d): \n"
		"(%ld) => @result: \"",
		tofind, cursor, location
	);

	print_location(text, count, (int) location, cursor);

	puts("\"\n");
}


static void test_strstr(const char* given_text, const char* given_tofind, const int cursor) {
	char* text = strdup(given_text);
	const int count = (int) strlen(text);
	char* tofind = strdup(given_tofind);
	//const int length = (int) strlen(tofind);

	char* r = strstr(text + cursor, tofind);
	long location = r - text;
	printf(	"strstr(\"%s\", %d): \n"
		"(%ld) => @result: \"",
		tofind, cursor, location
	);

	print_location(text, count, (int) location, cursor);

	puts("\"\n");
}

int main(void) {
	const char* text = "hello this is my bubbles and beans. my favorite bubbles is perhaps the bubble and beans.";
	const int count = (int) strlen(text);

	puts("-----------------------bubbles 0 ----------------------");

	test_find(text, "bubbles", 0);
	test_strstr(text, "bubbles", 0);
	test_find_reverse(text, "bubbles", 0);
	

	puts("-----------------------les 30 ----------------------");

	test_find(text, "les", 30);
	test_strstr(text, "les", 30);
	test_find_reverse(text, "les", 30);
	

	puts("-----------------------[empty] 0 ----------------------");

	test_find(text, "", 0);
	test_strstr(text, "", 0);
	test_find_reverse(text, "", 0);
	

	puts("-----------------------[empty] 30 ----------------------");

	test_find(text, "", 30);
	test_strstr(text, "", 30);
	test_find_reverse(text, "", 30);
	

	puts("-----------------------[empty] count ----------------------");

	test_find(text, "", count);
	test_strstr(text, "", count);
	test_find_reverse(text, "", count);
	

	puts("----------------------- bubbles count ----------------------");

	test_find(text, "bubbles", count);
	test_strstr(text, "bubbles", count);
	test_find_reverse(text, "bubbles", count);


	puts("----------------------- NOT_PRESENT count ----------------------");

	test_find(text, "NOT_PRESENT", count);
	test_strstr(text, "NOT_PRESENT", count);
	test_find_reverse(text, "NOT_PRESENT", count);
	
	puts("----------------------- NOT_PRESENT 30 ----------------------");

	test_find(text, "NOT_PRESENT", 30);
	test_strstr(text, "NOT_PRESENT", 30);
	test_find_reverse(text, "NOT_PRESENT", 30);


	puts("----------------------- NOT_PRESENT 0 ----------------------");

	test_find(text, "NOT_PRESENT", 0);
	test_strstr(text, "NOT_PRESENT", 0);
	test_find_reverse(text, "NOT_PRESENT", 0);
}









