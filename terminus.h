/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2009 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
\* ========================================================================= */

#ifndef _TERMINUS_H
#define _TERMINUS_H 1

#define HISTORY_SIZE 32

typedef char *(*expnfunc)(const char *, int);
typedef struct keydef
{
	struct keydef	*next;
	struct keydef	*prev;
	int				 key;
	expnfunc		 handler;
} keydef;

extern void tprintf (const char *, ...);

typedef struct
{
	int			 size;
	int			 wsize;
	int			 len;
	int			 crsr;
	int			 ocrsr;
	int			 owcrsr;
	int			 wcrsr;
	int			 rdpos; /* absolute position from which point text must be redrawn */
	int			 smaxpos; /* rightmost absolute position that is not a whitespace */
	char		*buffer;
	int			 promptsz;
	keydef		*first;
	keydef		*last;
	
	char		*history[HISTORY_SIZE];
	int			 historypos;
	int			 historycrsr;
} termbuf;

void terminus_on (void);
void terminus_off (void);
int terminus_more (const char *);
termbuf *init_termbuf (int, int);
void terminus_insert (termbuf *, char);
void terminus_backspace (termbuf *);
void terminus_crleft (termbuf *);
void terminus_cright (termbuf *);
void terminus_crhome (termbuf *);
void terminus_crend  (termbuf *);
void terminus_crup   (termbuf *);
void terminus_crdown (termbuf *);
int  terminus_getkey (void);

void terminus_add_handler (termbuf *, int, expnfunc);

const char *lastword (const char *);
char *terminus_builtin_tab (const char *, int);

#endif
