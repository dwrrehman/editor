#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iso646.h>


struct node {
	int left;
	int right;              // we should flatten this datastructure to make it just an array of ints, indexed 4-pair-wise! simpler, faster
	int size;
	int word;
};

struct word {                 // this is required, i think. 
	char* data;
	int count;
	int _padding;
};


static const int max_node_count = 100;
static const int max_word_count = 100;
static const int max_word_size = 5;



static int word_count = 0;
static struct word* words = NULL;

static int node_count = 0;
static struct node* nodes = NULL;





static void insert(char c) {
	
	if (not word_count) {
		struct word new = (struct word) { .data = malloc(1), .count = 1 };
		new.data[0] = c;
		words = realloc(words, (size_t) (word_count + 1) * sizeof(struct word));
		words[word_count++] = new;

		nodes = realloc(nodes, (size_t) (node_count + 1) * sizeof(struct node));

		nodes[node_count++] = (struct node) {.left = 0, .right = 0, .size = 1, .word = 0 };
		
		return;
	}

	if (words[word_count - 1].count < max_word_size) {
		struct word* const this = words + word_count - 1;
		this->data = realloc(this->data, (size_t) (this->count + 1) * sizeof(char));
		this->data[this->count++] = c;


	/*
		we have to actually walk the tree!!!    like the nodes.  starting with root,   0     and find the word to insert into, i think. 

			or wait. we know its the last one. crap. uhh... hmm.. 

	*/




	} else {

		abort();
	}
}

static void print() {
	puts("----------------");
	for (int i = 0; i < word_count; i++) {
		printf("word #%d : { .data = \"", i);
		for (int e = 0; e < words[i].count; e++) putchar(words[i].data[e]);
		printf("\", .count = %d }\n", words[i].count);
	}
	puts("<<<<<>>>>>>");
	for (int i = 0; i < node_count; i++) {
		printf("node #%d : { .left = %d, .right = %d, .size = %d, .word = %d}\n", i, nodes[i].left, nodes[i].right, nodes[i].size, nodes[i].word);
	}
	puts("----------------.\n\n");
}

int main() {



	words = calloc(max_word_count, sizeof(struct word));
	for (int i = 0; i < max_word_count; i++) {
		words[i].data = calloc(max_word_size, sizeof(char));
	}

	nodes = calloc(max_node_count, sizeof(struct node));




	node_count = 0;
	word_count = 0;

	
	for (int i = 0; i < 10; i++) {


		
		insert('A');
		print();
	}
	

	

	

	

	
}





/*
static void form_tree(	
	const char* string, 
	const int length,
	struct node* nodes, 
	int* node_count,
	char** words, 
	int* word_count 
) {
	
	
}
*/



