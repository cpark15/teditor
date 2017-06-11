#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>

/********************************
* Data
********************************/

typedef struct erow {
    int idx;
    int size;
    int rsize;
    char * chars;
    char * render;
    unsigned char * hl;
    int hl_open_comment;
} erow;

struct editor_config {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int row;
    int col;
    int numrows;
    erow * editor_row;
    int dirty;
    char * filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editor_syntax * syntax;
    struct termios original_term;
};

struct abuf {
    char * b;
    int len;
};

struct editor_syntax {
    char * filetype;
    char ** filematch;
    char ** keywords;
    char * singleline_comment_start;
    char * multiline_comment_start;
    char * multiline_comment_end;
    int flags;
};

enum editor_key {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

enum editor_highlight {
    HL_NORMAL = 0,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

/********************************
* Init
********************************/

void init_editor(void);

/********************************
* Input
********************************/

void editor_move_cursor(int key);
void editor_process_keypress(void);
char * editor_prompt(char * prompt, void (*callback)(char *, int));

/********************************
* Output
********************************/

void editor_refresh_screen(void);
void editor_draw_rows(struct abuf * ab);
void editor_scroll();
void editor_draw_status_bar(struct abuf * ab);
void editor_set_status_message(const char * fmt, ...);
void editor_draw_message_bar(struct abuf * ab);

/********************************
* Append Buffer
********************************/

void ab_append(struct abuf * ab, const char * s, int len);
void ab_free(struct abuf * ab);

/********************************
* Row Operations
********************************/

void editor_insert_row(int at, char * s, size_t len);
void editor_update_row(erow * row);
int editor_row_cx_to_rx(erow * row, int cx);
int editor_row_rx_to_cx(erow * row, int rx);
void editor_row_insert_char(erow * row, int at, int c);
void editor_row_del_char(erow * row, int at);
void editor_free_row(erow * row);
void editor_del_row(int at);
void editor_row_append_string(erow * row, char * s, size_t len);

/********************************
* Editor Operations
********************************/

void editor_insert_char(int c);
void editor_del_char();
void editor_insert_newline();

/********************************
* Find
********************************/

void editor_find();
void editor_find_callback(char * query, int key);

/********************************
* Syntax Highlighting
********************************/

void editor_update_syntax(erow * row);
void editor_select_syntax_highlight();
int editor_syntax_to_color(int hl);
int is_separator(int c);

/********************************
* File I/O
********************************/

void editor_open(char * filename);
char * editor_rows_to_string(int * buflen);
void editor_save();

/********************************
* Terminal
********************************/

int editor_read_key(void);
void enable_raw_mode(void);
void disable_raw_mode(void);
int get_window_size(int * rows, int * cols);
int get_cursor_position(int * rows, int * cols);
void unix_error(const char * s);
