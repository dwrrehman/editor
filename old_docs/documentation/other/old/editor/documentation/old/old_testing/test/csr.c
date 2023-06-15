/// Daniel Rehman,
/// CE202008285.121444
/// modified on 2009233.121623

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef size_t nat;
typedef const char* string;
static string context = "-.hello.bubbles_there_.";
static string input = "bubbleshellotherehello";

static inline void debug_memory(nat* memory, nat size, nat head, nat tail) {
    puts("\n");
    for (nat i = 0; i < size; i++) {
        printf("%5lu   %c%c  {  i:%-5lu p:%-5lu b:%-5lu q:%-5lu }\n",
               i * 4, i * 4 == head ? 'H' : ' ', i * 4 == tail ? 'T' : ' ',
               memory[i * 4 + 0], memory[i * 4 + 1], memory[i * 4 + 2], memory[i * 4 + 3]);
    }
    puts("\n");
}

static inline void display_signature(string context, nat at) {
    printf("\n         signature: ");
    for (nat i = 0; i < strlen(context) + 1; i++)
        printf(i == at ? "[%c] " : "%c ", i == strlen(context) ? '?' : context[i]);
    puts("");
}

static inline void display_string_at_char(string input, nat at) {
    printf("\n            string:  ");
    for (nat i = 0; i < strlen(input) + 1; i++)
        printf(i == at ? "[%c] " : "%c ", input[i]);
    puts("\n");
}

static inline void solve(nat* m, nat size) {
    m[6] = 0;
    for (nat head = 4, tail = 0, next = 4; head; head = m[head + 3]) {
        
        debug_memory(m, size, head, tail);
        
        m[tail + 3] = next;
        m[next++] = 0;
        m[next++] = head; // parent is head.
        m[next++] = m[head + 2];
        m[next++] = 0;
        
        // { 0, head, m[head + 2], 0 }
        
        // break;
        
    }
}

int main() {
    nat size = 10, * m = malloc(4 * size * sizeof(nat));
    m[0] = 99999; m[1] = 99999; m[2] = 99999; m[3] = 99999;
    solve(m, size);
    debug_memory(m, size, 0, 0);
    free(m);
}















    
/*
 
 
 
 
 
 
 
 for (nat head = 4, tail = 0, next = 4; head; head = m[head + 3]) { //note: this /might/ be a do-while.
 
 
 
 
 
 
 
 
 
 
 m[next++] = 0; // starting index of 0.
 m[next++] = head; // parent is head.
 m[next++] = m[head + 2];  // child uses the begin of its parent.
 m[next++] = 0; // we are pushing back this node into the queue, and thus, there is no next node in the queue.

 m[tail + 3] = next;
 // { 0, head, m[head + 2], 0 }
 
 
 
 before:
 
 
 //        m[next++] = 0;
 //        m[next++] = 0;
 //        m[next++] = 0;
 //        m[next++] = 0;
 //        /// { 0, head, m[head + 2], 99999 }
     
 */
