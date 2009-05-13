/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2009 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
\* ========================================================================= */

#include "args.h"
#include <string.h>
#include <stdlib.h>

#define isspace(q) ((q==' ')||(q=='\t'))

/* ------------------------------------------------------------------------- *\
 * FUNCTION findspace (src)                                                  *
 * ------------------------                                                  *
 * Finds the first whitespace (tab or space) in the given string.            *
\* ------------------------------------------------------------------------- */
inline char *findspace (char *src)
{
	register char *t1;
	register char *t2;
	
	t1 = strchr (src, ' ');
	t2 = strchr (src, '\t');
	if (t1 && t2)
	{
		if (t1<t2) t2 = NULL;
		else t1 = NULL;
	}
	
	if (t1) return t1;
	return t2;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION argcount (string)                                                *
 * --------------------------                                                *
 * Counts the number of arguments (islands of text separated by any amount   *
 * of whitespace) there are in a given string.                               *
\* ------------------------------------------------------------------------- */
int argcount (const char *string)
{
	int ln;
	int cnt;
	int i;
	
	cnt = 1;
	ln = strlen (string);
	i = 0;

	if (i == ln) cnt = 0;
	
	while (isspace(string[i])) ++i;
	
	while (i < ln)
	{
		if (isspace(string[i]))
		{
			while (isspace(string[i])) ++i;
			if (string[i]) ++cnt;
		}
		++i;
	}
	
	return cnt;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION new_args (void)                                                  *
 * ------------------------                                                  *
 * Allocates and initializes a new arglist structure.                        *
\* ------------------------------------------------------------------------- */
term_arglist *new_args (void)
{
	term_arglist *res;
	
	res = (term_arglist *) malloc (sizeof (term_arglist));
	res->argc = 0;
	res->argv = NULL;
	return res;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION add_args (arglist, element)                                      *
 * ------------------------------------                                      *
 * Adds a 'word' element to an arglist structure.                            *
\* ------------------------------------------------------------------------- */
void add_args (term_arglist *arg, const char *elm)
{
	if (arg->argc)
	{
		arg->argv = (char **) realloc (arg->argv, (arg->argc + 1) * sizeof (char *));
		arg->argv[arg->argc++] = strdup (elm);
	}
	else
	{
		arg->argc = 1;
		arg->argv = (char **) malloc (sizeof (char *));
		arg->argv[0] = strdup (elm);
	}
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION make_args (string)                                               *
 * ---------------------------                                               *
 * Splits the provided string into an arglist structure containing an array  *
 * of individual words (islands of text, yadda yadda).                       *
\* ------------------------------------------------------------------------- */
term_arglist *make_args (const char *string)
{
	term_arglist *result;
	char		 *rightbound;
	char		 *word;
	char		 *crsr;
	int			  count;
	int			  pos;
	
	result = (term_arglist *) malloc (sizeof (term_arglist));
	count = argcount (string);
	result->argc = count;
	result->argv = (char **) malloc (count * sizeof (char *));
	
	crsr = (char *) string;
	pos = 0;
	
	while (isspace (*crsr)) ++crsr;
	
	while ((rightbound = findspace (crsr)))
	{
		word = (char *) malloc ((rightbound-crsr+3) * sizeof (char));
		memcpy (word, crsr, rightbound-crsr);
		word[rightbound-crsr] = 0;
		result->argv[pos++] = word;
		crsr = rightbound;
		while (isspace(*crsr)) ++crsr;
	}
	if (*crsr)
	{
		word = strdup (crsr);
		result->argv[pos++] = word;
	}
	
	return result;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION destroy_args (arglist)                                           *
 * -------------------------------                                           *
 * Deallocate an arglist structure and the horse it rode in on.              *
\* ------------------------------------------------------------------------- */
void destroy_args (term_arglist *lst)
{
	int i;
	for (i=0;i<lst->argc;++i) free (lst->argv[i]);
	free (lst->argv);
	free (lst);
}
