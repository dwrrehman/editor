
typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

struct location {
    i32 line;
    i32 column;
};

struct screen_location {
    i16 line;
    i16 column;
};

struct window {
    char* screen;
    i16 columns;
    i16 rows;
    i32 _padding;
};

struct logical_line {
    u8* line;
    i32 capacity;
    i32 length;
};

struct render_line {
    u8* line;
    i32 capacity;
    i32 length;
    i32 visual_length;
    i32 continued;
};

struct logical_lines {
    struct logical_line* lines;
    i32 count;
    i32 capacity;
};

struct render_lines {
    struct render_line* lines;
    i32 count;
    i32 capacity;
};

struct file {
    struct location begin;
    struct location cursor;
    struct location render_cursor;
    struct location visual_cursor;
    struct location visual_origin;
    struct logical_lines logical;
    struct render_lines render;
    struct screen_location visual_screen;
    i32 visual_desired;
    
    char filename[4096];
};