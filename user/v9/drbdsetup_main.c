/*
 * DRBD setup via genetlink
 *
 * This file is part of DRBD by Philipp Reisner and Lars Ellenberg.
 *
 * Copyright (C) 2001-2008, LINBIT Information Technologies GmbH.
 * Copyright (C) 1999-2008, Philipp Reisner <philipp.reisner@linbit.com>.
 * Copyright (C) 2002-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.
 *
 * drbd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * drbd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with drbd; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include "drbdsetup.h"

/*
 * Actual "main". In a separate file so that it can be excluded from the unit
 * tests.
 */
int main(int argc, char **argv)
{
	return drbdsetup_main(argc, argv);
}
