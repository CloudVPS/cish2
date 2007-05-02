/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2004 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
\* ========================================================================= */

#include "cmdtree.h"
#include "args.h"
#include "tcrt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

cmdnode *CMD_ROOT; /* the root node of our current context */
int USERLEVEL; /* enable mode on or off */
extern termbuf *GLOBAL_TERMBUF; /* we need this for partial expansion */

/* ------------------------------------------------------------------------- *\
 * FUNCTION destroy_cmdnode (node)                                           *
 * -------------------------------                                           *
 * Deallocates a cmdnode structure's storage and any child nodes the         *
 * structure carries.                                                        *
\* ------------------------------------------------------------------------- */

void destroy_cmdnode (cmdnode *p)
{
	cmdnode *c, *nc;

	if (p->word) free (p->word);
	if (p->trigger) free (p->trigger);
	if (p->help) free (p->help);
	
	c = p->first;
	
	while (c)
	{
		nc = c->next;
		destroy_cmdnode (c);
		c = nc;
	}
	
	free (p);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION reload_treedata (path)                                           *
 * -------------------------------                                           *
 * Deallocates the current tree structure and reads a new structured         *
 * file into CMD_ROOT.                                                       *
\* ------------------------------------------------------------------------- */

char *reload_treedata (const char *path)
{
	destroy_cmdnode (CMD_ROOT);
	return read_treedata (path);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION new_cmdnode (parentnode)                                         *
 * ---------------------------------                                         *
 * Allocates and initializes a cmdnode structure. If the parent argument     *
 * is not NULL, the created node is rooted into the parent structure.        *
\* ------------------------------------------------------------------------- */

cmdnode *new_cmdnode (cmdnode *p)
{
	cmdnode *res;
	
	/* Allocate node and initialize struct members */
	
	res = (cmdnode *) malloc (sizeof (cmdnode));
	res->word = NULL;
	res->level = 0;
	res->trigger = NULL;
	res->help = NULL;
	res->parent = p;
	res->prev = NULL;
	res->next = NULL;
	res->first = NULL;
	res->last = NULL;
	
	/* Link to root node if provided */
	
	if (p)
	{
		if (p->first)
		{
			res->prev = p->last;
			p->last->next = res;
			p->last = res;
		}
		else
		{
			p->first = res;
			p->last = res;
		}
	}
	return res;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION read_treedata                                                    *
 * ----------------------                                                    *
 * Loads a structured file into a cmdtree structure.                         *
\* ------------------------------------------------------------------------- */

char CRSR[256];
char CMD_PATH[256];

char *read_treedata (const char *fname)
{
	char	 buffer[1024];
	cmdnode *crsr;
	cmdnode *node;
	int		 nestlevel;
	int		 nnestlevel;
	char    *n_word;
	char	*n_trigger;
	int		 n_level;
	char	*n_help;
	FILE	*F;
	FILE	*fi;
	char	*tl, *tr;
	int		 committed;
	int		 i;
	char	*splitpoint;

	/* create root node */
	CMD_ROOT = new_cmdnode (NULL);
	
	/* fill out globals */
	CMD_PATH[0] = 0;
	strcpy (CRSR, "cish% ");
	
	crsr = CMD_ROOT;
	nestlevel = -1;
	n_word = n_trigger = n_help = NULL;
	n_level = 0;
	
	F = fopen (fname, "r");
		
	while (F && (! feof(F)))
	{
		*buffer = 0;
		
		n_word = NULL;
		n_trigger = NULL;
		n_level = 0;
		n_help = NULL;
		
		fgets (buffer, 1023, F);
		if (*buffer == '#')
		{
			buffer[strlen(buffer)-1] = 0;
			if (strncmp (buffer, "#prompt ", 8) == 0)
			{
				strcpy (CRSR, buffer+8);
			}
			else if (strncmp (buffer, "#path ", 6) == 0)
			{
				strncpy (CMD_PATH, buffer+6, 254);
				CMD_PATH[255] = 0;
			}
		}
		else if (strlen (buffer)>1)
		{
			/* figure out the indentation level */
			nnestlevel = 0;
			while (buffer[nnestlevel] == ' ') ++nnestlevel;
			
			if (nnestlevel > nestlevel)
			{
				/* line is indented more to the right than the previous
				   line, it is assumed to be the child node of the 
				   predecessor */
				
				node = new_cmdnode (crsr);
				++nestlevel;
			}
			else if (nestlevel > nnestlevel)
			{
				/* line is indented more to the left, iterate up the tree
				   to match the number of levels */
				   
				while (nestlevel > nnestlevel)
				{
					crsr = crsr->parent;
					--nestlevel;
				}
				node = new_cmdnode (crsr->parent);
			}
			else
			{
				/* line is at same level as the preceeding line, so it is 
				   a sibling to be inserted at the same level */
				   
				node = new_cmdnode (crsr->parent);
			}
						
			crsr = node;
			
			/* isolate command word */
			
			tl = buffer;
			while (isspace (*tl)) ++tl;
			tr = tl;
			while ((*tr) && (! isspace (*tr))) ++tr;
			
			n_word = substr (buffer, (tl-buffer), (tr-tl));
			tl = tr;
			
			if (*tr)
			{
				while ((*tl)&&(!isdigit (*tl))) ++tl;
				tr = tl;
				while ((*tr)&&(isdigit (*tr))) ++ tr;
				
				/* read the required access level field */
				n_level = atoi (tl);
				
				/* if the line's not finished yet, go for the rest */
				if (*tr)
				{
					/* the trigger field is embraced by gt/lt */
					tl = tr+1;
					while ((*tl)&&(*tl != '<')) ++tl;
					if (*tl) ++tl;
					tr = tl;
					while ((*tr)&&(*tr != '>')) ++tr;
					if (*tr)
					{
						n_trigger = substr (buffer, tl-buffer, tr-tl);
						tl = tr+1;
						
						while ((*tl)&&(isspace(*tl))) ++tl;
						if (*tl);
						tr = tl;
						while (*tr > 31) ++tr;
						n_help = substr (buffer, tl-buffer, tr-tl);

						/* special case for a trigger word starting with a
						   pipe: expansion options should be read from
						   a command at runtime, which we will do now */
						   
						if (n_word[0] == '|')
						{
							committed = 0;
							sprintf (buffer, "%s/%s", CMD_PATH, n_word+1);
							fi = popen (buffer, "r");
							if (fi) while (! feof (fi))
							{
								buffer[0] = 0;
								fgets (buffer, 255, fi);
								if (*buffer)
								{
									buffer[strlen(buffer)-1] = 0;
									if (committed)
									{
										node = new_cmdnode (crsr->parent);
									}
									splitpoint = strchr (buffer, '\t');
									if (splitpoint)
									{
										*splitpoint = 0;
										node->help = strdup (splitpoint+1);
									}
									else
									{
										node->help = strdup (n_help);
									}
									node->word = strdup (buffer);
									node->level = n_level;
									crsr = node;
									
									if (n_trigger[0] == '@')
									{
										strcpy (buffer, n_trigger);
										if (strchr (buffer, ':'))
										{
											i = strchr (buffer, ':') - buffer;
											buffer[i++] = 0;
											node->trigger = strdup(buffer+i);
										}
										else node->trigger = strdup ("");
										node = new_cmdnode (crsr);
										node->word = strdup (buffer);
										node->level = n_level;
										node->trigger = strdup ("");
										node->help = strdup ("");
									}
									else
									{
										node->trigger = strdup (n_trigger);
									}
									committed = 1;
								}
							}
							if (fi)
							{
								pclose (fi);
								free (n_trigger);
								free (n_help);
							}
							else
							{
								node->word = strdup ("<string>");
								node->level = n_level;
								node->trigger = n_trigger;
								node->help = n_help;
							}
							free (n_word);
						}
						else
						{
							/* All the relevant information has been
							   extracted, put it in the structure now */
							
							node->word = n_word;
							node->level = n_level;
							node->trigger = n_trigger;
							node->help = n_help;
						}
					}
				}
			}
		}
	}
	if (F) fclose (F);
	
	return CRSR;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION expand_cmdtree                                                   *
 * -----------------------                                                   *
 * Gives the required text to complete a commandline on a press of <tab>,    *
 * or prints a list of options when the input is ambiguous.                  *
\* ------------------------------------------------------------------------- */

char *expand_cmdtree (const char *cmdline, int cpos)
{
	cmdnode *crsr;
	cmdnode *t;
	cmdnode *jmp;
	int nmatch;
	cmdnode *lmatch;
	term_arglist *args;
	int i, j;
	int exactmatch;
	char *res;
	int fnd;
	char expandbuffer[1024];
	
	/* Currently expanding only takes place at the end of the line */
	if (cpos < strlen(cmdline)) return NULL;
	
	if (! *cmdline) return NULL;
	
	crsr = CMD_ROOT;	
	args = make_args (cmdline);
	
	/* Now we'll walk through the tree with the provided arguments */
	for (i=0; i < args->argc; ++i)
	{
		t = crsr->first;
		nmatch = 0;
		lmatch = NULL;
		exactmatch = 0;
		
		/* Figure out for every possibility at this level whether the system
		   can expand based on the provided text and the desired expansion */
		
		while (t)
		{
			if (t->level <= USERLEVEL)
			{
				if (t->word[0] == '@')
				{
					if (t->parent != CMD_ROOT)
					{
						fnd = 0;
						jmp = CMD_ROOT->first;
						while (jmp)
						{
							if (strcmp (jmp->word, t->word) == 0)
							{
								t = jmp->first;
								jmp = NULL;
								fnd = 1;
							}
							else jmp = jmp->next;
						}
					}
				}
				if ((t->word[0] != '@') &&
					(cmd_match (args->argv[i], t->word)))
				{
					if (! exactmatch)
					{
						lmatch = t;
						++nmatch;
						if (! strcmp (args->argv[i], t->word))
						{
							exactmatch = 1;
							nmatch = 1;
						}
					}
				}
			}
			t = t->next;
		}
		if (nmatch == 1)
		{
			/* Exactly one match so we can safely expand */
		   if ( ((i+1) >= args->argc) || (*(lmatch->word) != '~') )
				crsr = lmatch;
		}
		else if (nmatch == 0)
		{
			/* No matches, tough titties */
			tprintf ("\n%% Syntax error at '%s'\n", args->argv[i]);
			destroy_args (args);
			return NULL;
		}
		else
		{
			/* Multiple matches */
			if ( (i+1) < args->argc)
			{
				/* This is not the last argument, so spit out an error */
				tprintf ("\n%% Ambiguous command at '%s'\n", args->argv[i]);
				destroy_args (args);
				return NULL;
			}
			else
			{
				/* Give the user a list of possible expansions so he/she can
				   keep typing until the input is no longer ambiguous */
				tprintf ("\n");
				if ((crsr->trigger) && (strlen (crsr->trigger)))
				{
					tprintf ("  [ENTER]                  Execute command\n");
				}
				t = crsr->first;
				
				nmatch = 0;
				
				while (t)
				{
					if (t->level <= USERLEVEL)
					{
						if (cmd_match (args->argv[i], t->word))
						{
							if (*(t->word) == '~')
							{
								tprintf ("  [%-22s] %s\n", t->word+1, t->help);
							}
							else if (*(t->word) != '@')
							{
								tprintf ("  %-24s %s\n", t->word, t->help);
								
								/* The expandbuffer is designed to contain the
								   minimum number of characters that all
								   possible expansions have in common. In cases
								   where there is no single match, we can at
								   least finish completion up to this point.
								   
								   On the first match, we start out with its
								   full target word, from there on we barter
								   down by comparing the expandbuffer with the 
								   new target word and cutting it short at the
								   first point where the two differ. */
								
								if (! nmatch) /* first pass */
								{
									strncpy (expandbuffer, t->word, 1023);
									expandbuffer[1023] = 0;
								}
								else /* second and later passes */
								{
									for (	j=0;
											(t->word[j])&&(expandbuffer[j]);
											++j
										)
									{
										if (expandbuffer[j] != t->word[j])
										{
											expandbuffer[j] = 0;
											break;
										}
									}
								}
								++nmatch;
							}
						}
					}
					t = t->next;
				}
				
				if (*expandbuffer) /* Can we do partial expansion? */
				{
					/* if the lowest common denominator is what the user
					    already typed, our work is obviously done here. */
					    
					if (strlen (expandbuffer) > strlen (args->argv[i]))
					{
						GLOBAL_TERMBUF->rdpos = -1;
						terminus_updatecrt (GLOBAL_TERMBUF);
						return strdup (expandbuffer + strlen (args->argv[i]));
					}
				}
				destroy_args (args);
				return NULL;
			}
		}
	}
	
	res = cmd_expandterm (args->argv[args->argc-1], crsr->word);
	destroy_args (args);
	return res;
	
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION expand_cmdtree_full                                              *
 * ----------------------------                                              *
 * Fully completes all individual arguments of a full command line.          *
\* ------------------------------------------------------------------------- */

term_arglist *expand_cmdtree_full (const char *cmdline)
{
	cmdnode *crsr;
	cmdnode *t;
	cmdnode *jmp;
	cmdnode *optional;
	int nmatch;
	cmdnode *lmatch;
	term_arglist *args;
	int i;
	char *tres;
	char *res;
	term_arglist *resarg;
	int fnd;
	int exactmatch;
	
	resarg = new_args();
	if (! resarg)
	{
		tprintf ("%% Allocation error\n");
		return NULL;
	}
	
	if (! strlen (cmdline)) return NULL;
	
	crsr   = CMD_ROOT;	
	args   = make_args (cmdline);
	
	/* Now we'll walk through the tree with the provided arguments */
	for (i=0; i < args->argc; ++i)
	{
		t      = crsr->first;
		nmatch = 0;
		lmatch = NULL;
		exactmatch = 0;
		
		/* Figure out for every possibility at this level whether the system
		   could expand based on the provided text and the desired expansion */
		   
		while (t)
		{
			if (t->word && (t->level <= USERLEVEL))
			{
				if (t->word[0] == '@')
				{
					if (t->parent != CMD_ROOT)
					{
						fnd = 0;
						jmp = CMD_ROOT->first;
						while (jmp)
						{
							if (strcmp (jmp->word, t->word) == 0)
							{
								t = jmp->first;
								jmp = NULL;
								fnd = 1;
							}
							else jmp = jmp->next;
						}
						if (! fnd) t = t->next;
					}
				}
				if ((t->word[0] != '@') &&
					(cmd_match (args->argv[i], t->word)))
				{
					if (! exactmatch)
					{
						lmatch = t;
						++nmatch;
						if (! strcmp (args->argv[i], t->word))
						{
							exactmatch = 1;
							nmatch = 1;
						}
					}
				}
			}
			t = t->next;
		}
		
		optional = NULL;
		
		if (nmatch == 1)
		{
			/* Exactly one match so we can safely expand */
			if (*(lmatch->word) != '~')
				crsr = lmatch;
			else
				optional = lmatch;
		}
		else if (nmatch == 0)
		{
			/* No matches, tough titties */
			tprintf ("\n%% Syntax error at '%s'\n", args->argv[i]);
			destroy_args (args);
			destroy_args (resarg);
			return NULL;
		}
		else
		{
			/* This is not the last argument either, so spit out an error */
			tprintf ("\n%% Ambiguous command at '%s'\n", args->argv[i]);
			destroy_args (args);
			destroy_args (resarg);
			return NULL;
		}
		
		tres = cmd_expandterm (args->argv[i],
							   optional ? optional->word : crsr->word);
		
		if (tres)
		{
			res = (char *) malloc ((size_t) (strlen(args->argv[i]) +
											 strlen (tres) + 1));
			
			strcpy (res, args->argv[i]);
			strcat (res, tres);
			if (strlen (res))
			{
				res[strlen(res)-1] = 0;
			}
			
			add_args (resarg, res);
			
			free (res);
			free (tres);
		}
	}
	
	if (crsr) add_args (resarg, crsr->trigger);
		
	destroy_args (args);
	return resarg;
	
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION explain_cmdtree (cmdline, cursorpos)                             *
 * ---------------------------------------------                             *
 * Shows an overview of all possible expansions, without actually            *
 * expanding anything. Normally bound to the question-mark key.              *
\* ------------------------------------------------------------------------- */

char *explain_cmdtree (const char *cmdline, int cpos)
{
	cmdnode *crsr;
	cmdnode *t;
	cmdnode *jmp;
	int nmatch;
	cmdnode *lmatch;
	term_arglist *args;
	int i;
	char *res;
	int fnd;
	int exactmatch;
	
	if (cpos < strlen(cmdline)) return NULL;
	
	crsr = CMD_ROOT;
	
	if (strlen (cmdline))
	{
		args = make_args (cmdline);
		for (i=0; i < args->argc; ++i)
		{
			t = crsr->first;
			nmatch = 0;
			lmatch = NULL;
			exactmatch = 0;

			while (t)
			{
				if (t->level <= USERLEVEL)
				{
					if (t->word[0] == '@')
					{
						if (t->parent != CMD_ROOT)
						{
							jmp = CMD_ROOT->first;
							while (jmp)
							{
								if (strcmp (jmp->word, t->word) == 0)
								{
									t = jmp->first;
									jmp = NULL;
								}
								else jmp = jmp->next;
							}
						}
					}
					if ((t->word[0] != '@') &&
						(cmd_match (args->argv[i], t->word)))
					{
						if ((i+1) == args->argc)
						{
							lmatch = t;
							++nmatch;
						}
						else if (! exactmatch)
						{
							lmatch = t;
							++nmatch;
							if (! strcmp (args->argv[i], t->word))
							{
								exactmatch = 1;
								nmatch = 1;
							}
						}
					}
				}
				t = t->next;
			}
			if (nmatch == 1)
			{
				if (lmatch->word[0] != '~') crsr = lmatch;
			}
			else if (nmatch == 0)
			{
				tprintf ("\n%% Syntax error at '%s'\n", args->argv[i]);
				destroy_args (args);
				return NULL;
			}
			else
			{
				if ( (i+1) < args->argc)
				{
					tprintf ("\n%% Ambiguous command at '%s'\n",
							 args->argv[i]);
					destroy_args (args);
					return NULL;
				}
				else
				{
					tprintf ("\n");
					if ((crsr->trigger) && (strlen (crsr->trigger)))
					{
						tprintf ("  [ENTER]                  "
								 "Execute command\n");
					}
					t = crsr->first;
					while (t)
					{
						if (t->level <= USERLEVEL)
						{
							if (cmd_match (args->argv[i], t->word))
							{
								if (*(t->word) == '~')
								{
									tprintf ("  [%-22s] %s\n",
											 t->word+1, t->help);
								}
								else if (*(t->word) != '@')
								{
									tprintf ("  %-24s %s\n", t->word, t->help);
								}
							}
						}
						t = t->next;
					}
					destroy_args (args);
					return NULL;
				}
			}
		}
		destroy_args (args);
	}
	tprintf ("\n");
	
	t = crsr->first;
	if (! t)
	{
		tprintf ("%% No further commands\n");
	}
	else
	{
		if ((crsr->trigger) && (strlen (crsr->trigger)))
		{
			tprintf ("  [ENTER]                  Execute command\n");
		}
	}
	while (t)
	{
		if (t->word && (t->level <= USERLEVEL))
		{
			if (*(t->word) == '~')
			{
				tprintf ("  [%-22s] %s\n", t->word+1, t->help);
				t = t->next;
			}
			else if (*(t->word) != '@')
			{
				tprintf ("  %-24s %s\n", t->word, t->help);
				t = t->next;
			}
			else if (t->parent != CMD_ROOT)
			{
				fnd = 0;
				jmp = CMD_ROOT->first;
				while (jmp)
				{
					if (! strcmp (jmp->word, t->word))
					{
						t = jmp->first;
						jmp = NULL;
						fnd = 1;
					}
					else jmp = jmp->next;
				}
				if (! fnd) t = t->next;
			}
			else t = t->next;
		}
		else t = t->next;
	}
	return NULL;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION cmd_match (word, schematerm)                                     *
 * -------------------------------------                                     *
 * Internal function that evaluates a 'word' against a menu term,            *
 * which is defined in the schema language as being on of:                   *
 * - A literal string, for example: command                                  *
 * - An optional string, for example: ~option                                *
 * - A generic type, one of:                                                 *
 * - <integer>       : 42                                                    *
 * - <string>        : foobar                                                *
 * - <ipaddr>        : 10.42.69.23                                           *
 * - <emailaddress>  : john@doe.org                                          *
 * - <iproute>       : 83.149.15.0                                           *
 * - <netmask>		: 255.255.255.0                                              *
\* ------------------------------------------------------------------------- */

#define ISOCTET(x) (atoi(x)||(*x == '0'))

int cmd_match (const char *word, const char *mterm_)
{
	int i;
	const char *crsr;
	const char *mterm = mterm_;
	
	if (*mterm == '~') ++mterm;
	
	if (*mterm == '<') /* category-matches */
	{
		if (strcasecmp (mterm, "<integer>") == 0)
		{
			for (i=0; word[i]; ++i) if (! isdigit(word[i])) return 0;
			return 1;
		}
		if (strcasecmp (mterm, "<string>") == 0)
		{
			return 1;
		}
		if (strcasecmp (mterm, "<ipaddr>") == 0)
		{
			crsr = word;
			if (ISOCTET(crsr))
			{
				for (i=0; i<3; ++i)
				{
					crsr = strchr(crsr, '.');
					if (crsr)
					{
						crsr++;
						if (!ISOCTET(crsr)) return 0;
					}
					else return 0;
				}
				return 1;
			}
			return 0;
		}
		if (strcasecmp (mterm, "<emailaddress>") == 0)
		{
			crsr = word;
			while ((*crsr)&&(*crsr!='@'))
			{
				if (isspace (*crsr)) return 0;
				if (!isalnum (*crsr))
				{
					if ((*crsr != '.')&&(*crsr != '-')&&(*crsr != '.'))
						return 0;
				}
				++crsr;
			}
			if (! (*crsr)) return 0;
			++crsr;
			while (*crsr)
			{
				if (isspace (*crsr)) return 0;
				if (!isalnum (*crsr))
				{
					if ((*crsr != '.')&&(*crsr != '-')&&(*crsr != '.'))
					{
						return 0;
					}
				}
				++crsr;
			}
			return 1;
		}
		if ((strcasecmp (mterm, "<iproute>") == 0) ||
			(strcasecmp (mterm, "<netmask>") == 0))
		{
			crsr = word;
			if (ISOCTET(crsr))
			{
				for (i=0; i<3; ++i)
				{
					crsr = strchr(crsr, '.');
					if (crsr)
					{
						crsr++;
						if (*crsr == 0) return 1;
						if (!ISOCTET(crsr)) return 0;
					}
					else return 0;
				}
				return 1;
			}
			return 0;
		}
		return 0;
	}

	if (strncmp (word, mterm, strlen(word)) == 0) return 1;
	return 0;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION cmd_expandterm (word, schematerm)                                *
 * ------------------------------------------                                *
 * Tries to do the proper thing in expanding a given word to a valid         *
 * word adhering to the defined schema term. In many cases, this means       *
 * that either the function returns a NULL if the word cannot be             *
 * correctly expanded, or just a single ASCII whitespace if the word         *
 * was valid in its unaltered shape. In some cases, the schema term          *
 * can define a sensible expansion (like adding a ".0" for a <netmask>       *
 * if confronted with a word like "255.255.255").                            *
\* ------------------------------------------------------------------------- */

char *cmd_expandterm (const char *word, const char *mterm_)
{
	char *res;
	int i;
	const char *crsr;
	const char *mterm = mterm_;
	
	if (*mterm == '~') ++mterm;
	
	if (*mterm == '<')
	{
		if (strcasecmp (mterm, "<integer>") == 0)
		{
			for (i=0; word[i]; ++i) if (! isdigit(word[i])) return NULL;
		}
		else if (strcasecmp (mterm, "<string>") == 0)
		{
		}
		else if (strcasecmp (mterm, "<ipaddr>") == 0)
		{
			if (! cmd_match (word, mterm)) return NULL;
		}
		else if (strcasecmp (mterm, "<emailaddress>") == 0)
		{
			if (! cmd_match (word, mterm)) return NULL;
		}
		else if ((strcasecmp (mterm, "<iproute>" )== 0) ||
				 (strcasecmp (mterm, "<netmask>")==0))
		{
			if (! cmd_match (word, mterm)) return NULL;
			
			crsr = word;
			i = 0;
			while (crsr)
			{
				crsr = strchr (crsr, '.');
				if (crsr)
				{
					++i;
					++crsr;
					if (! (*crsr))
					{
						res = (char *) malloc (24 * sizeof(char));
						res[0] = 0;
						for (;i<4;++i)
						{
							if (*res) strcat (res, ".");
							strcat (res, "0");
						}
						strcat (res, " ");
						return res;
					}
				}
			}
		}
		res = (char *) malloc ((size_t) 2);
		strcpy (res, " ");
		return res;
	}
	
	res = (char *) malloc (sizeof(char) * (strlen (mterm) + 2));
	if (res)
	{
		sprintf (res, "%s ", mterm + strlen(word));
	}
	return res;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION substr (text, position, length)                                  *
 * ----------------------------------------                                  *
 * Allocates a string of the provided length and populates it with a         *
 * substring of the provided text.                                           *
\* ------------------------------------------------------------------------- */

char *substr (const char *txt, int pos, int len)
{
	char *res;
	
	res = (char *) malloc ((len+2) * sizeof (char));
	
	strncpy (res, txt+pos, len);
	res[len] = 0;
	return res;
}

