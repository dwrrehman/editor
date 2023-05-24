# My Text Editor.
##### Written By Daniel Warren Riaz Rehman.

This is a simple modal programmable text editor intended for my personal use, based on minimalism, ergonomics and simplicity. It is written in C11, and depends only on libc. 
It's features and behavior are loosely based on that of TextEdit, ed, and vi.

### Features:

 - minimalist feature set and UI
 - few ergonomic and simple input commands to learn
 - fully configurable key bindings                                    (coming soon)
 - runs fast and is highly memory/energy efficient
 - supports UTF8 unicode
 - adjustable tab stops
 - uses an undo-tree system
 - copy/paste to global clipboard
 - fully programmable: uses an interpreter for a language I also wrote        (coming soon!)
 - no dependancies besides the C standard library.
 - source code is currently less than 500 lines of code. 

### Other notes:
 - commands are written in the buffer itself, and removed upon execution.
 - no live displaying of text while not in insert mode.
 - no unicode encoding besides UTF8, ever.
 - no syntax highlighting, ever.
 - no mulitple-file support at all, ever.
 - VT100 terminal based.

##### deleted:

 - x:smooth scrolling and mouse support;
 - x:togglable absolute and relative line numbers;
 - x:horizontal and vertical view scrolling;
 - x:adjustable soft wrapping of lines;
 - x:word wrapping;
 - x:built-in file tree explorer;
 - x:highly versatile "\*n buffer";
 - x:multiple buffers;
 - x:line numbers are 0-based.;
