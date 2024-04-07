/*
 * codeconv.h - header file for code conversion
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * Written by Takashi Nemoto (tnemoto@mvi.biglobe.ne.jp).
 * Modified by Kazuhiko <kazuhiko@ring.gr.jp>
 * Modified by Satomi <satomi@ring.gr.jp>
 */

#ifndef __CODECONV_H__
#define __CODECONV_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#define USE_STDARG_H

#define CODECONV_BUFFER_OVERFLOW ((size_t) -2)
#define CODECONV_ERROR ((size_t) -1)
#define CODECONV_OK ((size_t) 0)

#ifndef PROTO
#if defined(__STDC__)
#define PROTO(p) p
#else
#define PROTO(p) ()
#endif
#endif /* PROTO */

extern size_t locale_init PROTO((const char *encoding));
extern int xvfprintf PROTO((FILE *fp, const char *fmt, va_list ap));
extern int xfprintf PROTO((FILE *fp, const char *fmt, ...));
extern int xprintf PROTO((const char *fmt, ...));
extern int xfputs PROTO((const char *str, FILE *fp));
extern int xputs PROTO((const char *str));
extern char *xfgets PROTO((char *str, size_t size, FILE *fp));

extern char *euc_to_jis PROTO((char *jis, const char *euc, int len));
extern char *jis_to_euc PROTO((char *euc, const char *jis, int len));
extern size_t current_to_euc PROTO((char **current, size_t *in_len,
				    char **euc, size_t *out_len));

#endif
