/*
 * codeconv.c
 * Copyright(c) 2001 Takashi NEMOTO
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
 *
 */

/* #define DEBUG_CODECONV */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "codeconv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

#ifdef HAVE_ICONV_H
#  include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#ifdef HAVE_ICONV
static iconv_t cur_to_euc = (iconv_t)-1;
static iconv_t euc_to_cur = (iconv_t)-1;
static const char *eucjp_code_name = NULL;
#endif

/* Return code <0: error, -2: Output Buffer Overflow */
size_t current_to_euc PROTO((char **current, size_t *in_len,
			     char **euc, size_t *out_len));
static size_t euc_to_current PROTO((char **euc, size_t *in_len,
				   char **current, size_t *out_len));


enum CONV_MODE {IO_AUTO, IO_ICONV, IO_SJIS, IO_EUC} conv_mode;

static enum CONV_MODE detect_conv_mode PROTO((const char *encoding));
     



#define TMP_SIZE 10240

static int	xputs_raw	PROTO((const char *str, int len, FILE *fp));
static int	xputs2		PROTO((const char *str, int len, FILE *fp));

static const char *euc_jp_names[] = {
    "eucJP", "EUC-JP", "eucjp", "euc-jp", "EUCJP", "ujis", "UJIS",
    "euc", "EUC", NULL
};

static const char *shift_jis_names[] = {
    "SHIFT-JIS", "SHIFT_JIS", "SJIS", "CSSHIFTJIS", "SHIFTJIS", NULL
};

#ifdef HAVE_ICONV
static const char *iso_2022_jp_names[] = {
  "ISO-2022-JP-3", "ISO-2022-JP-2", "ISO-2022-JP", 
  "CSISO2022JP", "CSISO2022JP2", "CSISO2022JP3", 
  "ISO-2022", "ISO2022", "ISO2022JP", "ISO2022-JP", "JIS", NULL
};

static const char *japanese_names[] = {
    "ja", "japanese", NULL
};
#endif

static int match_str(str,str_list)
     const char *str;
     const char **str_list;
{
    const char **ptr;
    for(ptr=str_list;*ptr!=NULL;ptr++) {
      if (strcasecmp(str,*ptr)==0) return 1;
    }
    return 0;
}

/* 

 コード変換関数初期化手順 

 
 1. encoding が指定されていたら、まず指定された encoding を試す
   1a. まず 指定された encoding が EUC/SJIS かどうかを判定 => IO_EUC / IO_SJIS
   1b. だめなら iconv で変換できる encoding かを判定       => IO_ICONV
   以下同様。

 2. 既に初期化済みだったら ここで終了

 3. だめなら locale からの取得を試みる
   3a. nl_langinfo(CODESET) からの encoding 取り出しを試みる。
   3b. LC_CTYPE からの encoding 取り出しを試みる。
   2e. LC_CTYPE 後半部(.以降) からの encoding 取り出しを試みる。

 3. FALLBACK_ENCODING を試す

 4. あきらめて EUC_JP 

   問題になる例
   SJIS の locale 名が ja/japanese の場合
          iconv が有る場合 => EUC_JP の iconv 名は？
	  iconv が無い場合 => 4. で EUC_JP かな？
   locale が C とか en_US だったら？ => 当面 FALLBACK する。
        gettext 対応等の場合は要手直し。

 */

#ifdef HAVE_ICONV

static int 
setup_eucjp_code_name()
{
    const char **enc;
    iconv_t ic;
    if (eucjp_code_name == NULL) {
        for (enc = euc_jp_names; *enc != NULL; enc++) {
	    ic = iconv_open(*enc, *enc);
	    if (ic != (iconv_t)-1) {
	        eucjp_code_name = *enc;
		iconv_close(ic);
		break;
	    }
	}
	if (eucjp_code_name == NULL) {
	    /* EUC-JP 設定に失敗 - 曖昧な "ja" "japanese" も試す */
	    for (enc = japanese_names; *enc != NULL; enc++) {
	        ic = iconv_open(*enc,*enc);
		if (ic != (iconv_t)-1) {
		    eucjp_code_name = *enc;
		    iconv_close(ic);
		    break;
		}
	    }
	}
    }
    return eucjp_code_name != NULL;
}

/* Current locale の codeset で日本語が扱えるか？
   失敗なら 0, 成功なら 1 を返す */

static int 
iconv_test(ctoe, etoc)
     iconv_t ctoe, etoc;
{
  /* 文字列 "実験" */
#define TEST_STRING "\xBC\xC2\xB8\xB3"
#define TEST_LENGTH 50
    char test1_0[TEST_LENGTH],test2_0[TEST_LENGTH],test3_0[TEST_LENGTH];
    char *test1,*test2,*test3;
    size_t ilen,olen;

    if (ctoe == (iconv_t)-1 || etoc == (iconv_t)-1) 
        return 0;
    strcpy(test1_0,TEST_STRING); 
    test1=test1_0;
    test2=test2_0;
    test3=test3_0;
    ilen=strlen(TEST_STRING);
    olen=TEST_LENGTH;

    /* euc-jp => current code の変換テスト */
    if (iconv(etoc,&test1,&ilen,&test2,&olen) == ((size_t)-1))
        return 0;
    if (iconv(etoc,NULL,&ilen,&test2,&olen) == ((size_t)-1))
        return 0;

    /* current code から 元に戻るか */
    test2=test2_0;
    ilen=TEST_LENGTH-olen;
    olen=TEST_LENGTH;
    if (iconv(ctoe,&test2,&ilen,&test3,&olen) == ((size_t)-1)) 
        return 0;
    if (iconv(ctoe,NULL,&ilen,&test3,&olen) == ((size_t)-1))
        return 0;

    if (strncmp(test1_0,test3_0,strlen(test1_0)) != 0)
        return 0;

    return 1;
}

static int
iconv_setup(current_code_name) 
     const char *current_code_name;
{
    iconv_t ctoe,etoc;
    static int disable_iconv = 0;

    if (disable_iconv) 
        return 0;
    if (eucjp_code_name == NULL) {
        if (! setup_eucjp_code_name()) {
	    disable_iconv = 1;
	    return 0;
	}
    }

    if (current_code_name == NULL || eucjp_code_name == NULL)
        return 0;

    ctoe = iconv_open(eucjp_code_name, current_code_name);
    etoc = iconv_open(current_code_name, eucjp_code_name);


    if (iconv_test(ctoe, etoc)) {
        /* うまくいったら 設定する */
        if (cur_to_euc != (iconv_t) -1) 
	    iconv_close(cur_to_euc);
	if (euc_to_cur != (iconv_t) -1) 
	    iconv_close(euc_to_cur);
	cur_to_euc=ctoe;
	euc_to_cur=etoc;
	return 1;
    } else {
        if (ctoe != (iconv_t)-1) 
	    iconv_close(ctoe);
	if (etoc != (iconv_t)-1) 
	    iconv_close(etoc);
	return 0;
    }
}
#endif

enum CONV_MODE detect_conv_mode(encoding) 
     const char *encoding;
{
    if (encoding == NULL) return IO_AUTO;
    if (match_str(encoding,euc_jp_names)) return IO_EUC;
    if (match_str(encoding,shift_jis_names)) return IO_SJIS;
#ifdef HAVE_ICONV
    if (match_str(encoding,iso_2022_jp_names)) {
        const char **enc;
	for(enc = iso_2022_jp_names;*enc != NULL; enc++){
	    if (iconv_setup(*enc))
	        return IO_ICONV;
	}
    } else if (iconv_setup(encoding)) {
        return IO_ICONV;
    }
#endif
    return IO_AUTO;
}

size_t
locale_init(encoding)
     const char *encoding;
{
    static int		initialized = 0;
#ifdef HAVE_SETLOCALE
    static char *locale_name = NULL;
    static char *current_code_name = NULL;
#endif
    enum CONV_MODE cm_temp;

#ifdef HAVE_SETLOCALE
    locale_name = setlocale(LC_CTYPE, "");
#endif

    /* 1. encoding による指定 
            ： 有効な encoding が指定されれば以前の値を上書き  */
    cm_temp = detect_conv_mode(encoding);
    if (cm_temp != IO_AUTO) {
        conv_mode = cm_temp;
        goto init_finish;
    }

    /* すでに 初期化済みであれば そのまま帰る */
    if (initialized != 0 && 
	(conv_mode != IO_ICONV
#ifdef HAVE_ICONV
	 || (cur_to_euc != (iconv_t)-1 && euc_to_cur != (iconv_t)-1)
#endif
	 ))
	return CODECONV_OK;
    initialized = 0;
    conv_mode = IO_AUTO;

#ifdef HAVE_SETLOCALE
    /* 2. current_locale から 決定を試みる */
#if defined(HAVE_NL_LANGINFO) && defined(CODESET)
    /* 2a/2b. nl_langinfo(CODESET) からの取得の試み */
    current_code_name=nl_langinfo(CODESET);
    conv_mode=detect_conv_mode(current_code_name);
    if (conv_mode != IO_AUTO) 
        goto init_finish;
#endif
    /* 2c/2d. locale LC_CTYPE そのものの確認 */ 
    conv_mode=detect_conv_mode(locale_name);
    if (conv_mode != IO_AUTO) 
        goto init_finish;

    /* 2e/2f. locale LC_CTYPE 後半部の確認 */ 
    if (locale_name != NULL) {
        char *try2;
        locale_name = strdup(locale_name);
	if (locale_name == NULL) 
	    return CODECONV_ERROR;
	try2 = strtok(locale_name, ".@");
	if (try2 != NULL) 
	    try2 = strtok(NULL, ".@");
	if (try2 != NULL) {
	    conv_mode = detect_conv_mode(try2);
	    if (conv_mode != IO_AUTO) goto init_finish;
	}
    }
#endif /* HAVE_SETLOCALE */
    
    /* 3a/3b. それでもだめなら FALLBACK する */
#ifdef FALLBACK_ENCODING
    conv_mode = detect_conv_mode(FALLBACK_ENCODING);
#endif

    /* 4. あきらめて EUC_JP */
    if (conv_mode == IO_AUTO) conv_mode = IO_EUC;

 init_finish:
    initialized = 1;
    return CODECONV_OK;
}

static int
xputs_raw(str, len, fp)
     const char	*str;
     int	len;
     FILE	*fp;
{
    int outlen = 0;
    int len1 = len;
    int wlen;

    while (outlen < len) {
        wlen = fwrite(str, 1, len1, fp);
	if (wlen == 0) 
	    break;
	outlen += wlen;
	len1 -= wlen;
	str += wlen;
    }
    return outlen;
}

static int
xputs2(str, len, fp)
     const char	*str;
     int	len;
     FILE	*fp;
{
    char	*buf1p, *buf1p0;
    char	*buf2p, *buf2p0;
    size_t	len1, len2;
    size_t	outlen;
    int		ret_code;
    size_t	status;

    /* The maximum size of output is 4 times larger than input. */
    outlen = len * 4;

    len1 = len;
    len2 = outlen;
#ifdef HAVE_ALLOCA
    buf1p = buf1p0 = alloca(len1);
#else
    buf1p = buf1p0 = malloc(len1);
#endif
    if (buf1p == NULL)
	return EOF;
#ifdef HAVE_ALLOCA
    buf2p = buf2p0 = alloca(len2);
#else
    buf2p = buf2p0 = malloc(len2);
#endif
    if (buf2p == NULL) {
        free(buf1p0);
	return EOF;
    }
    memcpy(buf1p, str, len);
    status=euc_to_current(&buf1p, &len1, &buf2p, &len2);
    if (status == CODECONV_BUFFER_OVERFLOW) { /* 一回だけ メモリ領域を拡大 */
        buf1p = buf1p0;
	len1 = len;
        outlen *= 3;
	len2 = outlen;
#ifdef HAVE_ALLOCA
        buf2p = buf2p0 = alloca(outlen);
#else
	free(buf2p0);
	buf2p = buf2p0 = malloc(outlen);
#endif
	if (buf2p == NULL){
	    free(buf1p0);
	    return EOF;
	}
	status=euc_to_current(&buf1p, &len1, &buf2p, &len2);
    } 
    if (status == CODECONV_ERROR || status == CODECONV_BUFFER_OVERFLOW) {
        /* Conversion Error  あきらめて そのまま出力 */
#ifndef HAVE_ALLOCA
        free(buf1p0);
        free(buf2p0);
#endif
        return xputs_raw(str, len, fp);
    }
#ifndef HAVE_ALLOCA
    free(buf1p0);
#endif
    ret_code = xputs_raw(buf2p0, outlen - len2, fp);
#ifndef HAVE_ALLOCA
    free(buf2p0);
#endif
    return ret_code;
}

int
xfputs(str, fp)
     const char *str;
     FILE* fp;
{
    return xputs2(str, strlen(str), fp);
}

int
xputs(str)
     const char *str;
{
    int len;
    len=xfputs(str, stdout);
    if (len<0) return EOF;
    putchar('\n');
    return len+1;
}

int
xvfprintf(fp, fmt, ap)
    FILE *fp;
    const char *fmt;
    va_list ap;
{
    char buf1[TMP_SIZE];
    int len;
#ifdef HAVE_VSNPRINTF
    len = vsnprintf(buf1, TMP_SIZE - 1, fmt, ap);
    buf1[TMP_SIZE - 1]=0;
#else
    len = vsprintf(buf1, fmt, ap);
#endif
    return xputs2(buf1, len, fp);
}

/* USE_STDARG_H is defined in codeconv.h */
#ifdef USE_STDARG_H
int
xfprintf(FILE *fp, const char *fmt, ...)
#else
int
xfprintf(fp, fmt, va_alist)
    FILE	*fp;
    const char	*fmt;
    va_dcl
#endif
{
    int len;
    va_list ap;
#ifdef USE_STDARG_H
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
    len = xvfprintf(fp, fmt, ap);
    va_end(ap);
    return len;
}

int
#ifdef USE_STDARG_H
xprintf(const char *fmt, ...)
#else
xprintf(fmt, va_alist)
    const char *fmt;
    va_dcl
#endif
{
    int len;
    va_list ap;
#ifdef USE_STDARG_H
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
    len = xvfprintf(stdout, fmt, ap);
    va_end(ap);
    return len;
}

char *
xfgets(str, size, fp)
     char *str;
     size_t size;
     FILE *fp;
{
    char *ibuf, *ibuf0;
    size_t ilen;
    size_t status;
    char *str0;
    size_t size0;

    str0 = str;
    size0 = size;

    /* The maximum size of input is 4 times larger than size. */
    ilen = size * 4;
#ifdef HAVE_ALLOCA
    ibuf0 = ibuf = alloca(ilen+1);
#else
    ibuf0 = ibuf = malloc(ilen+1);
#endif
    if (ibuf == NULL)
        return NULL;

    if (fgets(ibuf, ilen, fp) == NULL) {
#ifndef HAVE_ALLOCA
        free(ibuf);
#endif
        return NULL;
    }
    ibuf[ilen]=0;
    ilen=strlen(ibuf);

    status = current_to_euc(&ibuf,&ilen,&str,&size);
    str0[size0-size]=0;
#ifndef HAVE_ALLOCA
    free(ibuf0);
#endif
    if (status != CODECONV_ERROR) return str0;
    return NULL;
}

/* ================================================================== */

char*
jis_to_euc(euc, jis, len)
     char	*euc;
     const char *jis;
     int	len;
{
    const char	*jis_end;
    char	*q;
    jis_end = jis + len;
    /* Remove white space at tail of string */
    while (jis_end >= jis + 2 &&
	((jis_end[-1] == '\0' && jis_end[-2] == '\0') ||
	 (jis_end[-1] == 0x21 && jis_end[-2] == 0x21)))
	jis_end -= 2;
    q = euc;
    while (jis < jis_end)
	*q++ = (*jis++ | 0x80);
    *q = '\0';
    return (char *)euc;
}

char*
euc_to_jis(jis, euc, len)
     char	*jis;
     const char *euc;
     int	len;
{
    const char	*euc_end;
    char 	*q;
    euc_end = euc + len;
    /* Remove white space at tail of string */
    while (euc_end >= euc + 2 &&
	((euc_end[-1] == '\0' && euc_end[-2] == '\0') ||
	 (euc_end[-1] == 0x21 && euc_end[-2] == 0x21)))
	euc_end -= 2;
    q = jis;
    while (euc < euc_end)
	*q++ = (*euc++ & 0x7f);
    *q = '\0';
    return (char *)jis;
}

size_t current_to_euc (in_buf,in_len,out_buf,out_len)
     char **in_buf, **out_buf;
     size_t *in_len,*out_len;
{
    int c1, c2;
    size_t count = 0;

#ifdef HAVE_ICONV
    if (conv_mode == IO_ICONV) {
        size_t ret;
        if (cur_to_euc == (iconv_t) -1)
	    return CODECONV_ERROR;
        ret = iconv(cur_to_euc,in_buf,in_len,out_buf,out_len);
	if (ret != ((size_t)-1)) 
	    ret = iconv(cur_to_euc, NULL, in_len, out_buf, out_len);
	if (ret == ((size_t)-1)) {
	    iconv(cur_to_euc,NULL,NULL,NULL,NULL);
#ifdef E2BIG
	    if (errno == E2BIG) 
	        return CODECONV_BUFFER_OVERFLOW;
	    else 
#endif /* E2BIG */
	        return CODECONV_ERROR;
	}
	return ret;
    }
#endif /* HAVE_ICONV */

    if (conv_mode == IO_SJIS) {
        while(*in_len>0) {
	    if (*out_len<=0) break;
	    c1 = *((*in_buf)++) & 0xff;
	    (*in_len)--;
	    if (c1 < 0x80) { /* ASCII 文字 */
	        (*out_len)--;
		count++;
		*((*out_buf)++)=c1;
		continue;
	    } else if ((c1 < 0x81 || c1 > 0x9f) && (c1 < 0xe0 || c1 > 0xef)) {
		/*  半角カナ */
		if (0xa1 <= c1 && c1 <= 0xdf) {
		    c2 = c1 - 0x80;
		    c1 = 0x8e;
		} else {
		    return -1;
		}
	    } else {
		c2 = *((*in_buf)++) & 0xff;
		(*in_len)--;
		if (c1 > 0x9f)
		    c1 -= 0x40;
		c1 += c1;
		if (c2 <= 0x9e) {
		    c1 -= 0xe1;
		    if (c2 >= 0x80)
			c2 -= 1;
		    c2 -= 0x1f;
		} else {
		    c1 -= 0xe0;
		    c2 -= 0x7e;
		}
		c2 |= 0x80;
	    }
	    *((*out_buf)++) = c1 | 0x80;
	    (*out_len)--;
	    count++;
	    if (*out_len <= 0)
		return CODECONV_BUFFER_OVERFLOW;
	    *((*out_buf)++) = c2;
	    (*out_len)--;
	    count++;
	}
	if (*in_len == 0) return count;
	if (*out_len == 0) return CODECONV_BUFFER_OVERFLOW;
	return CODECONV_ERROR;
    } else { /* IO_EUC */
        if (*out_len < *in_len) {
	    memcpy(*out_buf,*in_buf,*out_len);
	    count = *out_len;
	    (*out_buf) += *out_len;
	    (*in_buf) += *out_len;
	    (*in_len) -= *out_len;
	    *out_len = 0;
	    return CODECONV_BUFFER_OVERFLOW;
	} else {
	    memcpy(*out_buf,*in_buf,*in_len);
	    count = *in_len;
	    (*out_buf)+=*in_len;
	    (*in_buf)+=*in_len;
	    (*out_len)-=*in_len;
	    *in_len=0;
	    return count;
	}
    }
    return CODECONV_ERROR; /* Never */
}

size_t euc_to_current (in_buf,in_len,out_buf,out_len)
     char **in_buf, **out_buf;
     size_t *in_len,*out_len;
{
    int c1, c2;
    size_t count = 0;

#ifdef HAVE_ICONV
    if (conv_mode == IO_ICONV) {
        size_t ret;
        if (euc_to_cur == (iconv_t) -1)
	    return CODECONV_ERROR;
        ret = iconv(euc_to_cur,in_buf,in_len,out_buf,out_len);
        if (ret != ((size_t)-1))
	    ret = iconv(euc_to_cur,NULL,in_len,out_buf,out_len);
	if (ret == ((size_t)-1)) {
	    iconv(euc_to_cur,NULL,NULL,NULL,NULL);
#ifdef E2BIG
	    if (errno == E2BIG)
		return CODECONV_BUFFER_OVERFLOW;
	    else 
#endif /* E2BIG */
		return CODECONV_ERROR;
	}
	return ret;
    }
#endif /* HAVE_ICONV */

    if (conv_mode == IO_SJIS) {
        while(*in_len>0) {
	    if (*out_len<=0) break;
	    c1 = *((*in_buf)++) & 0xff;
	    (*in_len)--;
	    if ((c1 & 0x80) == 0) {
	        *((*out_buf)++) = c1;
		(*out_len)--;
		count++;
		continue;
	    }
	    if (0x8e == c1) {
	        *((*out_buf)++) = *((*in_buf)++) | 0x80;
	        (*in_len)--;
	        (*out_len)--;
	        count++;
	        continue;
	    }
	    c1 &= 0x7f;
	    c2 = *((*in_buf)++) & 0x7f;
	    (*in_len)--;
	    if (c1 & 0x01) {
	        c2 += 0x1f;
		if (c2 > 0x7e)
		    c2++;
	    } else {
	        c2 += 0x7e;
	    }
	    c1 = (c1 + 0xe1) >> 1;
	    if (c1 > 0x9f)
	        c1 += 0x40;
	    *((*out_buf)++) = c1;
	    (*out_len)--;
	    count++;
	    if (*out_len <= 0)
		return CODECONV_BUFFER_OVERFLOW;
	    *((*out_buf)++) = c2;
	    (*out_len)--;
	    count++;
	}
	if (*in_len == 0) return count;
	if (*out_len == 0) return CODECONV_BUFFER_OVERFLOW;
	return CODECONV_ERROR;
    } else { /* IO_EUC */
        if (*out_len < *in_len) {
	  /* There are no needs to convert partially. Because caller
	     never refer to partially converted string and other
	     parameters. */
	    memcpy(*out_buf,*in_buf,*out_len);
	    count  = *out_len;
	    (*out_buf)+=*out_len;
	    (*in_buf)+=*out_len;
	    (*in_len)-=*out_len;
	    *out_len=0;
	    return CODECONV_BUFFER_OVERFLOW;
	} else {
	    memcpy(*out_buf,*in_buf,*in_len);
	    count  = *in_len;
	    (*out_buf)+=*in_len;
	    (*in_buf)+=*in_len;
	    (*out_len)-=*in_len;
	    *in_len=0;
	    return count;
	}
    }
    return CODECONV_ERROR; /* Never */
}

