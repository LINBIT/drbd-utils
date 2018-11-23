/*
   drbdadm_parser.h a hand crafted parser

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2006-2008, LINBIT Information Technologies GmbH
   Copyright (C) 2006-2008, Philipp Reisner <philipp.reisner@linbit.com>
   Copyright (C) 2006-2008, Lars Ellenberg  <lars.ellenberg@linbit.com>

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


enum range_checks
{
	R_MINOR_COUNT,
	R_DIALOG_REFRESH,
	R_PORT,
	R_META_IDX,
	R_NODE_ID,
};

enum yytokentype {
	TK_GLOBAL = 258,
	TK_RESOURCE,
	TK_ON,
	TK_STACKED,
	TK_NET,
	TK_DISK,
	TK_SKIP,
	TK_SYNCER, /* depricated after 8.3 */
	TK_STARTUP,
	TK_DISABLE_IP_VERIFICATION,
	TK_DIALOG_REFRESH,
	TK_PROTOCOL,
	TK_HANDLER,
	TK_COMMON,
	TK_ADDRESS,
	TK_DEVICE,
	TK_MINOR,
	TK_META_DISK,
	TK_FLEX_META_DISK,
	TK_NODE_ID,
	TK_MINOR_COUNT,
	TK_IPADDR,
	TK_INTEGER,
	TK_STRING,
	TK_ELSE,
	TK_USAGE_COUNT,
	TK_ASK,
	TK_YES,
	TK_NO,
	TK__THIS_HOST,
	TK__REMOTE_HOST,
	TK__PEER_NODE_ID,
	TK__IS_STANDALONE,
	TK_PROXY,
	TK_INSIDE,
	TK_OUTSIDE,
	TK_MEMLIMIT,
	TK_ERR_STRING_TOO_LONG,
	TK_ERR_DQSTRING_TOO_LONG,
	TK_ERR_DQSTRING,
	TK_SCI,
	TK_SDP,
	TK_SSOCKS,
	TK_IPV4,
	TK_IPV6,
	TK_IPADDR6,
	TK_INCLUDE,
	TK_BWLIMIT,
	TK_FLOATING,
	TK_VOLUME,
	TK_CMD_TIMEOUT_SHORT,
	TK_CMD_TIMEOUT_MEDIUM,
	TK_CMD_TIMEOUT_LONG,
	TK_OPTIONS,
	TK_CONNECTION,
	TK_HOST,
	TK_PORT,
	TK_CONNECTION_MESH,
	TK_HOSTS,
	TK_VIA,
	TK_TEMPLATE_FILE,
	TK_PATH,
	TK_UDEV_ALWAYS_USE_VNR,
	TK_MODULE_EQ,
	TK_MODULE_NE,
	TK_MODULE_GT,
	TK_MODULE_GE,
	TK_MODULE_LT,
	TK_MODULE_LE,
	TK_KMODVERS,
};

typedef struct YYSTYPE {
	char* txt;
} YYSTYPE;

#define yystype YYSTYPE /* obsolescent; will be withdrawn */
#define YYSTYPE_IS_DECLARED 1
#define YYSTYPE_IS_TRIVIAL 1

extern yystype yylval;
extern char* yytext;
extern FILE* yyin;

void my_parse(void);

/* avoid compiler warnings about implicit declaration */
int yylex(void);
void my_yypush_buffer_state(FILE *f);
void yypop_buffer_state (void );
void yyrestart(FILE *input_file);
void free_btrees(void);
void check_minor_nonsense(const char *devname, const int explicit_minor);
void pe_expected(const char *exp);
void check_string_error(int got);
void pe_expected_got(const char *exp, int got);

#define EXP(TOKEN1)						\
({								\
	int token;						\
	token = yylex();					\
	if (token != TOKEN1) {					\
		if (TOKEN1 == TK_STRING)			\
			check_string_error(token);		\
		pe_expected_got( #TOKEN1, token);		\
	}							\
	token;							\
})

