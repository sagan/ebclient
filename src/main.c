
#include <stdio.h>
#include <stdlib.h>

#include "book.h"
#include "conv.h"
#include "functions.h"
#include "parson.h"

void dumpHex(const void* data, size_t size) {
  char ascii[17];
  size_t i, j;
  ascii[16] = '\0';
  for (i = 0; i < size; ++i) {
    printf("%02X ", ((unsigned char*)data)[i]);
    if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
      ascii[i % 16] = ((unsigned char*)data)[i];
    } else {
      ascii[i % 16] = '.';
    }
    if ((i+1) % 8 == 0 || i+1 == size) {
      printf(" ");
      if ((i+1) % 16 == 0) {
        printf("|  %s \n", ascii);
      } else if (i+1 == size) {
        ascii[(i+1) % 16] = '\0';
        if ((i+1) % 16 <= 8) {
          printf(" ");
        }
        for (j = (i+1) % 16; j < 16; ++j) {
          printf("   ");
        }
        printf("|  %s \n", ascii);
      }
    }
  }
}

int output_and_free_json(JSON_Value* value) {
  if( value != NULL ) {
    char* s = json_serialize_to_string(value);
    printf("%s\n", s);
    fflush(stdout);
    json_value_free(value);
    json_free_serialized_string(s);
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s books-path\n", argv[0]);
    exit(1);
  }

  init_conv();
  books_init(argv[1]);
  output_and_free_json(book_list());

  char* line = NULL;
  size_t n = 0;
  char word[513] = {0};
  char marker[1024] = {0};
  int index;
  int code; // gaiji code
  int type = 0;
  int max_hit = 10;
  int page = 0;
  int offset = 0;
  int endpage = 0;
  int endoffset = 0;
  char* binary_buf;
  size_t binary_size;
  int mono_width;
  int mono_height;

  while( 1 ) {
    getline(&line, &n, stdin);
    if( *line == 'a' ) {
      if( sscanf(line, "a %d %d %d", &index, &page, &offset) != 3 || !output_and_free_json(book_get(index, page, offset)) ) {
        printf("[]\n");
        fflush(stdout);
      }
    } else if( *line == 'b' ) { // read binary mono graph bmp
      if( sscanf(line, "b %d %d %d %d %d", &index, &page, &offset, &mono_width, &mono_height) != 5 ) {
        fwrite("\x00\x01\x00\x00\x00\x00", 1, 6, stdout);
        fflush(stdout);
        continue;
      }
      binary_buf = book_binary_mono(index, page, offset, mono_width, mono_height, &binary_size);
      if( binary_buf == NULL ) {
        fwrite("\x00\x02\x00\x00\x00\x00", 1, 6, stdout);
      } else {
        // dumpHex(binary_buf,256);
        fwrite("\x00\x00", 1, 2, stdout);
        fwrite(&binary_size, 4, 1, stdout); // only support little endian machine
        fwrite(binary_buf, 1, binary_size, stdout);
      }
      fflush(stdout);
    } else if( *line == 'c' ) { // read binary color graph
      if( sscanf(line, "c %d %d %d", &index, &page, &offset) != 3 ) {
        fwrite("\x00\x01\x00\x00\x00\x00", 1, 6, stdout);
        fflush(stdout);
        continue;
      }
      binary_buf = book_binary_color(index, page, offset, &binary_size);
      if( binary_buf == NULL ) {
        fwrite("\x00\x02\x00\x00\x00\x00", 1, 6, stdout);
      } else {
        // dumpHex(binary_buf,256);
        fwrite("\x00\x00", 1, 2, stdout);
        fwrite(&binary_size, 4, 1, stdout);
        fwrite(binary_buf, 1, binary_size, stdout);
      }
      fflush(stdout);
    } else if( *line == 'd' ) { // read binary wav
      if( sscanf(line, "d %d %d %d %d %d", &index, &page, &offset, &endpage, &endoffset) != 5 ) {
        fwrite("\x00\x01\x00\x00\x00\x00", 1, 6, stdout);
        fflush(stdout);
        continue;
      }
      binary_buf = book_binary_wav(index, page, offset, endpage, endoffset, &binary_size);
      if( binary_buf == NULL ) {
        fwrite("\x00\x02\x00\x00\x00\x00", 1, 6, stdout);
      } else {
        // dumpHex(binary_buf,256);
        fwrite("\x00\x00", 1, 2, stdout);
        fwrite(&binary_size, 4, 1, stdout);
        fwrite(binary_buf, 1, binary_size, stdout);
      }
      fflush(stdout);
    } else if( *line == 'e' ) { // copyright
      if( sscanf(line, "e %d", &index) != 1 || !output_and_free_json(book_copyright(index)) ) {
        printf("[]\n");
        fflush(stdout);
      }
    } else if( *line == 'f' ) { // menu
      if( sscanf(line, "f %d", &index) != 1 || !output_and_free_json(book_menu(index)) ) {
        printf("[]\n");
        fflush(stdout);
      }
    } else if( *line == 'g' ) { // gaiji png
      if( sscanf(line, "g %d %d %04X", &index, &type, &code) != 3 ) {
        fwrite("\x00\x01\x00\x00\x00\x00", 1, 6, stdout);
        fflush(stdout);
        continue;
      }
      if( type ) { // wide
        binary_buf = book_binary_gaiji_wide(index, code, &binary_size);
      } else { // type == 0 narrow
        binary_buf = book_binary_gaiji_narrow(index, code, &binary_size);
      }
      if( binary_buf == NULL ) {
        fwrite("\x00\x02\x00\x00\x00\x00", 1, 6, stdout);
      } else {
        fwrite("\x00\x00", 1, 2, stdout);
        fwrite(&binary_size, 4, 1, stdout);
        fwrite(binary_buf, 1, binary_size, stdout);
      }
      fflush(stdout);
    } else if( *line == 'h' ) { //
      if( sscanf(line, "h %d", &index) != 1 || !output_and_free_json(book_text(index)) ) {
        printf("[]\n");
        fflush(stdout);
      }
    } else if( *line == 'i' ) { //
      if( sscanf(line, "i %d %d", &index, &page) != 2 || !output_and_free_json(book_page(index, page)) ) {
        printf("[]\n");
        fflush(stdout);
      }
    } else {
      if( sscanf(line, "%d %d %d %[^\t\r\n,],%[^\t\r\n]", &index, &type, &max_hit, &marker, word) != 5 || !output_and_free_json(book_query(index, type, max_hit, word, marker)) ) {
        printf("[]\n");
        fflush(stdout);
      }
    }
    free(line);
    line = NULL;
    n = 0;
  }
  
  exit(0);
}
