/*  SCCS Id: @(#)topl.c 3.4 1996/10/24  */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#ifdef TTY_GRAPHICS

#include "tcap.h"
#include "wintty.h"
#include <ctype.h>

#ifndef C   /* this matches src/cmd.c */
#define C(c)    (0x1f & (c))
#endif

static void redotoplin(const char*);
static void topl_putsym(char);
static void removetopl(int);

int
tty_doprev_message(void)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    winid prevmsg_win;
    int i;
    if ((iflags.prevmsg_window != 's') && !ttyDisplay->inread) { /* not single */
        if(iflags.prevmsg_window == 'f') { /* full */
            prevmsg_win = create_nhwindow(NHW_MENU);
            putstr(prevmsg_win, 0, "Message History");
            putstr(prevmsg_win, 0, "");
            cw->maxcol = cw->maxrow;
            i = cw->maxcol;
            do {
                if(cw->data[i] && strcmp(cw->data[i], "") )
                    putstr(prevmsg_win, 0, cw->data[i]);
                i = (i + 1) % cw->rows;
            } while (i != cw->maxcol);
            putstr(prevmsg_win, 0, toplines);
            display_nhwindow(prevmsg_win, TRUE);
            destroy_nhwindow(prevmsg_win);
        } else if (iflags.prevmsg_window == 'c') {      /* combination */
            do {
                morc = 0;
                if (cw->maxcol == cw->maxrow) {
                    ttyDisplay->dismiss_more = C('p');  /* <ctrl/P> allowed at --More-- */
                    redotoplin(toplines);
                    cw->maxcol--;
                    if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
                    if (!cw->data[cw->maxcol])
                        cw->maxcol = cw->maxrow;
                } else if (cw->maxcol == (cw->maxrow - 1)) {
                    ttyDisplay->dismiss_more = C('p');  /* <ctrl/P> allowed at --More-- */
                    redotoplin(cw->data[cw->maxcol]);
                    cw->maxcol--;
                    if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
                    if (!cw->data[cw->maxcol])
                        cw->maxcol = cw->maxrow;
                } else {
                    prevmsg_win = create_nhwindow(NHW_MENU);
                    putstr(prevmsg_win, 0, "Message History");
                    putstr(prevmsg_win, 0, "");
                    cw->maxcol = cw->maxrow;
                    i = cw->maxcol;
                    do {
                        if(cw->data[i] && strcmp(cw->data[i], "") )
                            putstr(prevmsg_win, 0, cw->data[i]);
                        i = (i + 1) % cw->rows;
                    } while (i != cw->maxcol);
                    putstr(prevmsg_win, 0, toplines);
                    display_nhwindow(prevmsg_win, TRUE);
                    destroy_nhwindow(prevmsg_win);
                }

            } while (morc == C('p'));
            ttyDisplay->dismiss_more = 0;
        } else { /* reversed */
            morc = 0;
            prevmsg_win = create_nhwindow(NHW_MENU);
            putstr(prevmsg_win, 0, "Message History");
            putstr(prevmsg_win, 0, "");
            putstr(prevmsg_win, 0, toplines);
            cw->maxcol=cw->maxrow-1;
            if(cw->maxcol < 0) cw->maxcol = cw->rows-1;
            do {
                putstr(prevmsg_win, 0, cw->data[cw->maxcol]);
                cw->maxcol--;
                if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
                if (!cw->data[cw->maxcol])
                    cw->maxcol = cw->maxrow;
            } while (cw->maxcol != cw->maxrow);

            display_nhwindow(prevmsg_win, TRUE);
            destroy_nhwindow(prevmsg_win);
            cw->maxcol = cw->maxrow;
            ttyDisplay->dismiss_more = 0;
        }
    } else if(iflags.prevmsg_window == 's') { /* single */
        ttyDisplay->dismiss_more = C('p');  /* <ctrl/P> allowed at --More-- */
        do {
            morc = 0;
            if (cw->maxcol == cw->maxrow)
                redotoplin(toplines);
            else if (cw->data[cw->maxcol])
                redotoplin(cw->data[cw->maxcol]);
            cw->maxcol--;
            if (cw->maxcol < 0) cw->maxcol = cw->rows-1;
            if (!cw->data[cw->maxcol])
                cw->maxcol = cw->maxrow;
        } while (morc == C('p'));
        ttyDisplay->dismiss_more = 0;
    }
    return 0;
}

static void
redotoplin(const char *str)
{
    int otoplin = ttyDisplay->toplin;
    home();
    if(*str & 0x80) {
        /* kludge for the / command, the only time we ever want a */
        /* graphics character on the top line */
        g_putch((int)*str++);
        ttyDisplay->curx++;
    }
    end_glyphout(); /* in case message printed during graphics output */
    putsyms(str);
    cl_end();
    ttyDisplay->toplin = 1;
    if(ttyDisplay->cury && otoplin != 3)
        more();
}

void
show_topl(const char *str)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    /* show if either STOP isn't set or current message specifies NOSTOP */
    if ((cw->flags & (WIN_STOP | WIN_NOSTOP)) != WIN_STOP) {
        /* NOSTOP cancels persistent STOP and is a one-shot operation;
           force both to be cleared (no-op for either bit that isn't set) */
        cw->flags &= ~(WIN_STOP | WIN_NOSTOP);

        if (ttyDisplay->cury && ttyDisplay->toplin == TOPLINE_NON_EMPTY) {
            tty_clear_nhwindow(WIN_MESSAGE);
        }

        cw->curx = cw->cury = 0;
        home();
        cl_end();
        addtopl(str);

        if (ttyDisplay->cury && ttyDisplay->toplin != TOPLINE_SPECIAL_PROMPT) {
            ttyDisplay->toplin = TOPLINE_NON_EMPTY;
        }
    }
}

void
remember_topl(void)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];
    int idx = cw->maxrow;
    unsigned len = strlen(toplines) + 1;

    if ((cw->flags & WIN_LOCKHISTORY) || !*toplines) {
        return;
    }

    if (len > (unsigned)cw->datlen[idx]) {
        if (cw->data[idx]) free(cw->data[idx]);
        if (cw->attributes && cw->attributes[idx]) {
            free(cw->attributes[idx]);
        }
        len += (8 - (len & 7)); /* pad up to next multiple of 8 */
        cw->data[idx] = (char *)alloc(len);
        cw->datlen[idx] = (short)len;
    }
    Strcpy(cw->data[idx], toplines);
    *toplines = '\0';
    cw->maxcol = cw->maxrow = (idx + 1) % cw->rows;
}

void
addtopl(const char *s)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    tty_curs(BASE_WINDOW, cw->curx+1, cw->cury);
    putsyms(s);
    cl_end();
    ttyDisplay->toplin = 1;
}

void
more(void)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    /* avoid recursion -- only happens from interrupts */
    if(ttyDisplay->inmore++)
        return;

    if(ttyDisplay->toplin) {
        tty_curs(BASE_WINDOW, cw->curx+1, cw->cury);
        if(cw->curx >= CO - 8) topl_putsym('\n');
    }

    if(flags.standout)
        standoutbeg();
    putsyms(defmorestr);
    if(flags.standout)
        standoutend();

    xwaitforspace("\033 ");

    if (morc == '\033') {
        if (!(cw->flags & WIN_NOSTOP)) {
            cw->flags |= WIN_STOP;
        }
    }

    if(ttyDisplay->toplin && cw->cury) {
        docorner(1, cw->cury+1);
        cw->curx = cw->cury = 0;
        home();
    } else if(morc == '\033') {
        cw->curx = cw->cury = 0;
        home();
        cl_end();
    }
    ttyDisplay->toplin = 0;
    ttyDisplay->inmore = 0;
}

void
update_topl(const char *bp)
{
    char *tl, *otl;
    int n0;
    int notdied = 1;
    struct WinDesc *cw = wins[WIN_MESSAGE];
    boolean skip = (cw->flags & (WIN_STOP | WIN_NOSTOP)) == WIN_STOP;

    /* If there is room on the line, print message on same line */
    /* But messages like "You die..." deserve their own line */
    n0 = strlen(bp);
    if ((ttyDisplay->toplin == 1 || (cw->flags & WIN_STOP)) &&
        cw->cury == 0 &&
        n0 + (int)strlen(toplines) + 3 < CO-8 &&  /* room for --More-- */
        (notdied = strncmp(bp, "You die", 7))) {
        Strcat(toplines, "  ");
        Strcat(toplines, bp);
        cw->curx += 2;
        if (!skip) {
            addtopl(bp);
        }

        return;
    } else if (!skip) {
        if (ttyDisplay->toplin == TOPLINE_NEED_MORE) {
            more();
        } else if (cw->cury) { /* for toplin==TOPLINE_NON_EMPTY && cury > 1 */
            docorner(1, cw->cury + 1); /* reset cury = 0 if redraw screen */
            cw->curx = cw->cury = 0;   /* from home--cls() & docorner(1,n,0) */
        }
    }
    remember_topl();
    (void) strncpy(toplines, bp, TBUFSZ);
    toplines[TBUFSZ - 1] = 0;

    for(tl = toplines; n0 >= CO; ) {
        otl = tl;
        for(tl+=CO-1; tl != otl && !isspace(*tl); --tl);
        if(tl == otl) {
            /* Eek!  A huge token.  Try splitting after it. */
            tl = index(otl, ' ');
            if (!tl) break; /* No choice but to spit it out whole. */
        }
        *tl++ = '\n';
        n0 = strlen(tl);
    }
    if(!notdied) cw->flags &= ~WIN_STOP;
    if(!(cw->flags & WIN_STOP)) redotoplin(toplines);
}

static void
topl_putsym(char c)
{
    struct WinDesc *cw = wins[WIN_MESSAGE];

    if(cw == (struct WinDesc *) 0) panic("Putsym window MESSAGE nonexistant");

    switch(c) {
    case '\b':
        if(ttyDisplay->curx == 0 && ttyDisplay->cury > 0)
            tty_curs(BASE_WINDOW, CO, (int)ttyDisplay->cury-1);
        backsp();
        ttyDisplay->curx--;
        cw->curx = ttyDisplay->curx;
        return;
    case '\n':
        cl_end();
        ttyDisplay->curx = 0;
        ttyDisplay->cury++;
        cw->cury = ttyDisplay->cury;
#ifdef WIN32CON
        (void) putchar(c);
#endif
        break;
    default:
        if(ttyDisplay->curx == CO-1)
            topl_putsym('\n'); /* 1 <= curx <= CO; avoid CO */
#ifdef WIN32CON
        (void) putchar(c);
#endif
        ttyDisplay->curx++;
    }
    cw->curx = ttyDisplay->curx;
    if(cw->curx == 0) cl_end();
#ifndef WIN32CON
    (void) putchar(c);
#endif
}

void
putsyms(const char *str)
{
    while(*str)
        topl_putsym(*str++);
}

static void
removetopl(int n)
{
    /* assume addtopl() has been done, so ttyDisplay->toplin is already set */
    while (n-- > 0) putsyms("\b \b");
}

extern char erase_char;     /* from xxxtty.c; don't need kill_char */

char
tty_yn_function(const char *query, const char *resp, char def)
/*
 *   Generic yes/no function. 'def' is the default (returned by space or
 *   return; 'esc' returns 'q', or 'n', or the default, depending on
 *   what's in the string. The 'query' string is printed before the user
 *   is asked about the string.
 *   If resp is NULL, any single character is accepted and returned.
 *   If not-NULL, only characters in it are allowed (exceptions:  the
 *   quitchars are always allowed, and if it contains '#' then digits
 *   are allowed); if it includes an <esc>, anything beyond that won't
 *   be shown in the prompt to the user but will be acceptable as input.
 */
{
    char q;
    char rtmp[40];
    boolean digit_ok, allow_num;
    struct WinDesc *cw = wins[WIN_MESSAGE];
    boolean doprev = 0;
    char prompt[BUFSZ];

    if (ttyDisplay->toplin == TOPLINE_NEED_MORE &&
         (cw->flags & (WIN_STOP | WIN_NOSTOP)) != WIN_STOP) {
        more();
    }
    cw->flags &= ~(WIN_STOP | WIN_NOSTOP);
    ttyDisplay->toplin = TOPLINE_SPECIAL_PROMPT;
    ttyDisplay->inread++;
    if (resp) {
        char *rb, respbuf[QBUFSZ];

        allow_num = (index(resp, '#') != 0);
        Strcpy(respbuf, resp);
        /* any acceptable responses that follow <esc> aren't displayed */
        if ((rb = index(respbuf, '\033')) != 0) *rb = '\0';
        (void) strncpy(prompt, query, QBUFSZ-1);
        prompt[QBUFSZ-1] = '\0';
        Sprintf(eos(prompt), " [%s]", respbuf);
        if (def) {
            Sprintf(eos(prompt), " (%c)", def);
        }
        /* not pline("%s ", prompt);
           trailing space is wanted here in case of reprompt */
        Strcat(prompt, " ");
        pline("%s", prompt);
    } else {
        pline("%s ", query);
        q = readchar();
        goto clean_up;
    }

    do {    /* loop until we get valid input */
        q = lowc(readchar());
        if (q == '\020') { /* ctrl-P */
            if (iflags.prevmsg_window != 's') {
                int sav = ttyDisplay->inread;
                ttyDisplay->inread = 0;
                (void) tty_doprev_message();
                ttyDisplay->inread = sav;
                tty_clear_nhwindow(WIN_MESSAGE);
                cw->maxcol = cw->maxrow;
                addtopl(prompt);
            } else {
                if(!doprev)
                    (void) tty_doprev_message(); /* need two initially */
                (void) tty_doprev_message();
                doprev = 1;
            }
            q = '\0'; /* force another loop iteration */
            continue;
        } else if (doprev) {
            /* BUG[?]: this probably ought to check whether the
               character which has just been read is an acceptable
               response; if so, skip the reprompt and use it. */
            tty_clear_nhwindow(WIN_MESSAGE);
            cw->maxcol = cw->maxrow;
            doprev = 0;
            addtopl(prompt);
            q = '\0'; /* force another loop iteration */
            continue;
        }
        digit_ok = allow_num && digit(q);
        if (q == '\033') {
            if (index(resp, 'q'))
                q = 'q';
            else if (index(resp, 'n'))
                q = 'n';
            else
                q = def;
            break;
        } else if (index(quitchars, q)) {
            q = def;
            break;
        }
        if (!index(resp, q) && !digit_ok) {
            tty_nhbell();
            q = (char)0;
        } else if (q == '#' || digit_ok) {
            char z, digit_string[2];
            int n_len = 0;
            long value = 0;
            addtopl("#"),  n_len++;
            digit_string[1] = '\0';
            if (q != '#') {
                digit_string[0] = q;
                addtopl(digit_string),  n_len++;
                value = q - '0';
                q = '#';
            }
            do { /* loop until we get a non-digit */
                z = lowc(readchar());
                if (digit(z)) {
                    value = (10 * value) + (z - '0');
                    if (value < 0) break; /* overflow: try again */
                    digit_string[0] = z;
                    addtopl(digit_string),  n_len++;
                } else if (z == 'y' || index(quitchars, z)) {
                    if (z == '\033') value = -1; /* abort */
                    z = '\n'; /* break */
                } else if (z == erase_char || z == '\b') {
                    if (n_len <= 1) { value = -1;  break; }
                    else { value /= 10;  removetopl(1),  n_len--; }
                } else {
                    value = -1; /* abort */
                    tty_nhbell();
                    break;
                }
            } while (z != '\n');
            if (value > 0) yn_number = value;
            else if (value == 0) q = 'n';   /* 0 => "no" */
            else { /* remove number from top line, then try again */
                removetopl(n_len),  n_len = 0;
                q = '\0';
            }
        }
    } while(!q);

    if (q != '#') {
        Sprintf(rtmp, "%c", q);
        addtopl(rtmp);
    }
clean_up:
    ttyDisplay->inread--;
    ttyDisplay->toplin = 2;
    if (ttyDisplay->intr) ttyDisplay->intr--;
    if(wins[WIN_MESSAGE]->cury)
        tty_clear_nhwindow(WIN_MESSAGE);

    return q;
}

#endif /* TTY_GRAPHICS */

/*topl.c*/
