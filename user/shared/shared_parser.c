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

#include "shared_parser.h"
#include "drbdadm.h"
#include "drbdadm_parser.h"

extern void my_parse(void);


void save_parse_context(struct include_file_buffer *buffer, FILE *f, char *name)
{
	buffer->line = line;
	buffer->config_file = config_file;
	buffer->config_save = config_save;

	line = 1;
	config_file = name;
	config_save = canonify_path(name);

	my_yypush_buffer_state(f);

}

void restore_parse_context(struct include_file_buffer *buffer)
{
	yypop_buffer_state();

	line = buffer->line;
	config_file = buffer->config_file;
	config_save = buffer->config_save;
}

void include_file(FILE *f, char *name)
{
	struct include_file_buffer buffer;

	save_parse_context(&buffer, f, name);
	my_parse();
	restore_parse_context(&buffer);
}
