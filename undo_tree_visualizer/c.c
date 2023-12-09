#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iso646.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

struct action {
	off_t parent;
	off_t pre;
	off_t post;
	off_t length;
};

struct entry {
	struct action action;
	char* text;
	int64_t inserting;
};

static const bool debug = false;

int main(int argc, const char** argv) {
	if (argc != 1) exit(puts("usage: ./run"));

	char** files = NULL;
	size_t file_count = 0;

	FILE* list = fopen("listing.txt", "r");
		
	char line[2048] = {0};
	
	while (fgets(line, sizeof line, list)) {
		line[strlen(line) - 1] = 0;
		char path[2048] = {0};
		strlcpy(path, "./historyfiles/", sizeof path);
		strlcat(path, line, sizeof path);

		files = realloc(files, sizeof(char*) * (file_count + 1));
		files[file_count++] = strdup(path);
	}

	for (size_t f = 0; f < file_count; f++) {
		
		printf("info: reading file \"%s\"...\n", files[f]);

		int fd = open(files[f], O_RDONLY);
		if (fd < 0) {
			perror("open");
			puts(files[f]);
			exit(1);
		}
		int filepos = 0;

		off_t head = 0;
		read(fd, &head, sizeof head);

		struct action node = {0};

		struct entry* entries = NULL;
		size_t entry_count = 0;
		
		while (1) {
			ssize_t n = read(fd, &node, sizeof node);
			if (n <= 0) break;	
		
			if (debug) printf("@ %d :   read node{.parent=%llu,.pre=%llu,.post=%llu,.length=%lld}\n",
					filepos, node.parent, node.pre, node.post, node.length);
	
			filepos += n;
			bool inserting = true;				
			if (node.length < 0) {inserting = false; node.length = -node.length; }
			char* text = calloc((size_t) node.length + 1, 1);
			read(fd, text, (size_t) node.length);
			filepos += node.length;
		
			if (debug) {
				if (inserting) printf("\033[32m"); else printf("\033[31m");
				printf("     %c text = %s\n", inserting ? '+' : '-', text);
				printf("\033[0m");
				puts("");
			}

			entries = realloc(entries, sizeof(struct entry) * (entry_count + 1));
			entries[entry_count++] = (struct entry) {.action = node, .text = text, .inserting = inserting };
		}

		printf("finished reading file!\n");

		puts("textual changes: ");
		for (size_t i = 0; i < entry_count; i++) {
			if (entries[i].inserting) printf("\033[32;1m"); else printf("\033[31m");
			printf("%s", entries[i].text);
			if (entries[i].inserting) printf("\033[0m"); else printf("\033[0m");
			fflush(stdout);		
		}
		puts("\n\n");
		close(fd);
	}

}


















/*
	basically this program should output the following table:


	file position      parent      length      inserting     precursor   postcursor         text

	8			0	4		1		45	24             "word"

	16			8	2		1		56	13		"hi"



ie, we are showing each entry in the undo tree, but not showing it as a tree. its implied to be a tree from the .parent connections of course lol. so yeah. 


		all we have to do is read in the undo tree action    node      struct   first

				then, read that many number of charcaters after that,  to get the text/string


				and then we are positioned at the first node .

				each field is a int64_t so i mean, we can probably just do that actaully, no struct neccessary lol. 



yay





*/





