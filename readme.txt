minimalist command-line modal text editor
-----------------------------------------
1202412301.140433 by dwrr

this is a plain text editor meant to be used in terminal/command-line, written in 518 lines of C code with zero dependancies, and is primarily intended for my own use for programming, but perhaps might be of interest to others.

[disclaimer: this editor is still in the process of being tested for crashing bugs and other bugs which could cause data loss, and thus should not be used for editing any document which is sensitive to this kind of data loss. Use it at your own risk!]

feature list:
	- ergonomic modal editor, all commands are under/near the home row alpha keys
	- simple set of 12 editing primitives to learn
	- byte and word-wise cursor movement
	- column-0 and half-paging vertical cursor movement
	- undo tree: all possible histories of the document are recorded and accessible.	
	- jump to numeric line number or file offset
	- search mode for content-based navigation 
	- local and system-wide clipboard copy/paste (system-wide clipboard support currently only implemented for MacOS/Linux)	
	- execution of arbitrary shell commands, and insert its output into document
	- open other files or an empty document from within editor.
	- saving safety/integrity features to prevent data loss of document contents.
	- immutable read-only mode, for browsing documents 
	- semi-easily adjustable keybindings for various keyboard layouts	
	- only dependancy is libc. 

unimplemented planned features: 
	- unicode support
	- cycle through search results forward and backwards.
	- backward search 
	- autosaving system

features which are not planned to be implemented ever:
	- syntax highlighting: not neccessary, helpful or required
	- line numbers: functionality is already acheived through builtin command "goto NNNl"
	- multiple window support: functionality is already acheived through builtin "open <FILE>"
	- language server integration: not neccessary for programming
	- visual-based cursor movement (as opposed to logical-based movement): implementation complexity doesnt outweigh the benefits.



how to use this editor:
---------------------------------------


to compile the source, use any C99 (or later) compiler:

	clang -Weverything -Os c.c -o editor

	gcc -Wall -Os c.c -o editor


to start the editor, mulitple usage patterns can be used:

	./editor your_file.txt readonly  <--- this opens "your_file.txt" for reading only, all editing is disabled.

	./editor your_file.txt           <--- this opens "your_file.txt" for writing/editing.

	./editor 		         <--- this creates and opens a new document with a time-stamped unique name in the current directory, for writing.



to quit/close the editor, use the command 'Q'. this will automatically save any unwritten changes to the file. there is no way to quit the editor without saving these unsaved changes first, by design. 

if the file was opened in as read-only using the first usage pattern, simply typing lowercase 'q' is sufficient.

quitting/closing edit-mode commands:
+-----------------------------------------------------------------------
| QWERTY |  Workman  |  function
+--------+-----------+--------------------------------------------------
|   q    |    q      |  quit the editor, must be opened as read-only
|   Q    |    Q      |  quit the editor. (must be in edit mode)
+--------+-----------+--------------------------------------------------

note, that this editor was developed using and is used with the workman keyboard layout. however, in the above types of tables, both the QWERTY and Workman keyboard layout keybindings will be given, as many people don't use the Workman keyboard layout. Of course, using the remap() function in the C source code, you can easily modify the editors keymappsings to make sense and be ergonomic for any keyboard layout you happen to use. 

upon entering the editor, you will always be in what is known as "edit mode". it should be noted the above two commands only work in this mode. in this mode, you are able to navigate through the document, and perform editing actions such as deleting, selecting, and copying text, pasting clipboard contents, undoing/redoing previously made edits, and saving the document. 

[note: this editor uses a block cursor, as this is the correct cursor shape to use. if the user is not acustomed to this cursor shape, it might take some getting used to, at first. the highlighted character is the one which is after the cursor.]

the default navigational keybindings are given in the table below: 

edit-mode navigating commands:
+-----------------------------------------------------------------------
| QWERTY |  Workman  |  function
+--------+-----------+--------------------------------------------------
|   j    |    n      |  move cursor left one character.
|   ;    |    i      |  move cursor right one character.
+~~~~~~~~+~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
|   k    |    e      |  move cursor left one alphanumeric word.
|   l    |    o      |  move cursor right one alphanumeric word.
+~~~~~~~~+~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
|   i    |    u      |  move cursor down one line, and position at column 0.
|   o    |    p      |  move cursor up one line, and position at column 0.
+~~~~~~~~+~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
|   c    |    m      |  move cursor down one half-page of lines, and position at column 0.
|   d    |    h      |  move cursor up one half-page of lines, and position at column 0.
+--------+-----------+--------------------------------------------------

to make edits to the document, there are two main ways this is acheived, common with other modal text editors. edit mode, and insert mode. insert mode is used for more lengthy insertions of many characters.

in edit mode, there are the following keybindings, which can edit the document. the first four of these are only acknowledged when the editor is not in read-only mode.

edit-mode editing commands:
+-----------------------------------------------------------------------
| QWERTY   |  Workman  |  function
+----------+-----------+--------------------------------------------------
|   e      |    r      |  backspace/delete one character behind the cursor.
| s w k m  |  s d e l  |  delete the text between the anchor and cursor.
|   f      |    t      |  switch to insert mode.
|   r      |    w      |  paste the current local clipboard contents at the cursor.
+~~~~~~~~~~+~~~~~~~~~~~+~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
|   a      |    a      |  reposition the anchor to be the same as the current cursor position.
|   v      |    c      |  copy the text between the anchor and cursor to the local clipboard.
+----------+-----------+--------------------------------------------------

the third keybinding is special in that it changes the editor from edit mode, to being in insert mode. this will be discussed further later. 

for the second key binding, deleting the current selection, this is actually a key sequence (using the Workman keybindings, you type "s", followed by "d", etc), in order to prevent accidently deleting a large portion of the document, because of current setting of anchor being misunderstood/forgotten. if this kind of safety feature is not desirable to the user, it is trivial to add a simpler single-letter keybind if they so wish to. 

the selecting, deleting, and copy/paste system is a bit different here than most editors. instead of having seperate mechanisms of creating and extending selections of text, merely the anchor keybind ("a") is used. in practice, copying or deleting a range of text looks like the following:

	1. navigate/position the cursor at the beginning or end of the text you wish to select.

	2. drop the anchor by pressing "a" (or the equivalent keybind for this function).

	3. move the cursor to the other end of the text you wish to select (beginning or end, but alternate one)

	4. perform the action you wish to do: the keybinding for either deleting or copying the selected text.


[note: it does not appear like the text is visually selected, however internally it is. this is working as intended.]

additionally, particular characters can be inserted without even entering insert mode. this is acheived via the "s" key, which does nothing by itself. rather, there are sub-keybindings which are recognized to peform a vareity of insertion-related actions:

edit-mode editing commands:
+-----------------------------------------------------------------------
| QWERTY |  Workman  |  function
+--------+-----------+--------------------------------------------------
|  s e   |   s r     |  insert a newline character at cursor.
|  s d   |   s h     |  insert a space character at cursor.
|  s c   |   s m     |  insert a tab character at cursor.
|  s c   |   s m     |  insert a tab character at cursor.
|  s a   |   s a     |  inserts the current date and time using the datetime format at the cursor.
+--------+-----------+--------------------------------------------------

(the last keybinding was added just for my own personal use case, however you might find it useful too perhaps.)

if the keybinding for switching to insert mode is used, this causes all the above keybindings to not be acknowledged anymore, rather, those keys now insert their letter character into the buffer as a non-modal text editor would normally. all printable characters, except for the ESC (escape) character, do as well. the delete/backspace key, which on Unix based systems is ASCII code 127 or 8, can be used for deleting characters behind the cursor, while in insert mode.

however, there is one notable exception. if the exact sequence of characters "drtpun", (or equivalently, "wefoij" for the QWERTY layout), the editor switches from insert mode back to edit mode. if user desires to actually insert this sequence of characters instead of switching back to edit mode, simply insert a space followed by a backspace anywhere in the middle of the "drtpun" sequence.  

note, upon using the "drtpun" sequence of characters while in insert mode, the actual characters which were of course inserted into the buffer while typing this sequence, are subsequently deleted upon successful recognition of the entire sequence. 

exiting insert mode commands:
+-----------------------------------------------------------------------
|  QWERTY       |  Workman       |  function
+---------------+----------------+--------------------------------------------------
|  w e f o i j  |  d r t p u n   |  switch from insert mode back to edit mode.
|   <ESC>       |     <ESC>      |  switch from insert mode back to edit mode, same as above.
+---------------+----------------+--------------------------------------------------



...



WORK IN PROGRESS: sections to finish documentation on:


	- search mode: works similar to insert mode.

	- execution of shell commands

	- opening other files

	- jump numeric, line and offsets,

	- the undo/redo bindings, and how the undo tree system works

	- global clipboard functionality. 

	- 
