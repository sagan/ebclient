/* -*- mode: C; c-basic-offset: 4; -*- */
/* bmp2ppm.c - part of eblook, interactive EB interface command
 *
 * Copyright (C) 2001 Yamagata, T. Nemoto.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef ENABLE_EBU
#include <ebu/eb.h>
#include <ebu/text.h>
#include <ebu/font.h>
#include <ebu/appendix.h>
#include <ebu/error.h>
#include <ebu/binary.h>
#else
#include <eb/eb.h>
#include <eb/text.h>
#include <eb/font.h>
#include <eb/appendix.h>
#include <eb/error.h>
#include <eb/binary.h>
#endif

extern EB_Book current_book;

int parse_entry_id (char *code,EB_Position *pos);

extern unsigned char *LoadBMP (int *w, int *h);

void command_bmp2tiff (int argc, char *argv[]);
void command_bmp2ppm (int argc, char *argv[]);
static int tiff_put_word (FILE *fp, unsigned short w);
static int tiff_put_dword (FILE *fp, unsigned long dw);
/* static int tifftag_write_byte (FILE *fp, int id, unsigned char ch); */
static int tifftag_write_word (FILE *fp, int id, unsigned short val);
static int tifftag_write_dword (FILE *fp, int id, unsigned long val);
static int tifftag_write_words (FILE *fp, int id, unsigned char *buf,
				     int *bp, int base, int count, 
				     const unsigned short *val);
static int tifftag_write_string (FILE *fp, int id, unsigned char *buf,
				      int *bp, int base, const char *val);



void
command_bmp2ppm(argc, argv)
     int argc;
     char *argv[];
{
    EB_Error_Code error_code;
    EB_Position pos;
    int w, h;
    FILE *fp;
    unsigned char *p;
    const char *ppm_error = NULL;
    
    if (argc != 3) {
        ppm_error = "parameter error";
        goto ppm_fail2;
    }
    if (parse_entry_id(argv[1], &pos) == 0) {
	ppm_error = "entry address error";
	goto ppm_fail2;
    }

    error_code = eb_set_binary_color_graphic(&current_book, &pos);
    if (error_code != EB_SUCCESS) {
	ppm_error = "data read error";
        goto ppm_fail2;
    }
    p = LoadBMP(&w, &h);
    if (p == NULL) {
	ppm_error = "bmp decode error";
        goto ppm_fail2;
    }

    fp = fopen(argv[2], "wb");
    if (fp == NULL) {
	ppm_error = "output file open error";
        goto ppm_fail3;
    }

    if (fprintf(fp, "P6\n%d %d\n255\n", w, h) != 0 &&
	1 == fwrite(p, 3*w*h, 1, fp)) {
        fclose(fp);
	free(p);
	printf("OK\n");
	return;
    } 
    ppm_error = "file write error";
/* ppm_fail: */
    fclose(fp);
    unlink(argv[2]);
 ppm_fail3:
    free(p);
 ppm_fail2:
    if (ppm_error == NULL) 
	printf("NG\n");
    else 
	printf("NG: bmp2ppm : %s\n", ppm_error);
}

#if 0
static int 
tiff_put_byte(fp, ch)
     FILE *fp;
     unsigned char ch;
{
    return putc(255 & ch, fp);
}
#endif

static int 
tiff_put_word(fp, w)
     FILE *fp;
     unsigned short w;
{
    if (EOF == putc(255 & (w >> 8), fp))
        return EOF;
    return putc(255 & w, fp);
}

static int 
tiff_put_dword(fp, dw)
     FILE *fp;
     unsigned long dw;
{
    if (EOF == tiff_put_word(fp, 0xffff & (dw >> 16)))
        return EOF;
    return tiff_put_word(fp, 0xffff & dw);
}

#if 0
static int
tifftag_write_byte(fp, id, ch)
     FILE *fp;
     int id;
     unsigned char ch;
{
    if (EOF == tiff_put_word(fp, id))
        return EOF;
    if (EOF == tiff_put_word(fp, 1))	/* TIFF_BYTE */
        return EOF;
    if (EOF == tiff_put_dword(fp, 1))	/* length */
        return EOF;
    if (EOF == tiff_put_byte(fp, ch))	
        return EOF;
    if (EOF == tiff_put_byte(fp, 0))	/* padding 1/3 */
        return EOF;
    return tiff_put_word(fp, 0);	/* padding 2/3 */
}
#endif

static int
tifftag_write_word(fp, id, val)
     FILE *fp;
     int id;
     unsigned short val;
{
    if (EOF == tiff_put_word(fp, id))
        return EOF;
    if (EOF == tiff_put_word(fp, 3))	/* TIFF_SHORT */
        return EOF;
    if (EOF == tiff_put_dword(fp, 1))	/* length */
        return EOF;
    if (EOF == tiff_put_word(fp, val))	
        return EOF;
    return tiff_put_word(fp, 0);	/* padding */
}

static int
tifftag_write_dword(fp, id, val)
     FILE *fp;
     int id;
     unsigned long val;
{
    if (EOF == tiff_put_word(fp, id))
        return EOF;
    if (EOF == tiff_put_word(fp, 4))	/* TIFF_LONG */
        return EOF;
    if (EOF == tiff_put_dword(fp, 1))	/* length */
        return EOF;
    return tiff_put_dword(fp, val);
}

static int
tifftag_write_words(fp, id, buf, bp, base, count, val)
     FILE *fp;
     int id;
     unsigned char *buf;
     int *bp;
     int base;
     int count;
     const unsigned short *val;
{
    if (EOF == tiff_put_word(fp, id))
        return EOF;
    if (EOF == tiff_put_word(fp, 3))		/* TIFF_SHORT */
        return EOF;
    if (EOF == tiff_put_dword(fp, count))	/* length */
        return EOF;
    if (EOF == tiff_put_dword(fp, base+*bp))
        return EOF;
    while(count > 0) {
        buf[(*bp)++] = 255 & (*val >> 8);
	buf[(*bp)++] = 255 & *val;
	val++;
	count--;
    }
    return 1;
}

static int
tifftag_write_string(fp, id, buf, bp, base, val)
     FILE *fp;
     int id;
     unsigned char *buf;
     int *bp;
     int base;
     const char *val;
{
    int count;
    count = strlen(val) + 1;
    if (EOF == tiff_put_word(fp, id))
        return EOF;
    if (EOF == tiff_put_word(fp, 2))		/* TIFF_ASCII */
        return EOF;
    if (EOF == tiff_put_dword(fp, count))	/* length */
        return EOF;
    if (EOF == tiff_put_dword(fp, base+*bp))
        return EOF;
    memcpy(buf+*bp, val, count);
    *bp += count;
    return 1;
}

void
command_bmp2tiff(argc, argv)
     int argc;
     char *argv[];
{
    unsigned char binary_data[EB_SIZE_PAGE];
    int binary_data_pointer;
    EB_Error_Code error_code;
    EB_Position pos;
    unsigned int end, base;
    int w, h;
    FILE *fp;
    unsigned char *p;
    const unsigned short dep[3] = {8, 8, 8};
    int n_tags;
    const char *tiff_error = NULL;

    if (argc != 3) {
	tiff_error = "parameter error";
        goto tiff_fail3;
    }

    binary_data_pointer = 0;
    if (parse_entry_id(argv[1], &pos) == 0) {
	tiff_error = "entry address error";
        goto tiff_fail3;
    }

    error_code = eb_set_binary_color_graphic(&current_book, &pos);
    if (error_code != EB_SUCCESS) {
	tiff_error = "bmp read error";
        goto tiff_fail3;
    }

    p = LoadBMP(&w, &h);
    if (p == NULL) {
	tiff_error = "bmp decode error";
        goto tiff_fail3;
    }

    fp = fopen(argv[2], "wb");
    if (fp == NULL) {
	tiff_error = "output file open error";
	goto tiff_fail2;
    }

    end = w*h*3+8;  /* Bitmap Size + TIFF Header Size */

    /* Write Header */
    n_tags = 11;
    base = end + 12 * n_tags + 6; /* end of TIFF directory */
    /* TIFF magic number (big endian) / TIFF version 4.2 */
    if (fwrite("\x4d\x4d\x00\x2a",1,4,fp) != 4 ||
	/* TIFF directory offset */
	tiff_put_dword(fp, (unsigned long) end) == EOF ||
    
	/* Write Bitmap */
	3*w*h != fwrite(p, 1, 3*w*h, fp) ||

	/* Write Information */
	/* TIFF directory count */
	tiff_put_word(fp, n_tags) == EOF ||
	/* TIFFTAG_IMAGEWIDTH */
	tifftag_write_word(fp, 256, w) == EOF ||
	/* TIFFTAG_IMAGELENGTH */
	tifftag_write_word(fp, 257, h) == EOF ||
	/* TIFFTAG_BITSPARSAMPLE */
	tifftag_write_words(fp, 258, binary_data, &binary_data_pointer,
			    base, 3, dep) == EOF ||
	/* TIFFTAG_COMPRESSION */
	tifftag_write_word(fp, 259, 1) == EOF ||
	/* TIFFTAG_PHOTOMETRIC */
	tifftag_write_word(fp, 262, 2) == EOF ||
	/* TIFFTAG_IMAGEDESCRIPTION */
	tifftag_write_string(fp, 270, binary_data, &binary_data_pointer, base,
		 "eblook temporary data. Don't copy this file!") == EOF ||
	/* TIFFTAG_STRIPOFFSET */
	tifftag_write_dword(fp, 273, 8) == EOF ||
	/* TIFFTAG_ORIENTATION */
	tifftag_write_word(fp, 274, 1) == EOF ||
	/* TIFFTAG_SAMPLESPARPIXEL */
	tifftag_write_dword(fp, 277, 3) == EOF ||
	/* TIFFTAG_STRIPBYTECOUNTS */
	tifftag_write_dword(fp, 279,3*w*h) == EOF ||
	/* TIFFTAG_SOFTWARE */
	tifftag_write_string(fp, 305, binary_data, &binary_data_pointer,
			     base, PACKAGE " " VERSION) == EOF ||
	/* TIFF directory end */
	tiff_put_dword(fp, 0) == EOF ||
	fwrite(binary_data,1,binary_data_pointer,fp) != binary_data_pointer
	) {
	tiff_error = "output file write error";
        goto tiff_fail;
    }
    fclose(fp);
    free(p);
    printf("OK\n");
    return;

 tiff_fail:
    fclose(fp);
    unlink(argv[2]);
 tiff_fail2:
    free(p);
 tiff_fail3:
    if (tiff_error == NULL) 
	printf("NG\n");
    else 
	printf("NG: bmp2tiff: %s\n",tiff_error);
    return;
}
