#ifndef _BASE64_H_
#define _BASE64_H_

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>

typedef enum {
    Base64UseOriginalAlphabet   = 0x0,
    Base64UseUrlAlphabet        = 0x1,
    Base64InsertLineBreaks      = 0x2,
} base64_options_t;

extern size_t base64_encode_buffer_size(size_t len, bool break_line);
extern size_t base64_encode(char *out, const void *in, size_t n, base64_options_t options);
extern size_t base64_decode_buffer_size(size_t len);
extern size_t base64_decode(void *out, const char *in, size_t n, base64_options_t options);

#endif /* _BASE64_H_ */
