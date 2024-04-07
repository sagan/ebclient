
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <linux/limits.h>

#include "book.h"
#include "conv.h"

#define MAX_HITS 100
#define MAXLEN_HEADING 255
#define MAXLEN_TEXT 65535

typedef struct book_node {
  book_t* book;
  char* title;
  int subbook_index;
  struct book_node_t* next;
} book_node_t;

book_t* current_bookw;
EB_Hookset hookset;
EB_Hookset hookset_header;
char xpath[32] = {0};
char utf16bec[3] = {0};
char heading[MAXLEN_HEADING + 1];
char text[MAXLEN_TEXT+1];
char buf[128]; // general temp buf
char buf_color[EB_MAX_COLOR_VALUE_LENGTH + 1];
char buf_gaiji[10];
char buf_gaiji_narrow_bitmap[EB_SIZE_NARROW_FONT_16];
char buf_gaiji_wide_bitmap[EB_SIZE_WIDE_FONT_16];
char buf_binary[1024*1024*16]; // 32MB max
int hit_count;
ssize_t heading_length;
ssize_t text_length;
EB_Hit hits[MAX_HITS];
int hits_index_sorted[MAX_HITS];
book_node_t* books = NULL;
size_t books_count = 0;
char in[3] = {0};

#define EUC_TO_ASCII_TABLE_START        0xa0
#define EUC_TO_ASCII_TABLE_END          0xff

static const unsigned char euc_a1_to_ascii_table[] = {
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

EB_Book* select_book(int index) {
  book_t* bookw;
  EB_Book* book;
  book_node_t* current;

  current = books;
  while( index > 0 ) {
    if( current != NULL )
      current = current->next;
    index--;
  }
  if( current == NULL )
    return NULL;

  bookw = current->book;
  book = &bookw->book;

  EB_Error_Code error_code = eb_set_subbook(book, bookw->subbook_list[current->subbook_index]);
  if( bookw->app != NULL) {
    eb_set_appendix_subbook(bookw->app, bookw->subbook_list[current->subbook_index]);
  }
  if (error_code != EB_SUCCESS) {
    return NULL;
  }

  current_bookw = bookw;
  return book;
}

char* convert_to_internal_encoding(EB_Book* book, char* s) {
  if( book->character_code == EB_CHARCODE_JISX0208 )
    return conv_utf8_to_euc_str(s, strlen(s));
  return s;
}

char* convert_from_internal_encoding(EB_Book* book, char* s) {
  if( book->character_code == EB_CHARCODE_JISX0208 )
    return conv_euc_str(s, strlen(s));
  return s;
}

EB_Error_Code hook_iso8859(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  in[0] = argv[0];
  eb_write_text_string(book, conv_iso8859_str(in, 1));
  return EB_SUCCESS;
}

EB_Error_Code hook_euc(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  in[0] = argv[0] >> 8;
  in[1] = argv[0] & 0xff;
  eb_write_text_string(book, conv_euc_str(in, 3));
  //eb_write_text_byte2(book, in_code1, in_code2);
  return EB_SUCCESS;
}

EB_Error_Code hook_euc_narrow(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  int in_code1, in_code2;
    int out_code = 0;

    in_code1 = argv[0] >> 8;
    in_code2 = argv[0] & 0xff;

    if (in_code2 < EUC_TO_ASCII_TABLE_START
        || EUC_TO_ASCII_TABLE_END < in_code2) {
        out_code = 0;
    } else if (in_code1 == 0xa1) {
        out_code = euc_a1_to_ascii_table[in_code2 - EUC_TO_ASCII_TABLE_START];
    } else if (in_code1 == 0xa3) {
        out_code = euc_a3_to_ascii_table[in_code2 - EUC_TO_ASCII_TABLE_START];
    }

    if (out_code == 0) {
      in[0] = in_code1;
      in[1] = in_code2;
      eb_write_text_string(book, conv_euc_str(in, 3));
      //eb_write_text_byte2(book, in_code1, in_code2);
    } else {
      eb_write_text_byte1(book, out_code);
    }

    return EB_SUCCESS;
}

// GAIJI  EPWINGの外字(書籍定義文字)です。hXXXXは半角、zXXXXは全角を表します。

EB_Error_Code narrow_character_text(EB_Book *book, EB_Appendix *appendix, void *container, EB_Hook_Code hook_code, int argc, const unsigned int *argv) {

  if( current_bookw->gaijimap_tree != NULL ) {
    sprintf(xpath, "%04X", argv[0]);
    mxml_node_t *node = mxmlFindElement(current_bookw->gaijimap_tree, current_bookw->gaijimap_tree, "gaijiMap", "ebcode", xpath, MXML_DESCEND);
    if( node != NULL ) {
      char *unicode = mxmlElementGetAttr(node, "unicode"); // "#x60FD" format
      sscanf(unicode,"#x%2x%2x", utf16bec, utf16bec+1);
      eb_write_text_string(book, conv_utf16be_str(utf16bec, sizeof(utf16bec)));
      return EB_SUCCESS;
    }
  }

  sprintf(buf_gaiji,"{{h%04x}}", argv[0]);
  eb_write_text_string(book, buf_gaiji);
  return EB_SUCCESS;
}

EB_Error_Code wide_character_text(EB_Book *book, EB_Appendix *appendix, void *container, EB_Hook_Code hook_code, int argc, const unsigned int *argv) {

  if( current_bookw->gaijimap_tree != NULL ) {
    sprintf(xpath, "%04X", argv[0]);
    mxml_node_t *node = mxmlFindElement(current_bookw->gaijimap_tree, current_bookw->gaijimap_tree, "gaijiMap", "ebcode", xpath, MXML_DESCEND);
    if( node != NULL ) {
      char *unicode = mxmlElementGetAttr(node, "unicode"); // "#x60FD" format
      sscanf(unicode,"#x%2x%2x", utf16bec, utf16bec+1);
      eb_write_text_string(book, conv_utf16be_str(utf16bec, sizeof(utf16bec)));
      return EB_SUCCESS;
    }
  }

  sprintf(buf_gaiji,"{{z%04x}}", argv[0]);
  eb_write_text_string(book, buf_gaiji);
  return EB_SUCCESS;
}

// ebu 4.5 色見本 (color chart)
EB_Error_Code hook_color(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {

  // argv[0] color number
  EB_Error_Code error_code = eb_color_value(book, argv[1], buf_color);
  /*
  黄色： 5Y8\/14
  */
  if (error_code == EB_SUCCESS) {
    sprintf(buf, "Munsell color system: %s", buf_color);
    eb_write_text_string(book, buf);
  }
  return EB_SUCCESS;
}

EB_Error_Code hook_general(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  switch( argv[0] ) {
    case 0x1f0e:
      eb_write_text_string(book, "[superscript]");
      break;
    case 0x1f0f:
      eb_write_text_string(book, "[/superscript]");
      break;
    case 0x1f41:
      eb_write_text_string(book, "[keyword]");
      break;
    case 0x1f61:
      eb_write_text_string(book, "[/keyword]");
      break;
    case 0x1f06:
      eb_write_text_string(book, "[subscript]");
      break;
    case 0x1f07:
      eb_write_text_string(book, "[/subscript]");
      break;
    case 0x1fe0:
      eb_write_text_string(book, "[decoration]");
      break;
    case 0x1fe1:
      eb_write_text_string(book, "[/decoration]");
      break;
    case 0x1f12:
      eb_write_text_string(book, "[emphasis]");
      break;
    case 0x1f13:
      eb_write_text_string(book, "[/emphasis]");
      break;
    case 0x1f42:
      eb_write_text_string(book, "[reference]");
      break;
    case 0x1f62:
      sprintf(buf, "[/reference page=%d,offset=%d]", argv[1], argv[2]);
      eb_write_text_string(book, buf);
      break;
    default:
      break;
  }
  return EB_SUCCESS;
}

EB_Error_Code hook_bmp(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  switch( argv[0] ) {
    case 0x1f32: //  EB_HOOK_BEGIN_MONO_GRAPHIC
    case 0x1f44:
      sprintf(buf, "[mono width=%d,height=%d]", argv[3], argv[2]);
      eb_write_text_string(book, buf);
      break;
    case 0x1f4d: // EB_HOOK_BEGIN_COLOR_BMP,EB_HOOK_BEGIN_COLOR_JPEG
      sprintf(buf, "[image format=bmp,inline=0,page=%d,offset=%d]", argv[2], argv[3]);
      eb_write_text_string(book, buf);
      break;
    case 0x1f3c: // EB_HOOK_BEGIN_IN_COLOR_BMP,EB_HOOK_BEGIN_IN_COLOR_JPEG
      sprintf(buf, "[image format=bmp,inline=1,page=%d,offset=%d]", argv[2], argv[3]);
      eb_write_text_string(book, buf);
      break;
    case 0x1f52: //EB_HOOK_END_MONO_GRAPHIC
    case 0x1f64:
      sprintf(buf, "[/mono page=%d,offset=%d]", argv[1], argv[2]);
      eb_write_text_string(book, buf);
      break;
    case 0x1f6d: //EB_HOOK_END_COLOR_GRAPHIC, 8045
    case 0x1f5c: //EB_HOOK_END_IN_COLOR_GRAPHIC
      eb_write_text_string(book, "[/image]");
      break;
    default:
      break;
  }
  return EB_SUCCESS;
}

EB_Error_Code hook_jpg(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  switch( argv[0] ) {
    case 0x1f4d: // EB_HOOK_BEGIN_COLOR_BMP,EB_HOOK_BEGIN_COLOR_JPEG
      sprintf(buf, "[image format=jpg,inline=0,page=%d,offset=%d]", argv[2], argv[3]);
      eb_write_text_string(book, buf);
      break;
    case 0x1f3c: // EB_HOOK_BEGIN_IN_COLOR_BMP,EB_HOOK_BEGIN_IN_COLOR_JPEG
      sprintf(buf, "[image format=jpg,inline=1,page=%d,offset=%d]", argv[2], argv[3]);
      eb_write_text_string(book, buf);
      break;
    default:
      break;
  }
  return EB_SUCCESS;
}

EB_Error_Code hook_wav(EB_Book *book, EB_Appendix *appendix, void *container,
  EB_Hook_Code hook_code, int argc, const unsigned int *argv) {
  switch( argv[0] ) {
    case 0x1f4a: // EB_HOOK_BEGIN_WAVE
      sprintf(buf, "[wav page=%d,offset=%d,endpage=%d,endoffset=%d]", argv[2], argv[3], argv[4], argv[5]);
      eb_write_text_string(book, buf);
      break;
    case 0x1f6a: // EB_HOOK_END_WAVE
      eb_write_text_string(book, "[/wav]");
      break;
    default:
      break;
  }
  return EB_SUCCESS;
}

void books_init(const char* rootpath) {
  DIR *dp;
  struct dirent *ep;     
  char path[PATH_MAX] = {0};

  eb_initialize_library();
  eb_initialize_hookset(&hookset);
  eb_initialize_hookset(&hookset_header);
  hookset.hooks[EB_HOOK_ISO8859_1].function = hook_iso8859;
  hookset.hooks[EB_HOOK_WIDE_JISX0208].function = hook_euc;
  hookset.hooks[EB_HOOK_NARROW_JISX0208].function = hook_euc_narrow;
  hookset.hooks[EB_HOOK_WIDE_FONT].function = wide_character_text;
  hookset.hooks[EB_HOOK_NARROW_FONT].function= narrow_character_text;
  hookset.hooks[EB_HOOK_BEGIN_REFERENCE].function= hook_general;
  hookset.hooks[EB_HOOK_END_REFERENCE].function= hook_general;
  hookset.hooks[EB_HOOK_BEGIN_KEYWORD].function= hook_general;
  hookset.hooks[EB_HOOK_END_KEYWORD].function= hook_general;
  hookset.hooks[EB_HOOK_BEGIN_DECORATION].function= hook_general;
  hookset.hooks[EB_HOOK_END_DECORATION].function= hook_general;
  hookset.hooks[EB_HOOK_BEGIN_SUBSCRIPT].function= hook_general;
  hookset.hooks[EB_HOOK_END_SUBSCRIPT].function= hook_general;
  hookset.hooks[EB_HOOK_BEGIN_SUPERSCRIPT].function= hook_general;
  hookset.hooks[EB_HOOK_END_SUPERSCRIPT].function= hook_general;
  hookset.hooks[EB_HOOK_BEGIN_EMPHASIS].function= hook_general;
  hookset.hooks[EB_HOOK_END_EMPHASIS].function= hook_general;

  hookset.hooks[EB_HOOK_BEGIN_MONO_GRAPHIC].function= hook_bmp;
  hookset.hooks[EB_HOOK_END_MONO_GRAPHIC].function= hook_bmp;
  hookset.hooks[EB_HOOK_BEGIN_COLOR_BMP].function= hook_bmp;
  hookset.hooks[EB_HOOK_BEGIN_COLOR_JPEG].function= hook_jpg;
  hookset.hooks[EB_HOOK_END_COLOR_GRAPHIC].function= hook_bmp;
  hookset.hooks[EB_HOOK_BEGIN_IN_COLOR_BMP].function= hook_bmp;
  hookset.hooks[EB_HOOK_BEGIN_IN_COLOR_JPEG].function= hook_jpg;
  hookset.hooks[EB_HOOK_END_IN_COLOR_GRAPHIC].function= hook_bmp;
  hookset.hooks[EB_HOOK_BEGIN_WAVE].function= hook_wav;
  hookset.hooks[EB_HOOK_END_WAVE].function= hook_wav;
  hookset.hooks[EB_HOOK_BEGIN_COLOR_CHART].function= hook_color;

  hookset_header.hooks[EB_HOOK_ISO8859_1].function = hook_iso8859;
  hookset_header.hooks[EB_HOOK_WIDE_JISX0208].function = hook_euc;
  hookset_header.hooks[EB_HOOK_NARROW_JISX0208].function = hook_euc_narrow;
  hookset_header.hooks[EB_HOOK_WIDE_FONT].function = wide_character_text;
  hookset_header.hooks[EB_HOOK_NARROW_FONT].function= narrow_character_text;
  hookset_header.hooks[EB_HOOK_BEGIN_DECORATION].function= hook_general;
  hookset_header.hooks[EB_HOOK_END_DECORATION].function= hook_general;
  hookset_header.hooks[EB_HOOK_BEGIN_SUBSCRIPT].function= hook_general;
  hookset_header.hooks[EB_HOOK_END_SUBSCRIPT].function= hook_general;
  hookset_header.hooks[EB_HOOK_BEGIN_SUPERSCRIPT].function= hook_general;
  hookset_header.hooks[EB_HOOK_END_SUPERSCRIPT].function= hook_general;
  hookset_header.hooks[EB_HOOK_BEGIN_EMPHASIS].function= hook_general;
  hookset_header.hooks[EB_HOOK_END_EMPHASIS].function= hook_general;

  dp = opendir(rootpath);
  if (dp != NULL) {

    while (ep = readdir(dp)) {
      if( ep->d_type != DT_DIR || ep->d_name[0] == '.')
        continue;
      if( rootpath[strlen(rootpath)-1] == '/' )
        sprintf(path, "%s%s", rootpath, ep->d_name);
      else
        sprintf(path, "%s/%s", rootpath, ep->d_name);
      book_load(path);
    }

    closedir (dp);
  }
}

book_t* book_load(const char* path) {
  book_t* bookw = (book_t*)malloc(sizeof(book_t));
  eb_initialize_book(&(bookw->book));
  bookw->gaijimap_tree = NULL;
  bookw->app = NULL;
  EB_Book* book = &bookw->book;
  int i = 0;
  char title[256];

  EB_Error_Code error_code = eb_bind(book, path);
  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "failed to bind the book, %s: %s\n", eb_error_message(error_code), path);
    goto die;
  }
  error_code = eb_subbook_list(book, bookw->subbook_list, &bookw->subbook_count);
  if (error_code != EB_SUCCESS) {
      fprintf(stderr, "failed to get the subbbook list, %s\n", eb_error_message(error_code));
      goto die;
  }
  char gaijimap_path[PATH_MAX] = {0};
  if( path[strlen(path)-1] == '/' )
    sprintf(gaijimap_path, "%sgaijimap.xml", path);
  else
    sprintf(gaijimap_path, "%s/gaijimap.xml", path);
  FILE *fp = fopen(gaijimap_path, "r");
  if( fp != NULL ) {
    bookw->gaijimap_tree = mxmlLoadFile(NULL, fp, MXML_NO_CALLBACK);
    fclose(fp);
  }

  EB_Appendix *app_pointer;
  app_pointer = (EB_Appendix *) malloc(sizeof(EB_Appendix));
  eb_initialize_appendix(app_pointer);
  if (eb_bind_appendix(app_pointer, path) == EB_SUCCESS) {
    bookw->app = app_pointer;
  } else {
    eb_finalize_appendix(app_pointer);
    free(app_pointer);
  }

  int count = bookw->subbook_count; //  这尼玛见鬼了,直接比较 i < bookw->subbook_count 在某些机器上死活有问题!!
  for(i = 0; i < count; i++) {
    book_node_t* new_book_node = (book_node_t*)malloc(sizeof(book_node_t));
    memset(new_book_node, 0, sizeof(book_node_t));
    new_book_node->book = bookw;
    new_book_node->subbook_index = i;

    eb_subbook_title2(book, bookw->subbook_list[i], title);
    char* utf8title = convert_from_internal_encoding(book, title);
    new_book_node->title = (char*)malloc(strlen(utf8title) + 1);
    strcpy(new_book_node->title, utf8title);

    // printf("subbook title: %d %s %d\n", i, utf8title, bookw->subbook_list[i]);
    // eb_subbook_directory2(book, bookw->subbook_list[i], title);
    // printf("subbook path: %s\n", utf8title);

    if( books == NULL ) {
      books = new_book_node;
    } else {
      book_node_t* end = books;
      while( end->next )
        end = end->next;
      end->next = new_book_node;
    }
  }

  return bookw;
die:
  book_unload(bookw);
  return NULL;
}

void book_unload(book_t* bookw) {
  if( bookw == NULL )
    return;
  if( bookw->gaijimap_tree != NULL )
    mxmlDelete(bookw->gaijimap_tree);
	eb_finalize_book(&(bookw->book));
	free(bookw);
}

char* book_binary_gaiji_narrow(int index, int code, size_t* size) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }
  eb_set_font(book, EB_FONT_16);

  EB_Error_Code error_code = eb_narrow_font_character_bitmap(book, code, buf_gaiji_narrow_bitmap);
  if (error_code != EB_SUCCESS) {
    return NULL;
  }

  if (eb_bitmap_to_png(buf_gaiji_narrow_bitmap, EB_WIDTH_NARROW_FONT_16,
    EB_HEIGHT_FONT_16, buf_binary, size) != EB_SUCCESS) {
    return NULL;
  }
  return buf_binary;
}

char* book_binary_gaiji_wide(int index, int code, size_t* size) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }
  eb_set_font(book, EB_FONT_16);

  EB_Error_Code error_code = eb_wide_font_character_bitmap(book, code, buf_gaiji_wide_bitmap);
  if (error_code != EB_SUCCESS) {
    return NULL;
  }

  if (eb_bitmap_to_png(buf_gaiji_wide_bitmap, EB_WIDTH_WIDE_FONT_16,
    EB_HEIGHT_FONT_16, buf_binary, size) != EB_SUCCESS) {
    return NULL;
  }
  return buf_binary;
}

char* book_binary_mono(int index, int page, int offset, int width, int height, size_t* size) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  EB_Position position;
  position.page = page;
  position.offset = offset;
  EB_Error_Code error_code = eb_set_binary_mono_graphic(book, &position, width, height);
  if (error_code != EB_SUCCESS) {
    return NULL;
  }

  *size = 0;
  ssize_t readcnt = 0;
  while( 1 ) {
    error_code = eb_read_binary(book, sizeof(buf_binary) - *size, buf_binary + *size, &readcnt);
    if( error_code != EB_SUCCESS )
      return NULL;
    if( readcnt == 0 )
      break;
    *size += readcnt;
    if( *size == sizeof(buf_binary) ) { // buff full, consider as fail
      return NULL;
    }
  }

  return buf_binary;
}

char* book_binary_color(int index, int page, int offset, size_t* size) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  EB_Position position;
  position.page = page;
  position.offset = offset;
  EB_Error_Code error_code = eb_set_binary_color_graphic(book, &position);
  if (error_code != EB_SUCCESS) {
    return NULL;
  }

  *size = 0;
  ssize_t readcnt = 0;
  while( 1 ) {
    error_code = eb_read_binary(book, sizeof(buf_binary) - *size, buf_binary + *size, &readcnt);
    if( error_code != EB_SUCCESS )
      return NULL;
    if( readcnt == 0 )
      break;
    *size += readcnt;
    if( *size == sizeof(buf_binary) ) { // buff full, consider as fail
      return NULL;
    }
  }

  return buf_binary;
}

char* book_binary_wav(int index, int page, int offset, int endpage, int endoffset, size_t* size) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  EB_Position position, endposition;
  position.page = page;
  position.offset = offset;
  endposition.page = endpage;
  endposition.offset = endoffset;
  EB_Error_Code error_code = eb_set_binary_wave(book, &position, &endposition);
  if (error_code != EB_SUCCESS) {
    return NULL;
  }

  *size = 0;
  ssize_t readcnt = 0;
  while( 1 ) {
    error_code = eb_read_binary(book, sizeof(buf_binary) - *size, buf_binary + *size, &readcnt);
    if( error_code != EB_SUCCESS )
      return NULL;
    if( readcnt == 0 )
      break;
    *size += readcnt;
    if( *size == sizeof(buf_binary) ) { // buff full, consider as fail
      return NULL;
    }
  }

  return buf_binary;
}

JSON_Value* book_page(int index, int page) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  JSON_Value *root_value = json_value_init_array();
  JSON_Array *root_array = json_value_get_array(root_value);

  EB_Position position;
  position.page = page;
  position.offset = 0;

  EB_Error_Code error_code = eb_seek_text(book, &position);
  if (error_code != EB_SUCCESS) {
    goto page_end;
  }

  while (1) {
    error_code = eb_forward_text(book, NULL);
    if (error_code != EB_SUCCESS) {
      break;
    }
    error_code = eb_tell_text(book, &position);
    if (error_code != EB_SUCCESS || position.page > page + 1 || (position.page == page + 1 && position.offset != 0) ) {
      break;
    }

    error_code = eb_seek_text(book, &position);
    if (error_code != EB_SUCCESS) {
      break;
    }
    error_code = eb_read_heading(book, NULL, &hookset_header, NULL, MAXLEN_HEADING, heading, &heading_length);
    if (error_code != EB_SUCCESS) {
      break;
    }
    error_code = eb_seek_text(book, &position);
    error_code = eb_read_text(book, current_bookw->app, &hookset, NULL, MAXLEN_TEXT, text, &text_length);
    if (error_code != EB_SUCCESS) { 
      break;
    }

    json_array_append_string(root_array, heading);
    json_array_append_string(root_array, text);
    json_array_append_number(root_array, position.page);
    json_array_append_number(root_array, position.offset);
  }
page_end:
  return root_value;
}

// directly read a position
JSON_Value* book_get(int index, int page, int offset) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  JSON_Value *root_value = json_value_init_array();
  JSON_Array *root_array = json_value_get_array(root_value);

  EB_Position position;
  position.page = page;
  position.offset = offset;

  EB_Error_Code error_code = eb_seek_text(book, &position);
  if (error_code != EB_SUCCESS) {
    goto get_end;
  }

  error_code = eb_read_heading(book, NULL, &hookset_header, NULL, MAXLEN_HEADING, heading, &heading_length);
  if (error_code != EB_SUCCESS) {
    goto get_end;
  }
  // printf("heading: %s\n", heading);

  error_code = eb_seek_text(book, &position);
  if (error_code != EB_SUCCESS) {
    goto get_end;
  }

  error_code = eb_read_text(book, current_bookw->app, &hookset, NULL, MAXLEN_TEXT, text, &text_length);
  if (error_code != EB_SUCCESS) {
    goto get_end;
  }

  json_array_append_string(root_array, heading);
  json_array_append_string(root_array, text);
  json_array_append_number(root_array, position.page);
  json_array_append_number(root_array, position.offset);

get_end:
  return root_value;
}

JSON_Value* book_text(int index) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  EB_Position position;
  EB_Error_Code error_code = eb_text(&book, &position);
  if (error_code == EB_SUCCESS) {
    JSON_Value *root_value = json_value_init_array();
    JSON_Array *root_array = json_value_get_array(root_value);

    json_array_append_number(root_array, position.page);
    json_array_append_number(root_array, position.offset);
    return root_value;
  }
  //printf("error %d\n", error_code);
  return NULL; // no menu or error
}

JSON_Value* book_menu(int index) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  EB_Position position;
  EB_Error_Code error_code = eb_menu(&book, &position);
  if (error_code == EB_SUCCESS) {
    // printf("eb_menu result %d %d %d\n", error_code, position.page, position.offset);
    JSON_Value *root_value = json_value_init_array();
    JSON_Array *root_array = json_value_get_array(root_value);

    eb_seek_text(book, &position);
    eb_read_text(book, current_bookw->app, &hookset, NULL, MAXLEN_TEXT, text, &text_length);
    json_array_append_string(root_array, text);
    return root_value;
  }
  return NULL; // no menu or error
}

JSON_Value* book_copyright(int index) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  EB_Position position;
  EB_Error_Code error_code = eb_copyright(&book, &position);
  if (error_code == EB_SUCCESS) {
    JSON_Value *root_value = json_value_init_array();
    JSON_Array *root_array = json_value_get_array(root_value);

    eb_seek_text(book, &position);
    eb_read_text(book, current_bookw->app, &hookset, NULL, MAXLEN_TEXT, text, &text_length);
    json_array_append_string(root_array, text);
    return root_value;
  }
  return NULL; // no menu or error
}

// type:
// 0 prefix
// 1 suffix
// 2 exactly

JSON_Value* book_query(int index, int type, int max_hit, const char* s, const char* marker) {
  EB_Book* book = select_book(index);
  if( book == NULL ) {
    return NULL;
  }

  int i,j;
  EB_Error_Code error_code;

  switch(type) {
    case 1:
      error_code = eb_search_endword(book, convert_to_internal_encoding(book, s));
    break;
    case 2:
      error_code = eb_search_exactword(book, convert_to_internal_encoding(book, s));
    break;
    default:
      error_code = eb_search_word(book, convert_to_internal_encoding(book, s));
  }

  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "failed to search for the word, %s: %s\n", eb_error_message(error_code), s);
    return NULL;
  }

  if( marker != NULL && strcmp(marker, "0") != 0 ) {
    int page;
    int offset;
    int page_id;
    int entry_count;
    int entry_index;
    int entry_length;
    int entry_arrangement;
    int in_group_entry;
    if( sscanf(marker, "%d_%d_%d_%d_%d_%d_%d_%d", &page, &offset, &page_id, &entry_count,
      &entry_index, &entry_length, &entry_arrangement, &in_group_entry) != 8  ) {
      return NULL;
    }
    book->search_contexts->page = page;
    book->search_contexts->offset = offset;
    book->search_contexts->page_id = page_id;
    book->search_contexts->entry_count = entry_count;
    book->search_contexts->entry_index = entry_index;
    book->search_contexts->entry_length = entry_length;
    book->search_contexts->entry_arrangement = entry_arrangement;
    book->search_contexts->in_group_entry = in_group_entry;
  }

  if( max_hit < 0 || max_hit > MAX_HITS )
    max_hit = MAX_HITS;
  error_code = eb_hit_list(book, max_hit, hits, &hit_count);
  if (error_code != EB_SUCCESS) {
    fprintf(stderr, "failed to get hit entries, %s\n", eb_error_message(error_code));
    return NULL;
  }

  JSON_Value *root_value = json_value_init_array();
  JSON_Array *root_array = json_value_get_array(root_value);

  if( hit_count == 0 )
    return root_value;

  //insert sort hits indexes according to text position to help detecting duplication
  hits_index_sorted[0] = 0;
  for(i = 1; i < hit_count;i++) {
    j = i;
    while( j > 0 &&
      ( hits[hits_index_sorted[j-1]].text.page >  hits[i].text.page ||
        (hits[hits_index_sorted[j-1]].text.offset >  hits[i].text.offset && (hits[hits_index_sorted[j-1]].text.page ==  hits[i].text.page))
      )
    ) {
      hits_index_sorted[j] = hits_index_sorted[j-1];
      j--;
    }
    hits_index_sorted[j] = i;
  }
  // for( i = 0; i < hit_count; i++ )
  //   printf("%d: %d, %d\n", hits_index_sorted[i], hits[hits_index_sorted[i]].text.page, hits[hits_index_sorted[i]].text.offset);

  EB_Hit* last = NULL;
  for (i = 0; i < hit_count; i++) {
    j = hits_index_sorted[i];
    //printf("hit: heading: %d %d text: %d %d\n", hits[j].heading.page, hits[j].heading.offset, hits[j].text.page, hits[j].text.offset);
    
    if( last != NULL && memcmp(&(last->text), &(hits[j].text), sizeof(EB_Position)) == 0 ) {
      // printf("detech duplicate");
      continue;
    }

    error_code = eb_seek_text(book, &(hits[j].heading));
    if (error_code != EB_SUCCESS) {
      continue;
    }

    error_code = eb_read_heading(book, NULL, &hookset_header, NULL, MAXLEN_HEADING, heading, &heading_length);
    if (error_code != EB_SUCCESS) {
      continue;
    }
    // printf("heading: %s\n", heading);

    error_code = eb_seek_text(book, &(hits[j].text));
    if (error_code != EB_SUCCESS) {
      continue;
    }

    error_code = eb_read_text(book, current_bookw->app, &hookset, NULL, MAXLEN_TEXT, text, &text_length);
    if (error_code != EB_SUCCESS) {
      continue;
    }
    // printf("text: %s\n", text);

    last = &hits[j];
    json_array_append_string(root_array, heading);
    json_array_append_string(root_array, text);
    json_array_append_number(root_array, hits[j].text.page);
    json_array_append_number(root_array, hits[j].text.offset);
  }
  char nextPageMarker[1024] = {0};
  if( book->search_contexts->comparison_result >= 0) {
    sprintf(nextPageMarker, "%d_%d_%d_%d_%d_%d_%d_%d",
      book->search_contexts->page,
      book->search_contexts->offset,
      book->search_contexts->page_id,
      book->search_contexts->entry_count,
      book->search_contexts->entry_index,
      book->search_contexts->entry_length,
      book->search_contexts->entry_arrangement,
      book->search_contexts->in_group_entry
    );
  }
  json_array_append_string(root_array, nextPageMarker);

  return root_value;
}

JSON_Value* book_list() {
  JSON_Value *root_value = json_value_init_array();
  JSON_Array *root_array = json_value_get_array(root_value);

  book_node_t* current = books;
  while( current != NULL ) {
    json_array_append_string(root_array, current->title);
    current = current->next;
  }

  return root_value;
}
