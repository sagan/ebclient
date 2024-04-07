
#ifndef _BOOK_H
#define _BOOK_H

#include <stddef.h>
#include <mxml.h>
#include <ebu/eb.h>
#include <ebu/error.h>
#include <ebu/text.h>
#include <ebu/font.h>

#include "parson.h"

typedef struct {
  EB_Book book;
  EB_Appendix* app;
  mxml_node_t *gaijimap_tree;
  EB_Subbook_Code subbook_list[EB_MAX_SUBBOOKS]; // EB_MAX_SUBBOOKS: 50
  size_t subbook_count;
} book_t;

extern EB_Hookset hookset;
extern book_t* current_bookw;

void books_init(const char* rootpath);
book_t* book_load(const char* path);
void book_unload(book_t* book);
char* convert_to_internal_encoding(EB_Book* book, char* s);
JSON_Value* book_query(int index, int type, int max_hit, const char* s, const char* marker);
JSON_Value* book_get(int index, int page, int offset);
JSON_Value* book_menu(int index);
JSON_Value* book_text(int index);
JSON_Value* book_page(int index, int page);
JSON_Value* book_copyright(int index);
char* book_binary_mono(int index, int page, int offset, int width, int height, size_t* size);
char* book_binary_color(int index, int page, int offset, size_t* size);
char* book_binary_wav(int index, int page, int offset, int endpage, int endoffset, size_t* size);
char* book_binary_gaiji_wide(int index, int code, size_t* size); // gaiji bitmap to png
char* book_binary_gaiji_narrow(int index, int code, size_t* size);
JSON_Value* book_list();

#endif
