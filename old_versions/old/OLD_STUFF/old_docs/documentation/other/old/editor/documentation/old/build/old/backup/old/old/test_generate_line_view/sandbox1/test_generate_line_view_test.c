//
//  main.c
//  sandbox1
//
//  Created by Daniel Rehman on 1911107.
//                                                       
//

//##########################################################

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define not !
#define and &&
#define or ||

typedef ssize_t nat;

struct line {
    char* line;
    nat length;
    bool continued;
};

void print_lines(struct line* lines, nat line_count) {
    printf("printing %d lines: \n\n:::\n", (int) line_count);
    for (nat i = 0; i < line_count; i++) {
        printf("#%d: %s <<<", (int) i, lines[i].continued ? "(cont)" : "");
        for (nat c = 0; c < lines[i].length; c++)
            printf("%c", lines[i].line[c]);
        printf(">>>\n");
    }
    printf("\n:::\n\n");
}


void print_pretty_lines(struct line* lines, nat line_count) {
    printf("pretty printing %d lines: \n\n:::", (int) line_count);
    for (nat i = 0; i < line_count; i++) {
        if (!lines[i].continued) printf("%d | ", (int) i);
        for (nat c = 0; c < lines[i].length; c++)
            printf("%c", lines[i].line[c]);
        puts("");
    }
    printf(":::\n\n");
}

static inline struct line* generate_line_view(char* source, nat* count, nat width) {
    *count = 1;
    nat length = 0;
    bool continued = false;
    struct line* lines = calloc(1, sizeof(struct line));
    lines[0] = (struct line){source, 0, false};
    while (*source) {
        if (*source++ == '\n') { continued = false; length = 0; }
        else { length++; lines[*count - 1].length++; }
        if (width and length == width and *source != '\n') { continued = true; length = 0; }
        if (not length) {
            lines = realloc(lines, sizeof(struct line) * (*count + 1));
            lines[*count].line = source;
            lines[*count].continued = continued;
            lines[(*count)++].length = 0;
        }
    } return lines;
}

int main(int argc, const char * argv[]) {
    
    const char* s0 = "";
        const char* s1 = "hello\nthere";
        const char* s2 = "hello there.im cool.yes ineed.bubbles.\nnewline.\nthatsit.";
            const char* s3 = "\n\nhello\nworld\nfrom\nspace\n";
            const char* s4 = "\n\n\n";
            const char* s5 = "\n";
    const char* s6 = "wef\nweg";
    const char* s7 = "g";
    const char* s8 = "\th\n\tg";
    const char* s9 = "hello\n\n";
    const char* s10 = "\n\nhello";
    
    
    const char* s = s2;
    
    printf("the source was:\n :::%s:::\n\n", s);
    
    nat count = 0;
    struct line* lines = generate_line_view((char*) s, &count, 10);
    print_lines(lines, count);
    print_pretty_lines(lines, count);
    
    return 0;
}
