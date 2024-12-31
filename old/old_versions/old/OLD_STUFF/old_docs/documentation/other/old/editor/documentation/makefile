# makefile for my editor.

disabled_warnings = -Wno-poison-system-directories

warning_flags = -Wall -Wextra -Wpedantic -Weverything $(disabled_warnings)

debug_flags = -fsanitize=address,undefined

editor: source/main.c 
	clang -g -O1 $(warning_flags) $(debug_flags) -o editor source/main.c

release: source/main.c 
	clang -Ofast $(warning_flags) -o editor source/main.c

clean:
	rm -rf editor
	rm -rf editor.dSYM