/*                                    -*- mode:C; c-basic-offset:2; -*-  */

/* eblook.c - interactive EB interface command
 *
 * Copyright (C) 1997,1998,1999 Keisuke Nishida <knishida@ring.gr.jp>
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
#if defined(WIN32) && !defined(DOS_FILE_PATH)
#define DOS_FILE_PATH 1
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "getopt.h"
#include "codeconv.h"

#ifdef EBCONF_ENABLE_PTHREAD
#define ENABLE_PTHREAD
#endif

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

#ifndef BUFSIZ
#define BUFSIZ	1024
#endif

/*
 *	maximum path length, same as in eb source files.
 */
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX        MAXPATHLEN
#else /* not MAXPATHLEN */
#define PATH_MAX        1024
#endif /* not MAXPATHLEN */
#endif /* not PATH_MAX */

#define MAX_HIT_SIZE	256
#define MAX_TEXT_SIZE	8192
#define MAX_DUMP_SIZE	2048

#if defined(__DOS__) || defined(MSDOS)
/* DOS only. not for Win32, OS/2 */
#define USER_INIT_FILE  "~/eblookrc"
#else /* UNIX, Windows, OS/2, etc. */
#define USER_INIT_FILE  "~/.eblookrc"
#endif

#ifdef USE_READLINE
#if defined(__DOS__) || defined(MSDOS)
char HIST_FILE[] = "~/eblkhist.txt";
#else
char HIST_FILE[] = "~/.eblook_history";
#endif
#define MAX_INPUT_HISTORY	128
#endif


/*
 * String A-list
 */
typedef struct _StringAlist {
  char                 *key;
  char                 *value;
  struct _StringAlist  *next;
} StringAlist;

/*
 * Internal functions
 */
#ifdef USE_PAGER
FILE *popen_pager (void);
int pclose_pager (FILE *);
#endif

char *read_command (char *, size_t, FILE *);
int excute_command (char *);
int parse_command_line (char *, char *[]);
#ifdef USE_READLINE
char *read_command2 (char *, size_t, const char *);
char *stripwhite (char *);
char *command_generator (char *, int);
char **fileman_completion (char *,int,int);
#endif

void command_book (int, char *[]);
void command_info (int, char *[]);
void command_list (int, char *[]);
void command_select (int, char *[]);
void command_subinfo (int, char *[]);
void command_copyright (int, char *[]);
void command_menu (int, char *[]);
void command_image_menu (int, char *[]);
void command_search (int, char *[]);
void command_content (int, char *[]);
void command_dump (int, char *[]);
void command_font (int, char *[]);
void command_show (int, char *[]);
void command_set (int, char *[]);
void command_unset (int, char *[]);
void command_xbm (int, char *[]);
void command_pbm (int, char *[]);
void command_bmp2ppm (int, char *[]);
void command_bmp2tiff (int, char *[]);
void command_jpeg (int, char *[]);
void command_bmp (int, char *[]);
void command_wav (int, char *[]);
void command_mpeg (int, char *[]);
void command_mpeg_path (int, char *[]);
void command_color (int, char *[]);
void command_help (int, char *[]);

void command_candidate (int, char *[]);
void command_label (int, char *[]);

void show_entry_candidate ( EB_Book *, int, int );
void show_label (EB_Book *, int);

int check_book ();
int check_subbook ();

int internal_set_font (EB_Book *, char *);
int parse_dict_id (char *, EB_Book *);
int parse_entry_id (char *, EB_Position *);

int search_pattern (EB_Book *, EB_Appendix *, char *, int, int);
int hitcomp (const void *a, const void *b);
int insert_content (EB_Book *, EB_Appendix *, EB_Position *, int, int);
int insert_dump (EB_Book *, EB_Appendix *, EB_Position *, int);
int insert_font (EB_Book *, const char *);
int insert_font_list (EB_Book *);

EB_Error_Code hook_font (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
#ifdef EB_HOOK_GB2312
EB_Error_Code hook_gb2312 (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
#endif
#ifdef EB_HOOK_EBXAC_GAIJI
EB_Error_Code hook_ebxac_gaiji (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
#endif
EB_Error_Code hook_stopcode (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code hook_tags (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code hook_img (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code hook_decoration (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code hook_euc_to_ascii (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code hook_iso8859_1 (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);

EB_Error_Code can_menu_narrow_char (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code can_menu_wide_char (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
#ifdef EB_HOOK_GB2312
EB_Error_Code can_menu_gb2312 (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
#endif
EB_Error_Code can_menu_gaiji (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);

#if 0
EB_Error_Code can_menu_begin (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
EB_Error_Code can_menu_end (EB_Book *, EB_Appendix *, void *, EB_Hook_Code, int, const unsigned int *);
#endif

void show_version (void);
void show_help (void);
void show_try_help (void);
void set_error_message (EB_Error_Code);
void unset_error_message ();

int search_wild_match_pattern(char *key, char *pattern);

StringAlist *salist_set (StringAlist *, const char *, const char *);
char *salist_ref (StringAlist *, const char *);

EB_Error_Code eblook_search_keyword (EB_Book *, const char *);
#ifdef EB_MAX_CROSS_ENTRIES
EB_Error_Code eblook_search_cross (EB_Book *, const char *);
#endif
EB_Error_Code eblook_search_multi (EB_Book *, const char *);
static void output_multi_information (EB_Book *);

int eblook_have_wild_search(EB_Book *book);
EB_Error_Code eblook_search_wild(EB_Book *book, const char *pattern);
EB_Error_Code eblook_hit_list_wild (EB_Book *book, int max_hit_count, EB_Hit *hit_list, int *hit_count);
int search_wild_convert_pattern (unsigned char *query, const unsigned char *pattern, int buff_length);


#define variable_set(key, value) \
     variable_alist = salist_set (variable_alist, key, value)
#define variable_ref(key) \
     salist_ref (variable_alist, key)

#ifdef DOS_FILE_PATH /* in win32.c */
size_t euc_to_sjis(const char *, char *, size_t);
size_t sjis_to_euc(const char *, char *, size_t);
void dos_fix_path(char *, int);
#endif       /* DOS_FILE_PATH */

#define uint1(p) (*(const unsigned char *)(p))

#define uint2(p) ((*(const unsigned char *)(p) << 8) \
        + (*(const unsigned char *)((p) + 1)))

#define uint4(p) ((*(const unsigned char *)(p) << 24) \
        + (*(const unsigned char *)((p) + 1) << 16) \
        + (*(const unsigned char *)((p) + 2) << 8) \
        + (*(const unsigned char *)((p) + 3)))

/*
 * Constants
 */
#define DECORATE_OFF          0
#define DECORATE_ON           1

const char *program_name = PACKAGE;
const char *program_version = VERSION;
const char *default_prompt = "eblook> ";
const char *default_method = "glob";
const char *default_use_narrow_kana = "false";
const char *default_decorate_mode = "off";
const char *default_escape_text = "false";
#ifdef USE_PAGER
const char *default_pager = "off";
#endif

/* From EB Library */
#define EB_INDEX_STYLE_CONVERT		0

/*
 * Internal variables
 */
const char *invoked_name;

EB_Book current_book;
EB_Appendix current_appendix;

EB_Hookset text_hookset, heading_hookset;
StringAlist *variable_alist = NULL;
int last_search_begin = 0;
int last_search_length = 0;
int (*last_search_function) (EB_Book *, const char *) = NULL;
int use_narrow_kana = 0;
int decorate_mode = 0;
int escape_text = 0;
int interactive_mode = 1;
int show_prev_next_flag = 0;

unsigned char eblook_wild_pattern[EB_MAX_WORD_LENGTH * 2 + 1];
int eblook_wild_page = 0;
unsigned char eblook_wild_count = 0;

/*
 * Interactive command table
 */
struct command_table_t {
  const char *name;
  const char *option_string;
  void (*func) (int, char *[]);
  const char *help;
};

struct command_table_t *find_command ();
struct command_table_t command_table[] = {
  {"book", "[directory [appendix]]", command_book, "Set a book directory.\n"},
  {"info", "", command_info, "Show information of the selected book.\n"},
  {"list", "", command_list, "List all dictionaries in the selected book.\n"},
  {"select", "subbook", command_select, "Select a subbook.\n"},
  {"subinfo", "", command_subinfo, "Show information of the selected subbook.\n"},
  {"copyright", "", command_copyright, "Show copyright of the selected subbook.\n"},
  {"menu", "", command_menu, "Show the menu of the selected subbook.\n"},
#ifdef EB_HOOK_BEGIN_IMAGE_PAGE
  {"image_menu", "", command_image_menu, "Show the graphic menu of the selected subbook.\n"},
#endif
  {"search", "pattern [offset]", command_search, "Search for a word\n"},
  {"content", "[-]entry [offset]", command_content, "Display contents of entry.\n"},
  {"dump", "entry [offset]", command_dump, "Display dumps of entry.\n"},
  {"pbm", "entry width height", command_pbm, "dump mono image in pbm.\n"},
  {"xbm", "entry width height", command_xbm, "dump mono image in xbm.\n"},
  {"bmp", "entry file", command_bmp, "dump bmp image into file.\n"},
  {"bmp2ppm", "entry file", command_bmp2ppm, "dump bmp image into file in PPM format.\n"},
  {"bmp2tiff", "entry file", command_bmp2tiff, "dump bmp image into file in TIFF format.\n"},
  {"jpg", "entry file", command_jpeg, "dump jpeg image into file.\n"},
  {"jpeg", "entry file", command_jpeg, "dump jpeg image into file.\n"},
  {"font", "[id]", command_font, "Display the bitmap of gaiji.\n"},
  {"show", "[variable]", command_show, "Show the value of variables.\n"},
  {"set", "variable value", command_set, "Set a variable to the value.\n"},
  {"unset", "variable...", command_unset, "Unset variables.\n"},
  {"candidate", "", command_candidate, "Show candidates for multi search\n"},
  {"label", "[id]", command_label, "Show label for multi search\n"},
  {"help", "", command_help, "Show this message.\n"},
  {"quit", "", NULL, "Quit program.\n"},
#ifdef EB_HOOK_BEGIN_WAVE
  {"wave", "entry_start entry_end file", command_wav, "dump wav sound into file.\n"},
  {"wav", "entry_start entry_end file", command_wav, "dump wav sound into file.\n"},
#endif
#ifdef EB_HOOK_BEGIN_MPEG
  {"mpeg", "id0 id1 id2 id3 file", command_mpeg, "dump mpeg1 movie into file.\n"},
  {"mpe", "id0 id1 id2 id3 file", command_mpeg, "dump mpeg1 movie into file.\n"},
  {"mpeg_path", "id0 id1 id2 id3", command_mpeg_path, "show full path name of mpeg1 movie.\n"},
#endif
#ifdef EB_HOOK_BEGIN_COLOR_CHART
  {"color", "number", command_color, "show color chart information.\n"},
#endif
  {NULL, NULL, NULL, NULL}
};

/*
 * Text hooks
 */
EB_Hook text_hooks[] = {
  {EB_HOOK_ISO8859_1, hook_iso8859_1},
  {EB_HOOK_NARROW_JISX0208, hook_euc_to_ascii},
  {EB_HOOK_NARROW_FONT,     hook_font},
  {EB_HOOK_WIDE_FONT,	    hook_font},
#ifdef EB_HOOK_GB2312
  {EB_HOOK_GB2312,			hook_gb2312},
#endif
#ifdef EB_HOOK_EBXAC_GAIJI
  {EB_HOOK_EBXAC_GAIJI,		hook_ebxac_gaiji},
#endif
  {EB_HOOK_NEWLINE,         eb_hook_newline},
#ifdef EB_HOOK_STOP_CODE
  {EB_HOOK_STOP_CODE,       hook_stopcode},
#endif
  {EB_HOOK_BEGIN_MONO_GRAPHIC, hook_img},
  {EB_HOOK_END_MONO_GRAPHIC, hook_img},
  {EB_HOOK_BEGIN_COLOR_JPEG,hook_img},
  {EB_HOOK_BEGIN_COLOR_BMP, hook_img},
  {EB_HOOK_END_COLOR_GRAPHIC, hook_img},
#ifdef EB_HOOK_BEGIN_IN_COLOR_BMP
  {EB_HOOK_BEGIN_IN_COLOR_JPEG,hook_img},
  {EB_HOOK_BEGIN_IN_COLOR_BMP, hook_img},
  {EB_HOOK_END_IN_COLOR_GRAPHIC, hook_img},
#endif
/*   {EB_HOOK_BEGIN_SOUND,     hook_tags}, */
/*   {EB_HOOK_END_SOUND,       hook_tags}, */
#ifdef EB_HOOK_BEGIN_WAVE
  {EB_HOOK_BEGIN_WAVE,     hook_img}, 
  {EB_HOOK_END_WAVE,       hook_img}, 
#endif
#ifdef EB_HOOK_BEGIN_MPEG
  {EB_HOOK_BEGIN_MPEG,     hook_img}, 
  {EB_HOOK_END_MPEG,       hook_img}, 
#endif
  {EB_HOOK_BEGIN_REFERENCE, hook_tags},
  {EB_HOOK_END_REFERENCE,   hook_tags},
  {EB_HOOK_BEGIN_CANDIDATE, hook_tags},
  {EB_HOOK_END_CANDIDATE_GROUP, hook_tags},
#ifdef EB_HOOK_BEGIN_UNICODE
  {EB_HOOK_BEGIN_UNICODE, hook_tags},
  {EB_HOOK_END_UNICODE, hook_tags},
#endif
#ifdef EB_HOOK_BEGIN_IMAGE_PAGE
  {EB_HOOK_BEGIN_IMAGE_PAGE, hook_img},
  {EB_HOOK_END_IMAGE_PAGE,   hook_img},
#endif
#ifdef EB_HOOK_BEGIN_CLICKABLE_AREA
  {EB_HOOK_BEGIN_GRAPHIC_REFERENCE, hook_tags},
  {EB_HOOK_END_GRAPHIC_REFERENCE,   hook_tags},
  {EB_HOOK_GRAPHIC_REFERENCE, hook_tags},
  {EB_HOOK_BEGIN_CLICKABLE_AREA, hook_tags},
  {EB_HOOK_END_CLICKABLE_AREA, hook_tags},
#endif
#ifdef EB_HOOK_BEGIN_COLOR_CHART
  {EB_HOOK_BEGIN_COLOR_CHART, hook_tags},
  /* {EB_HOOK_END_COLOR_CHART,   hook_tags}, */
#endif

  {EB_HOOK_BEGIN_SUBSCRIPT, hook_decoration},
  {EB_HOOK_END_SUBSCRIPT, hook_decoration},
  {EB_HOOK_BEGIN_SUPERSCRIPT, hook_decoration},
  {EB_HOOK_END_SUPERSCRIPT, hook_decoration},
  {EB_HOOK_BEGIN_NO_NEWLINE, hook_decoration},
  {EB_HOOK_END_NO_NEWLINE, hook_decoration},
  {EB_HOOK_BEGIN_EMPHASIS, hook_decoration},
  {EB_HOOK_END_EMPHASIS, hook_decoration},
  {EB_HOOK_SET_INDENT, hook_decoration},
  {EB_HOOK_BEGIN_DECORATION, hook_decoration},
  {EB_HOOK_END_DECORATION, hook_decoration},

  {EB_HOOK_NULL, NULL},
};

EB_Hook heading_hooks[] = {
  {EB_HOOK_ISO8859_1, hook_iso8859_1},
  {EB_HOOK_NARROW_JISX0208, hook_euc_to_ascii},
  {EB_HOOK_NARROW_FONT,     hook_font},
  {EB_HOOK_WIDE_FONT,	    hook_font},
#ifdef EB_HOOK_GB2312
  {EB_HOOK_GB2312,          hook_gb2312},
#endif
#ifdef EB_HOOK_EBXAC_GAIJI
  {EB_HOOK_EBXAC_GAIJI,	    hook_ebxac_gaiji},
#endif
  {EB_HOOK_NEWLINE,         eb_hook_newline},
#ifdef EB_HOOK_STOP_CODE
  {EB_HOOK_STOP_CODE,       hook_stopcode},
#endif
#ifdef EB_HOOK_BEGIN_UNICODE
  {EB_HOOK_BEGIN_UNICODE, hook_tags},
  {EB_HOOK_END_UNICODE, hook_tags},
#endif
  {EB_HOOK_BEGIN_SUBSCRIPT, hook_decoration},
  {EB_HOOK_END_SUBSCRIPT, hook_decoration},
  {EB_HOOK_BEGIN_SUPERSCRIPT, hook_decoration},
  {EB_HOOK_END_SUPERSCRIPT, hook_decoration},
  {EB_HOOK_BEGIN_NO_NEWLINE, hook_decoration},
  {EB_HOOK_END_NO_NEWLINE, hook_decoration},
  {EB_HOOK_BEGIN_EMPHASIS, hook_decoration},
  {EB_HOOK_END_EMPHASIS, hook_decoration},
  {EB_HOOK_BEGIN_DECORATION, hook_decoration},
  {EB_HOOK_END_DECORATION, hook_decoration},

  {EB_HOOK_NULL, NULL},
};

static const char *short_options = "e:hqiv";
static struct option long_options[] = {
  {"encoding",        required_argument, NULL, 'e'},
  {"help",            no_argument,       NULL, 'h'},
  {"no-init-file",    no_argument,       NULL, 'q'},
  {"non-interactive", no_argument,       NULL, 'i'},
  {"version",         no_argument,       NULL, 'v'},
  {NULL,              no_argument,       NULL, 0}
};


int
main (argc, argv)
     int argc;
     char *const *argv;
{
  int optch;
  int no_init = 0;
  char buff[BUFSIZ];
  const char *book, *appendix, *s;
  FILE *fp;
  EB_Error_Code error_code = EB_SUCCESS;

#ifdef USE_READLINE
  char *histfile = NULL;
#endif

  invoked_name = argv[0];
#ifdef USE_TINY_GETTEXT
  set_invoked_name(argv[0]);
#endif
  locale_init(NULL);

#ifdef USE_READLINE
  rl_readline_name = "EBlook";
  rl_attempted_completion_function =
    (rl_completion_func_t *)fileman_completion;
#endif

  /* parse command line options */
  while ((optch = getopt_long(argc, argv, short_options, long_options, NULL))
	 != EOF) {
    switch (optch) {
    case 'e':
      locale_init(optarg);
      break;
    case 'h':
      show_help ();
      exit (0);
    case 'v':
      show_version ();
      exit (0);
    case 'q':
      no_init = 1;
      break;
    case 'i':
      interactive_mode = 0;
      break;
    default:
      show_try_help ();
      exit (1);
    }
  }

  /* check the rest arguments */
  book = appendix = NULL;
  switch (argc - optind) {
  case 2:
    appendix = argv[optind + 1];
  case 1:
    book = argv[optind];
  case 0:
    break;

  default:
    xfprintf (stderr, "%s: too many arguments\n", invoked_name);
    show_try_help ();
    exit (1);
  }

  /* initialize variables */
  eb_initialize_library ();
  eb_initialize_book (&current_book);
  eb_initialize_appendix (&current_appendix);
  eb_initialize_hookset (&text_hookset);
  eb_initialize_hookset (&heading_hookset);
  eb_set_hooks (&text_hookset, text_hooks);
  eb_set_hooks (&heading_hookset, heading_hooks);

  if (!isatty(fileno(stdin))) interactive_mode = 0;

  variable_set ("prompt", default_prompt);
  variable_set ("search-method", default_method);
  variable_set ("use-narrow-kana", default_use_narrow_kana);
  variable_set ("decorate-mode", default_decorate_mode);
  variable_set ("escape-text", default_escape_text);
#ifdef USE_PAGER
  variable_set ("pager", default_pager);
#endif

  sprintf (buff, "%d", MAX_HIT_SIZE);
  variable_set ("max-hits", buff);

  sprintf (buff, "%d", MAX_TEXT_SIZE);
  variable_set ("max-text", buff);

  sprintf (buff, "%d", MAX_DUMP_SIZE);
  variable_set ("max-dump", buff);

  sprintf (buff, "%s %s (with EB %d.%d)", program_name, program_version,
	   EB_VERSION_MAJOR, EB_VERSION_MINOR);
  variable_set ("version", buff);

  variable_set ("multi-search-id", "1");

  /* load init file */
  if (!no_init) {
    buff[0] = 0;
    if (!strncmp (USER_INIT_FILE, "~/", 2)) {
      char *homedir = getenv ("HOME");
      if (homedir && strlen(homedir) + strlen(USER_INIT_FILE  + 1) < BUFSIZ) {
        strcpy (buff, homedir);
        strcat (buff, USER_INIT_FILE + 1);
      }
    } else {
      if (strlen(USER_INIT_FILE) < BUFSIZ)
	strcpy (buff, USER_INIT_FILE);
    }
    if (buff[0] && (fp = fopen (buff, "r")) != NULL)
      while (read_command (buff, BUFSIZ, fp) != NULL)
	if (!excute_command (buff))
	  break;
  }

  /* Read readline history */
#ifdef USE_READLINE
  if (interactive_mode) {
    buff[0] = 0;
    if (!strncmp (HIST_FILE, "~/", 2)) {
      char *homedir = getenv ("HOME");
      if (homedir && strlen(homedir) + strlen(HIST_FILE + 1) < BUFSIZ) {
	strcpy (buff, homedir);
	strcat (buff, HIST_FILE + 1);
      }
    } else {
      if (strlen(HIST_FILE) < BUFSIZ)
	strcpy (buff, HIST_FILE);
    }
    if (buff[0] && (histfile=strdup(buff)) != NULL)
      if (read_history(buff) != 0)
	xprintf("Warning: History file %s not found\n",buff);
  }
#endif /* USE_READLINE */

  /* set book and appendix */
  if (book) {
#ifndef DOS_FILE_PATH
    error_code = eb_bind (&current_book, book);
    if (EB_SUCCESS != error_code) {
      xprintf ("Warning: invalid book directory: %s\n", book);
      set_error_message (error_code);
    }
#else	/* DOS_FILE_PATH */
    strncpy(buff, book, sizeof(buff));
    if (strncmp(buff, "ebnet://", 8) != 0)
      dos_fix_path(buff, 0);
    error_code = eb_bind (&current_book, buff);
    if (EB_SUCCESS != error_code) {
      sjis_to_euc(book, buff, sizeof(buff));
      xprintf ("Warning: invalid book directory: %s\n", buff);
      set_error_message (error_code);
    }
#endif	/* DOS_FILE_PATH */
  }
  if (appendix) {
#ifndef DOS_FILE_PATH
    error_code = eb_bind_appendix (&current_appendix, appendix);
    if (EB_SUCCESS != error_code) {
      xprintf ("Warning: invalid appendix directory: %s\n", appendix);
      set_error_message (error_code);
    }
#else	/* DOS_FILE_PATH */
    strncpy(buff, appendix, sizeof(buff));
    if (strncmp(buff, "ebnet://", 8) != 0)
      dos_fix_path(buff, 0);
    error_code = eb_bind_appendix (&current_appendix, buff);
    if (EB_SUCCESS != error_code) {
      sjis_to_euc(appendix, buff, sizeof(buff));
      xprintf ("Warning: invalid appendix directory: %s\n", buff);
      set_error_message (error_code);
    }
#endif	/* !DOS_FILE_PATH */
  }

  /* check the book directory */
  if (!eb_is_bound (&current_book))
    xputs ("Warning: you should specify a book directory first\n");

  /* enter command loop */
  while (1) {
    /* kanji code */
    if ((s = variable_ref ("kanji-code")) != NULL) {
      if (strcasecmp (s, "JIS") == 0)
	locale_init ("JIS");
      else if (strcasecmp (s, "SJIS") == 0)
	locale_init ("SJIS");
      else if (strcasecmp (s, "EUC") == 0)
	locale_init ("EUC");
      else if (strcasecmp (s, "UTF8") == 0)
	locale_init ("UTF8");
      else if (strcasecmp (s, "AUTO") == 0)
#ifndef WIN32
	locale_init (NULL);
#else	/* WIN32  - need for Visual C++ ? */
      locale_init ("SJIS");
#endif	/* !WIN32 */
      else {
	xprintf ("Invalid kanji code: %s\n", s);
	variable_set ("kanji-code", NULL);
      }
    }

    /* narrow kana */
    s = variable_ref ("use-narrow-kana");
    if (s != NULL && (strcasecmp(s, "true") == 0 || strcasecmp(s, "on") == 0))
	use_narrow_kana = 1;
    else
	use_narrow_kana = 0;

    /* decorate_mode */
    s = variable_ref ("decorate-mode");
    if (s != NULL && (strcasecmp(s, "true") == 0 || strcasecmp(s, "on") == 0))
      decorate_mode = 1;
    else
      decorate_mode = 0;

    /* escape text */
    s = variable_ref ("escape-text");
    if (s != NULL && (strcasecmp(s, "true") == 0 || strcasecmp(s, "on") == 0))
      escape_text = 1;
    else
      escape_text = 0;

    /* prompt */
    if ((s = variable_ref ("prompt")) == NULL)
      s = default_prompt;

    /* read and excute */
    unset_error_message ();
#ifdef USE_READLINE
    if (interactive_mode) {
      if (read_command2 (buff, BUFSIZ, s) == NULL)
	break;
    } else 
#endif
    {
      xfputs(s, stdout);
      fflush(stdout);
      if (read_command (buff, BUFSIZ, stdin) == NULL)
	break;
    }
    if (!excute_command (buff))
      break;
  }

#ifdef USE_READLINE
  if (interactive_mode && histfile!=NULL)
    write_history(histfile);
#endif

  eb_finalize_library ();
  return 0;
}

char *
read_command (command_line, size, stream)
     char *command_line;
     size_t size;
     FILE *stream;
{
  char *p;

  /* read string */
  if (xfgets (command_line, size, stream) == NULL)
    return NULL;

  /* delete '\r', '\n' */
  if ((p = strchr (command_line, '\r')) != NULL) { /* Mac, DOS? depend on cc */
    *p = '\0';
  } else if ((p = strchr (command_line, '\n')) != NULL) {
    *p = '\0';
  } else if (! feof(stream)) {
    xputs ("Input is too long");
    while (xfgets (command_line, BUFSIZ, stream) != NULL &&
	   strchr (command_line, '\r') == NULL &&
	   strchr (command_line, '\n') == NULL);
    command_line[0] = '\0';
  }
  return command_line;
}

#ifdef USE_READLINE
char *
read_command2 (buff, buf_len, prompt)
     char *buff;
     size_t buf_len;
     const char *prompt;
{
  static char *line_read;
  char *pr;
  if (line_read) {
    free (line_read);
    line_read = NULL;
  }
  pr=strdup(prompt);
  line_read = stripwhite ( readline (pr) );
  free(pr);
  if (line_read==NULL) 
    return NULL;

  if (*line_read!=0) {
    size_t status, ilen, size;
    char *ibuf, *str;
    if (strcmp (line_read, "quit") != 0) {
      add_history (line_read);
    }
    ibuf=line_read;
    ilen=strlen(ibuf);
    str = buff;
    size = buf_len;
    status = current_to_euc(&ibuf,&ilen,&str,&size);
    buff[buf_len-size]=0;
  } else {
    *buff = 0;
  }
  return buff;
}

#endif /* READLINE */


int
excute_command (command_line)
     char *command_line;
{
  int i, argc;
  char *argv[BUFSIZ / 2];			/* xxx: no good? */

  argc = parse_command_line (command_line, argv);

  /* if input is empty, do nothing */
  if (argc == 0)
    return 1;

  /* ignore comments in ".eblookrc" */
  if (argv[0][0]=='#' || argv[0][0]==';')
    return 1;

  /* if input is "quit", we should quit */
  if (strcmp (argv[0], "quit") == 0)
    return 0;

  /* otherwise, search command and execute */
  for (i = 0; command_table[i].name != NULL; i++) {
    if (strcmp (argv[0], command_table[i].name) == 0) {
      command_table[i].func (argc, argv);
      return 1;
    }
  }
  if (command_table[i].name == NULL)
    xprintf ("Unknown command: %s\n", argv[0]);
  return 1;
}

int
parse_command_line (command_line, argv)
     char *command_line;
     char *argv[];
{
  int num;
  int reserved, in_quote;
  char *p;

  /* devide string into tokens by white spaces */
  num = reserved = in_quote = 0;
  for (p = command_line; *p != '\0'; p++) {
    switch (*p) {
    case '"':
      if (!reserved) {
	argv[num++] = p;
	reserved = 1;
      }
      memmove (p, p + 1, strlen(p + 1) + 1); 
      p--;
      in_quote = !in_quote;
      break;

    case ' ':
    case '\t':
      if (!in_quote) {
	*p = '\0';
	reserved = 0;
      }
      break;

    case '\\':
      memmove (p, p + 1, strlen(p + 1) + 1); 
    default:
      if (!reserved) {
	argv[num++] = p;
	reserved = 1;
      }
    }
  }

  return num;
}

void
command_book (argc, argv)
     int argc;
     char *argv[];
{
  EB_Error_Code error_code = EB_SUCCESS;
#ifdef DOS_FILE_PATH
  char temp[PATH_MAX];
#endif	/* DOS_FILE_PATH */

  switch (argc) {
  case 3:
#ifndef DOS_FILE_PATH
    error_code = eb_bind_appendix (&current_appendix, argv[2]);
    if (EB_SUCCESS != error_code) {
      xprintf ("Invalid appendix directory: %s\n", argv[2]);
      set_error_message (error_code);
    }
#else	/* DOS_FILE_PATH */
    strncpy(temp, argv[2], sizeof(temp));
    if (strncmp(temp, "ebnet://", 8) != 0)
      dos_fix_path(temp, 1);
    error_code = eb_bind_appendix (&current_appendix, temp);
    if (EB_SUCCESS != error_code) {
      sjis_to_euc(argv[2], temp, sizeof(temp));
      xprintf ("Invalid appendix directory: %s\n", temp);
      set_error_message (error_code);
    }
#endif	/* !DOS_FILE_PATH */
  case 2:
    if (argc == 2)
      eb_finalize_appendix (&current_appendix);
#ifndef DOS_FILE_PATH
    error_code = eb_bind (&current_book, argv[1]);
    if (EB_SUCCESS != error_code) {
      xprintf ("Invalid book directory: %s\n", argv[1]);
      set_error_message (error_code);
    }
#else	/* DOS_FILE_PATH */
    strncpy(temp, argv[1], sizeof(temp));
    if (strncmp(temp, "ebnet://", 8) != 0)
      dos_fix_path(temp, 1);
    error_code = eb_bind (&current_book, temp);
    if (EB_SUCCESS != error_code) {
      sjis_to_euc(argv[1], temp, sizeof(temp));
      xprintf ("Invalid book directory: %s\n", temp);
      set_error_message (error_code);
    }
#endif	/* !DOS_FILE_PATH */
    break;

  case 1:
    if (eb_is_bound (&current_book)) {
#ifndef DOS_FILE_PATH
      char temp[BUFSIZ];
      eb_path (&current_book, temp);
      xprintf ("book\t%s\n", temp);
      if (eb_is_appendix_bound (&current_appendix)) {
	eb_appendix_path (&current_appendix, temp);
	xprintf ("appendix\t%s\n", temp);
      }
#else	/* DOS_FILE_PATH */
      char path[PATH_MAX];
      eb_path (&current_book, path);
      sjis_to_euc(path, temp, sizeof(temp));
      xprintf ("book\t%s\n", temp);
      if (eb_is_appendix_bound (&current_appendix)) {
        eb_appendix_path (&current_appendix, path);
        sjis_to_euc(path, temp, sizeof(temp));
        xprintf ("appendix\t%s\n", temp);
      }
#endif	/* !DOS_FILE_PATH */
    } else {
      xputs ("No book is specified");
    }
    break;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

void
command_info (argc, argv)
     int argc;
     char *argv[];
{
  EB_Subbook_Code code[EB_MAX_SUBBOOKS];
  if (argc > 1) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_book ()) {
    int subcount;
    EB_Disc_Code disccode;
    EB_Character_Code charcode;

    /* disc type */
    eb_disc_type (&current_book, &disccode);
    if (disccode >= 0) {
      xfputs (" disc type: ", stdout);
      xputs ((disccode == EB_DISC_EB) ? "EB/EBG/EBXA" : "EPWING");
    }

    /* character code */
    eb_character_code (&current_book, &charcode);
    if (charcode != EB_CHARCODE_INVALID) {
      xfputs (" character code: ", stdout);
      switch (charcode){
      case EB_CHARCODE_ISO8859_1:
	xputs ("ISO 8859-1");
	break;
      case EB_CHARCODE_JISX0208:
      case EB_CHARCODE_JISX0208_GB2312:
	xputs ("JIS X 0208");
	break;
#ifdef EB_CHARCODE_UTF8
      case EB_CHARCODE_UTF8:
	xputs ("UTF-8");
	break;
#endif
      default:
	xputs ("UNKNOWN");
	break;
      }
    }

    /* the number of dictionarys */
    if (EB_SUCCESS == eb_subbook_list (&current_book, code, &subcount)
        && subcount >= 0)
      xprintf (" the number of dictionries: %d\n", subcount);
  }
}

void
command_list (argc, argv)
     int argc;
     char *argv[];
{
  EB_Error_Code error_code = EB_SUCCESS;

  if (argc > 1) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_book ()) {
    int i, j, num;
    char buff[EB_MAX_TITLE_LENGTH + 1];
    char buff2[EB_MAX_SUBBOOKS][PATH_MAX + 4];
    EB_Subbook_Code list[EB_MAX_SUBBOOKS];

    error_code = eb_subbook_list (&current_book, list, &num);
    if (EB_SUCCESS != error_code)
      goto error;

    for (i = 0; i < num; i++) {
      xprintf ("%2d. ", i + 1);

      error_code = eb_subbook_directory2 (&current_book, list[i], buff2[i]);
      if (EB_SUCCESS != error_code)
	goto error;
      for(j = 0; j < i; j++) {
	if (strcmp(buff2[j], buff2[i]) == 0) {
	  sprintf(buff, ".%d", i + 1);
	  strcat(buff2[i], buff);
	  break;
	}
      }

      xprintf ("%s\t", buff2[i]);
      error_code = eb_subbook_title2 (&current_book, list[i], buff);
      if (EB_SUCCESS != error_code)
	goto error;

      xprintf("%s\n", buff);
    }

    return;

  error:
    xprintf ("An error occured in command_list: %s\n",
	    eb_error_message (error_code));
    set_error_message (error_code);
    return;
  }
}

void
command_select (argc, argv)
     int argc;
     char *argv[];
{
  switch (argc) {
  case 1:
    eb_unset_subbook (&current_book);
    eb_unset_appendix_subbook (&current_appendix);
    return;

  case 2:
    if (check_book ()) {
      if (parse_dict_id (argv[1], &current_book)) {
	if (eb_is_appendix_bound (&current_appendix))
	  {
	    EB_Subbook_Code code;

	    eb_subbook (&current_book, &code);
	    eb_set_appendix_subbook (&current_appendix, code);
	  }
      }
    }
    return;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

void
command_subinfo (argc, argv)
     int argc;
     char *argv[];
{
  if (argc > 1) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_subbook ()) {
    int i, num;
    char buff[PATH_MAX + 1];
    EB_Font_Code list[EB_MAX_FONTS];

    /* title */
	if (EB_SUCCESS == eb_subbook_title (&current_book, buff)) {
      xfputs (" title: ", stdout);
      xfputs (buff, stdout);
      fputc ('\n', stdout);
	}

    /* directory */
    if (EB_SUCCESS == eb_subbook_directory (&current_book, buff))
      xprintf (" directory: %s\n", buff);

    /* search methods */
    xfputs (" search methods:", stdout);
    if (eb_have_word_search (&current_book))
      xfputs (" word", stdout);
    if (eb_have_endword_search (&current_book))
      xfputs (" endword", stdout);
    if (eb_have_exactword_search (&current_book))
      xfputs (" exactword", stdout);
    if (eblook_have_wild_search (&current_book))
      xfputs (" wild", stdout);
    if (eb_have_keyword_search (&current_book))
      xfputs (" keyword", stdout);
#ifdef EB_MAX_CROSS_ENTRIES
    if (eb_have_cross_search (&current_book))
	xfputs (" cross", stdout);
#endif
    if (eb_have_multi_search (&current_book))
      xfputs (" multi", stdout);
    if (eb_have_menu (&current_book))
      xfputs (" menu", stdout);
#ifdef EB_HOOK_BEGIN_IMAGE_PAGE
    if (eb_have_image_menu (&current_book))
      xfputs (" image_menu", stdout);
#endif
#ifdef EB_HOOK_BEGIN_COLOR_CHART
    if (eb_have_color_chart (&current_book))
      xfputs (" color_chart", stdout);
#endif
/*     if (eb_have_graphic_search (&current_book)) */
/*       xfputs (" graphic", stdout); */
    fputc ('\n', stdout);

    /* font size */
    xfputs (" font sizes:", stdout);

    eb_font_list (&current_book, list, &num);
    for (i = 0; i < num; i++) {
      switch (list[i]) {
      case EB_FONT_16: xprintf (" 16"); break;
      case EB_FONT_24: xprintf (" 24"); break;
      case EB_FONT_30: xprintf (" 30"); break;
      case EB_FONT_48: xprintf (" 48"); break;
      default: xprintf (" 0"); break;
      }
    }

    fputc ('\n', stdout);
  }
}

void
command_copyright (argc, argv)
     int argc;
     char *argv[];
{
  if (argc > 1) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_subbook ()) {
    EB_Position pos;
    if (EB_SUCCESS != eb_copyright (&current_book, &pos))
      xputs ("Current dictionary has no copyright information.");
    else
      insert_content (&current_book, &current_appendix, &pos, 0, 0);
  }
}

void
command_menu (argc, argv)
     int argc;
     char *argv[];
{
  if (argc > 1) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_subbook ()) {
    EB_Position pos;
    if (EB_SUCCESS != eb_menu (&current_book, &pos))
      xputs ("Current dictionary has no menu.");
    else
      insert_content (&current_book, &current_appendix, &pos, 0, 0);
  }
}

#ifdef EB_HOOK_BEGIN_IMAGE_PAGE
void
command_image_menu (argc, argv)
     int argc;
     char *argv[];
{
  if (argc > 1) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_subbook ()) {
    EB_Position pos;
    if (EB_SUCCESS != eb_image_menu (&current_book, &pos))
      xputs ("Current dictionary has no graphic menu.");
    else
      insert_content (&current_book, &current_appendix, &pos, 0, 0);
  }
}
#endif

void
command_search (argc, argv)
     int argc;
     char *argv[];
{
  int begin, length;
  char *pattern;

  begin = 1;
  pattern = variable_ref ("max-hits");
  length = pattern ? atoi (pattern) : 0;
  pattern = NULL;

  switch (argc) {
  case 3:
    begin = atoi (argv[2]);
  case 2:
    pattern = argv[1];
  case 1:
    if (check_subbook ())
      search_pattern (&current_book, &current_appendix, pattern, begin, length);
    return;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

void
command_content (argc, argv)
     int argc;
     char *argv[];
{
  int begin, length;
  char *s;
  EB_Position pos, pos1, pos2;
  int backward = 0;
  EB_Book *book = &current_book;
  EB_Appendix *appendix = &current_appendix;
  EB_Error_Code error_code;

  begin = 1;
  s = variable_ref ("max-text");
  length = s ? (atoi (s) / EB_SIZE_PAGE) : 0;

  switch (argc) {
  case 1:
    xprintf ("%s: too few arguments\n", argv[0]);
    return;

  case 4:
    length = atoi (argv[3]);
  case 3:
    begin = atoi (argv[2]);
  case 2:
    if (check_subbook ()) {
      if (*argv[1] == '-') backward = 1;

      if (parse_entry_id(argv[1] + backward, &pos)) {
	if (backward) {
	  pos1 = pos;
	  pos1.offset += 4;
	  if (pos1.offset >= EB_SIZE_PAGE) {
	    pos1.page++;
	    pos1.offset -= EB_SIZE_PAGE;
	  }

	  if (eb_seek_text(book, &pos) == EB_SUCCESS) {
	    error_code = eb_forward_text(book, appendix);
	    switch (error_code) {
	    case EB_SUCCESS:
	    case EB_ERR_END_OF_CONTENT:
	      pos2 = pos;
	      do {
		if (eb_backward_text(book, appendix) != EB_SUCCESS
		    || eb_tell_text(book, &pos2) != EB_SUCCESS) {
		  pos2 = pos;
		  break;
		}
	      } while (pos1.page < pos2.page
		       || ((pos1.page == pos2.page)
			   && (pos1.offset < pos2.offset)));
	      pos = pos2;

	      if (eb_forward_text(book, appendix) == EB_SUCCESS
		  && eb_tell_text(book, &pos2) == EB_SUCCESS
		  && (pos2.page < pos1.page
		      || ((pos2.page == pos1.page)
			  && (pos2.offset < pos1.offset))))
		pos = pos2;
	      break;
	    default:
	      break;
	    }
	  }
	}

	insert_content (book, appendix, &pos, begin, length);
      }
    }
    return;
  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

char *uc2bit[] = {
"0 0 0 0 ", "0 0 0 1 ", "0 0 1 0 ", "0 0 1 1 ",
"0 1 0 0 ", "0 1 0 1 ", "0 1 1 0 ", "0 1 1 1 ",
"1 0 0 0 ", "1 0 0 1 ", "1 0 1 0 ", "1 0 1 1 ",
"1 1 0 0 ", "1 1 0 1 ", "1 1 1 0 ", "1 1 1 1 " };

#define MAX_BITMAP_SIZE 1024*1024
void
command_pbm(argc, argv)
    int argc;
    char *argv[];
{
    static unsigned char buffer[MAX_BITMAP_SIZE];
    EB_Position pos;
    ssize_t len;
    int ret;
    int i;
    int j;
    int w, h;

    if (argc != 4)
	return;

    w = atoi(argv[2]);
    h = atoi(argv[3]);

    parse_entry_id(argv[1], &pos);

    ret = eb_seek_text(&current_book, &pos);
    if (ret != EB_SUCCESS)
	return;
    if (w * h / 8 > MAX_BITMAP_SIZE) return;
    ret = eb_read_rawtext(&current_book, w*h/8, (char *)buffer, &len);
    if (ret != EB_SUCCESS || len == 0)
	return;
    printf("P1\n%d %d\n", w, h);
    for (i=0; i<len;) {
	for (j=0; j<4; j++, i++) {
	    fputs(uc2bit[buffer[i]>>4], stdout);
	    fputs(uc2bit[buffer[i]&0xF], stdout);
	}
	fputc('\n', stdout);
    }

    return;
}

unsigned char reversebit[] = {
0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

void
command_xbm(argc, argv)
    int argc;
    char *argv[];
{
    static unsigned char buffer[MAX_BITMAP_SIZE];
    EB_Position pos;
    ssize_t len;
    int ret;
    int i;
    int j;
    int w, h;

    if (argc != 4)
	return;

    w = atoi(argv[2]);
    h = atoi(argv[3]);

    parse_entry_id(argv[1], &pos);

    ret = eb_seek_text(&current_book, &pos);
    if (ret != EB_SUCCESS)
	return;
    if (w * h / 8 > MAX_BITMAP_SIZE) return;
    ret = eb_read_rawtext(&current_book, w*h/8, (char *)buffer, &len);
    if (ret != EB_SUCCESS || len == 0)
	return;

    fprintf(stdout, "#define mono_width %d\n#define mono_height %d\n", w, h);
    fprintf(stdout, "static char mono_bits[] = {\n");

    for (i=0; i<len;) {
	for (j=0; j<8; j++, i++) {
	    printf("0x%02x,", reversebit[buffer[i]]);
	}
	fputc('\n', stdout);
    }
    fprintf(stdout, "};\n");
    return;
}

void
command_bmp(argc, argv)
    int argc;
    char *argv[];
{
    unsigned char binary_data[EB_SIZE_PAGE];
    EB_Error_Code error_code;
    ssize_t read_length;
    FILE *fp;
    EB_Position pos;
    int firstpage = 1;
    char *bmp_error = NULL;
#ifdef DOS_FILE_PATH
    char *path;
#endif

    if (argc != 3) {
        printf("NG: parameter error\n");
        return;
    }
    if (parse_entry_id(argv[1], &pos) == 0) {
        printf("NG: address error\n");
	return;
    }

    error_code = eb_set_binary_color_graphic(&current_book, &pos);
    if (error_code != EB_SUCCESS) {
        xprintf("NG: eb_set_binary_color_graphic:  %s\n", 
		eb_error_message(error_code));
        return;
    }

#ifndef DOS_FILE_PATH
    fp = fopen(argv[2], "wb");
#else	/* DOS_FILE_PATH */
    path = strdup(argv[2]);
    if (!path) {
	xprintf ("NG: memory full\n");
	return;
    }
    euc_to_sjis(path, path, strlen(path) + 1);
    fp = fopen(path, "wb");
    free(path);
#endif	/* !DOS_FILE_PATH */
    if (fp == NULL) {
	xprintf("NG: fopen %s: %s\n", argv[2], strerror(errno));
	return;
    }

    while (1) {
	error_code = eb_read_binary(&current_book, EB_SIZE_PAGE, 
				    (char *)binary_data, &read_length);
	if (error_code != EB_SUCCESS) {
	    bmp_error = "BMP read error";
	    goto bmp_fail;
	}
	if (read_length == 0) 
	    break;
	if (firstpage != 0) {
	    if (read_length < 2 || 
		binary_data[0]!='B' || binary_data[1]!='M') {
	        bmp_error="data is not BMP";
		goto bmp_fail;
	    }
	    firstpage = 0;
	}
	if (1 != fwrite((char *)binary_data, read_length, 1, fp)) {
	    xprintf("NG: fwrite %s: %s\n", argv[2], strerror(errno));
	    fclose(fp);
	    return;
	}
    }
    fclose(fp);
    printf("OK\n");
    return;

 bmp_fail:
    fclose(fp);
    if (bmp_error != NULL) 
        printf("NG: %s\n",bmp_error);
    else 
        printf("NG\n");
    return;
}

void
command_jpeg(argc, argv)
    int argc;
    char *argv[];
{
    static unsigned char binary_data[EB_SIZE_PAGE];
    EB_Error_Code error_code;
    ssize_t read_length;
    EB_Position pos;
    int i;
    FILE *fp = NULL;
    unsigned char *p;
    int lastchar = 0;
    int firstpage = 1;
    int canstop = 1;
    int file_length = 1000000; /* Limit 1MB */
    char *jpeg_error = NULL;
#ifdef DOS_FILE_PATH
    char *path;
#endif

    if (argc != 3) {
        jpeg_error = "parameter error";
	goto jpeg_fail;
    }
    if (parse_entry_id(argv[1], &pos) == 0) {
        jpeg_error = "entry address error";
        goto jpeg_fail;
    }

    error_code = eb_set_binary_color_graphic(&current_book, &pos);
    if (error_code != EB_SUCCESS) {
        jpeg_error = "jpeg decode error";
        goto jpeg_fail;
    }

#ifndef DOS_FILE_PATH
    fp = fopen(argv[2], "wb");
#else	/* DOS_FILE_PATH */
    path = strdup(argv[2]);
    if (!path) {
	jpeg_error = "memory full";
	goto jpeg_fail;
    }
    euc_to_sjis(path, path, strlen(path) + 1);
    fp = fopen(path, "wb");
    free(path);
#endif	/* !DOS_FILE_PATH */
    if (fp == NULL) {
        jpeg_error = "jpeg output file open error";
        goto jpeg_fail;
    }

    while (file_length >= 0) {
	error_code = eb_read_binary(&current_book, EB_SIZE_PAGE, 
				    (char *)binary_data, &read_length);
	if (error_code != EB_SUCCESS) {
	    jpeg_error = "jpeg read error";
	    goto jpeg_fail;
	    /*
	    printf("eb_read_rawtext: %s\n",
		eb_error_message(error_code));
	    break;;
	    */
	}
	if (read_length == 0)
	    break;
	file_length -= read_length;

	if (firstpage &&
	    strncmp((char *)binary_data, "data", 4) == 0 &&
	    strncmp((char *)binary_data+14, "JFIF", 4) == 0) {
	  /* for old eb library ? */
	    p = binary_data+8;
	    read_length -= 8;
	} else {
	    p = binary_data;
	}
	p = binary_data;

	firstpage = 0;

	if (lastchar == 0xff) {
	    switch (p[0]) {
	    case 0xd9:
		if (canstop) {
		    fwrite(p, 1, 1, fp);
		    goto jpeg_ok;
		} else {
		    canstop = 1;
		}
		break;
	    case 0xed:
		canstop = 0;
		break;
	    }
	}

	lastchar = p[read_length-1];
	for (i=0; i<read_length-1; i++) {
	    if (p[i] == 0xff) {
		switch (p[i+1]) {
		case 0xd9:
		    if (canstop) {
			fwrite(p, i+2, 1, fp);
			goto jpeg_ok;
		    } else {
			canstop = 1;
		    }
		    break;
		case 0xed:
		    canstop = 0;
		    break;
		}
	    }
	}
	if (1 != fwrite(binary_data, read_length, 1, fp)) {
	    xprintf("NG: fwrite %s: %s\n", argv[2], strerror(errno));
	    fclose(fp);
	    return;
	}
    }
    if (file_length < 0) {
        jpeg_error = "input overrun. Huge jpeg file?";
	goto jpeg_fail;
    }
 jpeg_ok:
    fclose(fp);
    printf("OK\n");
    return;

 jpeg_fail:
    if (fp) fclose(fp);
    if (jpeg_error != NULL)
        printf("NG: %s\n",jpeg_error);
    else
        printf("NG\n");
    return;
}

#ifdef EB_HOOK_BEGIN_WAVE
void
command_wav(argc, argv)
    int argc;
    char *argv[];
{
    unsigned char binary_data[EB_SIZE_PAGE];
    EB_Error_Code error_code;
    ssize_t read_length;
    FILE *fp;
    EB_Position pos_start, pos_end;
    int firstpage = 1;
    char *wav_error = NULL;
#ifdef DOS_FILE_PATH
    char *path;
#endif

    if (argc != 4) {
        printf("NG: parameter error\n");
        return;
    }
    if (parse_entry_id(argv[1], &pos_start) == 0) {
        printf("NG: address error\n");
	return;
    }
    if (parse_entry_id(argv[2], &pos_end) == 0) {
        printf("NG: address error\n");
	return;
    }

    error_code = eb_set_binary_wave(&current_book, &pos_start, &pos_end);
    if (error_code != EB_SUCCESS) {
        xprintf("NG: eb_set_binary_wave:  %s\n", 
		eb_error_message(error_code));
        return;
    }

#ifndef DOS_FILE_PATH
    fp = fopen(argv[3], "wb");
#else	/* DOS_FILE_PATH */
    path = strdup(argv[3]);
    if (!path) {
	xprintf ("NG: memory full\n");
	return;
    }
    euc_to_sjis(path, path, strlen(path) + 1);
    fp = fopen(path, "wb");
    free(path);
#endif	/* !DOS_FILE_PATH */
    if (fp == NULL) {
	xprintf("NG: fopen %s: %s\n", argv[3], strerror(errno));
	return;
    }

    while (1) {
	error_code = eb_read_binary(&current_book, EB_SIZE_PAGE, 
				    (char *)binary_data, &read_length);
	if (error_code != EB_SUCCESS) {
	    wav_error = "WAV read error";
	    goto wav_fail;
	}
	if (read_length == 0) 
	    break;
	if (1 != fwrite((char *)binary_data, read_length, 1, fp)) {
	    xprintf("NG: fwrite %s: %s\n", argv[3], strerror(errno));
	    fclose(fp);
	    return;
	}
    }
    fclose(fp);
    printf("OK\n");
    return;

 wav_fail:
    fclose(fp);
    if (wav_error != NULL) 
        printf("NG: %s\n",wav_error);
    else 
        printf("NG\n");
    return;
}
#endif

#ifdef EB_HOOK_BEGIN_MPEG
/* command_mpeg id0 id1 id2 id3 file */
void
command_mpeg(argc, argv)
    int argc;
    char *argv[];
{
    unsigned char binary_data[EB_SIZE_PAGE];
    EB_Error_Code error_code;
    ssize_t read_length;
    FILE *fp;
    unsigned int mpegid[4];
    int i;
    char *mpeg_error = NULL;
#ifdef DOS_FILE_PATH
    char *path;
#endif

    if (argc != 6) {
        printf("NG: parameter error\n");
        return;
    }
    
    i = 0;
    while (i < 4) {
      mpegid[i] = atoi(argv[i+1]);
      ++i;
    }
    error_code = eb_set_binary_mpeg(&current_book, &mpegid[0]);
    if (error_code != EB_SUCCESS) {
        xprintf("NG: eb_set_binary_mpeg:  %s\n", 
		eb_error_message(error_code));
        return;
    }

#ifndef DOS_FILE_PATH
    fp = fopen(argv[5], "wb");
#else	/* DOS_FILE_PATH */
    path = strdup(argv[5]);
    if (!path) {
	xprintf ("NG: memory full\n");
	return;
    }
    euc_to_sjis(path, path, strlen(path) + 1);
    fp = fopen(path, "wb");
    free(path);
#endif	/* !DOS_FILE_PATH */
    if (fp == NULL) {
	xprintf("NG: fopen %s: %s\n", argv[3], strerror(errno));
	return;
    }

    while (1) {
	error_code = eb_read_binary(&current_book, EB_SIZE_PAGE, 
				    (char *)binary_data, &read_length);
	if (error_code != EB_SUCCESS) {
	    mpeg_error = "MPEG read error";
	    goto mpeg_fail;
	}
	if (read_length == 0) 
	    break;
	if (1 != fwrite((char *)binary_data, read_length, 1, fp)) {
	    xprintf("NG: fwrite %s: %s\n", argv[3], strerror(errno));
	    fclose(fp);
	    return;
	}
    }
    fclose(fp);
    printf("OK\n");
    return;

 mpeg_fail:
    fclose(fp);
    if (mpeg_error != NULL) 
        printf("NG: %s\n",mpeg_error);
    else 
        printf("NG\n");
    return;
}

/* command_mpeg id0 id1 id2 id3*/
void
command_mpeg_path(argc, argv)
    int argc;
    char *argv[];
{
    unsigned char composed_path_name[EB_MAX_PATH_LENGTH + 1];
    unsigned char binary_data[EB_SIZE_PAGE];
    EB_Error_Code error_code;
    ssize_t read_length;
    FILE *fp;
    unsigned int mpegid[4];
    int i;
    char *mpeg_error = NULL;
#ifdef DOS_FILE_PATH
    char *path;
#endif

    if (argc != 5) {
        printf("NG: parameter error\n");
        return;
    }
    
    i = 0;
    while (i < 4) {
      mpegid[i] = atoi(argv[i+1]);
      ++i;
    }
    error_code = eb_compose_movie_path_name(&current_book, &mpegid[0], (char *)composed_path_name);
    if (error_code != EB_SUCCESS) {
      xprintf("NG: eb_compose_movie_path_name:  %s\n", 
	      eb_error_message(error_code));
      return;
    }

    xprintf("%s\n", composed_path_name);

    printf("OK\n");
    return;
}
#endif

void
command_dump (argc, argv)
     int argc;
     char *argv[];
{
  int begin, length;
  char *s;
  EB_Position pos;

  begin = 1;
  s = variable_ref ("max-dump");
  length = s ? (atoi (s) / EB_SIZE_PAGE) : (MAX_DUMP_SIZE / EB_SIZE_PAGE);
  if (length == 0)
    length = 1;

  switch (argc) {
  case 1:
    xprintf ("%s: too few arguments\n", argv[0]);
    return;

  case 4:
    length = atoi (argv[3]);
  case 3:
    begin = atoi (argv[2]);
  case 2:
    if (check_subbook ()) {
      if (parse_entry_id (argv[1], &pos))
	insert_dump (&current_book, &current_appendix, &pos, length);
    }
    return;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

void
command_font (argc, argv)
     int argc;
     char *argv[];
{
  if (argc > 2) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  if (check_subbook () && internal_set_font (&current_book, NULL)) {
    if (argc == 1)
      insert_font_list (&current_book);
    else
      insert_font (&current_book, argv[1]);
  }
}

#ifdef EB_HOOK_BEGIN_COLOR_CHART
/* command_color number */
void
command_color(argc, argv)
    int argc;
    char *argv[];
{
  char *endp = NULL;
  int number;
  char name[EB_MAX_COLOR_NAME_LENGTH + 1];
  char value[EB_MAX_COLOR_VALUE_LENGTH + 1];
  EB_Error_Code error_code;

  if (argc < 2) {
    xprintf ("%s: too few arguments\n", argv[0]);
    return;
  } else if (argc > 2) {
    xprintf ("%s: too many arguments\n", argv[0]);
    return;
  }

  number = strtol(argv[1], &endp, 0);
  if (*endp != 0) {
    xprintf ("Illegal argument: %s\n", argv[1]);
    return;
  }

  if (check_subbook ()) {
    if (!eb_have_color_chart (&current_book)) {
      xprintf ("This subbook does not have color chart\n", argv[1]);
      return;
    }

    error_code = eb_color_name(&current_book, number, name);
    if (error_code != EB_SUCCESS) {
      xfputs (eb_error_message(error_code), stdout);
      fputc ('\n', stdout);
      return;
    }

    error_code = eb_color_value(&current_book, number, value);
    if (error_code != EB_SUCCESS) {
      xfputs (eb_error_message(error_code), stdout);
      fputc ('\n', stdout);
      return;
    }

    xfputs (name, stdout);
    xfputs (" (", stdout);
    xfputs (value, stdout);
    xfputs (")\n", stdout);
  }
}
#endif

void
command_show (argc, argv)
     int argc;
     char *argv[];
{
  char *s;
  StringAlist *var;
  int showall = 0;

  if (argc == 2 && strcmp(argv[1],"-a") == 0) {
    showall = 1;
    argc --;
    argv ++;
  }

  switch (argc) {
  case 1:
    /*
     * Show all variables and their values
     */
    for (var = variable_alist; var != NULL; var = var->next)
      if (var->key[0] != '_' || showall != 0)
	xprintf ("%s\t%s\n", var->key, var->value);
    return;

  case 2:
    /*
     * Show value of variable
     */
    if ((s = variable_ref (argv[1])) != NULL)
      xputs (s);
    else
      xprintf ("Unbounded variable: %s\n", argv[1]);
    return;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

void
command_set (argc, argv)
     int argc;
     char *argv[];
{
  switch (argc) {
  case 1:
    xprintf ("%s: too few arguments\n", argv[0]);
    return;

  case 2:
    argv[2] = "";
  case 3:
    variable_set (argv[1], argv[2]);
    return;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

void
command_unset (argc, argv)
     int argc;
     char *argv[];
{
  int i;
  if (argc == 1) {
    xprintf ("%s: too few arguments\n", argv[0]);
  } else {
    for (i = 1; i < argc; i++)
      variable_set (argv[i], NULL);
    return;
  }
}

void
command_help (argc, argv)
     int argc;
     char *argv[];
{
  int i;
  char buff[256];
  const char *p;
  switch (argc) {
  case 1:
    /*
     * List up all command helps
     */
    for (i = 0; command_table[i].name != NULL; i++) {
      sprintf (buff, "%s %s",
	      command_table[i].name, command_table[i].option_string);
      xprintf (" %-22s - ", buff);
      for (p = command_table[i].help;
	   *p != '\0' && *p != '.' && *p != '\n';
	   p++)
	putchar (*p);
      putchar ('\n');
    }
    return;

  case 2:
    /*
     * Show command help
     */
    for (i = 0; command_table[i].name != NULL; i++) {
      if (strcmp (command_table[i].name, argv[1]) == 0)
	break;
    }
    if (command_table[i].name == NULL) {
      xprintf ("No such command: %s\n", argv[1]);
    } else {
      xprintf ("Usage: %s %s\n\n%s",
	      command_table[i].name, command_table[i].option_string,
	      command_table[i].help);
    }
    return;

  default:
    xprintf ("%s: too many arguments\n", argv[0]);
  }
}

int
check_book ()
{
  if (eb_is_bound (&current_book)) {
    return 1;
  } else {
    xputs ("You should specify a book directory first");
    return 0;
  }
}

int
check_subbook ()
{
  if (check_book ()) {
    EB_Subbook_Code code;
    if (EB_SUCCESS == eb_subbook (&current_book, &code))
      return 1;
    else
      xputs ("You should select a subbook first");
  }
  return 0;
}

int
internal_set_font (book, height)
     EB_Book *book;
     char *height;
{
  EB_Font_Code font;
  EB_Error_Code error_code = EB_SUCCESS;

  if (height == NULL)
    if ((height = variable_ref ("font")) == NULL)
      height = "16";

  font = atoi (height);
  switch (font) {
  case 16: font = EB_FONT_16; break;
  case 24: font = EB_FONT_24; break;
  case 30: font = EB_FONT_30; break;
  case 48: font = EB_FONT_48; break;
  default:
    xprintf ("Illegal font height: %s\n", height);
    return 0;
  }

  if (!eb_have_font (book, font) ) {
    xprintf ("Invalid font for %s: %s\n",
	    book->subbook_current->directory_name,
	    height);
    /* set_error_message (); */
    return 0;
  }

  error_code = eb_set_font (book, font);
  if (EB_SUCCESS != error_code) {
    xprintf ("An error occurred in internal_set_font: %s\n",
	   eb_error_message (error_code));
    set_error_message (error_code);
    return 0;
  }
  return 1;
}

int
parse_dict_id (name, book)
     char *name;
     EB_Book *book;
{
  int i, num;
  EB_Subbook_Code sublist[EB_MAX_SUBBOOKS];
  EB_Error_Code error_code = EB_SUCCESS;

#ifdef	_DEBUG
  /* to avoid strange behavior in VC++ 6.0 debug mode. */
  memset (sublist, -1, sizeof (sublist));
#endif	/* !DEBUG */
  error_code = eb_subbook_list (book, sublist, &num);
  if (EB_SUCCESS != error_code)
    goto error;

  if (strchr (name, '.') != NULL) {
    /*
     * repeated directory, different index page
     */
    name = strchr(name, '.') + 1;
  }
  if ((i = atoi (name)) > 0) {
    /*
     * Numbered dictionary
     */
    if (--i < num) {
      error_code = eb_set_subbook (book, sublist[i]);
      if (EB_SUCCESS != error_code)
	goto error;
      return 1;
    } else {
      xprintf ("No such numberd dictionary : %s\n", name);
      return 0;
    }
  } else {
    /*
     * Named dictionary
     */
    char dir[PATH_MAX + 1];

    for (i = 0; i < num; i++) {
      error_code = eb_subbook_directory2 (book, sublist[i], dir);
      if (EB_SUCCESS != error_code)
	goto error;

      if (strcmp (name, dir) == 0) {
	error_code = eb_set_subbook (book, sublist[i]);
        if (EB_SUCCESS != error_code)
	  goto error;
	return 1;
      }
    }
    xprintf ("No such dictionary: %s\n", name);
    return 0;
  }

 error:
  xprintf ("An error occurred in parse_dict_id: %s\n",
	  eb_error_message (error_code));
  set_error_message (error_code);
  return 0;
}

int
parse_entry_id (code, pos)
     char *code;
     EB_Position *pos;
{
  EB_Error_Code error_code = EB_SUCCESS;
  EB_Error_Code (*hit_list) (EB_Book *, int, EB_Hit *, int *);

  if (strchr (code, ':') != NULL) {
    /*
     * Encoded position
     */
    char *endp;
    pos->page = strtol (code, &endp, 0);
    if (*endp != ':')
      goto illegal;

    pos->offset = strtol (endp + 1, &endp, 0);
    if (*endp != '\0')
      goto illegal;

    return 1;

  illegal:
    xprintf ("Illegal position: %s\n", code);
    return 0;

  } else {
    /*
     * Numbered entry
     */
    int num, count;
    const char *pattern = variable_ref ("_last_search_pattern");
    EB_Hit list[MAX_HIT_SIZE];

    if (!pattern) {
      xputs ("No search has been executed yet.");
      return 0;
    }
    if ((count = atoi (code) - 1) < 0) {
      xprintf ("Invalid entry number: %s\n", code);
      return 0;
    }
    if (check_subbook ()) {
      error_code = last_search_function (&current_book, pattern);
      if (EB_SUCCESS != error_code) {
	xprintf ("An error occured in parse_entry_id: %s\n",
		eb_error_message (error_code));
	set_error_message (error_code);
	return 0;
      }
      if (last_search_function == eblook_search_wild) {
	hit_list = eblook_hit_list_wild;
      }
      else 
	hit_list = eb_hit_list;
      while (EB_SUCCESS == hit_list (&current_book, MAX_HIT_SIZE, list,
				     &num) && 0 < num) {
	if (count < num) {
	  qsort(list, num, sizeof(EB_Hit), hitcomp);
	  pos->page = list[count].text.page;
	  pos->offset = list[count].text.offset;
	  return 1;
	}
	count -= num;
	pattern = NULL;
      }
      if (num == 0)
	xprintf ("Too big: %s\n", code);
    }
    return 0;
  }
}


int hitcomp(a, b)
     const void *a;
     const void *b;
{
  const EB_Hit *x, *y;
  x = (EB_Hit *)a;
  y = (EB_Hit *)b;
  if (x->heading.page < y->heading.page) return -1;
  if (x->heading.page == y->heading.page) {
    if (x->heading.offset < y->heading.offset) return -1;
    if (x->heading.offset == y->heading.offset) {
      if (x->text.page < y->text.page) return -1;
      if (x->text.page == y->text.page) {
	if (x->text.offset < y->text.offset) return -1;
	if (x->text.offset == y->text.offset) return 0;
      }
    }
  } 
  return 1;
}


int
search_pattern (book, appendix, pattern, begin, length)
     EB_Book *book;
     EB_Appendix *appendix;
     char *pattern;
     int begin;
     int length;
{
  int i, num, point;
  char headbuf1[BUFSIZ];
  char headbuf2[BUFSIZ];
  char headbuf3[BUFSIZ];
  char *head;
  const char *s;
  EB_Error_Code (*search) (EB_Book *, const char *);
  EB_Error_Code (*hit_list) (EB_Book *, int, EB_Hit *, int *);
  EB_Hit hitlist[MAX_HIT_SIZE];
  EB_Error_Code error_code = EB_SUCCESS;

  char* prevhead;
  int prevpage;
  int prevoffset;
  ssize_t heading_len;

  if (pattern == NULL) {
    /* check last search */
    begin = last_search_begin;
    length = last_search_length;
    search = last_search_function;
    pattern = variable_ref ("_last_search_pattern");
    if (pattern == NULL) {
      xputs ("No search has been executed yet.");
      return 0;
    }
    if (last_search_begin == 0) {
      xprintf ("Last search had finished\n");
      return 0;
    }
  } else {
    /* get search method */
    if ((s = variable_ref ("search-method")) == NULL)
      s = default_method;

    if (strcmp (s, "exact") == 0)
      search = eb_search_exactword;
    else if (strcmp (s, "word") == 0)
      search = eb_search_word;
    else if (strcmp (s, "endword") == 0)
      search = eb_search_endword;
    else if (strcmp (s, "glob") == 0) {
      search = eb_search_exactword;

      i = strlen (pattern) - 1;
      if (strchr(pattern, '=') && eb_have_keyword_search(book))
	/* check for keyword search */
	search = eblook_search_keyword;
#ifdef EB_MAX_CROSS_ENTRIES
      else if (strchr(pattern, '&') && eb_have_cross_search(book))
	/* check for cross search */
	search = eblook_search_cross;
#endif
      else if (strchr(pattern, ':') && eb_have_multi_search(book))
	/* check for multi search */
	search = eblook_search_multi;
      if (pattern[i] == '*') {
	/* check for word search */
	pattern[i] = '\0';
	search = eb_search_word;
      } else if (pattern[0] == '*') {
	/* check for endword search */
	pattern++;
	search = eb_search_endword;
      }
    } else if (strcmp (s, "keyword") == 0)
      search = eblook_search_keyword;
#ifdef EB_MAX_CROSS_ENTRIES
    else if (strcmp (s, "cross") == 0)
      search = eblook_search_cross;
#endif
    else if (strcmp (s, "multi") == 0)
      search = eblook_search_multi;
    else if (strcmp (s, "wild") == 0)
      search = eblook_search_wild;
    else {
      xprintf ("Invalid search method: %s\n", s);
      return 0;
    }
    /* reserve search information */
    /* use EB_Book structure directly here so as not to use more buffer. */
    variable_set ("_last_search_book", book->path);
    variable_set ("_last_search_dict", book->subbook_current->directory_name);
    variable_set ("_last_search_pattern", pattern);
    last_search_begin = 0;
    last_search_length = length;
    last_search_function = search;
  }

  /* search */
  error_code = search (book, pattern);
  if (EB_SUCCESS != error_code) {
    xprintf ("An error occured in search_pattern: %s\n",
	    eb_error_message (error_code));
    set_error_message (error_code);
    return 0;
  }

  point = 0;
  head = headbuf1;
  prevhead = headbuf2;
  *prevhead = '\0';
  prevpage = 0;
  prevoffset = 0;

  if (search == eblook_search_wild) {
    hit_list = eblook_hit_list_wild;
  } else 
    hit_list = eb_hit_list;

  while (EB_SUCCESS == hit_list (book, MAX_HIT_SIZE, hitlist, &num)
	 && 0 < num) {
    qsort(hitlist, num, sizeof(EB_Hit), hitcomp);
    for (i = 0; i < num; i++) {
      point++;
      if (point >= begin + length && length > 0) {
	xprintf ("<more point=%d>\n", point);
	last_search_begin = point;
	goto exit;
      }

      if (point >= begin) {
  	error_code = eb_seek_text (book, &hitlist[i].heading);
        if (error_code != EB_SUCCESS)
	  continue;
	error_code = eb_read_heading (book, appendix, &heading_hookset, NULL,
				      BUFSIZ - 1, head, &heading_len);
        if (error_code != EB_SUCCESS || heading_len == 0)
	  continue;
        *(head + heading_len) = '\0';

	if (prevpage == hitlist[i].text.page &&
	    prevoffset == hitlist[i].text.offset &&
	    strcmp (head, prevhead) == 0)
	  continue;

	xprintf ("%2d. %d:%d\t", point,
	       hitlist[i].text.page, hitlist[i].text.offset);
	xfputs (head, stdout);
	while (eb_read_heading (book, appendix, &heading_hookset,
				NULL, BUFSIZ - 1, headbuf3,
				&heading_len) == EB_SUCCESS &&
	       heading_len > 0) {
	  *(headbuf3 + heading_len) = '\0';
	  xfputs (headbuf3, stdout);
	}
	fputc ('\n', stdout);
      }

      if (head == headbuf1) {
	head = headbuf2;
	prevhead = headbuf1;
      } else {
	head = headbuf1;
	prevhead = headbuf2;
      }
      prevpage = hitlist[i].text.page;
      prevoffset = hitlist[i].text.offset;
    }
    if (num < MAX_HIT_SIZE)
      break;
  }

 exit:
  return 1;
}

static EB_Error_Code insert_prev_next(book, appendix, pos, fp)
     EB_Book *book;
     EB_Appendix *appendix;
     EB_Position *pos;
     FILE *fp;
{
  EB_Error_Code error_code;
  EB_Position pos_temp;
  char head[BUFSIZ];
  ssize_t heading_len;

  error_code = eb_seek_text(book, pos);
  if (error_code != EB_SUCCESS) 
    return error_code;
  error_code = eb_seek_text(book, pos);
  if (error_code != EB_SUCCESS) 
    return error_code;

  if (eb_backward_text(book, appendix) == EB_SUCCESS) {
    error_code = eb_tell_text(book, &pos_temp);
    if (error_code != EB_SUCCESS) 
      return error_code;
    error_code = eb_seek_text(book, &pos_temp);
    if (error_code != EB_SUCCESS) 
      return error_code;
    fprintf(fp, "<prev><reference>");
    while (eb_read_heading (book, appendix, &heading_hookset, NULL,
			    BUFSIZ - 1, head, &heading_len) == EB_SUCCESS &&
	   heading_len > 0) {
      *(head + heading_len) = '\0';
      xfputs (head, fp);
    } 
    fprintf(fp, "</reference=%d:%d></prev>\n", pos_temp.page, pos_temp.offset);
  }
  error_code = eb_seek_text(book, pos);
  if (error_code != EB_SUCCESS) 
    return error_code;
  if (eb_forward_text(book, appendix) == EB_SUCCESS) {
    error_code = eb_tell_text(book, &pos_temp);
    if (error_code != EB_SUCCESS) 
      return error_code;
    error_code = eb_seek_text(book, &pos_temp);
    if (error_code != EB_SUCCESS) 
      return error_code;
    fprintf(fp, "<next><reference>");
    while (eb_read_heading (book, appendix, &heading_hookset, NULL,
			    BUFSIZ - 1, head, &heading_len)  == EB_SUCCESS &&
	   heading_len > 0) {
      *(head + heading_len) = '\0';
      xfputs (head, fp);
    }
    fprintf(fp, "</reference=%d:%d></next>\n", pos_temp.page, pos_temp.offset);
  }
  error_code = eb_seek_text(book, pos);
  return error_code;
};

int
insert_content (book, appendix, pos, begin, length)
     EB_Book *book;
     EB_Appendix *appendix;
     EB_Position *pos;
     int begin;
     int length;
{
  int point;
  ssize_t len;
  char last = '\n';
  char buff[EB_SIZE_PAGE];
  EB_Error_Code error_code = EB_SUCCESS;
  FILE *outFP = stdout;

  show_prev_next_flag = 1;

  /* insert */
  point = 0;
  error_code = eb_seek_text(book, pos);
  if (error_code != EB_SUCCESS) {
    xprintf("An error occured in seek_position: %s\n",
	   eb_error_message(error_code));
    set_error_message (error_code);
    return 0;
  }

#ifdef USE_PAGER
  if (interactive_mode) {
    outFP = popen_pager();
    if (outFP == NULL) outFP = stdout;
  }
#endif

  while (EB_SUCCESS == eb_read_text (book, appendix, &text_hookset, NULL,
				     EB_SIZE_PAGE - 1, buff, &len) &&
	 0 < len) {
    *(buff + len) = '\0';
    /* count up */
    point++;
    if (point >= begin + length && length > 0) {
      xfprintf (outFP, "<more point=%d>\n", point);
      goto exit;
    }

    /* insert */
    if (point >= begin) {
      xfputs (buff, outFP);
      last = buff[len - 1];
    }
  }

  /* insert a newline securely */
  if (last != '\n')
    putc('\n', outFP);

  if (show_prev_next_flag != 0) {
    insert_prev_next(book, appendix, pos, outFP);
  }

 exit:

#ifdef USE_PAGER
  if (interactive_mode) {
    if (outFP != stdout)
      pclose_pager(outFP);
  }
#endif
  return 1;
}

int
insert_dump (book, appendix, pos, length)
     EB_Book *book;
     EB_Appendix *appendix;
     EB_Position *pos;
     int length;
{
  int page;
  ssize_t len;
  unsigned char buff[EB_SIZE_PAGE];
  EB_Error_Code error_code = EB_SUCCESS;
  int i, count;
  long position;
  FILE *outFP = stdout;
  int ret_code = 0;

#ifdef USE_PAGER
  if (interactive_mode) {
    outFP = popen_pager();
    if (outFP == NULL) outFP = stdout;
  }
#endif

  /* insert */
  for (page = 0; page < length; page++) {
    error_code = eb_seek_text(book, pos);
    if (error_code != EB_SUCCESS) {
      xprintf("An error occured in seek_position: %s\n",
	      eb_error_message(error_code));
      set_error_message (error_code);
      goto exit;
    }

    position = (pos->page - 1) * 2048 + pos->offset;

    error_code = eb_read_rawtext (book, EB_SIZE_PAGE, (char *)buff, &len);
    if ((error_code != EB_SUCCESS) || len <= 0) {
      xprintf ("An error occured in command_dump: %s\n",
	       eb_error_message (error_code));
      set_error_message (error_code);
      goto exit;
    }

    /* insert */
    count = 0;
    while (count < EB_SIZE_PAGE) {
      xfprintf(outFP, "%08d:%04d ", position / 2048 + 1, position % 2048);
      for (i = 0; i < 16; i+= 2) {
	xfprintf(outFP, "%02x%02x", buff[count+i], buff[count+i+1]);
      }
      xfprintf(outFP, " ");
      for (i = 0; i < 16; i+= 2) {
	if (0x21 <= buff[count+i] && buff[count+i] <=0x7e &&
	    0x21 <= buff[count+i+1] && buff[count+i+1] <= 0x7e) {
	  xfprintf(outFP, "[%c%c]", buff[count+i] | 0x80, 
		   buff[count+i+1] | 0x80);
	} else {
	  xfprintf(outFP, "%02x%02x", buff[count+i], buff[count+i+1]);
	}
      }
      xfprintf(outFP, "\n");
      position += 16;
      count += 16;
    }
    pos->page ++;
  }
  ret_code = 1;

 exit:
#ifdef USE_PAGER
  if (interactive_mode) {
    if (outFP != stdout)
      pclose_pager(outFP);
  }
#endif

  return ret_code;
}

int
insert_font (book, id)
     EB_Book *book;
     const char *id;
{
  int ch, width, height, start, end;
  size_t size;
  char bitmap[EB_SIZE_WIDE_FONT_48];
  char xbm[EB_SIZE_WIDE_FONT_48_XBM];
  EB_Error_Code error_code = EB_SUCCESS;

  switch (*id) {
  case 'h':
    ch = strtol (id + 1, NULL, 16);
    eb_narrow_font_start (book, &start);
    eb_narrow_font_end (book, &end);
    if (start <= ch && ch <= end) {
      eb_narrow_font_width (book, &width);
      error_code = eb_narrow_font_character_bitmap (book, ch, bitmap);
      if (EB_SUCCESS != error_code)
	goto error;
    } else {
      xprintf ("No such character font: %s\n", id);
      return 0;
    }
    break;

  case 'z':
    ch = strtol (id + 1, NULL, 16);
    eb_wide_font_start (book, &start);
    eb_wide_font_end (book, &end);
    if (start <= ch && ch <= end) {
      eb_wide_font_width (book, &width);
      error_code = eb_wide_font_character_bitmap (book, ch, bitmap);
      if (EB_SUCCESS != error_code)
	goto error;
    } else {
      xprintf ("No such character font: %s\n", id);
      return 0;
    }
    break;

  default:
    xprintf ("Invalid font id: %s\n", id);
    return 0;
  }

  eb_font_height (book, &height);
  eb_bitmap_to_xbm (bitmap, width, height, xbm, &size);
  xbm[size] = '\0';
  xfputs (xbm, stdout);
  return 1;

 error:
  xprintf ("An error occured in insert_font: %s\n",
	  eb_error_message (error_code));
  set_error_message (error_code);
  return 0;
}

int
insert_font_list (book)
     EB_Book *book;
{
  int ch, width, height, start, end;
  size_t size;
  char bitmap[EB_SIZE_WIDE_FONT_48];
  char xbm[EB_SIZE_WIDE_FONT_48_XBM];

  eb_font_height (book, &height);

  eb_narrow_font_width (book, &width);
  eb_narrow_font_start (book, &start);
  eb_narrow_font_end (book, &end);

  for (ch = start; ch < end; ch++)
    if (EB_SUCCESS == eb_narrow_font_character_bitmap (book, ch, bitmap)) {
      eb_bitmap_to_xbm (bitmap, width, height, xbm, &size);
      xbm[size] = '\0';
      xfprintf (stdout, "\nid = h%04x\n", ch);
      xfputs (xbm, stdout);
    }

  eb_wide_font_width (book, &width);
  eb_wide_font_start (book, &start);
  eb_wide_font_end (book, &end);
  for (ch = start; ch < end; ch++)
    if (EB_SUCCESS == eb_wide_font_character_bitmap (book, ch, bitmap)) {
      eb_bitmap_to_xbm (bitmap, width, height, xbm, &size);
      xbm[size] = '\0';
      xfprintf (stdout, "\nid = z%04x\n", ch);
      xfputs (xbm, stdout);
    }
  return 1;
}

void
command_candidate(argc, argv)
     int argc;
     char *argv[];
{
    switch (argc) {
    case 3:
	show_entry_candidate(&current_book, atoi(argv[1])-1, atoi(argv[2])-1);
	break;
    default:
	output_multi_information(&current_book);
	break;
    }
}

void
command_label(argc, argv)
    int argc;
    char *argv[];
{
    if (argc == 1) {
	show_label(&current_book, -1);
    } else {
	show_label(&current_book, atoi(argv[1]) - 1);
    }

}


 
EB_Error_Code
hook_font (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  char buff[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];
  EB_Error_Code result = EB_SUCCESS;

  switch (code) {
  case EB_HOOK_NARROW_FONT:
    if (EB_SUCCESS != eb_narrow_alt_character_text (appendix, argv[0], buff))
      sprintf (buff, "<gaiji=h%04x>", argv[0]);
    result = eb_write_text_string(book, buff);
    break;

  case EB_HOOK_WIDE_FONT:
    if (EB_SUCCESS != eb_wide_alt_character_text (appendix, argv[0], buff))
      sprintf (buff, "<gaiji=z%04x>", argv[0]);
    result = eb_write_text_string(book, buff);
    break;
  }
  return result;
}

#ifdef EB_HOOK_GB2312
EB_Error_Code
hook_gb2312 (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  char buff[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];
  sprintf (buff, "<gaiji=g%04x>", argv[0]);
  eb_write_text_string(book, buff);
  return 0;
}
#endif

#ifdef EB_HOOK_EBXAC_GAIJI
EB_Error_Code
hook_ebxac_gaiji (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  char buff[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];
  sprintf (buff, "<gaiji=c%04x>", (argv[0] & 0x7f7f));
  eb_write_text_string(book, buff);
  return 0;
}
#endif

EB_Error_Code
hook_decoration (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  char buff[256];
  EB_Error_Code result = EB_SUCCESS;

  if (decorate_mode == DECORATE_OFF)
    return EB_SUCCESS;
  switch (code) {
  case EB_HOOK_BEGIN_SUBSCRIPT:
    result = eb_write_text_string(book, "<sub>");
    break;
  case EB_HOOK_END_SUBSCRIPT:
    result = eb_write_text_string(book, "</sub>");
    break;
  case EB_HOOK_BEGIN_SUPERSCRIPT:
    result = eb_write_text_string(book,"<sup>");
    break;
  case EB_HOOK_END_SUPERSCRIPT:
    result = eb_write_text_string(book, "</sup>");
    break;
  case EB_HOOK_BEGIN_NO_NEWLINE:
    result = eb_write_text_string(book, "<no-newline>");
    break;
  case EB_HOOK_END_NO_NEWLINE:
    result = eb_write_text_string(book, "</no-newline>");
    break;
  case EB_HOOK_BEGIN_EMPHASIS:
    result = eb_write_text_string(book, "<em>");
    break;
  case EB_HOOK_END_EMPHASIS:
    result = eb_write_text_string(book, "</em>");
    break;
  case EB_HOOK_SET_INDENT:
    sprintf (buff, "<ind=%d>", argv[1] > 8 ? 9 : argv[1]);
    result = eb_write_text_string(book, buff);
    break;
  case EB_HOOK_BEGIN_DECORATION:
    switch (argv[1]) {
    case 3:
      result = eb_write_text_string(book, "<font=bold>");
      break;
    default:
      result = eb_write_text_string(book, "<font=italic>");
      break;
    }
    break;
  case EB_HOOK_END_DECORATION:
    result = eb_write_text_string(book, "</font>");
    break;
  }
  return result;
}

#ifdef EB_HOOK_STOP_CODE
EB_Error_Code
hook_stopcode (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  const char *stop = variable_ref ("stop-code");

  if (stop) {
    unsigned int c;
    c = strtol (stop, NULL, 0);
    if (c == (argv[0] << 16) + argv[1])
      return EB_ERR_STOP_CODE;
    else
      return EB_SUCCESS;
  }

  return (eb_hook_stop_code (book, appendix, container, code, argc, argv));
}
#endif

EB_Error_Code
hook_img (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  static int imgW, imgH, page, offset;
  static char buff[256];
  EB_Error_Code result = EB_SUCCESS;

  switch (code) {
  case EB_HOOK_BEGIN_MONO_GRAPHIC:
    imgW = argv[3];
    imgH = argv[2];
    if (imgW && imgH) {
      sprintf (buff, "<img=mono:%dx%d>", argv[3], argv[2]);
      result = eb_write_text_string(book, buff);
    }
    break;

  case EB_HOOK_END_MONO_GRAPHIC:
    if (imgW && imgH) {
      sprintf (buff, "</img=%d:%d>", argv[1], argv[2]);
      imgW = imgH = 0;
      result = eb_write_text_string(book, buff);
    }
    break;

  case EB_HOOK_BEGIN_COLOR_JPEG:
    page = argv[2];
    offset = argv[3];
    result = eb_write_text_string(book, "<img=jpeg>");
    break;

  case EB_HOOK_BEGIN_COLOR_BMP:
    page = argv[2];
    offset = argv[3];
    result = eb_write_text_string(book, "<img=bmp>");
    break;

  case EB_HOOK_END_COLOR_GRAPHIC:
    sprintf (buff, "</img=%d:%d>", page, offset);
    result = eb_write_text_string(book, buff);
    break;

#ifdef EB_HOOK_BEGIN_IN_COLOR_JPEG	/* eb-3.3 */
  case EB_HOOK_BEGIN_IN_COLOR_JPEG:
    page = argv[2];
    offset = argv[3];
    result = eb_write_text_string(book, "<inline=jpeg>");
    break;
#endif

#ifdef EB_HOOK_BEGIN_IN_COLOR_BMP	/* eb-3.3 */
  case EB_HOOK_BEGIN_IN_COLOR_BMP:
    page = argv[2];
    offset = argv[3];
    result = eb_write_text_string(book, "<inline=bmp>");
    break;
#endif

#ifdef EB_HOOK_END_IN_COLOR_GRAPHIC
  case EB_HOOK_END_IN_COLOR_GRAPHIC:
    sprintf (buff, "</inline=%d:%d>", page, offset);
    result = eb_write_text_string(book, buff);
    break;
#endif

#ifdef EB_HOOK_BEGIN_IMAGE_PAGE
  case EB_HOOK_BEGIN_IMAGE_PAGE:
    result = eb_write_text_string(book, "<image-page>");
    break;
  case EB_HOOK_END_IMAGE_PAGE:
    show_prev_next_flag = 0;
    result = eb_write_text_string(book, "</image-page>");
    break;
#endif
#ifdef EB_HOOK_BEGIN_WAVE
  case EB_HOOK_BEGIN_WAVE:
    sprintf (buff, "<snd=wav:%d:%d-%d:%d>", argv[2], argv[3], argv[4], argv[5]);
    result = eb_write_text_string(book, buff);
    break;
  case EB_HOOK_END_WAVE:
    result = eb_write_text_string(book, "</snd>");
    break;
#endif
#ifdef EB_HOOK_BEGIN_MPEG
  case EB_HOOK_BEGIN_MPEG:
    sprintf (buff, "<mov=mpg:%d,%d,%d,%d>", argv[2], argv[3], argv[4], argv[5]);
    result = eb_write_text_string(book, buff);
    break;
  case EB_HOOK_END_MPEG:
    result = eb_write_text_string(book, "</mov>");
    break;
#endif
  }
  return result;
}


EB_Error_Code
hook_tags (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  char buff[256];
  EB_Error_Code result = EB_SUCCESS;

  switch (code) {
  case EB_HOOK_BEGIN_REFERENCE:
  case EB_HOOK_BEGIN_CANDIDATE:
    result = eb_write_text_string(book, "<reference>");
    break;
  case EB_HOOK_END_REFERENCE:
  case EB_HOOK_END_CANDIDATE_GROUP:
    sprintf (buff, "</reference=%d:%d>", argv[1], argv[2]);
    result = eb_write_text_string(book, buff);
    break;
#ifdef EB_HOOK_BEGIN_UNICODE
  case EB_HOOK_BEGIN_UNICODE:
    result = eb_write_text_string(book, "<unicode>");
    break;
  case EB_HOOK_END_UNICODE:
    result = eb_write_text_string(book, "</unicode>");
    break;
#endif
#ifdef EB_HOOK_BEGIN_CLICKABLE_AREA
  case EB_HOOK_BEGIN_GRAPHIC_REFERENCE:
    sprintf (buff, "<paged-reference=%d:%d>", argv[1], argv[2]);
    result = eb_write_text_string(book, buff);
    break;
  case EB_HOOK_END_GRAPHIC_REFERENCE:
    result = eb_write_text_string(book, "</paged-reference>");
    break;
  case EB_HOOK_GRAPHIC_REFERENCE:
    sprintf (buff, "<auto-jump-reference></auto-jump-reference=%d:%d>",
	    argv[1], argv[2]);
    result = eb_write_text_string(book, buff);
    show_prev_next_flag = 0;
    break;
  case EB_HOOK_BEGIN_CLICKABLE_AREA:
    sprintf (buff, 
	     "<clickable-area x=%d y=%d w=%d h=%d %d:%d>",
	     argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
    result = eb_write_text_string(book, buff);
    break;
  case EB_HOOK_END_CLICKABLE_AREA:
    result = eb_write_text_string(book, "</clickable-area>");
    break;
#endif
#ifdef EB_HOOK_BEGIN_COLOR_CHART
  case EB_HOOK_BEGIN_COLOR_CHART:
    sprintf (buff, "<color=%d>", argv[1]);
    result = eb_write_text_string(book, buff);
    break;
#endif
  }
  return result;
}


void
show_version ()
{
  xprintf ("%s %s (with EB %d.%d)\n", program_name, program_version,
	  EB_VERSION_MAJOR, EB_VERSION_MINOR);
  xputs ("Copyright (C) 1997,1998,1999,2000 NISHIDA Keisuke");
  xputs ("Copyright (C) 2000-2002 Satomi");
  xputs ("Copyright (C) 2000,2001 Kazuhiko");
  xputs ("Copyright (C) 2000-2002 NEMOTO Takashi");
  xputs ("Copyright (C) 2000,2001 YAMAGATA");
  xputs ("Copyright (C) 2006-2020 Kazuhiro Ito");
  xputs ("eblook may be distributed under the terms of the GNU General Public Licence;");
  xputs ("certain other uses are permitted as well.  For details, see the file");
  xputs ("`COPYING'.");
  xputs ("There is no warranty, to the extent permitted by law.");
}

void
show_help ()
{
  xfprintf (stderr, "Usage: %s [option...] [book-directory [appendix-directory]]\n", program_name);
  xfprintf (stderr, "Options:\n");
  xfprintf (stderr, "  -e, --encoding=NAME   specify input/output encoding\n");
  xfprintf (stderr, "  -q, --no-init-file    ignore user init file\n");
  xfprintf (stderr, "  -i, --non-interactive enter non interactive mode\n");
  xfprintf (stderr, "  -h, --help            show this message\n");
  xfprintf (stderr, "  -v, --version         show version number\n");
  fflush (stderr);
}

void
show_try_help ()
{
  xfprintf (stderr, "Try `%s --help' for more information.\n", invoked_name);
  fflush (stderr);
}

void
set_error_message (error_code)
     EB_Error_Code error_code;
{
  variable_set ("_error", strerror (errno));
  variable_set ("_eb_error", eb_error_message (error_code));
}

void
unset_error_message ()
{
  variable_set ("_error", NULL);
  variable_set ("_eb_error", NULL);
}

EB_Error_Code
eblook_search_keyword(book, pattern)
     EB_Book *book;
     const char *pattern;
{
  char *keyword[EB_MAX_KEYWORDS+2];
  EB_Error_Code error_code;
  char *p = strdup(pattern);
  int i;

  keyword[0] = strtok(p, "=");

  for (i=1; i<EB_MAX_KEYWORDS+2; i++)
    keyword[i] = strtok(0, "=");

  error_code = eb_search_keyword(book, (const char * const *)keyword);

  free(p);
  return error_code;
}

#ifdef EB_MAX_CROSS_ENTRIES
EB_Error_Code
eblook_search_cross(book, pattern)
     EB_Book *book;
     const char *pattern;
{
  char *keyword[EB_MAX_CROSS_ENTRIES+2];
  EB_Error_Code error_code;
  char *p = strdup(pattern);
  int i;

  keyword[0] = strtok(p, "&");

  for (i=1; i<EB_MAX_CROSS_ENTRIES+2; i++)
    keyword[i] = strtok(0, "&");

  error_code = eb_search_cross(book, (const char * const *)keyword);

  free(p);
  return error_code;
}
#endif

EB_Error_Code
eblook_search_multi(book, pattern)
     EB_Book *book;
     const char *pattern;
{
  char *candidate[EB_MAX_MULTI_ENTRIES+2];
  EB_Error_Code error_code;
  char *p = strdup(pattern);
  int i;
  int multi_id = atoi(variable_ref("multi-search-id")) - 1;

#ifdef EBLOOK_SEARCH_MULTI_DEBUG
  xprintf("multi_id = %d\n", multi_id);
#endif

  candidate[0] = strtok(p, ":");

  for (i=1; i<EB_MAX_MULTI_ENTRIES+2; i++) {
    candidate[i] = strtok(0, ":");
  }

  for (i=0; i<EB_MAX_MULTI_ENTRIES+2; i++) {
    if (candidate[i] && 0 == strcmp(candidate[i], "*")) {
      candidate[i] = "";
    }
#ifdef EBLOOK_SEARCH_MULTI_DEBUG
    xprintf("candidate[%d] = %s\n", i, candidate[i]);
#endif
  }

  error_code = eb_search_multi(book, multi_id,
    (const char * const *)candidate);

#ifdef EBLOOK_SEARCH_MULTI_DEBUG
  xprintf("return = %d(%s)\n", error_code, eb_error_message(error_code));
#endif

  free(p);
  return error_code;
}

EB_Error_Code
eblook_search_wild (book, pattern)
     EB_Book *book;
     const char *pattern;
{
  EB_Error_Code error_code;

  if (book->subbook_current == NULL) {
    error_code = EB_ERR_NO_CUR_SUB;
    goto failed;
  }

  if (eblook_have_wild_search(book) == 0) {
    error_code = EB_ERR_NO_SUCH_SEARCH;
    goto failed;
  }

  search_wild_convert_pattern(eblook_wild_pattern, (const unsigned char *)pattern, EB_MAX_WORD_LENGTH * 2);
  eblook_wild_page = 0;
  eblook_wild_count = 0;
  error_code = EB_SUCCESS;

 failed:
  return error_code;
}

EB_Error_Code eblook_hit_list_wild (book, max_hit_count, hit_list, hit_count)
     EB_Book *book;
     int max_hit_count;
     EB_Hit *hit_list;
     int *hit_count;
{
  int start, end, current;
  int i, pos, key_length;
  unsigned char count, current_count = 0;
  static unsigned char buff[EB_SIZE_PAGE];
  unsigned char key[EB_MAX_WORD_LENGTH+1];
  EB_Position eb_pos, content_pos, heading_pos;
  EB_Error_Code error_code;
  ssize_t read_length;

  start = book->subbook_current->word_asis.start_page;
  end = book->subbook_current->word_asis.end_page;
  if (eblook_wild_page != 0) {
    current = eblook_wild_page;
  } else {
    current = start;
  }
  count = 0;

  *hit_count = 0;
  eb_pos.offset = 0;
  while (current <= end) {
    eb_pos.page = current;
    error_code = eb_seek_text(book, &eb_pos);
    if ( error_code != EB_SUCCESS )
      goto failed;

    error_code = eb_read_rawtext(book, EB_SIZE_PAGE, (char *)buff, &read_length);
    if (error_code != EB_SUCCESS || read_length != EB_SIZE_PAGE)
      goto failed;

    i = 0;
    switch (buff[0]) {
    case 0x80:
    case 0xa0:
    case 0xc0:
    case 0xe0:
      pos = 4;
      for (i = uint2(buff + 2); i > 0; i--) {
	key_length = uint1(buff + pos);
	jis_to_euc((char *)key, (char *)buff+pos+1, key_length);
	pos += key_length + 1;

	if (search_wild_match_pattern (key, eblook_wild_pattern)) {
	  count++;
	  if (eblook_wild_count < count) {
	    content_pos.page = uint4(buff + pos);
	    content_pos.offset = uint2(buff + pos + 4);
	    heading_pos.page = uint4(buff + pos + 6);
	    heading_pos.offset = uint2(buff + pos + 10);

	    hit_list[*hit_count].heading = heading_pos;
	    hit_list[*hit_count].text = content_pos;
	    (*hit_count)++;
	    if (*hit_count >= max_hit_count)
	      break;
	  }
	}
	pos += 12;
      }
    default:
      break;
    }

    if (i==0) {
      current++;
      count = 0;
      eblook_wild_count = 0;
    } else {
      eblook_wild_count = count - 1;
      break;
    }
  }

  eblook_wild_page = current;
  return EB_SUCCESS;
 failed:
  eblook_wild_page = 0;
  return error_code;
}

void search_wild_insert_euc_char (pos, ch)
     unsigned char *pos;
     unsigned char ch;
{
  /*小文字は大文字に変換される*/
  static const unsigned char ascii_euc_table_u[] = {
    0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
    0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
    0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
    0xa1, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa3, 0xa1, 0xa1, 0xa1, 0xa1, 0xa1,
    0xa1, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3, 0xa3,
    0xa3, 0xa3, 0xa3, 0xa1, 0xa1, 0xa1, 0xa1
  };

  static const unsigned char ascii_euc_table_l[] = {
    0xa1, 0xaa, 0xc9, 0xf4, 0xf0, 0xf3, 0xf5, 0xc7,
    0xca, 0xcb, 0xf6, 0xdc, 0xa4, 0xdd, 0xa5, 0xbf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xa7, 0xa8, 0xe3, 0xe1, 0xe4, 0xa9,
    0xf7, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xce, 0xef, 0xcf, 0xb0, 0xb2,
    0xae, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xd0, 0xc3, 0xd1, 0xc1
  };

  if (ch >=0x7f || ch < 0x20)
    ch = 0x20;
  ch -= 0x20;
  *pos = ascii_euc_table_u[ch];
  *(pos + 1) = ascii_euc_table_l[ch];
}

int search_wild_convert_pattern (query, pattern, buff_length)
     unsigned char *query;
     const unsigned char *pattern;
     int buff_length;
{
  int length = 0, read = 0;
  int escaped = 0;

  while (length < buff_length) {
    if (pattern[read] == 0)
      break;
    
    if (pattern[read] >= 0x80) {
      escaped = 0;
      if (length + 1 < buff_length) {
	query[length] = pattern[read];
	query[length+1] = pattern[read+1];
	length += 2;
	read += 2;
      } else
	break;
    } else {
      switch (pattern[read]) {
      case '\\':
	if (escaped == 0)
	  escaped = 1;
	else {
	  escaped = 0;
	  if (length + 1 < buff_length) {
	    search_wild_insert_euc_char(query+length, '\\');
	    length += 2;
	  }
	}
	read++;
	break;
      case '*':
      case '?':
	if (escaped==0) {
	  query[length++] = pattern[read++];
	  break;
	}
      default:
	escaped = 0;
	if (length + 1 < buff_length) {
	  search_wild_insert_euc_char(query+length, pattern[read]);
	  length += 2;
	  read++;
	}
      }
    }
  }
  query[length] = 0;

  /* remove spaces */
  length = 0;
  read = 0;
  while (query[read] != 0) {
    if (query[read] < 0x80) {
      if (read != length) {
	query[length] = query[read];
      }
      read++;
      length++;
    } else if (query[read] == 0xa1 || query[read+1] == 0xa1) {
      read += 2;
    } else if (read != length) {
      query[length++] = query[read++];
      query[length++] = query[read++];
    } else  {
      read += 2;
      length += 2;
    }
    if (query[length - 1] == 0) {
      length--;
      break;
    }
  }
  query[length] = 0;

  /* convert lower case to upper as required. */
  if (current_book.subbook_current->word_asis.lower == EB_INDEX_STYLE_CONVERT) {
    read = 0;
    while (read < length) {
      if(query[read] < 0x80) {
	read++;
      } else if (query[read] == 0xa3
		 && query[read + 1] >= 0xe1
		 && query[read + 1] <= 0xfa) {
	query[read + 1] -= 0x20;
	read += 2;
      } else {
	read += 2;
      }
    }
  }

  return length;
}


int search_wild_match_pattern(char *key, char *pattern) {
  char *s;

  while (*key != 0) {
    switch (*pattern) {
    case '?':
      key += 2;
      pattern++;
      break;
    case '*':
      if (pattern + 1 == 0) {
	return 1;
      }
      s = key;
      for (;;) {
	if (search_wild_match_pattern(s, pattern + 1))
	  return 1;
	if (*s == 0)
	  break;
	s += 2;
      }
    case 0:
      return 0;
    default:
      if (*key != *pattern || key[1] != pattern[1])
	return 0;
      key += 2;
      pattern += 2;
      break;
    }
  }
  if (*pattern == 0 || (*pattern == '*' && pattern[1] == 0))
    return 1;
  return 0;
}

int eblook_have_wild_search (book)
     EB_Book *book;
{
  if (book->subbook_current == NULL ||
      book->subbook_current->word_asis.start_page == 0 ||
      book->character_code != EB_CHARCODE_JISX0208)
    return 0;
  return 1;
}


StringAlist *
salist_set (alist, key, value)
     StringAlist *alist;
     const char *key;
     const char *value;
{
  StringAlist *var;

  if (value) {
    /*
     * Set KEY to VALUE
     */
    StringAlist *prev = NULL;
    for (var = alist; var != NULL; var = var->next) {
      if (strcmp (key, var->key) == 0) {
	/* update original value */
	char *p = strdup (value);
	if (p != NULL) {
	  free (var->value);
	  var->value = p;
	} else {
	  xputs ("memory full");
	  if (prev)
	    prev->next = var->next;
	  else
	    alist = var->next;

	  free (var->key);
	  free (var->value);
	  free (var);
	}
	break;
      }
      prev = var;
    }
    if (var == NULL) {
      /* add new element */
      if ((var = malloc (sizeof (StringAlist))) == NULL) {
	xputs ("memory full");
      } else if ((var->key = strdup (key)) == NULL ||
	       (var->value = strdup (value)) == NULL) {
	xputs ("memory full");
	free (var->key);
	free (var);
      } else {
	var->next = alist;
	alist = var;
      }
    }
  } else {
    /*
     * Delete element
     */
    StringAlist *prev = NULL;
    for (var = alist; var != NULL; var = var->next) {
      if (strcmp (key, var->key) == 0) {
	/* delete from alist */
	if (prev)
	  prev->next = var->next;
	else
	  alist = var->next;

	/* free */
	free (var->key);
	free (var->value);
	free (var);
	break;
      }
      prev = var;
    }
  }

  return alist;
}

char *
salist_ref (alist, key)
     StringAlist *alist;
     const char *key;
{
  StringAlist *var;
  for (var = alist; var != NULL; var = var->next)
    if (strcmp (key, var->key) == 0)
      return var->value;

  return NULL;
}

const EB_Position zero_pos = { 0, 0 };

struct multi_can {
    char text[256];
    struct multi_can *child;
    EB_Position child_pos;
    struct multi_can *next;
    int terminated;
};

struct multi_can *head = 0, *tail = 0;

char can_word[256];

EB_Error_Code
can_menu_begin(book, appendix, workbuf, hook_code, argc, argv)
    EB_Book *book;
    EB_Appendix *appendix;
    void *workbuf;
    EB_Hook_Code hook_code;
    int argc;
    const unsigned int *argv;
{
    memset(can_word, 0, sizeof(can_word));
    return EB_SUCCESS;
}

EB_Error_Code
can_menu_end(book, appendix, workbuf, hook_code, argc, argv)
    EB_Book *book;
    EB_Appendix *appendix;
    void *workbuf;
    EB_Hook_Code hook_code;
    int argc;
    const unsigned int *argv;
{
#if MULTI_DEBUG
    xprintf(">> can_word = %s\n", can_word);
#endif

    if (head == 0) {
	head = malloc(sizeof(struct multi_can));
	tail = head;
    } else {
#if MULTI_DEBUG
	xprintf(">> current tail %s:%d->next %s:%d\n",
		tail->text, tail,
		tail->next->text, tail->next);
#endif
	tail->next = malloc(sizeof(struct multi_can));
#if MULTI_DEBUG
	xprintf(">> %s:%d->next = %s:%d\n", tail->text, tail, can_word, tail->next);
#endif
	tail = tail->next;
    }

    memset(tail, 0, sizeof(struct multi_can));
    strcpy(tail->text, can_word);
    memset(can_word, 0, sizeof(can_word));

#if MULTI_DEBUG
    xprintf(">> %s:%d\n", tail->text, tail);
#endif

    if (argv[1] || argv[2]) {
	tail->child_pos.page = argv[1];
	tail->child_pos.offset = argv[2];
    } else {
#if MULTI_DEBUG
	xprintf(">> %s\n", tail->text);
#endif
    }
    return EB_SUCCESS;
}

/*
 * EUC JP to ASCII conversion table.
 */
static const unsigned char euc_a1_to_ascii_table[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x00 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x08 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x10 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x18 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x20 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x28 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x30 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x38 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x40 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x48 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x50 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x58 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x60 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x68 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x70 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x78 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x80 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x88 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x90 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x98 */
    0x00, 0x20, 0x00, 0x00, 0x2c, 0x2e, 0x00, 0x3a,     /* 0xa0 */
    0x3b, 0x3f, 0x21, 0x00, 0x00, 0x00, 0x60, 0x00,     /* 0xa8 */
    0x5e, 0x7e, 0x5f, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xb0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x2f,     /* 0xb8 */
    0x5c, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x27,     /* 0xc0 */
    0x00, 0x22, 0x28, 0x29, 0x00, 0x00, 0x5b, 0x5d,     /* 0xc8 */
    0x7b, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xd0 */
    0x00, 0x00, 0x00, 0x00, 0x2b, 0x2d, 0x00, 0x00,     /* 0xd8 */
    0x00, 0x3d, 0x00, 0x3c, 0x3e, 0x00, 0x00, 0x00,     /* 0xe0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c,     /* 0xe8 */
    0x24, 0x00, 0x00, 0x25, 0x23, 0x26, 0x2a, 0x40,     /* 0xf0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xf8 */
};

static const unsigned char euc_a3_to_ascii_table[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x00 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x08 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x10 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x18 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x20 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x28 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x30 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x38 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x40 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x48 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x50 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x58 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x60 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x68 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x70 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x78 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x80 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x88 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x90 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0x98 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xa0 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xa8 */
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,     /* 0xb0 */
    0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xb8 */
    0x00, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,     /* 0xc0 */
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,     /* 0xc8 */
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,     /* 0xd0 */
    0x58, 0x59, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xd8 */
    0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,     /* 0xe0 */
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,     /* 0xe8 */
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,     /* 0xf0 */
    0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 0xf8 */
};

/*
 * Convert the `workbuf' string from EUC to ASCII.
 */
EB_Error_Code
can_menu_narrow_char(book, appendix, container, hook_code, argc, argv)
    EB_Book *book;
    EB_Appendix *appendix;
    void *container;
    EB_Hook_Code hook_code;
    int argc;
    const unsigned int *argv;
{
    int in_code1, in_code2;
    int out_code = 0;
    unsigned char outchars[3];

    in_code1 = argv[0] >> 8;
    in_code2 = argv[0] & 0xFF;

    if (in_code1 == 0xa1)
	out_code = euc_a1_to_ascii_table[in_code2];
    else if (in_code1 == 0xa3)
	out_code = euc_a3_to_ascii_table[in_code2];

    if (out_code == 0) {
	outchars[0] = in_code1;
	outchars[1] = in_code2;
	outchars[2] = 0;
    } else {
	outchars[0] = out_code;
	outchars[1] = 0;
    }

    strcat(can_word, (char *)outchars);

    return EB_SUCCESS;
}

EB_Error_Code
can_menu_wide_char(book, appendix, container, hook_code, argc, argv)
    EB_Book *book;
    EB_Appendix *appendix;
    void *container;
    EB_Hook_Code hook_code;
    int argc;
    const unsigned int *argv;
{
    unsigned char outchars[3];

    outchars[0] = argv[0] >> 8;
    outchars[1] = argv[0] & 0xFF;
    outchars[2] = 0;

    strcat(can_word, (char *)outchars);

    return EB_SUCCESS;
}

EB_Error_Code
can_menu_gaiji(book, appendix, container, hook_code, argc, argv)
    EB_Book *book;
    EB_Appendix *appendix;
    void *container;
    EB_Hook_Code hook_code;
    int argc;
    const unsigned int *argv;
{
    char workbuf[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];
    char c;

    switch (hook_code) {
    case EB_HOOK_NARROW_FONT:
	c = 'h';
	break;
    case EB_HOOK_WIDE_FONT:
	c = 'z';
	break;
    default:
	c = '?';
    }
    sprintf(workbuf, "<gaiji=%c%04x>", c, argv[0]);
    strcat(can_word, workbuf);
    return EB_SUCCESS;
}

#ifdef EB_HOOK_GB2312
EB_Error_Code
can_menu_gb2312(book, appendix, container, hook_code, argc, argv)
    EB_Book *book;
    EB_Appendix *appendix;
    void *container;
    EB_Hook_Code hook_code;
    int argc;
    const unsigned int *argv;
{
    char workbuf[EB_MAX_ALTERNATION_TEXT_LENGTH + 1];
    sprintf(workbuf, "<gaiji=g%04x>", argv[0]);
    strcat(can_word, workbuf);
    return EB_SUCCESS;
}
#endif

EB_Hookset multi_candidate_hookset;
EB_Hook
multi_candidate_hooks [] = {
  {EB_HOOK_NARROW_FONT,	    can_menu_gaiji},
  {EB_HOOK_WIDE_FONT,	    can_menu_gaiji},
  {EB_HOOK_NARROW_JISX0208, can_menu_narrow_char},
  {EB_HOOK_WIDE_JISX0208,   can_menu_wide_char},
#ifdef EB_HOOK_GB2312
  {EB_HOOK_GB2312,          can_menu_gb2312},
#endif
  {EB_HOOK_BEGIN_CANDIDATE,      can_menu_begin},
  {EB_HOOK_END_CANDIDATE_LEAF,   can_menu_end},
  {EB_HOOK_END_CANDIDATE_GROUP,  can_menu_end},
  {EB_HOOK_NULL,            NULL},
};

struct multi_can *
find_child(can)
    struct multi_can * can;
{
    while (can) {
	if (can->child_pos.page == 0 && can->child_pos.offset == 0) {
#if MULTI_DEBUG
	    xprintf(">> %s noref skip\n", can->text);
#endif
	    can = can->next;
	} else if (can->child != 0) {
#if MULTI_DEBUG
	    xprintf(">> %s has processed child skip\n", can->text);
#endif
	    can = can->next;
	} else {
#if MULTI_DEBUG
	    xprintf(">> unprocessed child found %d\n", can);
	    xprintf(">> %s[\n", can->text);
#endif
	    return can;
	}
    }
    return 0;
}

int
process_child(book, can)
    EB_Book *book;
    struct multi_can * can;
{
    EB_Error_Code error_code;
    char buf[2048];
    ssize_t buflen;

#if MULTI_DEBUG
    xprintf(">> seeking %d:%d\n", can->child_pos.page, can->child_pos.offset);
#endif
    error_code = eb_seek_text(book, &can->child_pos);
#if MULTI_DEBUG
    xprintf(">> eb_seek_text %s\n", eb_error_message(error_code));
#endif
    error_code = eb_read_text(book, 0, &multi_candidate_hookset, NULL, 2047, buf, &buflen);
#if MULTI_DEBUG
    xprintf(">> eb_read_text %s\n", eb_error_message(error_code));
    xprintf(">> buflen = %ld\n", (long)buflen);
    xprintf(">> ]\n");
#endif

    tail->terminated = 1;
    return(0);
}

void
show_candidates_level(can, level)
    struct multi_can * can;
    int level;
{
    char * indent;

    indent = malloc(level+1);
    if (!indent) {
      xputs("memory full");
      return;
    }
    memset(indent, '\t', level);
    *(indent + level) = '\0';

    while (1) {
	xprintf("%s%c%s\n", indent, can->child ? ' ': '*', can->text);
	if (can->child) {
	    show_candidates_level(can->child, level+1);
	}
	if (can->terminated)
	    break;
	can = can->next;
    }

    free(indent);
}

void
free_candidates_tree(can)
    struct multi_can * can;
{
    struct multi_can * next;

    do {
	next = can->next;
#if MULTI_DEBUG
	xfprintf(stderr, ">> freeing %s:%d\n", can->text, can);
	xfprintf(stderr, ">> next is %s:%d\n", next->text, next);
#endif
	free(can);
	can = next;
    } while (next);
}

int
show_candidate(book, pos0)
    EB_Book *book;
    EB_Position pos0;
{
    char buf[2048];
    ssize_t buflen;
    struct multi_can *child, *ptail;
    EB_Error_Code error_code = EB_SUCCESS;

    eb_initialize_hookset (&multi_candidate_hookset);
    eb_set_hooks (&multi_candidate_hookset, multi_candidate_hooks);

    error_code = eb_seek_text(book, &pos0);
    if (error_code != EB_SUCCESS) {
        xprintf("An error occured in seek_position: %s\n",
	       eb_error_message(error_code));
	set_error_message (error_code);
	return 0;
    }
    error_code = eb_read_text(book, 0, &multi_candidate_hookset, NULL,
			      2047, buf, &buflen);
    if (error_code != EB_SUCCESS) {
        xprintf("An error occured in read_text: %s\n",
	       eb_error_message(error_code));
	set_error_message (error_code);
	return 0;
    }
#if MULTI_DEBUG
    xprintf(">> buflen = %ld\n", (long)buflen);
#endif

    tail->terminated = 1;

    while ((child = find_child(head)) != 0) {
	ptail = tail;
#if MULTI_DEBUG
	xprintf(">> current tail %s:%d->next %s:%d\n",
		tail->text, tail,
		tail->next->text, tail->next);
#endif
	process_child(book, child);
	child->child = ptail->next;
    }

    show_candidates_level(head, 0);

    free_candidates_tree(head);
    head = 0;
    tail = 0;

    return 1;
}


/*
 * Output information about multi searches.
 */
static void
output_multi_information(book)
    EB_Book *book;
{
    EB_Error_Code error_code;
    EB_Multi_Search_Code multi_list[EB_MAX_MULTI_SEARCHES];
    EB_Multi_Entry_Code entry_list[EB_MAX_MULTI_ENTRIES];
    int multi_count;
    int entry_count;
    char entry_label[EB_MAX_MULTI_LABEL_LENGTH + 1];
    int i, j;

    error_code = eb_multi_search_list(book, multi_list, &multi_count);
    if (error_code != EB_SUCCESS) {
	xprintf("eb_multi_search_list %s\n", eb_error_message(error_code));
	return;
    }
    for (i = 0; i < multi_count; i++) {
	xprintf("  multi search %d:\n", i + 1);
	error_code = eb_multi_entry_list(book, multi_list[i], entry_list,
	    &entry_count);
	if (error_code != EB_SUCCESS) {
	    xprintf("eb_multi_entry_list %s\n", eb_error_message(error_code));
	    continue;
	}
	for (j = 0; j < entry_count; j++) {
	    error_code = eb_multi_entry_label(book, multi_list[i],
		entry_list[j], entry_label);
	    if (error_code != EB_SUCCESS) {
		xprintf("eb_multi_entry_label %s\n",
		eb_error_message(error_code));
		continue;
	    }

	    xprintf("    label %d: %s\n", j + 1, entry_label);
	    xfputs("      candidates: ", stdout);
	    if (eb_multi_entry_have_candidates(book, multi_list[i],
		entry_list[j])) {
		    EB_Position pos;

		    xfputs("exist\n", stdout);

		    eb_multi_entry_candidates(book,
				multi_list[i], entry_list[j], &pos);
#if MULTI_DEBUG
		    xprintf(">> candidate = %d:%d\n", pos.page, pos.offset);
#endif

		    show_candidate(book, pos);

		}
	    else
		xfputs("not-exist\n", stdout);
	}
    }
    fflush(stdout);
}



void
show_entry_candidate(book, search_id, entry_id)
    EB_Book *book;
    int search_id;
    int entry_id;
{
    EB_Error_Code error_code;
    EB_Multi_Search_Code multi_list[EB_MAX_MULTI_SEARCHES];
    EB_Multi_Entry_Code entry_list[EB_MAX_MULTI_ENTRIES];
    EB_Position candidate_pos;
    /*    char entry_label[EB_MAX_MULTI_LABEL_LENGTH + 1]; */
    int multi_count;
    int entry_count;

    if (!eb_have_multi_search(book))
	return;

    error_code = eb_multi_search_list(book, multi_list, &multi_count);
    if (error_code != EB_SUCCESS) {
	xprintf("eb_multi_search_list: %s\n", eb_error_message(error_code));
	return;
    }

    if (search_id >= multi_count || search_id < 0)
	return;
    error_code = eb_multi_entry_list(book, multi_list[search_id],
		entry_list, &entry_count);
    if (error_code != EB_SUCCESS) {
	xprintf("eb_multi_entry_list %s\n", eb_error_message(error_code));
	return;
    }

    if (entry_id >= entry_count || entry_count < 0)
	return;
    if (!eb_multi_entry_have_candidates(book, multi_list[search_id], entry_list[entry_id])) {
	xprintf(" no-candidate\n");
	return;
    }


    if ((error_code = eb_multi_entry_candidates(book, multi_list[search_id],
		entry_list[entry_id], &candidate_pos)) != EB_SUCCESS) {
	xprintf("eb_multi_entry_candidates %s\n", eb_error_message(error_code));
	return;
    }

    show_candidate(book, candidate_pos);
}

void
show_label(book, id)
    EB_Book *book;
    int id;
{
    EB_Error_Code error_code;
    EB_Multi_Search_Code multi_list[EB_MAX_MULTI_SEARCHES];
    EB_Multi_Entry_Code entry_list[EB_MAX_MULTI_ENTRIES];
    char entry_label[EB_MAX_MULTI_LABEL_LENGTH + 1];
    int multi_count;
    int entry_count;
    int i, j;


    if (!eb_have_multi_search(book))
	return;

    error_code = eb_multi_search_list(book, multi_list, &multi_count);
    if (error_code != EB_SUCCESS) {
	xprintf("eb_multi_search_list: %s\n", eb_error_message(error_code));
	return;
    }

    for (i=0; i<multi_count; i++) {
	if (id != -1 && id != i)
	    continue;

	xprintf("%2d. ", i+1);
	error_code = eb_multi_entry_list(book, multi_list[i], entry_list,
	    &entry_count);
	if (error_code != EB_SUCCESS) {
	    xprintf("eb_multi_entry_list %s\n", eb_error_message(error_code));
	    continue;
	}

	for (j=0; j<entry_count; j++) {

	    error_code = eb_multi_entry_label(book, multi_list[i],
		entry_list[j], entry_label);
	    if (error_code != EB_SUCCESS) {
		xprintf("eb_multi_entry_label %s\n", eb_error_message(error_code));
		continue;
	    }

	    xprintf("%s:", entry_label);
	}
	xprintf("\n");
    }

}

EB_Error_Code 
hook_euc_to_ascii (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  int high, low;

  static unsigned char table21[]=
    "Z\xa4\xa1ZZ\xa5ZZZZ\xde\xdfZZZZ"
    "ZZZZZZZZZZZ\xb0ZZZZ"
    "ZZZZZZZZZZZZZZZZ"
    "ZZZZZ\xa2\xa3ZZZZZZZZZ"
    "ZZZZZZZZZZZZZZZZ"
    "ZZZZZZZZZZZZZZ";
  static unsigned char table25[] = 
    "\xa7\xb1\xa8\xb2\xa9\xb3\xaa\xb4\xab\xb5" /* a - O */
    "\xb6\x76\xb7\x77\xb8\x78\xb9\x79\xba\x7a" /* Ka Ga .... Go */
    "\xbb\x7b\xbc\x7c\xbd\x7d\xbe\x7e\xbf\x7f" /* Sa Za Shi ... Zo */
    "\xc0\x40\xc1\x41\xaf\xc2\x42\xc3\x43\xc4\x44" /* Ta Da ... Do */
    "\xc5\xc6\xc7\xc8\xc9" /* Na Ni ... No */
    "\xca\x4a\x0a\xcb\x4b\x0b\xcc\x4c\x0c\xcd\x4d\x0d\xce\x4e\x0e" /* Ha - Po */
    "\xcf\xd0\xd1\xd2\xd3" "\xac\xd4\xad\xd5\xae\xd6" /* Ma - Yo */
    "\xd7\xd8\xd9\xda\xdb" " \xdc  \xa6\xdd   ";

  if (escape_text) {
    switch (argv[0]){
    case 0xa1f5: /* wide "&" */
      return eb_write_text_string(book, "&amp;");
    case 0xa1e3: /* wide "<" */
      return eb_write_text_string(book, "&lt;");
    case 0xa1e4: /* wide ">" */
      return eb_write_text_string(book, "&gt;");
    }
  }

  if (use_narrow_kana == 0)
      return eb_hook_euc_to_ascii (book, appendix, container, code, argc, argv);
  high = (argv[0] >> 8) & 0x7f;
  low = (argv[0] & 0xff) & 0x7f;

  if (high == 0x25) {
      int x;
      x = table25[low-0x21];
      if (x != 'Z') {
	  if (x & 0x80) { /* Japanese Hankaku Kana (JISX0201.1978) */
	      return eb_write_text_byte2(book, 0x8e, x);
	  } else if (x & 0x40) { /* Japanese Hankaku Dakuon */
	      if (x >= 0x60) x -= 0x40;
	      eb_write_text_byte2(book, 0x8e, x + 0x80);
	      return eb_write_text_byte2(book, 0x8e, 0xde);
	  } else {
	      if (x < 0x20) x += 0x40;
	      eb_write_text_byte2(book, 0x8e, x+0x80);
	      return eb_write_text_byte2(book, 0x8e, 0xdf);
	  }
      }
  } else if (high == 0x21) {
      if (table21[low-0x21] & 0x80) {
	  return eb_write_text_byte2(book, 0x8e, table21[low-0x21]);
      }

  }
  return eb_hook_euc_to_ascii (book, appendix, container, code, argc, argv);
}

EB_Error_Code 
hook_iso8859_1 (book, appendix, container, code, argc, argv)
     EB_Book *book;
     EB_Appendix *appendix;
     void *container;
     EB_Hook_Code code;
     int argc;
     const unsigned int *argv;
{
  if (escape_text) {
    switch (argv[0]) {
    case '&':
      return eb_write_text_string(book, "&amp;");
    case '<':
      return eb_write_text_string(book, "&lt;");
    case '>':
      return eb_write_text_string(book, "&gt;");
    }
  }

  switch (argv[0]) {
  case 0xa0:
    return eb_write_text_byte1(book, 0x20);
  default:
    return eb_write_text_byte1(book, argv[0]);
  }
}

#ifdef USE_PAGER 
FILE *popen_pager()
{
  char *pager;
  pager = variable_ref("pager");
  if (pager == NULL || strcasecmp(pager,"off")==0 ) return NULL;
  if (strcasecmp(pager,"on") == 0) {
    pager = getenv("PAGER");
  }
  if (pager == NULL) return NULL;
  return popen(pager, "w");
}

int pclose_pager(FILE *stream)
{
  fflush(stream);
  return pclose(stream);
}

#endif /* USE_PAGER */

#ifdef USE_READLINE
static int issp (char);
static int
issp(ch)
     char ch;
{
  if (ch=='\t' || ch==' ' || ch=='\r' || ch=='\n') return 1;
  return 0;
}

char *
stripwhite (string)
     char *string;
{
  unsigned char *s, *t;
  if (string == NULL) return NULL;
  s = string; 
  while (issp(*s)) 
    s++;
  if (*s == 0)
    return (s);

  t = s + strlen (s) - 1;
  while (t > s && issp(*t))
    t--;
  *++t = '\0';
  return s;
}

char **
fileman_completion (text, start, end)
     char *text;
     int start, end;
{
  char **matches;

  matches = NULL;

  /* If this word is at the start of the line, then it is a command
     to complete.  Otherwise it is the name of a file in the current
     directory. */
  if (start == 0)
    matches =
      rl_completion_matches (text, (rl_compentry_func_t *) command_generator);
  return matches;
}

/* Generator function for command completion.  STATE lets us know whether
   to start from scratch; without any state (i.e. STATE == 0), then we
   start at the top of the list. */
char *
command_generator (text, state)
     char *text;
     int state;
{
  static int list_index, len;
  const char *name;

  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (!state) {
    list_index = 0;
    len = strlen (text);
  }

  /* Return the next name which partially matches from the command list. */
  while ((name = command_table[list_index].name) != NULL) {
    list_index++;
    if (strncmp (name, text, len) == 0)
        return (strdup(name));
    }

  /* If no names matched, then return NULL. */
  return NULL;
}
#endif
