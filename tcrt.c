/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2004 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
 * ------------------------------------------------------------------------- *
 * Terminus Commandline Handler                                              *
 * ------------------------------------------------------------------------- *
 * This file implements the core of terminus, including terminus_readline()  *
 * and the VT100 control of text output.                                     *
\* ========================================================================= */

#include <tcrt.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>

/* Import from tgetchar.c */
extern void setupterm (int);
extern void restoreterm (int);

/* The global event-text pipe */
extern int FEV;

/* Don't ask :( */
#define STDOUT 0

/* Prototype */
void terminus_updatecrt (termbuf *tb);

struct tbuf_struct
{
	char buffer[16384];
	int  position;
} TBUF; /* line buffer for event text */


/* ------------------------------------------------------------------------- *\
 * FUNCTION tbuf_init (void)                                                 *
 * -------------------------                                                 *
 * Initializes the line buffer.                                              *
\* ------------------------------------------------------------------------- */

void tbuf_init (void)
{
	TBUF.position = 0;
	TBUF.buffer[0] = 0;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION tbuf_read (fileno)                                               *
 * ---------------------------                                               *
 * This function is called if a select() on the event pipe yielded a positiv *
 * result. The data is added to the line buffer. At a later point, the       *
 * tbuf_poll() and tbuf_gets() functions can be used to extract text lines   *
 * out of this buffer. Note that we're necessarily reimplementing stdio here *
 * because we need a reliable way to use select() and use buffered lines     *
 * at the same time.                                                         *
\* ------------------------------------------------------------------------- */

void tbuf_read (int fno) /* fill the line buffer */
{
	size_t sz;
	if ( (TBUF.position+1024) >= 16384 ) return;
	
	sz = read (fno, TBUF.buffer+TBUF.position, 1024);
	if (sz>0)
	{
		TBUF.position += sz;
		TBUF.buffer[TBUF.position] = 0;
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION tbuf_poll (void)                                                 *
 * -------------------------                                                 *
 * Returns a 1 if there is a complete line (terminated by a newline) inside  *
 * the text buffer.                                                          *
\* ------------------------------------------------------------------------- */

int tbuf_poll (void)
{
	if (strchr (TBUF.buffer, '\n')) return 1;
	return 0;
}

/* ------------------------------------------------------------------------- *\
 * FUCTION tbuf_gets (into, maxsize)                                         *
 * ---------------------------------                                         *
 * Reads a line from the text buffer (terminated by a newline characte).     *
 * Returns 0 on failure (no complete line in buffer) or the size of the      *
 * line (including its newline) as it was copied on success. If the          *
 * line is longer than the provided maximum, the remaining part will         *
 * be left in the buffer.                                                    *
\* ------------------------------------------------------------------------- */

int tbuf_gets (char *into, int maxsz)
{
	char *crs;
	int   pos;
	
	crs = strchr (TBUF.buffer, '\n');
	if (! crs) return 0;
	
	pos = crs - TBUF.buffer;
	++pos;
	if (pos > maxsz) pos = maxsz;
	memcpy (into, TBUF.buffer, pos);
	if (pos < maxsz) into[pos] = 0;
	if ((TBUF.position - pos) > 0)
	{
		memmove (TBUF.buffer, TBUF.buffer + pos, (TBUF.position - pos)+1);
		TBUF.position -= pos;
	}
	else
	{
		TBUF.position = 0;
		TBUF.buffer[0] = 0;
	}
	return pos;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION tprintf (format, ...)                                            *
 * ------------------------------                                            *
 * A replacement of the stdio printf that stays the hell away from buffered  *
 * i/o. This makes it possible to print output without switching terminal    *
 * modes.                                                                    *
\* ------------------------------------------------------------------------- */

void tprintf (const char *fmt, ...)
{
	va_list ap;
	char buffer[512];
	va_start (ap, fmt);
	vsnprintf (buffer, 512, fmt, ap);
	va_end (ap);
	
	write (1, buffer, strlen (buffer));
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_getchar (termbuf)                                       *
 * -----------------------------------                                       *
 * Waits for a character of input from the tty. If, after one second, the    *
 * lazy user didn't type, also start looking at the event pipe and print     *
 * notifications as they come in.                                            *
\* ------------------------------------------------------------------------- */

int terminus_getchar (termbuf *tb)
{
	fd_set fds;
	struct timeval tv;
	char eventline[512];
	int gotkey = 0;
	int didnewline;
	
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	
	if (! FEV)
	{
		return getchar();
	}
	
	FD_ZERO (&fds);
	FD_SET (0, &fds);
	/* Exclusively look at keystrokes the first second */
	
	while (1)
	{
		while (select(FEV+1, &fds, NULL, NULL, &tv) <= 0)
		{
			FD_ZERO (&fds);
			FD_SET (0, &fds);
			FD_SET (FEV, &fds);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
		}
		if (FD_ISSET (FEV, &fds))
		{
			didnewline = 0;
			
			do
			{
				tbuf_read (FEV);
				while (tbuf_poll())
				{
					if (! didnewline)
					{
						didnewline = 1;
						tprintf ("\n");
					}
					
					if (tbuf_gets (eventline, 511))
						tprintf ("%s", eventline);
				}
				FD_ZERO (&fds);
				FD_SET (FEV, &fds);
				tv.tv_sec = 0;
				tv.tv_usec = 0;
			} while (select(FEV+1, &fds, NULL, NULL, &tv) > 0);
			
			tb->rdpos = -1;
			terminus_updatecrt (tb);
			FD_ZERO (&fds);
			FD_SET (0, &fds);
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			select (FEV+1, &fds, NULL, NULL, &tv);
		}
		if (FD_ISSET (0, &fds))
		{
			/* abusing eventline here as a temp buffer, sorry for
			   the amiguous use */
			read (0, eventline, 1);
			return eventline[0];
		}
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_readline (termbuf, prompt)                              *
 * --------------------------------------------                              *
 * Takes care of keyboard input to input a line of text, using               *
 * any keyhandlers configured.                                               *
\* ------------------------------------------------------------------------- */

const char *terminus_readline (termbuf *tb, const char *prompt)
{
	int tmp, tmp2, tmp3;
	int fix1, handled, i;
	char *expn;
	keydef *kdef;
	char eventline[512];
	
	strcpy (tb->buffer, prompt);
	tb->promptsz = strlen (prompt);
	tb->crsr = tb->promptsz;
	tb->len = tb->promptsz;
	tb->ocrsr = 0;
	tb->owcrsr = 0;
	tb->smaxpos = 0;
	tb->wcrsr = 0;
	tb->historycrsr = tb->historypos;
	
	tb->rdpos = -1;
		
	tprintf ("%s", prompt); 
	terminus_updatecrt (tb);
	
	/*setupterm (STDOUT);*/
	
	while ((tmp = terminus_getchar(tb)) != '\n')
	{
		if (tmp == 1)
		{
			if (tb->wcrsr) fix1 = 0;
			else fix1 = 1;
			
			terminus_crhome (tb);
		}
		else if (tmp == 5)
		{
			terminus_crend (tb);
		}
		else if ((tmp == 127)||(tmp == 8))
		{
			terminus_backspace (tb);
		}
		else if ((tmp == 21))
		{
			while (tb->crsr > tb->promptsz)
				terminus_backspace (tb);
		}
		else if (tmp == 27)
		{
			tmp2 = terminus_getchar(tb);
			if (tmp2 == '[')
			{
				tmp3 = terminus_getchar(tb);
				if (tmp3 == 'C') terminus_cright (tb);
				else if (tmp3 == 'D') terminus_crleft (tb);
				else if (tmp3 == 'A') terminus_crup (tb);
				else if (tmp3 == 'B') terminus_crdown (tb);
				else
				{
					terminus_insert (tb, '^');
					terminus_updatecrt (tb);
					terminus_insert (tb, '[');
					terminus_updatecrt (tb);
					terminus_insert (tb, '[');
					terminus_updatecrt (tb);
					terminus_insert (tb, tmp3);
				}
			}
			else
			{
				terminus_insert (tb, '^');
				terminus_updatecrt (tb);
				terminus_insert (tb, '[');
				terminus_updatecrt (tb);
				terminus_insert (tb, tmp2);
			}
		}
		else
		{
			handled = 0;
			kdef = tb->first;
			while ((!handled) && (kdef))
			{
				if (kdef->key == tmp)
				{
					expn = kdef->handler (tb->buffer + tb->promptsz, tb->crsr);
					if (! expn)
					{
						tb->rdpos = -1;
						terminus_updatecrt (tb);
					}
					else
					{
						for (i=0; expn[i]; ++i)
						{
							if (expn[i] == 21)
							{
								while (tb->crsr > tb->promptsz)
								{
									terminus_backspace (tb);
								}
								terminus_updatecrt(tb);
							}
							else if (expn[i] == '\n')
							{
								return tb->buffer + tb->promptsz;
							}
							else
							{
								terminus_insert (tb, expn[i]);
								terminus_updatecrt (tb);
							}
						}
					}
					handled = 1;
				}
				else kdef = kdef->next;
			}
			
			if (!handled)
				terminus_insert (tb, tmp);
		}
		terminus_updatecrt (tb);
	}

	/* If there's already a history reading in the slot, use it */
	if (tb->history[tb->historypos])
	{
		free (tb->history[tb->historypos]);
	}
	
	/* Fill the history slot */
	tb->history[tb->historypos] = strdup (tb->buffer + tb->promptsz);
	tb->historypos++;
	
	/* Wrap the slot position if needed */
	if (tb->historypos >= HISTORY_SIZE)
		tb->historypos = 0;
	
	
	/* restoreterm (STDOUT); */
	return tb->buffer + tb->promptsz;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION terminus_updatecert (termbuf)                                    *
 * --------------------------------------                                    *
 * Syncs whatever we did to the termbuf to what should be visible on         *
 * the screen.                                                               *
\* ------------------------------------------------------------------------- */

void terminus_updatecrt (termbuf *tb)
{
	int xcrsr, xrdpos, xc;
	
	if (tb->rdpos < 0)
	{
		tprintf ("%c[1G", 27);
		for (xc=0; xc < tb->wsize-1; ++xc)
		{
			if ((xc + tb->wcrsr) >= tb->len)
				tprintf (" ");
			else
			{
				tb->smaxpos = xc;
				tprintf ("%c", tb->buffer[xc + tb->wcrsr]);
			}
		}
		xcrsr = tb->crsr - tb->wcrsr;
		if (tb->crsr > tb->len)
			xcrsr = tb->len - tb->wcrsr;
			
		if (xcrsr < xc)
		{
			if ((xc - xcrsr) == 1) tprintf ("%c[D", 27);
			else tprintf ("%c[%iG", 27, xcrsr+1);
		}
		
		return;
	}
	
	xcrsr = tb->crsr - tb->wcrsr;
		
	if ( (tb->owcrsr == tb->wcrsr) && ((xcrsr - tb->ocrsr)==1) && (tb->crsr == tb->len) )
	{
		tprintf ("%c", tb->buffer[tb->crsr -1]);
		tb->smaxpos++;
	}
	
	else
	{
		if (xcrsr != (tb->ocrsr - tb->owcrsr))
		{
			if (xcrsr>2) tprintf ("%c[%iG", 27, xcrsr+1);
			else if (xcrsr > 1) tprintf ("%c[1G%c[C", 27, 27);
			else tprintf ("%c[1G", 27);
		}

		if (tb->wcrsr != tb->owcrsr) tb->rdpos = tb->wcrsr;

		xrdpos = tb->rdpos - tb->wcrsr;
		xc = xrdpos;

		if ((tb->rdpos>=0) &&(xc >= 0))
		{
			if (tb->crsr == tb->len == (tb->rdpos +1))
			{
				if (tb->crsr > 0)
					tprintf ("%c",tb->buffer[tb->crsr -1]);

				tb->ocrsr = xcrsr;
				tb->owcrsr = tb->wcrsr;
				return;
			}

			if (xc != xcrsr)
			{
				if (xc>1)
					tprintf ("%c[%iG", 27, xc+1);
				else if (xc)
					tprintf ("%c[1G%c[C", 27, 27);
				else
					tprintf ("%c[1G", 27);
			}
			for (;xc < (tb->wsize -1); ++xc)
			{
				if ((xc + tb->wcrsr) >= tb->len)
				{
					if (xc > tb->smaxpos) break;
					tprintf (" ");
				}
				else
				{
					tb->smaxpos = xc;
					tprintf ("%c", tb->buffer[xc + tb->wcrsr]);
				}
			}
			if ((xc - xcrsr)>1) tprintf ("%c[%iG", 27, xcrsr+1);
			else if (xc - xcrsr) tprintf ("%c[D", 27);
		}
	}
	
	tb->ocrsr = xcrsr;
	tb->owcrsr = tb->wcrsr;
}
