/* ========================================================================= *\
 * CISH2 Configuration Internet Shell                                        *
 * ------------------------------------------------------------------------- *
 * Copyright (C) 2002-2004 Pim van Riezen <pi@madscience.nl>                 *
 *                                                                           *
 * This software is provided under the GNU General Public License (GPL)      *
 * Read the file LICENSE, that should be provided to you when you got        *
 * this source file, for full details of your rights under this license.     *
\* ========================================================================= */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "platform.h"
#include "readpass.h"
#include "version.h"
#include "tcrt.h"
#include "cmdtree.h"

/* FIXME: this should go away and never be talked about */
#define STDOUT 0
#define STDIN 0

/* enable level */
extern int USERLEVEL;

/* history file */
FILE *HISTORY;

/* where to find schema files */
extern char CMD_PATH[256];
char HOSTNAME[48]; /* our host name */
const char *PATH_PW; /* location of our passwd file */
const char *PATH_PW_ENABLE; /* location of our enable passwd file */
int FEV; /* pipe to the event notification program */
termbuf *GLOBAL_TERMBUF; /* export for cmdtree */

void termhandler (int); /* handle SIGTERM gracefully by resetting the term */

/* ------------------------------------------------------------------------- *\
 * FUNCTION condtrol_d (cmdline, cpos)                                       *
 * -----------------------------------                                       *
 * A terminus keyhandler for ^D. Sends a ^U followed by the literal command  *
 * string 'quit' and a newline.                                              *
\* ------------------------------------------------------------------------- */
char *control_d (const char *cmdline, int cpos)
{
	return ((char *) "\025quit\n");
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION control_z (cmdline, cpos)                                        *
 * ----------------------------------                                        *
 * A terminus keyhandler for ^Z. Sends a ^U followed by the literal command  *
 * string 'exit' and a newline.                                              *
\* ------------------------------------------------------------------------- */
char *control_z (const char *cmdline, int cpos)
{
	return ((char *) "\025exit\n");
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION setprompt (dst, sz, input)                                       *
 * -----------------------------------                                       *
 * Parses a prompt from a schema file into the destination string. Two major *
 * replacements are done: The tilde (~) is replaced by the system's hostname *
 * and the hash (#) is replaced by a gt (>) if the session is not enabled.   *
\* ------------------------------------------------------------------------- */
void setprompt (char *dst, int sz, const char *in)
{
	int sc;
	int dc;
	char c;
	
	sc = 0;
	dc = 0;
	
	while (in[sc])
	{
		c = in[sc];
		if (c == '~')
		{
			dst[dc] = 0;
			strncat (dst, HOSTNAME, sz-1);
			dst[sz-1] = 0;
			dc = strlen (dst);
		}
		else
		{
			if ((c == '#') && (!USERLEVEL))
				c = '>';
			if (dc<sz)
				dst[dc++] = c;
		}
		++sc;
	}
	dst[dc] = 0;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION checkpw (enable, plaintext)                                      *
 * ------------------------------------                                      *
 * Loads a password crypt file for either enable or plain login mode and     *
 * crypts the provided plaintext against its salt. If it matches, the        *
 * function returns 1, otherwise it returns 0. If the password file could    *
 * not be loaded, it will also return 0 after printing out an error.         *
 *                                                                           *
 * On a 'closed' CISH-controlled system, fixing this will require enticing   *
 * the bootloader or console server to give CISH a sign that authentication  *
 * should be bypassed. There are three ways to get there:                    *
 *                                                                           *
 * 1) Start the cish shell with the -P and -E flags                          *
 * 2) Start the shell with ${CISH_CONF_BYPASS_AUTH} set to 1                 *
 * 3) Boot the linux kernel with a 'bypassauth=1' in the arguments           *
 *                                                                           *
 * This all depends on how deeply you integrate CISH into your system.       *
 * It is also possible that you want to rely on external authentication      *
 * entirely. For sessions that you want to offer access only to commands     *
 * that don't require enabled mode, you can start the CISH shell with        *
 * the -P flag and a -e flag pointing to an invalid path. Sessions that      *
 * require password authentication for neiher unprivileged mode              *
 * nor enabled mode can start up cish with -E and -P arguments.              *
\* ------------------------------------------------------------------------- */
int checkpw (int enable, char *plain)
{
	FILE *F;
	char buf[256];
	char salt[3];
	char *cr;
	int ln;
	
	salt[2] = 0;
	
	F = fopen (enable ? PATH_PW_ENABLE : PATH_PW ,"r");
	if (!F)
	{
		fprintf (stderr, "%% could not open authentication database\n");
		return 0;
	}
	
	fgets (buf, 255, F);
	fclose (F);
	
	ln = strlen (buf);
	if (ln && (buf[ln-1] < 32)) buf[ln-1] = 0;
	salt[0] = buf[0];
	salt[1] = buf[1];
	
	cr = crypt (plain, salt);
	if (! strcmp (cr, buf))
	{
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION readhostname                                                     *
 * ---------------------                                                     *
 * Tries to set the global HOSTNAME string to what seems proper. It will     *
 * first try to open the file /etc/hostname. No linux standard defines       *
 * this file and that is fine. On embedded systems, this file can be used    *
 * in the place of getting the kernel's hostname set through a more complex  *
 * initialization sequence. This also makes it easier to make it possible    *
 * to change the configured hostname from within a cish script without       *
 * relying on the non-posix sethostname().                                   *
\* ------------------------------------------------------------------------- */
void readhostname (void)
{
	FILE *F;
	int ln;
	
	F = fopen ("/etc/hostname", "r");
	if (! F)
	{
		gethostname (HOSTNAME, 31);
		HOSTNAME[31] = 0;
		if (! *HOSTNAME)
			strcpy (HOSTNAME, "Router");
		return;
	}
	fgets (HOSTNAME, 31, F);
	HOSTNAME[31] = 0;
	
	ln = strlen(HOSTNAME);
	if (ln && (HOSTNAME[ln-1] < ' ')) HOSTNAME[ln-1] = 0;
	fclose (F);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION main (argc, argv)                                                *
 * --------------------------                                                *
 * The main shell loop. Parses the parameters. Loads the resource file.      *
 * Starts the event notification subprocess (if so configured). Then         *
 * goes into a loop reading commands and responding to them, like any        *
 * respectionable shell.                                                     *
\* ------------------------------------------------------------------------- */
int main (int argc, char *argv[])
{
	termbuf *tb; /* terminus termbuffer */
	char *result = NULL; /* the expanded string from terminus_readline() */
	term_arglist *res; /* the command tree */
	int i=0; /* generic counter */
	int option; /* getopt option char */
	int resultmallocd = 0; /* flag to keep track of whether we shoudl free() */
	const char *trig; /* temporary var points to a trigger */
	char *prompt; /* Pointer to the prompt string from read_treedata */
	char cmd[512]; /* Storage for a shell command */
	char buf[256]; /* Temporary buffer */
	char cprompt[256]; /* The fully parsed current prompt */
	char envar[32]; /* Storage for env variable names we want to set */
	pid_t pid; /* The pid of our event notification tool */
	int batchmode; /* Flag, set to 1 if we were started with -b */
	const char *deftree; /* The filename of the resrouce file for the root */
	int defloaded; /* Flag tracks whether we loaded the root file */
	int linesdone; /* Number of output lines (used by the pager) */
	int numrows; /* Number of rows in the terminal */
	struct winsize sz; /* Termios window size structure */
	int devnull; /* Fileno for an open /dev/null */
	int nologinpass; /* Flag for bypassing authentication */
	int noenablepass; /* Dito */
	int pwattempt; /* Number of the attempt to enter a password */
	char *tmp; /* generic temporary string pointer */
	const char *bannerfile;
	
	int inval; /* invalid flag */
	int cmdpipe[2]; /* downstream pipe from a command script */
	int retpipe[2]; /* upstream pipe to a command script */
	int evpipe[2]; /* downstream pipe from the event notification program */
	FILE *FP; /* a generic file pointer */
	
	/* Some initializations */
	batchmode = 0;
	defloaded = 0;
	nologinpass = 0;
	noenablepass = 0;
	bannerfile = NULL;
	USERLEVEL = 0;
	PATH_PW = "/var/run/cish-passwd";
	PATH_PW_ENABLE = "/var/run/cish-enable-passwd";
	HISTORY = NULL;
	
	/* Set the environment variable for the cish version */
	setenv ("CISH_VERSION", CISH_VERSION, 1);

	/* Some elementary initialization */
	signal (SIGTTOU, SIG_IGN);
	signal (SIGTERM, termhandler);
	readhostname();
	FEV = 0;
	
	if (getenv ("HOME"))
	{
		sprintf (cmd, "%s/.cish_history", getenv("HOME"));
		HISTORY = fopen (cmd, "a");
	}
	
	deftree = "termdef.cf";
	
	/* Parse the commandline options */
	if (argc > 1)
	{
		while ( (option = getopt (argc, argv, "bB:p:e:x:EPh")) > 0)
		{
			switch (option)
			{
				case 'b':
					batchmode = 1;
					break;
				
				case 'B':
					bannerfile = optarg;
					break;
				
				case 'p':
					PATH_PW = optarg;
					break;
				
				case 'e':
					PATH_PW_ENABLE = optarg;
					break;
					
				case 'E':
					noenablepass = 1;
					break;
				
				case 'P':
					nologinpass = 1;
					break;
					
				case 'x':
					pipe (evpipe);
					switch (pid = fork())
					{
						case -1:
							tprintf ("%% Error spawning process\n");
							break;
							
						case 0:
							for (i=0; i<256; ++i)
							{
								if (i != evpipe[1]) close (i);
							}
							open ("/dev/null", O_RDONLY);
							dup2 (evpipe[1], 1);
							dup2 (evpipe[1], 2);
							execl (optarg, optarg, NULL);
							return 0;
					}
					
					close (evpipe[1]);
					FEV = evpipe[0];
					break;
				
				default:
					if (option != 'h')
					{
						fprintf (stderr, "Unrecognized option '%c'\n", option);
					}
					fprintf (stderr, "Usage: %s [-b] [-p pwfile] "
									 "[-e pwfile] [termdef]\n",
									 argv[0]);
					return ((option=='h') ? 0 : 1);
				
			}
		}
		if ((optind>0) && (optind < argc))
		{
			deftree = argv[optind];
		}
	}
	
	/* Authentication can be bypassed by an environment variale */
	if (tmp = getenv ("CISH_CONF_BYPASS_AUTH"))
	{
		if (atoi (tmp))
		{
			nologinpass = 1;
			noenablepass = 1;
		}
	}
	else /* Or by a parameter on the linux kernel boot system */
	{
		tmp = (char *) malloc ((size_t) 1024);
		if (! tmp)
		{
			fprintf (stderr, "%% malloc() failure\n"); exit (1);
		}
		if (FP = fopen ("/proc/cmdline","r"))
		{
			tmp[0] = 0;
			fgets (tmp, 255, FP);
			fclose (FP);
			if (strstr (tmp, "bypassauth=1"))
			{
				nologinpass = 1;
				noenablepass = 1;
			}
		}
		free (tmp);
	}
	
	if (bannerfile)
	{
		tmp = (char *) malloc ((size_t) 1024);
		if (! tmp)
		{
			fprintf (stderr, "%% malloc() failure\n"); exit (1);
		}
		if (FP = fopen (bannerfile, "r"))
		{
			while (! feof (FP))
			{
				tmp[0] = 0;
				fgets (tmp, 1023, FP);
				tmp[1023] = 0;
				if (*tmp) printf ("%s", tmp);
			}
			fclose (FP);
			printf ("\n");
		}
		free (tmp);
	}
	
	/* Load the resource file, set up the prompt */
	prompt = read_treedata (deftree);
	setprompt (cprompt, 255, prompt);
	defloaded = 1;
	
	/* Outside batch mode, we should prompt for a password and go into
	   terminal mode with keyhandlers */
	if (! batchmode)
	{
		terminus_on ();
	
		if (! nologinpass)
		{
			tprintf ("User Access Verification\n\n");
			
			for (pwattempt=0; pwattempt<3; ++pwattempt)
			{
				tprintf ("Password: ");
				terminus_readpass (buf, 16);
		
				if (! checkpw (0, buf))
				{
					tprintf ("%% Access denied.\n");
				}
				else break;
			}
			
			if (pwattempt == 3)
			{
				memset (buf, 0, sizeof (buf));
				terminus_off ();
				return (1);
			}
		}
		
		memset (buf, 0, sizeof (buf));
		
		GLOBAL_TERMBUF = tb = init_termbuf (512, 0);
		terminus_add_handler (tb, 9, expand_cmdtree);
		terminus_add_handler (tb, '?', explain_cmdtree);
		terminus_add_handler (tb, 4, control_d);
		terminus_add_handler (tb, 26, control_z);
	}
	
	while (1)
	{
		if (! batchmode)
		{
			if (result && resultmallocd) free (result);
			result = (char *) terminus_readline (tb, cprompt);
			resultmallocd = 0;
			printf ("\n");
			if (HISTORY)
			{
				fprintf (HISTORY, "%s\n", result);
			}
		}
		else
		{
			if (result && resultmallocd) free (result);
			result = (char *) malloc (512 * sizeof (char));
			resultmallocd = 1;
			*result = 0;
			if (feof (stdin)) return 0;
			
			fgets (result, 511, stdin);
			if (*result == 0) return 0;
			if (*result) result[strlen(result)-1] = 0;
			if (*result == 0) explain_cmdtree ("", 0);
		}
		
		if (*result == '!')
		{
			if (! defloaded)
			{
				prompt = reload_treedata (deftree);
				setprompt (cprompt, 255, prompt);
				defloaded = 1;
			}
		}
		else
		{
			res = expand_cmdtree_full (result);
	
			if ((res) && (res->argc>1))
			{
				/* expand_cmdtree_full places the trigger in the last
				   argument */
				trig = res->argv[res->argc-1];
	
				/* cmd_exit builtin: reset terminal and bail out */
				if (! strcmp (trig, "cmd_exit"))
				{
					terminus_off ();
					return 0;
				}
				
				/* enable builtin: ask for password and enable (or not) */
				if (! strcmp (trig, "enable"))
				{
					if (! noenablepass)
					{
						tprintf ("Password: "); 
						terminus_readpass (buf, 16);
					
						if (! checkpw (1, buf))
						{
							tprintf ("%% Access denied.\n");
						}
						else
						{
							USERLEVEL = 1;
							setprompt (cprompt, 255, prompt);
						}
					}
					else
					{
						USERLEVEL = 1;
						setprompt (cprompt, 255, prompt);
					}
				}
				else if (*trig == ':') /* a subshell */
				{
					for (i=0; i<res->argc;++i)
					{
						sprintf (envar, "MARG%i", i);
						setenv (envar, res->argv[i], 1);
					}
					prompt = reload_treedata (trig+1);
					setprompt (cprompt, 255, prompt);
					if (! strcmp (trig+1, deftree)) defloaded = 1;
					else defloaded = 0;
				}
				else /* An empty or script trigger */
				{
					if (! strlen (trig))
					{
						tprintf ("%% Incomplete command\n");
					}
					else /* script trigger */
					{
						if (*trig != '/') /* relative path */
						{
							if (*CMD_PATH)
							{
								sprintf (cmd, "%s/%s", CMD_PATH, trig);
							}
							else
							{
								strcpy (cmd, trig);
							}
						}
						else /* absolute path */
						{
							strcpy (cmd, trig);
						}
						
						/* We kept the pointer in trig, so we make it
						   NULL now as to not confuse the child
						   process with this unneeded argument, we will
						   restore the pointer after we are done so
						   that destroy_args() can do its evil job
						   properly. */
						   
						res->argv[res->argc-1] = NULL;
						
						/* find out about the current windowsize so that
						   we have an accurate rowcount */
						ioctl (STDIN, TIOCGWINSZ, (char *) &sz);
						numrows = sz.ws_row -2;
						
						/* create the pipes */
						pipe (cmdpipe);
						pipe (retpipe);
						
						/* create the subprocess */
						switch (pid = fork())
						{
							case -1: /* uh oh */
								tprintf ("%% Error spawning process\n");
								break;
								
							case 0: /* we're it, let's run the script */
								close (0);
								close (1);
								close (2);
								
								dup2 (retpipe[0], 0);
								dup2 (cmdpipe[1], 1);
								dup2 (cmdpipe[1], 2);
								close (cmdpipe[0]);
								close (retpipe[1]);
								
								setenv ("CMD", res->argv[0], 1);
								
								execvp (cmd, res->argv);
								return 0;
							
							default: /* spawned, let it spin */
								linesdone = -1;
								close (cmdpipe[1]);
								close (retpipe[0]);
								FP = fdopen (cmdpipe[0], "r");
								while (! feof (FP))
								{
									*buf = 0;
									fgets (buf, 255, FP);
									if (*buf) tprintf ("%s", buf);
									++linesdone;
									if (linesdone >= numrows)
									{
										if (terminus_more ("--More--"))
										{
											close (retpipe[1]);
											kill (pid, SIGPIPE);
											break;
										}
										linesdone = 0;
									}
								}
								fclose (FP);
								wait (NULL);
								break;
						}
						
						/* restore the trigger string pointer */
						res->argv[res->argc-1] = (char *) trig;
					}
				}
				destroy_args (res);
			}
			else if (res)
			{
				destroy_args (res);
			}
		}
		if (result)
		{
			if (resultmallocd) free (result);
			result = NULL;
			resultmallocd = 0;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION termhandler (signal)                                             *
 * -----------------------------                                             *
 * On receiving a SIGTERM, reset the terminal screen as a friendly gesture   *
 * towards the bastard that killed us and exit this cruel world.             *
\* ------------------------------------------------------------------------- */
void termhandler (int sig)
{
	terminus_off ();
	tprintf ("%% Terminated\n");
	exit (1);
}
