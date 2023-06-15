




// nat wrap_line = 0; wrap_column = 0;



// static inline void move_left(struct file* file) {
// 	if (not file->column) {
// 		if (not file->line) return;
// 		file->line--; file->column = file->lines[file->line].length;
// 	} else do file->column--; while (file->column and 
// 		is_unicode(file->lines[file->line].line[file->column]));
// }

// static inline void move_right(struct file* file) {
// 	if (file->column >= file->lines[file->line].length) {
// 		if (file->line + 1 >= file->count) return;
// 		file->line++; file->column = 0;
// 	} else do file->column++; while (file->column < file->lines[file->line].length and 
// 		is_unicode(file->lines[file->line].line[file->column]));
// }








		// else if (c == 'W') { if (wrap_width) wrap_width--; }
		// else if (c == 'E') { wrap_width++; }
		// else if (c == '1') { if (tab_width > 1) tab_width--; }
		// else if (c == '2') { tab_width++; }








	/// note: doesnt put newlines (lineclears) on all lines in the window- only 
	//// touches the lines that have a line on them.... note to self, for wwhen debug use. 
	// for (i32 line = 0; line < file->count; line++) {
	// 	i32 column = 0; 
	// 	for (; column < file->lines[line].length; column++) {
	// 		i8 c = file->lines[line].line[column];
	// 		if (column == file->column and line == file->line) {
	// 			cursor_screen_line = screen_line; 
	// 			cursor_screen_column = screen_column;
	// 		}
	// 		if (c == '\t') {
	// 			do { 
	// 				// if (screen_column >= window_columns or 
	// 				//     screen_column >= wrap_width) break;
	// 				screen[length++] = ' ';
	// 				screen_column++;
	// 			} while (screen_column % tab_width);
	// 		} else {
	// 			screen[length++] = (char) c;
	// 			if (!is_unicode(c)) screen_column++;
	// 		}
	// 	}

	// 	if (column == file->column and line == file->line) {
	// 		cursor_screen_line = screen_line; 
	// 		cursor_screen_column = screen_column;
	// 	}
		
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




/*


static inline void move_left(struct file* file) {
	if (not file->column) {
		if (not file->line) return;
		file->line--;
		file->column = file->lines[file->line].length;

		// if (file->screen_line) file->screen_line--;
		// else if (file->origin_line) file->origin_line--;

		// if (file->column > window_columns) {
		// 	file->screen_column = window_columns;
		// 	file->origin_column = file->column - window_columns;
		// } else {
		// 	file->screen_column = (i16) file->column;
		// 	file->origin_column = 0;
		// }
	} else {
		do {
			file->column--;
			// if (file->screen_column) file->screen_column--;
			// else if (file->origin_column) file->origin_column--;
		} while (file->column and 
			is_unicode(file->lines[file->line].line[file->column]));
	}
}

static inline void move_right(struct file* file) {
	if (file->column >= file->lines[file->line].length) {
		if (file->line + 1 >= file->count) return;
		file->line++;
		file->column = 0;
		// file->origin_column = 0;
		// file->screen_column = 0;
		// if (file->screen_line < window_rows) file->screen_line++;
		// else file->origin_line++;
	} else {
		do {
			file->column++;
			// if (file->screen_column < window_columns) file->screen_column++;
			// else file->origin_column++;
		} while (file->column < file->lines[file->line].length and 
			is_unicode(file->lines[file->line].line[file->column]));
	}
}










	const i32 origin_line = file->line > window_rows ? file->line - window_rows : 0;
	const i32 origin_column = file->column > window_columns ? file->column - window_columns : 0;


if (file->line > window_rows) {
		file->screen_line = window_rows;
		file->origin_line = file->line - window_rows;
	} else {
		file->screen_line = (i16) file->line;
		file->origin_line = 0;
	}

	if (file->column > window_columns) {
		file->screen_column = window_columns;
		file->origin_column = file->column - window_columns;
	} else {
		file->screen_column = (i16) file->column;
		file->origin_column = 0;
	}







*/



