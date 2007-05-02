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
 * This file implements the password input mode of terminus. Not really      *
 * so fancy.                                                                 *
\* ========================================================================= */

#include <stdio.h>
#include "readpass.h"

#define STDOUT 1

/* ------------------------------------------------------------------------- *\
 * FUNCTION termnus_readpass (into, maxlen)                                  *
 * ----------------------------------------                                  *
 * Reads a password (basically fetches the characters without an echo), up   *
 * to the provided length.                                                   *
\* ------------------------------------------------------------------------- */

void terminus_readpass (char *buf, int size)
{
	int tmp, tmp2;
	int pos;
	
	pos = 0;
	
	while (((pos+1) < size) && ((tmp = getchar()) != '\n'))
	{
		if (tmp == 21) pos = 0;
		else if ((tmp == 8) && pos) --pos;
		else buf[pos++] = tmp;
	}
	buf[pos] = 0;
	
	printf ("\n");
}
