/* -*- mode: C; c-basic-offset: 4; -*- */

/* bmp_in.c - part of eblook, interactive EB interface command
 *
 *  Decoder for Windows BMP format without compression
 *
 * Copyright (C) 2001 T. Nemoto.
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

/* #define DEBUG */
/* #define BMPTEST */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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

#ifndef BI_BITFIELDS
#define BI_RGB		0
#define BI_RLE8		1
#define BI_RLE4		2
#define BI_BITFIELDS	3
#endif	/* not BI_BITFIELDS */

static ssize_t read_data_from_eb(unsigned char *buf, int len);
static unsigned long get_dword(unsigned char *p);

void deflate_rle4  (unsigned char *output, unsigned char *palette, int width, int height, int dir);
void deflate_rle8  (unsigned char *output, unsigned char *palette, int width, int height, int dir);
void deflate_rgb4  (unsigned char *output, unsigned char *palette, int width, int height, int dir);
void deflate_rgb8  (unsigned char *output, unsigned char *palette, int width, int height, int dir);
void deflate_rgb1  (unsigned char *output, unsigned char *palette, int width, int height, int dir);
void deflate_rgb24 (unsigned char *output, unsigned char *palette, int width, int height, int dir);


#define BMP_BUF_SIZE 2048
static unsigned char bmp_buf [BMP_BUF_SIZE];
static int bmp_buf_max_count = 0, bmp_buf_cur_count = 0;

void init_bmp_buf ()
{
    bmp_buf_max_count = read_data_from_eb(bmp_buf, BMP_BUF_SIZE);
    bmp_buf_cur_count = 0;
}

ssize_t read_bytes_from_bmp_buf (dest, count)
     unsigned char *dest;
     ssize_t count;
{
    ssize_t send;
    send = 0;

    while (send < count) {
	if (bmp_buf_cur_count >= bmp_buf_max_count) {
	    if (bmp_buf_max_count == BMP_BUF_SIZE) {
		bmp_buf_max_count = read_data_from_eb(bmp_buf, BMP_BUF_SIZE);
		if (bmp_buf_max_count <= 0)
		    return send;
		bmp_buf_cur_count = 0;
	    } else
		return send;
	}

	*(dest+send) = bmp_buf[bmp_buf_cur_count];
	send++;
	bmp_buf_cur_count++;
    }
    return send;
}

ssize_t seek_bytes_bmp_buf (count)
     ssize_t count;
{
    ssize_t seeked;
    seeked = 0;

    while (seeked < count) {
	if (bmp_buf_cur_count >= bmp_buf_max_count) {
	    if (bmp_buf_max_count == BMP_BUF_SIZE) {
		bmp_buf_max_count = read_data_from_eb(bmp_buf, BMP_BUF_SIZE);
		if (bmp_buf_max_count <= 0)
		    return seeked;
		bmp_buf_cur_count = 0;
	    } else
		return seeked;
	}

	if ((count - seeked) < (bmp_buf_max_count -  bmp_buf_cur_count)) {
	    bmp_buf_cur_count += (count - seeked);
	    seeked = count;
	} else {
	    seeked += bmp_buf_max_count -  bmp_buf_cur_count;
	    bmp_buf_cur_count = bmp_buf_max_count;
	}
    }
    return seeked;
}

static ssize_t read_data_from_eb(buf, len)
     unsigned char *buf;
     int len;
{
    ssize_t read_length;
    ssize_t count = 0;
    while(len>0) {
	if (eb_read_binary(&current_book, len, (char *)(buf + count), &read_length)
	    != EB_SUCCESS)
	    return -1;
	if (read_length == 0)
	    return count;
	len -= read_length;
	count += read_length;
    }
    return count;
}

static unsigned long get_dword(p)
     unsigned char *p;
{
    unsigned char *buf;
    buf = (unsigned char *) p;
    return (((unsigned long)(buf[3]))<<24) +
	(((unsigned long)(buf[2]))<<16) +
	(((unsigned long)(buf[1]))<<8) +
	(unsigned long)buf[0];
}

unsigned char *
LoadBMP(w,h)
     int *w;
     int *h;
{
    unsigned char buf [14 + 124], palette[4 * 256];
    int depth;
    unsigned long header_size, palette_size, image_offset;
    int width, height, dir, i, n_pal;
    unsigned char *output = NULL;

    init_bmp_buf ();
    /*Check BMP magic number and header size.*/
    if ((read_bytes_from_bmp_buf (buf, 14 + 20) != 14 + 20) ||
	buf[0] != 'B' || buf[1] != 'M') {
	fprintf(stderr,"Not BMP format.\n");
	goto failed;
    }

    header_size  = get_dword(buf + 14);
    image_offset = get_dword(buf + 10);

    /*Check header type.*/
    switch (header_size) {
    case  40: /*INFO header*/
    case  52:
    case  60:
    case  96: /*V4 header*/
    case 108:
    case 112:
    case 120:
    case 124: /*V5 header*/
	break;
    case  12: /*CORE header*/
    default:
	goto err_header;
    }

    if (read_bytes_from_bmp_buf (buf + 14 + 20, header_size - 20) != header_size - 20)
	goto err_data_end;
    if (image_offset > (14 + header_size))
	image_offset -= (14 + header_size);
    else
	image_offset = 0;

    *w = (int) get_dword (buf + 14 + 4);
    *h = (int) get_dword (buf + 14 + 8);
    if (*h < 0) {
	*h = -*h;
	dir = 1;
    } else
	dir = -1;
    width = *w;
    height = *h;

    output = malloc(width * height * 3);
    if (output == NULL)
	goto err_memory;

    depth = buf[14 + 14];
    /*Check bitcount.*/
    switch (depth) {
    case 1:
    case 4:
    case 8:
	/*Check and set palette.*/
	if (image_offset >= 4)
	    n_pal = (((image_offset / 4) - 1) & 0xff) + 1;
	else {
	    n_pal = get_dword (buf+14+32);
	    if (n_pal != 0)
		n_pal = ((n_pal - 1) & 0xff) + 1;
	    else
		n_pal = 1 << depth;
	}

	if (read_bytes_from_bmp_buf (palette, 4 * n_pal) != 4 * n_pal)
	    goto err_data_end;
	if (image_offset > 4 * n_pal)
	    image_offset -= 4 * n_pal;
	else
	    image_offset = 0;
	break;
    case 24:
	break;
    case 16:
    case 32:
    default:
	goto err_depth;
    }

    if (image_offset > 0)
	if (seek_bytes_bmp_buf(image_offset) != image_offset)
	    goto err_data_end;

    /*Check compression type*/
    switch(buf[14 + 16]) {
    case BI_RGB:
	switch(depth) {
	case 1:
	    deflate_rgb1 (output, palette, width, height, dir);
	    break;
	case 4:
	    deflate_rgb4 (output, palette, width, height, dir);
	    break;
	case 8:
	    deflate_rgb8 (output, palette, width, height, dir);
	    break;
	case 24:
	    deflate_rgb24 (output, palette, width, height, dir);
	    break;
	default:
	    goto err_depth;
	}
	break;
    case BI_RLE8:
	deflate_rle8 (output, palette, width, height, dir);
	break;
    case BI_RLE4:
	deflate_rle4 (output, palette, width, height, dir);
	break;
    case BI_BITFIELDS:
    default:
	goto err_compression;
    }
    return output;

 err_compression:
    fprintf(stderr,"Unsupported compression code.\n");
    goto failed;
 err_depth:
    fprintf(stderr,"Unsupported bitcount.\n");
    goto failed;
 err_header:
    fprintf(stderr,"Unsupported header format.\n");
    goto failed;
 err_memory:
    fprintf(stderr,"Memory error.\n");
    goto failed;
 err_data_end:
    fprintf(stderr,"Unexpected data end. \n");
    goto failed;
 failed:
    if (output != NULL)
	free (output);
    return NULL;
}

void deflate_rle4 (output, palette, width, height, dir)
     unsigned char *output;
     unsigned char *palette;
     int width;
     int height;
     int dir;
{
    int x, y, y0, l_length, i, col[2];
    unsigned char buf[2 + 256];

    x = 0;
    if (dir > 0)
	y = 0;
    else
	y = height -1;

    do {
	if (read_bytes_from_bmp_buf (buf, 2) != 2)
	    goto failed;
	switch(buf[0]) {
	case 0:
	    switch (buf[1]) {
	    case 0:
		x = 0;
		y += dir;

		if (y < 0 || y >= height)
		    goto check_eob;
		break;
	    case 1:
		goto rle4_end;
	    case 2:
		if (read_bytes_from_bmp_buf (buf, 2) != 2)
		    goto failed;
		x += buf[0];
		y += buf[1] * dir;

		if (y < 0 || y >= height)
		    goto check_eob;
		break;
	    default:
		if (x + buf[1] > width)
		    goto illegal;
		l_length = (buf[1]+1) / 2;
		l_length += l_length % 2;
		if (read_bytes_from_bmp_buf (buf+2, l_length) != l_length)
		    goto failed;
		for(i=0; i < buf[1]; i++) {
		    if ((i % 2) == 0) {
			col[0] = (buf[i/2 + 2] & 0xf0) >> 4;
		    } else {
			col[0] =  buf[i/2 + 2] & 0x0f;
		    }
		    output[(x+y*width)*3 + 0]=palette[col[0]*4 + 2];
		    output[(x+y*width)*3 + 1]=palette[col[0]*4 + 1];
		    output[(x+y*width)*3 + 2]=palette[col[0]*4 + 0];
		    x++;
		}
	    }
	    break;
	default:
	    if (x + buf[0] > width)
		goto illegal;
	    col[0] = (buf[1] & 0xf0) >> 4;
	    col[1] =  buf[1] & 0x0f;
	    for(i=0; i < buf[0]; i++) {
		output[(x+y*width)*3 + 0]=palette[col[i % 2]*4 + 2];
		output[(x+y*width)*3 + 1]=palette[col[i % 2]*4 + 1];
		output[(x+y*width)*3 + 2]=palette[col[i % 2]*4 + 0];
		x++;
	    }

	}
    } while (1);

 failed:
    fprintf(stderr,"(RLE4)Unexpected data end.\n");
    return;
 illegal:
    fprintf(stderr,"(RLE4)Reached illegal point.\n");
    return;
 check_eob:
    if (read_bytes_from_bmp_buf (buf, 2) != 2)
	goto failed;
    if (buf[0] != 0 || buf[1] != 1)
	goto illegal;
 rle4_end:
    return;
}



void deflate_rle8 (output, palette, width, height, dir)
     unsigned char *output;
     unsigned char *palette;
     int width;
     int height;
     int dir;
{
    int x, y, l_length, i;
    unsigned char buf[2 + 256];

    x = 0;
    if (dir > 0)
	y = 0;
    else
	y = height - 1;
    do {
	if (read_bytes_from_bmp_buf (buf, 2) != 2)
	    goto failed;
	switch(buf[0]) {
	case 0:
	    switch (buf[1]) {
	    case 0:
		x = 0;
		y += dir;

		if (y < 0 || y >=height)
		    goto check_eob;
		break;
	    case 1:
		goto rle8_end;
	    case 2:
		if (read_bytes_from_bmp_buf (buf, 2) != 2)
		    goto failed;
		x += buf[0];
		y += buf[1] * dir;

		if (y < 0 || y >= height)
		    goto check_eob;
		break;
	    default:
		if (x + buf[1] > width)
		    goto illegal;
		l_length = buf[1] + (buf[1] & 1);
		if (read_bytes_from_bmp_buf (buf+2, l_length) != l_length)
		    goto failed;
		for(i=0; i < buf[1]; i++) {
		    output[(x+y*width)*3 + 0]=palette[buf[i + 2]*4 + 2];
		    output[(x+y*width)*3 + 1]=palette[buf[i + 2]*4 + 1];
		    output[(x+y*width)*3 + 2]=palette[buf[i + 2]*4 + 0];
		    x++;
		}
	    }
	    break;
	default:
	    if (x + buf[0] > width)
		goto illegal;
	    for(i=0; i < buf[0]; i++) {
		output[(x+y*width)*3 + 0]=palette[buf[1]*4 + 2];
		output[(x+y*width)*3 + 1]=palette[buf[1]*4 + 1];
		output[(x+y*width)*3 + 2]=palette[buf[1]*4 + 0];
		x++;
	    }

	}
    } while (1);

 failed:
    fprintf(stderr,"(RLE8)Unexpected data end.\n");
    return;
 illegal:
    fprintf(stderr,"(RLE8)Reached illegal point.\n");
    return;
 check_eob:
    if (read_bytes_from_bmp_buf (buf, 2) != 2)
	goto failed;
    if (buf[0] != 0 || buf[1] != 1)
	goto illegal;
 rle8_end:
    return;
}

void deflate_rgb8 (output, palette, width, height, dir)
     unsigned char *output;
     unsigned char *palette;
     int width;
     int height;
     int dir;
{
    int x, y, y0, skipbytes, col;
    unsigned char buf[1];

    if (dir > 0)
	y = 0;
    else
	y = height - 1;

    skipbytes = (4 - (width % 4)) % 4;
    for (y0 = 0; y0 < height; y0++, y += dir) {
	for (x = 0; x < width; x++) {
	    if (read_bytes_from_bmp_buf (buf, 1) != 1)
		goto failed;
	    col = buf[0];
	    output[(x+y*width)*3 + 0]=palette[col*4 + 2];
	    output[(x+y*width)*3 + 1]=palette[col*4 + 1];
	    output[(x+y*width)*3 + 2]=palette[col*4 + 0];
	}
	if (skipbytes != 0) {
	    if (seek_bytes_bmp_buf (skipbytes) != skipbytes)
		goto failed;
	}
    }
    return;
 failed:
    fprintf(stderr,"(RGB8)Unexpected data end.\n");
    return;
}

void deflate_rgb4 (output, palette, width, height, dir)
     unsigned char *output;
     unsigned char *palette;
     int width;
     int height;
     int dir;
{
    int x, y, y0, skipbytes, col;
    unsigned char buf[1];

    if (dir > 0)
	y = 0;
    else
	y = height - 1;

    skipbytes = (4 - (((width + 1) / 2) % 4)) % 4;
    for (y0 = 0; y0 < height; y0++, y += dir) {
	for (x = 0; x < width; x++) {
	    if ((x % 2) == 0) {
		if (read_bytes_from_bmp_buf (buf, 1) != 1)
		    goto failed;
		col = (buf[0] & 0xf0) >> 4;
	    } else
		col = buf[0] & 0x0f;
	    output[(x+y*width)*3 + 0]=palette[col*4 + 2];
	    output[(x+y*width)*3 + 1]=palette[col*4 + 1];
	    output[(x+y*width)*3 + 2]=palette[col*4 + 0];
	}
	if (skipbytes != 0)
	    if (seek_bytes_bmp_buf(skipbytes) != skipbytes)
		goto failed;
    }
    return;
 failed:
    fprintf(stderr,"(RGB4)Unexpected data end.\n");
    return;
}


void deflate_rgb1 (output, palette, width, height, dir)
     unsigned char *output;
     unsigned char *palette;
     int width;
     int height;
     int dir;
{
    int x, y, y0, skipbytes, col;
    unsigned char buf[1];

    if (dir > 0)
	y = 0;
    else
	y = height - 1;

    skipbytes = (4 - (((width + 7) / 8) % 4)) % 4;
    for (y0 = 0; y0 < height; y0++, y+=dir) {
	for (x = 0; x < width; x++) {
	    if ((x % 8) == 0)
		if (read_bytes_from_bmp_buf (buf, 1) != 1)
		    goto failed;
	    col = (buf[0] & (0x80 >> (x % 8))) >> (7- (x % 8));
	    output[(x+y*width)*3 + 0]=palette[col*4 + 2];
	    output[(x+y*width)*3 + 1]=palette[col*4 + 1];
	    output[(x+y*width)*3 + 2]=palette[col*4 + 0];
	}

	if (skipbytes != 0) {
	    if (seek_bytes_bmp_buf (skipbytes) != skipbytes)
		goto failed;
	}
    }

    return;
 failed:
    fprintf(stderr,"(RGB1)Unexpected data end.\n");
    return;
}


void deflate_rgb24 (output, palette, width, height, dir)
     unsigned char *output;
     unsigned char *palette;
     int width;
     int height;
     int dir;
{
    int x, y, y0, skipbytes, col;
    unsigned char buf[3];

    if (dir > 0)
	y = 0;
    else
	y = height - 1;

    skipbytes = (4 - ((width * 3) % 4)) % 4;
    for (y0 = 0; y0 < height; y0++, y += dir) {
	for (x = 0; x < width; x++) {
	    if (read_bytes_from_bmp_buf (buf, 3) != 3)
		goto failed;
	    output[(x+y*width)*3 + 0]=buf[2];
	    output[(x+y*width)*3 + 1]=buf[1];
	    output[(x+y*width)*3 + 2]=buf[0];
	}

	if (skipbytes != 0) {
	    if (seek_bytes_bmp_buf(skipbytes) != skipbytes)
		goto failed;
	}
    }
    return;
 failed:
    fprintf(stderr,"(RGB24)Unexpected data end.\n");
    return;
}
