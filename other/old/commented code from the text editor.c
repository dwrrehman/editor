






























// ------------------------------------------old code-----------------------------------------















































///NOTE:   after you get display working perfectly, code up the MOVE up and down functions!!! 
//   		then do word movement, and maybe also try for word wrapping..?!?!? that would be amazing.
///        also try to see what other features from text edit, you are actually still missing!!




				// if (column >= file->lines[line].length) break;
				// if (column >= wrap_width) goto next_line;
				// if (column < file->origin_column) {
				// 	column++;
				// 	continue;
				// }

				// if (column == file->column and line == file->line) {
				// 	cursor_screen_line = screen_line; 
				// 	cursor_screen_column = screen_column;
				// }

				// i8 c = file->lines[line].line[column++];

				// if (c == '\t') {
				// 	do { 
				// 		if (screen_column >= window_columns or 
				// 		    screen_column >= wrap_width) break;
				// 		screen[length++] = ' ';
				// 		screen_column++;
				// 	} while (screen_column % tab_width);
				// } else {
				// 	screen[length++] = (char) c;
				// 	if (!is_unicode(c)) screen_column++;
				// }

			// while (column < file->lines[line].length) {
			// 	if (column == wrap_width) goto next_line;
			// 	column++;
			// 	screen_column++;
			// }



			// if (column == file->column and line == file->line) {
			// 	cursor_screen_line = screen_line; 
			// 	cursor_screen_column = screen_column;
			// }




/*
lcl
lcc
lol
loc
lsl
lsc
vcl
vcc
vol
voc
vsl
vsc
vdc

*/


	// i32 line = file->origin_line, column = 0;
	// i32 wrap_line = 0, wrap_column = 0;
	// i16 screen_line = 0, screen_column = 0;
	// i16 cursor_screen_line = 0, cursor_screen_column = 0;

	// while (screen_line < window_rows) {

	// 	if (line >= file->count) goto next_screen_line;

	// 	while (screen_column < window_columns) {

	// 		if (column >= file->lines[line].length) goto next_line;

	// 		i8 c = file->lines[line].line[column];

	// 		if (c == '\t') {
	// 			do { 
	// 				if (screen_column >= window_columns or 
	// 				    screen_column >= wrap_width) break;
	// 				screen[length++] = ' ';
	// 				screen_column++;
	// 			} while (screen_column % tab_width);
	// 		} else {
	// 			screen[length++] = (char) c;
	// 			if (not is_unicode(c)) screen_column++;
	// 		}
    
	// 		column++;
	// 	}

	// next_line:
	// 	line++;
	// 	column = 0;

	// next_screen_line:
	// 	screen[length++] = '\033';
	// 	screen[length++] = '[';
	// 	screen[length++] = 'K';
	// 	if (screen_line < window_rows - 1) {
	// 		screen[length++] = '\r';
	// 		screen[length++] = '\n';
	// 	}
	// 	screen_line++;
	// 	screen_column = 0;
	// }



		// while (column < file->lines[line].length) {
		// 	column++;
		// if (column >= wrap_width) goto next_line;
		// 			if (column < file->origin_column) {
		// 				column++;
		// 				continue;
		// 			}
		// 		i
		// }



/*

static inline void move_left(struct file* file) {
	if (not file->column) {
		if (not file->line) return;
		file->line--;
		file->column = file->lines[file->line].length;
		if (file->screen_line) file->screen_line--;
		else if (file->origin_line) file->origin_line--;
		if (file->column > window_columns) {
			file->screen_column = window_columns;
			file->origin_column = file->column - window_columns;
		} else {
			file->screen_column = (i16) file->column;
			file->origin_column = 0;
		}
	} else {
		do {
			file->column--;
			if (file->screen_column) file->screen_column--;
			else if (file->origin_column) file->origin_column--;
		} while (file->column and 
			is_unicode(file->lines[file->line].line[file->column]));
	}
}

static inline void move_right(struct file* file) {
	if (file->column >= file->lines[file->line].length) {
		if (file->line + 1 >= file->count) return;
		file->line++;
		file->column = 0;
		file->origin_column = 0;
		file->screen_column = 0;
		if (file->screen_line < window_rows) file->screen_line++;
		else file->origin_line++;
	} else {
		do {
			file->column++;
			if (file->screen_column < window_columns) file->screen_column++;
			else file->origin_column++;
		} while (file->column < file->lines[file->line].length and 
			 is_unicode(file->lines[file->line].line[file->column]));
	}
}


*/



//  move left:

	// 	file->line--;
	// 	file->column = file->lines[file->line].length;
	// 	if (file->screen_line) file->screen_line--;
	// 	else if (file->origin_line) file->origin_line--;
	// 	if (file->column > window_columns) {
	// 		file->screen_column = window_columns;
	// 		file->origin_column = file->column - window_columns;
	// 	} else {
	// 		file->screen_column = (i16) file->column;
	// 		file->origin_column = 0;
	// 	}
	//} else {
	// 	do {
	// 		file->column--;
	// 		if (file->screen_column) file->screen_column--;
	// 		else if (file->origin_column) file->origin_column--;
	// 	} while (file->column and 
	// 		is_unicode(file->lines[file->line].line[file->column]));



// move right:

	// 	file->line++;
	// 	file->column = 0;
	// 	file->origin_column = 0;
	// 	file->screen_column = 0;
	// 	if (file->screen_line < window_rows) file->screen_line++;
	// 	else file->origin_line++;
	// } else {
	// 	do {
	// 		file->column++;
	// 		if (file->screen_column < window_columns) file->screen_column++;
	// 		else file->origin_column++;
	// 	} while (file->column < file->lines[file->line].length and 
	// 		 is_unicode(file->lines[file->line].line[file->column]));



/*
	if (not file->column) {
		if (not file->line) return;
		file->line--;
		file->column = file->lines[file->line].length;
		if (file->screen_line) file->screen_line--;
		else if (file->origin_line) file->origin_line--;
		if (file->column > window_columns) {
			file->screen_column = window_columns;
			file->origin_column = file->column - window_columns;
		} else {
			file->screen_column = (i16) file->column;
			file->origin_column = 0;
		}
	} else {
		do {
			file->column--;
			if (file->screen_column) file->screen_column--;
			else if (file->origin_column) file->origin_column--;
		} while (file->column and 
			is_unicode(file->lines[file->line].line[file->column]));
	}



*/


		// if (column >= wrap_width) goto next_line;
				// if (column < file->origin_column) {
				// 	column++;
				// 	continue;
				// }
			



/*while (visual_length < voc and column < lines[line].count) {

			char c = lines[line].data[column++];
	
			if (c == '\t') {
				do {
					if (not(visual_length < voc and column < lines[line].count)) break;
					
				} while (visual_length % tab_width);
			} else if (visual(c)) visual_length++;
		}*/





/*
static int lol = 0; // logical origin line
static int loc = 0; // logical origin column

static int lsl = 0; // logical screen line
static int lsc = 0; // logical screen column












for (int line = vol; line < count; line++) {

		if (screen_line >= window_rows - 2) break;

		int column = 0; 
		int visual_length = 0;

		for (; column < lines[line].count; column++) {

			if (screen_column >= window_columns) break;
		
			if (column == lcc and line == lcl) {
				cl = screen_line;
				cc = screen_column;
			}
		
			char c = lines[line].data[column];

			if (c == '\t') {
				do { 
					if (visual_length >= voc) screen[length++] = ' ';
					screen_column++;
					visual_length++;
				} while (screen_column % tab_width);
			} else {
				if (visual_length >= voc) screen[length++] = c;
				if (visual(c)) { screen_column++; visual_length++; }
			}
		}

		if (column == lcc and line == lcl) {
			cl = screen_line;
			cc = screen_column;
		}

		screen[length++] = '\033';
		screen[length++] = '[';	
		screen[length++] = 'K';
		if (screen_line < window_rows - 1) {
			screen[length++] = '\r';
			screen[length++] = '\n';
		}
		screen_line++;
		screen_column = 0;
	}

	for (; screen_line < window_rows - 2; screen_line++) {
		screen[length++] = '\033';
		screen[length++] = '[';	
		screen[length++] = 'K';
		if (screen_line < window_rows - 1) {
			screen[length++] = '\r';
			screen[length++] = '\n';
		}
	}






*/





/*length += sprintf(screen + length,
		"\033[K\n\rxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\033[K\n\r[%d,%d:(%d,%d){%d,%d}[%d,%d:%d,%d]\033[K\n\r",
	count, lines[lcl].count, lcl,lcc, vcl,vcc, vol,voc, vsl,vsc );*/


