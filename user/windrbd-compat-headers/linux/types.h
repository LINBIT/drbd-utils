/*
	Copyright(C) 2007-2016, ManTechnology Co., LTD.
	Copyright(C) 2007-2016, wdrbd@mantech.co.kr

	Windows DRBD is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	Windows DRBD is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Windows DRBD; see the file COPYING. If not, write to
	the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _WINDOWS_TYPES_H
#define _WINDOWS_TYPES_H

#include <ctype.h>
#include <stdbool.h>
typedef signed char		    __s8;
typedef unsigned char		__u8;
typedef signed short		__s16;
typedef unsigned short		__u16;
typedef signed int		    __s32;
typedef unsigned int		__u32;
typedef signed long long	__s64;
typedef unsigned long long	__u64;
typedef signed char		    s8;
typedef unsigned char		u8;
typedef signed short		s16;
typedef unsigned short		u16;
typedef signed int		    s32;
typedef unsigned int		u32;
typedef signed long long	s64;
typedef unsigned long long	u64;
typedef unsigned long long	sector_t;

typedef __u16		__le16;
typedef __u16		__be16;
typedef __u32		__le32;
typedef __u32		__be32;
typedef __u64		__le64;
typedef __u64		__be64;


typedef		__u8		u_int8_t;
typedef		__s8		int8_t;
typedef		__u16		u_int16_t;
typedef		__s16		int16_t;
typedef		__u32		u_int32_t;
typedef		__s32		int32_t;

typedef		__u8		uint8_t;
typedef		__u16		uint16_t;
typedef		__u32		uint32_t;

/* Cygwin types.h doesn't have off64_t. */
typedef int64_t off64_t;

#endif
