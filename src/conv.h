
#ifndef _CONV_H
#define _CONV_H

#include <stddef.h>

#define MAX_STR_LEN 4096

void init_conv();
char* conv_iso8859_str(char* in, size_t len);
char* conv_euc_str(char* in, size_t len);
char* conv_utf16be_str(char* in, size_t len);
char* conv_utf8_to_euc_str(char* in, size_t len); // utf8 -> euc for internal usage


#endif