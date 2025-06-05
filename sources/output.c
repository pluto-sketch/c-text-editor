#include "../includes/library.h"

// moved globally for convinience
int line_number_width = 4; // width for line numbers


void editorScroll()
{
    int line_number_width = 5; // 4 digits + 1 space for gutter (match editorDrawRows)

    E.rx = 0;
    if (E.cy < E.numrows)
    {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    // Vertical scroll
    if (E.cy < E.rowoff)
    {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows)
    {
        E.rowoff = E.cy - E.screenrows + 1;
    }

    // Horizontal scroll, adjusted for gutter width
    if (E.rx < E.coloff)
    {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + (E.screencols - line_number_width))
    {
        E.coloff = E.rx - (E.screencols - line_number_width) + 1;
    }
}


void editorDrawRows(struct abuf *ab)
{
    int y;

    for (y = 0; y < E.screenrows; y++)
    {
        int filerow = y + E.rowoff;

        // 1) Print line number, right aligned, padded with spaces
        if (filerow < E.numrows) {
            char linenum[line_number_width + 1];
            // Format line number right-aligned in line_number_width space
            snprintf(linenum, sizeof(linenum), "%*d ", line_number_width - 1, filerow + 1);
            abAppend(ab, linenum, strlen(linenum));

            abAppend(ab, " ", 1);
        } else {
            // Lines beyond file end: print spaces + a space for alignment
            char empty_line_num[line_number_width + 1];
            snprintf(empty_line_num, sizeof(empty_line_num), "%*s ", line_number_width, " ");
            abAppend(ab, empty_line_num, strlen(empty_line_num));
        }

        // 2) print the actual line or empty spaces if no line

        if (filerow >= E.numrows)
        {
            if (E.numrows == 0 && y == E.screenrows / 3)
                       {
                           char welcome[80];
                           int welcomelen = snprintf(welcome, sizeof(welcome),
                                                     "vimwannabe editor -- version %s", VIMWANNABE_VERSION);
                           if (welcomelen > E.screencols)
                               welcomelen = E.screencols;
                           int padding = (E.screencols - welcomelen) / 2;
                           if (padding)
                           {
                               abAppend(ab, "~", 1);
                               padding--;
                           }
                       }
            // print blank space after line number
            int len = E.screencols - (line_number_width + 1);
            if (len > 0) {
                for (int i = 0; i < len; i++)
                    abAppend(ab, " ", 1);
            }
        }
        else
        {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0)
                len = 0;
            if (len > E.screencols - (line_number_width + 1))
                len = E.screencols - (line_number_width + 1);

            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;
            int j;

            for (j = 0; j < len; j++)
            {
                if (iscntrl(c[j]))
                {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_color != -1)
                    {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                }
                else if (hl[j] == HL_NORMAL)
                {
                    if (current_color != -1)
                    {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                }
                else
                {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color)
                    {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }

        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                       E.filename ? E.filename : "[No Name]", E.numrows,
                       E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
                        E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);
    if (len > E.screencols)
        len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols)
    {
        if (E.screencols - len == rlen)
        {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else
        {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab)
{
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols)
        msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen()
{
    editorScroll();

    struct abuf ab = BUFF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
             (E.rx - E.coloff) + 1 + line_number_width);

    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}
