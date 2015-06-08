/*
   shared_parser.h

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2014, LINBIT HA Solutions GmbH.

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

#ifndef __SHARED_PARSER_H__
#define __SHARED_PARSER_H__

struct include_file_buffer {
	int line;
	char *config_file;
	char *config_save;
};

void include_file(FILE *f, char *name);
void save_parse_context(struct include_file_buffer *buffer, FILE *f, char *name);
void restore_parse_context(struct include_file_buffer *buffer);

int check_uniq(const char *what, const char *fmt, ...);
int check_upr(const char *what, const char *fmt, ...);


#endif
