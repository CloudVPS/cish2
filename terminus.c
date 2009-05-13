/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2009 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
 * ------------------------------------------------------------------------- *
 * Terminus Commandline Handler                                              *
 * ------------------------------------------------------------------------- *
 * This file implements most of the elementary manipulations of the          *
 * termbuffer as they are triggered by keypresses form the user.             *
\* ========================================================================= */

#include "terminus.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>

#define STDIN 0

extern void setupterm (int);
extern void restoreterm (int);

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_on (void)                                               *
 * ---------------------------                                               *
 * Take over the terminal. Hack the planet!                                  *
\* ------------------------------------------------------------------------- */
void terminus_on (void)
{
	const char *TERM;
	setupterm (STDIN);
	
	/* We hunt for TERM=xterm most probably in a vain attempt to explicitly
	   tell it vt100 is spoken here. This extra escape was introduced when
	   hunting for ill behavior of gnome-terminal which was ultimately
	   caused by its misinterpretation of the CSI-Pn-G sequence in case of
	   a missing parameter Pn. Leaving this escape in does no harm for
	   tested implementations of the xterm family and it may poke yet
	   unknown breeds into submission in the distant future */
	   
	TERM = getenv ("TERM");
	if (! TERM) return;
	if (! strcmp (TERM, "xterm"))
	{
		tprintf ("\033 F");
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_off (void)                                              *
 * ----------------------------                                              *
 * Give our precious resources back to the Man.                              *
\* ------------------------------------------------------------------------- */
void terminus_off (void)
{
	restoreterm (STDIN);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION init_termbuf (buffersize, windowsize)                            *
 * ----------------------------------------------                            *
 * Sets up a new termbuf structure for reading a string of the provided      *
 * buffersize, at a window width as also provided by the arguments.          *
\* ------------------------------------------------------------------------- */
termbuf *init_termbuf (int size, int _wsize)
{
	termbuf *tb;
	struct winsize sz;
	int wsize;
	int i;
	
	wsize = _wsize;
	
	if (!wsize)
	{
		ioctl (STDIN, TIOCGWINSZ, (char *)&sz);
		wsize = sz.ws_col;
	}
	
	tb = (termbuf *) malloc (sizeof (termbuf));
	if (! tb)
	{
		fprintf (stderr, "%% terminus malloc() error\n");
		return NULL;
	}
	
	tb->size = size;
	tb->wsize = wsize;
	tb->crsr = 0;
	tb->ocrsr = 0;
	tb->owcrsr = 0;
	tb->wcrsr = 0;
	tb->promptsz = 0;
	tb->first = NULL;
	tb->last = NULL;
	
	for (i=0; i<HISTORY_SIZE; ++i)
	{
		tb->history[i] = NULL;
	}
	tb->historypos = 0;
	tb->historycrsr = 0;
		
	tb->buffer = (char *) malloc (size * sizeof (char));
	if (! tb->buffer)
	{
		fprintf (stderr, "%% terminus malloc() error\n");
		free (tb);
		return NULL;
	}
	
	tb->buffer[0] = 0;
	tb->len = 0;
	tb->rdpos = -1;
	
	terminus_add_handler (tb, 9, terminus_builtin_tab);
	
	return tb;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION lastword (string)                                                *
 * --------------------------                                                *
 * Returns a pointer to the last word of a string of text.                   *
\* ------------------------------------------------------------------------- */
const char *lastword (const char *buf)
{
	const char *res;
	const char *crsr = buf;
	res = crsr;
	
	while (crsr = strchr (res, ' '))
	{
		crsr++;
		res = crsr;
	}
	return res;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_builtin_tab (buffer, cpos)                              *
 * --------------------------------------------                              *
 * Stupid builtin handler, please do not tap the glass.                      *
\* ------------------------------------------------------------------------- */
char *terminus_builtin_tab (const char *buf, int cpos)
{
	const char *term;
	char *res;
	if (cpos == strlen (buf))
	{
		term = lastword (buf);
		if (strncmp (term, "term", 4) == 0)
		{
			res = (char *) malloc (6 * sizeof (char));
			strcpy (res, "inus");
			return res;
		}
	}
	return NULL;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_add_handler (termbuf, key, function)                    *
 * ------------------------------------------------------                    *
 * Adds a new keyhandler for the current termbuf context.                    *
\* ------------------------------------------------------------------------- */
void terminus_add_handler (termbuf *tb, int key, expnfunc func)
{
	keydef *nkeydef;
	keydef *k = tb->first;
	while (k)
	{
		if (k->key == key)
		{
			k->handler = func;
			return;
		}
		k = k->next;
	}
	
	nkeydef = (keydef *) malloc (sizeof (keydef));
	nkeydef->key = key;
	nkeydef->handler = func;
	
	nkeydef->next = NULL;
	if ((nkeydef->prev = tb->last))
	{
		tb->last->next = nkeydef;
		tb->last = nkeydef;
	}
	else
	{
		tb->last = nkeydef;
		tb->first = nkeydef;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_insert (termbuf, thechar)                               *
 * -------------------------------------------                               *
 * Inserts a new character at the current position of the termbuf.           *
\* ------------------------------------------------------------------------- */
void terminus_insert (termbuf *tb, char c)
{
	char *ptr;
	
	if (tb->len >= tb->size) return;
	
	if (tb->crsr == tb->len)
	{
		tb->buffer[tb->len] = c;
		tb->len++;
		tb->buffer[tb->len] = 0;
		tb->rdpos = tb->crsr;
		tb->crsr++;
	}
	else
	{
		memmove (tb->buffer + tb->crsr+1, tb->buffer + tb->crsr,
				 tb->len - tb->crsr);
				 
		tb->len++;
		tb->rdpos = tb->crsr;
		tb->buffer[tb->crsr++] = c;
	}
	
	if ( (tb->crsr - tb->wcrsr) > (tb->wsize - 2) )
	{
		ptr = strchr (tb->buffer + tb->wcrsr, ' ');

		if (ptr && ( (ptr - (tb->buffer + tb->wcrsr)) < 32))
		{
			tb->wcrsr = (ptr - tb->buffer)+1;
		}
		else
		{
			tb->wcrsr += 16;
		}
		tb->rdpos = tb->wcrsr;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_backspace (termbuf)                                     *
 * -------------------------------------                                     *
 * Handler for the backspace operation. Erases the character to the left of  *
 * the cursor and moves all text behind it one position to the left.         *
\* ------------------------------------------------------------------------- */
void terminus_backspace (termbuf *tb)
{
	char *ptr;
	
	if (tb->crsr <= tb->promptsz) return;
	
	if (tb->crsr == tb->len)
	{
		tb->buffer[tb->crsr -1] = 0;
		tb->len--;
		tb->crsr--;
		tb->rdpos = tb->crsr;
	}
	else
	{
		memmove (tb->buffer + tb->crsr-1, tb->buffer + tb->crsr,
				 tb->len - tb->crsr);
				 
		tb->len--;
		tb->crsr--;
		tb->rdpos = tb->crsr-1;
		tb->buffer[tb->len] = 0;
	}
	
	if ( tb->crsr < tb->wcrsr )
	{
		tb->wcrsr = tb->crsr - (tb->wsize - 6);
		if (tb->wcrsr < 0) tb->wcrsr = 0;
		tb->rdpos = tb->wcrsr;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_crup (termbuf)                                          *
 * --------------------------------                                          *
 * Handles the 'cursor up' key, going back through a history buffer.         *
\* ------------------------------------------------------------------------- */
void terminus_crup (termbuf *tb)
{
	if (tb->historycrsr == tb->historypos)
	{
		if (tb->history[tb->historypos] != NULL)
			free (tb->history[tb->historypos]);
			
		tb->history[tb->historypos] = strdup (tb->buffer + tb->promptsz);
	}
	tb->historycrsr--;
	if (tb->historycrsr<0) tb->historycrsr = HISTORY_SIZE-1;

	if (tb->history[tb->historycrsr] == NULL)
	{
		tb->historycrsr++;
		if (tb->historycrsr >= HISTORY_SIZE) tb->historycrsr = 0;
		return;
	}

	tb->crsr = tb->promptsz;
	tb->len = tb->promptsz;
	tb->rdpos = tb->promptsz;
	tb->buffer[tb->promptsz] = 0;
	strcat (tb->buffer, tb->history[tb->historycrsr]);
	tb->len += strlen (tb->history[tb->historycrsr]);
	terminus_crend (tb);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_crdown (termbuf)                                        *
 * ----------------------------------                                        *
 * Handles the 'cursor down' key, going forwards in the history buffer where *
 * we want backwards before.                                                 *
\* ------------------------------------------------------------------------- */
void terminus_crdown (termbuf *tb)
{
	if (tb->historypos == tb->historycrsr) return;
	
	tb->historycrsr++;
	if (tb->historycrsr >= HISTORY_SIZE) tb->historycrsr = 0;

	if (tb->history[tb->historycrsr] == NULL)
	{
		tb->historycrsr--;
		if (tb->historycrsr < 0) tb->historycrsr = HISTORY_SIZE-1;
		return;
	}

	tb->crsr = tb->promptsz;
	tb->len = tb->promptsz;
	tb->rdpos = tb->promptsz;
	tb->buffer[tb->promptsz] = 0;
	strcat (tb->buffer, tb->history[tb->historycrsr]);
	tb->len += strlen (tb->history[tb->historycrsr]);
	terminus_crend (tb);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_crleft (termbuf)                                        *
 * ----------------------------------                                        *
 * Handles cursor movement to the left, if possible.                         *
\* ------------------------------------------------------------------------- */
void terminus_crleft (termbuf *tb)
{
	if (tb->crsr > tb->promptsz)
	{
		tb->crsr--;
	}
	else
	{
		return;
	}

	tb->rdpos = -1;

	if ( tb->crsr < tb->wcrsr )
	{
		tb->wcrsr = tb->crsr - (tb->wsize - 6);
		if (tb->wcrsr < 0) tb->wcrsr = 0;
		tb->rdpos = tb->wcrsr;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_cright (termbuf)                                        *
 * ----------------------------------                                        *
 * Handles cursor movement to the right, if possible.                        *
\* ------------------------------------------------------------------------- */
void terminus_cright (termbuf *tb)
{
	char *ptr;
	
	if (tb->crsr < tb->len)
	{
		tb->crsr++;
	}
	else
	{
		tprintf ("%c", 7);
		return;
	}
	
	tb->rdpos = -1;

	if ( (tb->crsr - tb->wcrsr) > (tb->wsize - 2) )
	{
		ptr = strchr (tb->buffer + tb->wcrsr, ' ');

		if (ptr && ( (ptr - (tb->buffer + tb->wcrsr)) < 32))
		{
			tb->wcrsr = (ptr - tb->buffer)+1;
		}
		else
		{
			tb->wcrsr += 16;
		}
		tb->rdpos = tb->wcrsr;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_crhome (termbuf)                                        *
 * ----------------------------------                                        *
 * Handling for the ^A key, moves the cursor to the beginning of the line.   *
\* ------------------------------------------------------------------------- */
void terminus_crhome (termbuf *tb)
{
	if (tb->crsr >= tb->promptsz)
	{
		tb->crsr = tb->promptsz;
		tb->rdpos = 0;
	}
	
	if (tb->wcrsr)
	{
		tb->wcrsr = 0;
		tb->rdpos = 0;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_crend (termbuf)                                         *
 * ---------------------------------                                         *
 * Handling for the ^E key, moves the cursor to the end of the line.         *
\* ------------------------------------------------------------------------- */
void terminus_crend (termbuf *tb)
{
	int nwcrsr;
	char *tmp;
	
	tb->crsr = tb->len;
	nwcrsr = tb->crsr - (tb->wsize - 4);
	if (nwcrsr < 0) nwcrsr = 0;
	else
	{
		tmp = strchr (tb->buffer + nwcrsr, ' ');

		if ( ((tmp - tb->buffer) - nwcrsr) < 16)
		{
			nwcrsr = (tmp+1) - tb->buffer;
		}
	}
	
	if (nwcrsr != tb->wcrsr) tb->rdpos = nwcrsr;
	else tb->rdpos = -1;
	tb->wcrsr = nwcrsr;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_getkey (void)                                           *
 * -------------------------------                                           *
 * Sort of obsoleted, now just the gateway drug to getchar().                *
\* ------------------------------------------------------------------------- */
int terminus_getkey (void)
{
	int res;
/*	setupterm (STDIN); */
	res = getchar();
/*	restoreterm (STDIN); */
	return res;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_more (prompt)                                           *
 * -------------------------------                                           *
 * Implements a 'more' prompt for a pager. Returns 1 if the user responded   *
 * with the 'q' key, 0 if the user responded with another key. Keeps         *
 * waiting for input in other scenarios.                                     *
\* ------------------------------------------------------------------------- */
int terminus_more (const char *prompt)
{
	int c;
	int ln;
	int res = 0;
	
	tprintf (" \033[1m%s\033[0m ", prompt);
	
	if (terminus_getkey() == 'q')
	{
		res = 1;
	}
	
	ln = strlen (prompt) +2;
	for (c=0; c<ln; ++c)
	{
		tprintf ("\033[D \033[D");
	}
	
	return res;
}
