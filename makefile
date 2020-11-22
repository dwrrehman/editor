# makefile for my editor.

warning_flags =  -Wall -Wpedantic 

debug_flags = -fsanitize=address,undefined

include_flags = -I /usr/local/Cellar/llvm/10.0.1_1/include -I lib/libclipboard/include

linker_flags = -L /usr/local/Cellar/llvm/10.0.1_1/lib -L lib/libclipboard/lib

libraries = -lclang -lclipboard

editor: source/editor/main.c 
	gcc -g -O1 $(warning_flags) $(debug_flags) $(include_flags) $(linker_flags) -o editor source/editor/main.c $(libraries)