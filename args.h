/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2009 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
\* ========================================================================= */

#ifndef _ARGS_H
#define _ARGS_H 1

typedef struct
{
	int argc;
	char **argv;
} term_arglist;

int argcount (const char *);
term_arglist *new_args (void);
term_arglist *make_args (const char *);
void add_args (term_arglist *, const char *);
void destroy_args (term_arglist *);

#endif
