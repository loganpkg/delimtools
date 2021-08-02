/*
 * Copyright (c) 2021 Logan Ryan McLintock
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Double buffering terminal graphics
 */

#ifdef __linux__
#define _GNU_SOURCE
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../gen/gen.h"
#include "../buf/buf.h"
#include "minicurses.h"

#define INIT_BUF_SIZE 512

/* ANSI escape sequences */
#define phy_clear_screen() printf("\033[2J\033[1;1H")

/* Index starts at one. Top left is (1, 1) */
#define phy_move_cursor(y, x) printf("\033[%lu;%luH", (unsigned long) (y), \
    (unsigned long) (x))

#define phy_attr_off() printf("\033[m")

#define phy_inverse_video() printf("\033[7m")

int addnstr(char *str, int n)
{
    char ch;
    int ret;
    while (n-- && (ch = *str++)) {
        printch(ch);
        if (ret)
            return 1;
    }
    return 0;
}

static int get_screen_size(size_t * height, size_t * width)
{
    /* Gets the screen size */
#ifdef _WIN32
    HANDLE out;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return 1;
    if (!GetConsoleScreenBufferInfo(out, &info))
        return 1;
    *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    *width = info.srWindow.Right - info.srWindow.Left + 1;
    return 0;
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 1;
    *height = ws.ws_row;
    *width = ws.ws_col;
    return 0;
#endif
}

int erase(void)
{
    size_t new_h, new_w, req_vms;
    char *tmp_ns, *tmp_cs;
    if (get_screen_size(&new_h, &new_w))
        return 1;

    /* Reset virtual index */
    stdscr->v = 0;

    /* Clear hard or change in screen dimensions */
    if (stdscr->hard || new_h != stdscr->h || new_w != stdscr->w) {
        stdscr->h = new_h;
        stdscr->w = new_w;
        if (mof(stdscr->h, stdscr->w))
            return 1;
        stdscr->sa = stdscr->h * stdscr->w;
        /*
         * Add TABSIZE to the end of the virtual screen to
         * allow for characters to be printed off the screen.
         * Assumes that tab consumes the most screen space
         * out of all the characters.
         */
        if (aof(stdscr->sa, TABSIZE))
            return 1;
        req_vms = stdscr->sa + TABSIZE;
        /* Bigger screen */
        if (stdscr->vms < req_vms) {
            if ((tmp_ns = malloc(req_vms)) == NULL)
                return 1;
            if ((tmp_cs = malloc(req_vms)) == NULL) {
                free(tmp_ns);
                return 1;
            }
            free(stdscr->ns);
            stdscr->ns = tmp_ns;
            free(stdscr->cs);
            stdscr->cs = tmp_cs;
            stdscr->vms = req_vms;
        }
        /*
         * Clear the virtual current screen. No need to erase the
         * virtual screen beyond the physical screen size.
         */
        memset(stdscr->cs, ' ', stdscr->sa);
        phy_attr_off();
        stdscr->phy_iv = 0;
        phy_clear_screen();
        stdscr->hard = 0;
    }
    /* Clear the virtual next screen */
    memset(stdscr->ns, ' ', stdscr->sa);
    return 0;
}

int clear(void)
{
    stdscr->hard = 1;
    return erase();
}

int endwin(void)
{
    int ret = 0;
    /* Screen is not initialised */
    if (stdscr == NULL)
        return 1;
    phy_attr_off();
    phy_clear_screen();
#ifndef _WIN32
    if (tcsetattr(STDIN_FILENO, TCSANOW, &stdscr->t_orig))
        ret = 1;
#endif
    free(stdscr->ns);
    free(stdscr->cs);
    free_buf(stdscr->input);
    free(stdscr);
    return ret;
}

WINDOW *initscr(void)
{
#ifdef _WIN32
    HANDLE out;
    DWORD mode;
#else
    struct termios term_orig, term_new;
#endif

    /* Error, screen is already initialised */
    if (stdscr != NULL)
        return NULL;

#ifdef _WIN32
    /* Check input is from a terminal */
    if (!_isatty(_fileno(stdin)))
        return NULL;
    /* Turn on interpretation of VT100-like escape sequences */
    if ((out = GetStdHandle(STD_OUTPUT_HANDLE)) == INVALID_HANDLE_VALUE)
        return NULL;
    if (!GetConsoleMode(out, &mode))
        return NULL;
    if (!SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        return NULL;
#else
    if (!isatty(STDIN_FILENO))
        return NULL;
    /* Change terminal input to raw and no echo */
    if (tcgetattr(STDIN_FILENO, &term_orig))
        return NULL;
    term_new = term_orig;
    cfmakeraw(&term_new);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new))
        return NULL;
#endif

    if ((stdscr = calloc(1, sizeof(WINDOW))) == NULL) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        return NULL;
    }

    if ((stdscr->input = init_buf(INIT_BUF_SIZE)) == NULL) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        free(stdscr);
        stdscr = NULL;
        return NULL;
    }

    if (clear()) {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &term_orig);
#endif
        free_buf(stdscr->input);
        free(stdscr);
        stdscr = NULL;
        return NULL;
    }
#ifndef _WIN32
    stdscr->t_orig = term_orig;
#endif

    return stdscr;
}

static void draw_diff(void)
{
    /* Physically draw the screen where the virtual screens differ */
    int in_pos = 0;             /* In position for printing */
    char ch;
    size_t i;
    for (i = 0; i < stdscr->sa; ++i) {
        if ((ch = *(stdscr->ns + i)) != *(stdscr->cs + i)) {
            if (!in_pos) {
                /* Top left corner is (1, 1) not (0, 0) so need to add one */
                phy_move_cursor(i / stdscr->w + 1, i % stdscr->w + 1);
                in_pos = 1;
            }
            /* Inverse video mode */
            if (ivon(ch) && !stdscr->phy_iv) {
                phy_inverse_video();
                stdscr->phy_iv = 1;
            } else if (!ivon(ch) && stdscr->phy_iv) {
                phy_attr_off();
                stdscr->phy_iv = 0;
            }
            putchar(ch & 0x7F);
        } else {
            in_pos = 0;
        }
    }
}

int refresh(void)
{
    char *t;
    draw_diff();
    /* Set physical cursor to the position of the virtual cursor */
    if (stdscr->v < stdscr->sa)
        phy_move_cursor(stdscr->v / stdscr->w + 1,
                        stdscr->v % stdscr->w + 1);
    else
        phy_move_cursor(stdscr->h, stdscr->w);
    /* Swap virtual screens */
    t = stdscr->cs;
    stdscr->cs = stdscr->ns;
    stdscr->ns = t;
    return 0;
}

/* Raw getch */
#ifdef _WIN32
#define getch_raw() _getch()
#else
#define getch_raw() getchar()
#endif

/* Buffered getch with no key interpretation */
#define getch_nk() (stdscr->input->i ? *(stdscr->input->a + --stdscr->input->i) \
    : getch_raw())

int getch(void)
{
    /* Process multi-char keys */
#ifdef _WIN32
    int x;
    if ((x = getch_nk()) != 0xE0)
        return x;
    switch (x = getch_nk()) {
    case 'G':
        return KEY_HOME;
    case 'H':
        return KEY_UP;
    case 'K':
        return KEY_LEFT;
    case 'M':
        return KEY_RIGHT;
    case 'O':
        return KEY_END;
    case 'P':
        return KEY_DOWN;
    case 'S':
        return KEY_DC;
    default:
        if (ungetch(x))
            return EOF;
        return 0xE0;
    }
#else
    int x, z;
    if ((x = getch_nk()) != ESC)
        return x;
    if ((x = getch_nk()) != '[') {
        if (ungetch(x))
            return EOF;
        return ESC;
    }
    x = getch_nk();
    if (x != 'A' && x != 'B' && x != 'C' && x != 'D' && x != 'F'
        && x != 'H' && x != '1' && x != '3' && x != '4') {
        if (ungetch(x))
            return EOF;
        if (ungetch('['))
            return EOF;
        return ESC;
    }
    switch (x) {
    case 'A':
        return KEY_UP;
    case 'B':
        return KEY_DOWN;
    case 'C':
        return KEY_RIGHT;
    case 'D':
        return KEY_LEFT;
    }
    if ((z = getch_nk()) != '~') {
        if (ungetch(z))
            return EOF;
        if (ungetch(x))
            return EOF;
        if (ungetch('['))
            return EOF;
        return ESC;
    }
    switch (x) {
    case '1':
        return KEY_HOME;
    case '3':
        return KEY_DC;
    case '4':
        return KEY_END;
    }
    return EOF;
#endif
}
