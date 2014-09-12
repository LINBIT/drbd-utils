/*
 *
   drbdadm_parser.c a hand crafted parser

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2014, LINBIT HA Solutions GmbH

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 */


#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glob.h>
#include <search.h>
#include <fcntl.h>
#include <unistd.h>

#include "drbdadm.h"
#include "drbdadm_parser.h"

extern void my_parse(void);
extern int vcheck_uniq(struct hsearch_data *ht, const char *what, const char *fmt, va_list ap);
extern struct hsearch_data per_resource_htable;


void include_file(FILE *f, char *name)
{
	int saved_line;
	char *saved_config_file, *saved_config_save;

	saved_line = line;
	saved_config_file = config_file;
	saved_config_save = config_save;
	line = 1;
	config_file = name;
	config_save = canonify_path(name);

	my_yypush_buffer_state(f);
	my_parse();
	yypop_buffer_state();

	line = saved_line;
	config_file = saved_config_file;
	config_save = saved_config_save;
}

int check_uniq(const char *what, const char *fmt, ...)
{
	int rv;
	va_list ap;

	va_start(ap, fmt);
	rv = vcheck_uniq(&global_htable, what, fmt, ap);
	va_end(ap);

	return rv;
}

/* unique per resource */
int check_upr(const char *what, const char *fmt, ...)
{
	int rv;
	va_list ap;

	va_start(ap, fmt);
	rv = vcheck_uniq(&per_resource_htable, what, fmt, ap);
	va_end(ap);

	return rv;
}


