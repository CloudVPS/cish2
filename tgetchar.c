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
 * This file implements the arcane termios flags that need to be set. This   *
 * is a sloppy paste-job out of some example code which should be properly   *
 * attributed to Ross Combs and Jason Cordes. Also cleared ISIG in this      *
 * version so we can use ^Z for our own means.                               *
\* ========================================================================= */

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

struct termios NewInAttribs;
struct termios OldInAttribs;

/* ------------------------------------------------------------------------- *\
 * FUNCTION setupterm (filno)                                                *
 * --------------------------                                                *
 * Sets up termios 'our rules' mode, saving the old state.                   *
\* ------------------------------------------------------------------------- */

void setupterm(int const fd)
{
    int i;
    
    if(tcgetattr(fd,&OldInAttribs)<0)
        exit(1);
   
    memcpy (&NewInAttribs, &OldInAttribs, sizeof (NewInAttribs));
    NewInAttribs.c_lflag = OldInAttribs.c_lflag & ~(ECHO | ICANON | ISIG);
    NewInAttribs.c_cc[VMIN] = 1;
    NewInAttribs.c_cc[VTIME] = 0; 
       
    tcsetattr(fd,TCSAFLUSH,&NewInAttribs);
    cfmakeraw(&NewInAttribs);
}

/* ------------------------------------------------------------------------- *\
 * FUNCTION restoreterm (filno)                                              *
 * ----------------------------                                              *
 * Sets up termios to the old state.                                         *
\* ------------------------------------------------------------------------- */

void restoreterm(int const fd)
{
    tcsetattr(fd,TCSAFLUSH,&OldInAttribs);
}
