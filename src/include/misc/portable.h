/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Minimal portable.h for city.c compilation */

#ifndef MISC_PORTABLE_H
#define MISC_PORTABLE_H

#include <endian.h>
#include <byteswap.h>

/* Define endianness macros */
#if __BYTE_ORDER == __LITTLE_ENDIAN
/* Already the expected order for little endian */
#else
#define WORDS_BIGENDIAN 1
#endif

/* GCC builtin expect support */
#if defined(__GNUC__)
#define HAVE_BUILTIN_EXPECT 1
#endif

#endif /* MISC_PORTABLE_H */