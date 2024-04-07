
#include "conv.h"
#include <iconv.h>

char out[MAX_STR_LEN] = {0};
iconv_t iso8859_iconver;
iconv_t euc_iconver;
iconv_t utf16be_iconver;
iconv_t utf8_to_euc_iconver;
size_t inbytesleft;
size_t outbytesleft;

void init_conv() {
	iso8859_iconver = iconv_open("utf8", "iso-8859-1");
	euc_iconver = iconv_open("utf8", "euc-jp");
	utf16be_iconver = iconv_open("utf8", "utf16be");
	utf8_to_euc_iconver = iconv_open("euc-jp", "utf8");
}


char* conv(iconv_t cd, char* in, size_t len) {
    char *inptr = in, *outptr = out;
    inbytesleft = len;
    outbytesleft = MAX_STR_LEN;
    iconv(cd, &inptr, &inbytesleft, &outptr, &outbytesleft);
    *outptr = 0;
    return out;
}

char* conv_iso8859_str(char* in, size_t len) {
	return conv(iso8859_iconver, in, len);
}

char* conv_euc_str(char* in, size_t len) {
	return conv(euc_iconver, in, len);
}

char* conv_utf16be_str(char* in, size_t len) {
	return conv(utf16be_iconver, in, len);
}

char* conv_utf8_to_euc_str(char* in, size_t len) {
	return conv(utf8_to_euc_iconver, in, len);
}