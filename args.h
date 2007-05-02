/* ==============================================================================
 * cish - the cisco shell emulator for LRP
 *
 * (C) 2000 Mad Science Labs / Clue Consultancy
 * This program is licensed under the GNU General Public License
 * ============================================================================== */

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
