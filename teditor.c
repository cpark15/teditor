#include "teditor.h"

/**
 *
 * teditor, a simple text editor with basic functionalities,
 * created by following the tutorial that can be found here:
 * http://viewsourcecode.org/snaptoken/kilo/index.html
 * Author: Calvin Park
 * Date: 5/22/17
 *
 */

/********************************
* Defines
********************************/
#define TEDITOR_VERSION    "0.0.1"
#define TEDITOR_TAB_STOP   8
#define TEDITOR_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT          { NULL, 0 }

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

/********************************
* Data
********************************/
struct editor_config E;

/********************************
* Init
********************************/

/**
 * teditor main method
 */
int main(int argc, char * argv[]) {
    enable_raw_mode();
    init_editor();
    if (argc >= 2) {
        editor_open(argv[1]);
    }

    editor_set_status_message("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (1) {
        editor_refresh_screen();
        editor_process_keypress();
    }
    return 0;
}

/**
 * Intializes struct editor_config
 */
void init_editor() {
    E.cx             = 0;
    E.cy             = 0;
    E.rx             = 0;
    E.rowoff         = 0;
    E.coloff         = 0;
    E.numrows        = 0;
    E.editor_row     = NULL;
    E.dirty          = 0;
    E.filename       = NULL;
    E.statusmsg[0]   = '\0';
    E.statusmsg_time = 0;
    if (get_window_size(&E.row, &E.col) == -1) {
        unix_error("get_window_size");
    }
    E.row -= 2;
}

/********************************
* Input
********************************/

/**
 * Allows user to move cursor with the arrow keys
 */
void editor_move_cursor(int key) {
    erow * row = (E.cy >= E.numrows) ? NULL : &E.editor_row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            }
            else if (E.cy > 0) { // Moving left at the start of a line goes back a line
                E.cy--;
                E.cx = E.editor_row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) { // Limit scrolling to the right
                E.cx++;
            }
            else if (row && E.cx == row->size) { // Moving right at the end of a line goes to next line
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }
    // Snap cursor to the end of a line, prevents going past end when switching lines
    row = (E.cy >= E.numrows) ? NULL : &E.editor_row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
} /* editor_move_cursor */

/**
 * Calls editor_read_key() to capture a keypress, then handles it
 */
void editor_process_keypress() {
    static int quit_times = TEDITOR_QUIT_TIMES;
    int c = editor_read_key();

    switch (c) {
        case '\r':
            editor_insert_newline();
            break;
        case CTRL_KEY('q'):
            if (E.dirty && quit_times > 0) {
                editor_set_status_message("WARNING: File has unsaved changes. "
                  "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4); // Erase in display
            write(STDOUT_FILENO, "\x1b[H", 3);  // Reposition the cursor
            exit(0);
            break;
        case CTRL_KEY('s'):
            editor_save();
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.numrows) {
                E.cx = E.editor_row[E.cy].size;
            }
            break;
        case CTRL_KEY('f'):
            editor_find();
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) {
                editor_move_cursor(ARROW_RIGHT);
            }
            editor_del_char();
            break;
        case PAGE_UP:
        case PAGE_DOWN: {
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            }
            else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.row - 1;
                if (E.cy > E.numrows) {
                    E.cy = E.numrows;
                }
            }
            int times = E.row;
            while (times--) {
                editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
        }
                        break; // Uncrustify formats this code section a little awkwardly
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(c);
            break;
        case CTRL_KEY('l'):
        case '\x1b':
            break;
        default:
            editor_insert_char(c);
            break;
    }
    quit_times = TEDITOR_QUIT_TIMES;
} /* editor_process_keypress */

/**
 * Displays a prompt in the status bar, and lets the user input a line
 * of text after the prompt, acts as a 'save as' if the user did not
 * open a file
 */
char * editor_prompt(char * prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char * buf     = malloc(bufsize);
    size_t buflen  = 0;

    buf[0] = '\0';
    while (1) {
        editor_set_status_message(prompt, buf);
        editor_refresh_screen();
        int c = editor_read_key();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) {
                buf[--buflen] = '\0';
            }
        }
        else if (c == '\x1b') {
            editor_set_status_message("");
            if (callback) {
                callback(buf, c);
            }
            free(buf);
            return NULL;
        }
        else if (c == '\r') {
            if (buflen != 0) {
                editor_set_status_message("");
                if (callback) {
                    callback(buf, c);
                }
                return buf;
            }
        }
        else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf      = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen]   = '\0';
        }

        if (callback) {
            callback(buf, c);
        }
    }
} /* editor_prompt */

/********************************
* Output
********************************/

/**
 * Clears the screen and repositions the cursor
 */
void editor_refresh_screen() {
    editor_scroll();

    struct abuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6); // Hide the cursor during repainting
    ab_append(&ab, "\x1b[H", 3);    // Reposition the cursor

    editor_draw_rows(&ab);
    editor_draw_status_bar(&ab);
    editor_draw_message_bar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6); // Show the cursor after repainting
    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

/**
 * Fills in any blank lines with a tilda
 */
void editor_draw_rows(struct abuf * ab) {
    for (int i = 0; i < E.row; i++) {
        int filerow = i + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && i == E.row / 3) { // Print welcome message if no filename is given
                char welcome[80];
                int len_welcome = snprintf(welcome, sizeof(welcome), "Teditor -- version %s", TEDITOR_VERSION);
                if (len_welcome > E.col) {
                    len_welcome = E.col;
                }
                int padding = (E.col - len_welcome) / 2;
                if (padding) {
                    ab_append(ab, "~", 1);
                    padding--;
                }
                while (padding--) {
                    ab_append(ab, " ", 1);
                }
                ab_append(ab, welcome, len_welcome);
            }
            else {
                ab_append(ab, "~", 1);
            }
        }
        else {
            int len = E.editor_row[filerow].rsize - E.coloff;
            if (len < 0) {
                len = 0;
            }
            if (len > E.col) {
                len = E.col;
            }
            ab_append(ab, &E.editor_row[filerow].render[E.coloff], len);
        }
        ab_append(ab, "\x1b[K", 3); // Erase in display by line
        ab_append(ab, "\r\n", 2);
    }
} /* editor_draw_rows */

/**
 * Makes scrolling possible in editor
 */
void editor_scroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = editor_row_cx_to_rx(&E.editor_row[E.cy], E.cx);
    }

    // Vertical scrolling
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.row) {
        E.rowoff = E.cy - E.row + 1;
    }
    // Horizontal scrolling
    if (E.cx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.cx >= E.coloff + E.col) {
        E.coloff = E.rx - E.col + 1;
    }
}

/**
 * Display the status of a file at the bottom of the screen,
 * includes filename and line count
 */
void editor_draw_status_bar(struct abuf * ab) {
    ab_append(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
    if (len > E.col) {
        len = E.col;
    }
    ab_append(ab, status, len);
    while (len < E.col) {
        if (E.col - len == rlen) {
            ab_append(ab, rstatus, rlen);
            break;
        }
        else {
            ab_append(ab, " ", 1);
            len++;
        }
    }
    ab_append(ab, "\x1b[m", 3);
    ab_append(ab, "\r\n", 2);
}

/**
 * Create a status message to display at the bottom of the screen
 */
void editor_set_status_message(const char * fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/**
 * Create the message bar to display
 */
void editor_draw_message_bar(struct abuf * ab) {
    ab_append(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.col) {
        msglen = E.col;
    }
    if (msglen && time(NULL) - E.statusmsg_time < 5) {
        ab_append(ab, E.statusmsg, msglen);
    }
}

/********************************
* Append Buffer
********************************/

/**
 * Dynamic string support, allows string in struct ab to be appended
 */
void ab_append(struct abuf * ab, const char * s, int len) {
    char * new = realloc(ab->b, ab->len + len);

    if (new == NULL) {
        return;
    }
    memcpy(&new[ab->len], s, len);
    ab->b    = new;
    ab->len += len;
}

/**
 * Frees the struct ab
 */
void ab_free(struct abuf * ab) {
    free(ab->b);
}

/********************************
* Row Operations
********************************/

/**
 * Allocates space for a new erow, then copies the string to new erow
 */
void editor_insert_row(int at, char * s, size_t len) {
    if (at < 0 || at > E.numrows) {
        return;
    }
    E.editor_row = realloc(E.editor_row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.editor_row[at + 1], &E.editor_row[at], sizeof(erow) * (E.numrows - at));

    E.editor_row[at].size  = len;
    E.editor_row[at].chars = malloc(len + 1);
    memcpy(E.editor_row[at].chars, s, len);
    E.editor_row[at].chars[len] = '\0';

    E.editor_row[at].rsize  = 0;
    E.editor_row[at].render = NULL;
    editor_update_row(&E.editor_row[at]);

    E.numrows++;
    E.dirty++;
}

/**
 * Uses the chars string of an erow to fill in the contents of the render string,
 * replaces any tabs with spaces
 */
void editor_update_row(erow * row) {
    int tabs = 0;

    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            tabs++;
        }
    }
    free(row->render);
    row->render = malloc(row->size + (tabs * (TEDITOR_TAB_STOP - 1)) + 1);
    int idx = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TEDITOR_TAB_STOP != 0) {
                row->render[idx++] = ' ';
            }
        }
        else {
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize       = idx;
}

/**
 * Calculates the value of rx in editor_scroll
 */
int editor_row_cx_to_rx(erow * row, int cx) {
    int rx = 0;

    for (int i = 0; i < cx; i++) {
        if (row->chars[i] == '\t') {
            rx += (TEDITOR_TAB_STOP - 1) - (rx % TEDITOR_TAB_STOP);
        }
        rx++;
    }
    return rx;
}

/**
 * Converts the render index into a chars index
 */
int editor_row_rx_to_cx(erow * row, int rx) {
    int cur_rx = 0;
    int i;

    for (i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            cur_rx += (TEDITOR_TAB_STOP - 1) - (cur_rx % TEDITOR_TAB_STOP);
        }
        cur_rx++;
        if (cur_rx > rx) {
            return i;
        }
    }
    return i;
}

/**
 * Inserts a single character into an erow at a given position
 */
void editor_row_insert_char(erow * row, int at, int c) {
    if (at < 0 || at > row->size) {
        at = row->size;
    }
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editor_update_row(row);
    E.dirty++;
}

/**
 * Allows the deletion of characters from erow
 */
void editor_row_del_char(erow * row, int at) {
    if (at < 0 || at >= row->size) {
        return;
    }
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editor_update_row(row);
    E.dirty++;
}

/**
 * Removes and frees row from erow
 */
void editor_free_row(erow * row) {
    free(row->render);
    free(row->chars);
}

/**
 * Deletes a row
 */
void editor_del_row(int at) {
    if (at < 0 || at >= E.numrows) {
        return;
    }
    editor_free_row(&E.editor_row[at]);
    memmove(&E.editor_row[at], &E.editor_row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

/**
 * Appends a string to the end of a row
 */
void editor_row_append_string(erow * row, char * s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editor_update_row(row);
    E.dirty++;
}

/********************************
* Editor Operations
********************************/

/**
 * Insert character into the position the cursor is at
 */
void editor_insert_char(int c) {
    if (E.cy == E.numrows) {
        editor_insert_row(E.numrows, "", 0);
    }
    editor_row_insert_char(&E.editor_row[E.cy], E.cx, c);
    E.cx++;
}

/**
 * Deletes the character that is to the left of the cursor
 */
void editor_del_char() {
    if (E.cy == E.numrows) {
        return;
    }
    if (E.cx == 0 && E.cy == 0) {
        return;
    }
    erow * row = &E.editor_row[E.cy];
    if (E.cx > 0) {
        editor_row_del_char(row, E.cx - 1);
        E.cx--;
    }
    else {
        E.cx = E.editor_row[E.cy - 1].size;
        editor_row_append_string(&E.editor_row[E.cy - 1], row->chars, row->size);
        editor_del_row(E.cy);
        E.cy--;
    }
}

/**
 * Inserts a new line into the position the cursor is at,
 * if there is text to the right of the cursor, move it to the next row
 */
void editor_insert_newline() {
    if (E.cx == 0) {
        editor_insert_row(E.cy, "", 0);
    }
    else {
        erow * row = &E.editor_row[E.cy];
        editor_insert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row       = &E.editor_row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editor_update_row(row);
    }
    E.cy++;
    E.cx = 0;
}

/********************************
* Find
********************************/

/**
 * Method for a basic search feature
 */
void editor_find() {
    int saved_cx     = E.cx;
    int saved_cy     = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;
    char * query     = editor_prompt("Search: %s (ESC/Enter to cancel)", editor_find_callback);

    if (query) {
        free(query);
    }
    else {
        E.cx     = saved_cx;
        E.cy     = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

/**
 * Loops through rows of a file and if a row contains the query string,
 * move the cursor to the match, allows for iterative search
 */
void editor_find_callback(char * query, int key) {
    static int last_match = -1;
    static int direction  = 1;

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction  = 1;
        return;
    }
    else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    }
    else {
        last_match = -1;
        direction  = 1;
    }

    if (last_match == -1) {
        direction = 1;
    }
    int current = last_match;
    for (int i = 0; i < E.numrows; i++) {
        current += direction;
        if (current == -1) {
            current = E.numrows - 1;
        }
        else if (current == E.numrows) {
            current = 0;
        }
        erow * row   = &E.editor_row[current];
        char * match = strstr(row->render, query);
        if (match) { // If a match is found, convert substring location into an index
            last_match = current;
            E.cy       = current;
            E.cx       = editor_row_rx_to_cx(row, match - row->render);
            E.rowoff   = E.numrows;
            break;
        }
    }
} /* editor_find_callback */

/********************************
* File I/O
********************************/

/**
 * Attempts to open given filename for viewing
 */
void editor_open(char * filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE * fp = fopen(filename, "r");

    if (!fp) {
        unix_error("fopen");
    }
    char * line    = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }
        editor_insert_row(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

/**
 * Converts array of erow structs into a single string ready to
 * be written out to a file
 */
char * editor_rows_to_string(int * buflen) {
    int totlen = 0;

    for (int i = 0; i < E.numrows; i++) {
        totlen += E.editor_row[i].size + 1;
    }
    *buflen = totlen;

    char * buf = malloc(totlen);
    char * p   = buf;
    for (int i = 0; i < E.numrows; i++) {
        memcpy(p, E.editor_row[i].chars, E.editor_row[i].size);
        p += E.editor_row[i].size;
        *p = '\n';
        p++;
    }
    return buf;
}

/**
 * Save written text by writing it to a file, if it is a new file, prompt the
 * user for a filename, pressing 'esc' aborts the save (Note: For Bash on
 * Windows, 'esc' will have to be pressed 3 times)
 */
void editor_save() {
    if (E.filename == NULL) {
        E.filename = editor_prompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            editor_set_status_message("Save aborted");
            return;
        }
    }
    int len;
    char * buf = editor_rows_to_string(&len);
    int fd     = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editor_set_status_message("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editor_set_status_message("Can't save! I/O error: %s", strerror(errno));
}

/********************************
* Terminal
********************************/

/**
 * Waits for a keypress, then returns it
 */
int editor_read_key() {
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if ((nread == -1) && (errno != EAGAIN)) {
            unix_error("read");
        }
    }

    if (c == '\x1b') { // Char is a form of an escape sequence
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME_KEY;

                        case '3':
                            return DEL_KEY;

                        case '4':
                            return HOME_KEY;

                        case '5':
                            return PAGE_UP;

                        case '6':
                            return PAGE_DOWN;

                        case '7':
                            return HOME_KEY;

                        case '8':
                            return END_KEY;
                    }
                }
            }
            else {
                switch (seq[1]) {
                    case 'A':
                        return ARROW_UP;

                    case 'B':
                        return ARROW_DOWN;

                    case 'C':
                        return ARROW_RIGHT;

                    case 'D':
                        return ARROW_LEFT;

                    case 'H':
                        return HOME_KEY;

                    case 'F':
                        return END_KEY;
                }
            }
        }
        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H':
                    return HOME_KEY;

                case 'F':
                    return END_KEY;
            }
        }
        return '\x1b';
    }
    return c;
} /* editor_read_key */

/**
 * Turns off ECHO feature, this means that input is no longer
 * printed to the console
 * Turns off canonical mode, this means that input will be read byte by byte
 * rather than line by line
 * Other changes are made that is traditional to enabling 'raw mode' not all
 * modifications are necessary
 */
void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.original_term) == -1) { // Get terminal attributes
        unix_error("tcgetattr");
    }
    atexit(disable_raw_mode); // Call when the program exits
    struct termios raw = E.original_term;
    raw.c_iflag    &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Modify input flags
    raw.c_oflag    &= ~(OPOST);                                  // Modify output flags
    raw.c_cflag    |= (CS8);                                     // Sets character sizes to 8 bits per byte
    raw.c_lflag    &= ~(ECHO | ICANON | ISIG | IEXTEN);          // Modify local flags
    raw.c_cc[VMIN]  = 0;                                         // Sets the min number of bytes of input needed before read() can return
    raw.c_cc[VTIME] = 1;                                         // Sets max amount of time to wait before read() returns
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {        // Set terminal attributes
        unix_error("tcsetattr");
    }
}

/**
 * Restores the user's terminal original attributes
 */
void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_term) == -1) {
        unix_error("tcsetattr");
    }
}

/**
 * Gets the window size of the terminal and returns it
 */
int get_window_size(int * rows, int * cols) {
    struct winsize ws;

    if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) || (ws.ws_col == 0)) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
            return -1;
        }
        return get_cursor_position(rows, cols);
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

/**
 * Gets the position of the cursor for purposes of finding the
 * terminal window size, used if 'ioctl' fails
 */
int get_cursor_position(int * rows, int * cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
        return -1;
    }
    while (i < (sizeof(buf) - 1)) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) {
            break;
        }
        if (buf[i] == 'R') {
            break;
        }
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') {
        return -1;
    }
    if (sscanf(&buf[2], "%d,%d", rows, cols) != 2) {
        return -1;
    }
    return 0;
}

/**
 * Prints error message and exits the program
 */
void unix_error(const char * s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Erase in display
    write(STDOUT_FILENO, "\x1b[H", 3);  // Reposition the cursor

    perror(s);
    exit(1);
}
