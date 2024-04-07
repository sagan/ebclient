/*
 *  win32.c
 *  functions used in Win32 (or DOS) environment
 *  written by Satomi <satomi@ring.gr.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 *  Boston, MA 02111-1307 USA
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#if defined(WIN32) && !defined(DOS_FILE_PATH)
#define DOS_FILE_PATH 1
#endif

#ifdef DOS_FILE_PATH

#include <stddef.h>
#include <string.h>

size_t euc_to_sjis(const char *, char *, size_t);
size_t sjis_to_euc(const char *, char *, size_t);
void dos_fix_path(char *, int);

/*
 *  NOTE: euc_to_sjis() and sjis_to_euc() may be moved to codeconv.c,
 *  but their prototypes are rather different from other codeconv
 *  functions, which is the reason why I placed them here.
 *  euc_to_current() or current_to_euc() cannot be used as an alternative
 *  because the target kanji-code is always SJIS.
 */
size_t euc_to_sjis(const char *euc, char *sjis, size_t sjis_len)
{
	size_t len = 0;
	unsigned char c1, c2;

    while (*euc) {
		c1 = (unsigned char)(*euc++);
	    if (!(c1 & 0x80)) {
			/* ASCII char. */
			if (++len < sjis_len) *sjis++ = c1;
            continue;
	    }

		c2 = (unsigned char)(*euc++);
		if (0x8e == c1) {
			/* possibly single-byte kana. */
			if (0x21 <= c2 && c2 <= 0x5f) *sjis++ = c2 | 0x80;
			continue;
		}

		/* double-byte char. */
		len += 2;
		if (sjis_len <= len) continue;
	    c1 &= 0x7f;
		c2 &= 0x7f;
		if (c1 & 0x01) {
	        c2 += 0x1f;
	        if (c2 > 0x7e) c2++;
	    } else {
	        c2 += 0x7e;
	    }
	    c1 = (c1 + 0xe1) >> 1;
	    if (c1 > 0x9f) c1 += 0x40;
		*sjis++ = c1;
	    *sjis++ = c2;
    }

	if (sjis) *sjis = 0;
	return(len);
}

size_t sjis_to_euc(const char *sjis, char *euc, size_t euc_len)
{
	size_t len = 0;
	unsigned char c1, c2;

	while (*sjis) {
		c1 = (unsigned char)(*sjis++);
		if (!(c1 & 0x80)) {
			/* ASCII char. */
			if (++len < euc_len) *euc++ = c1;
			continue;
		}

		if (0xa1 <= c1 && c1 <= 0xdf) {
			/* single-byte kana. */
			len += 2;
			if (euc_len <= len) continue;
			c2 = c1 - 0x80;
			c1 = 0x8e;

		} else if ((0x81 <= c1 && c1 <= 0x9f)
				|| (0xe0 <= c1 && c1 <= 0xfc)) {
			/* double-byte char (not strictly checked). */
			c2 = *sjis++;
			if (!c2) break;
			len += 2;
			if (euc_len <= len) continue;
			if (c2 < 0x9f) {
				if (c1 < 0xdf) c1 = ((c1 - 0x30) << 1) - 1;
				else c1 = ((c1 - 0x70) << 1) - 1;
				if (c2 < 0x7f) c2 += 0x61;
				else c2 += 0x60;
			} else {
				if (c1 < 0xdf) c1 = (c1 - 0x30) << 1;
				else c1 = (c1 - 0x70) << 1;
				c2 += 0x02;
			}
		}

		*euc++ = c1;
		*euc++ = c2;
	}

	if (euc) *euc = 0;
	return(len);
}

void dos_fix_path(char *path, int path_is_euc)
{
	unsigned char *p = (unsigned char *)path;

	if (path_is_euc) euc_to_sjis(path, path, strlen(path) + 1);

	/* eb-3.0 does not accept '/' as a path delimitor. */
	while (*p) {
		if ((0x81 <= *p && *p <= 0x9f) ||
			(0xe0 <= *p && *p <= 0xfc))
			p++;
		else if ('/' == *p) *p = '\\';
		p++;
	}
}

#endif /* DOS_FILE_PATH */
