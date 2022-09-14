# My Text Editor.
##### Written By Daniel Warren Riaz Rehman.

This is a simple modal programmable text editor intended for my personal use, based on minimalism, ergonomics and simplicity. It is written in C11, and depends only on libc. 
It's features and behavior are loosely based on that of TextEdit, kakoune, ed, and vi.

### Features:

 - minimalist feature set and ui
 - ergonomic key bindings and display
 - fully configurable key bindings                                    (coming soon)
 - runs fast and is highly memory efficient
 - fully programmable: uses an interpreter for a language I also wrote        (coming soon!)
 - supports UTF8 unicode
 - adjustable soft wrapping of lines 
 - word wrapping 				                     (coming soon!)
 - adjustable tab stops
 - horizontal and vertical view scrolling
 - togglable absolute and relative line numbers 
 - copy/paste to global clipboard
 - uses an undo-tree system
 - built-in file tree explorer 
 - highly versatile "\*n buffer"
 - multiple buffers
 - smooth scrolling and mouse support                         (coming soon!)
 - No dependancies besides the C standard library.

### Other notes:

 - source code is currently less than 1800 lines of code. 
 - line numbers are 0-based.
 - no unicode encoding besides UTF8, ever.
 - no syntax highlighting, ever.
 - no mulitple-file display besides \*n buffer, ever.
 - VT100 terminal based.
