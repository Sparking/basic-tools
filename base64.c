#include "base64.h"

#ifndef BASE64_ENCODE_DISABLE
static const char alphabet[2][64] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
};

size_t base64_encode_buffer_size(size_t len, bool break_line)
{
    len = ((len + 5) / 3) << 2;
    if (break_line)
        len += (len + 63) >> 6;

    return len;
}

size_t base64_encode(char *out, const void *in, size_t n, base64_options_t options)
{
    size_t i;
    size_t o;
    char *mark;
    const unsigned char *p;
    const char *table = alphabet[options & Base64UseUrlAlphabet];

    mark = out;
    p = (unsigned char *) in;
    for (o = 0, i = n; i >= 3;) {
        /* write 4 chars x 6 bits = 24 bits */
        *out++ = table[p[0] >> 2];
        *out++ = table[((p[0] & 0x03) << 4) + (p[1] >> 4)];
        *out++ = table[((p[1] & 0x0F) << 2) + (p[2] >> 6)];
        *out++ = table[p[2] & 0x3f];
        i -= 3;
        p += 3;
        o += 4;
        if ((options & Base64InsertLineBreaks) && (o & 63) == 0 && i)
            *out++ = '\n';
    }

    if (i == 1) {
        *out++ = table[p[0] >> 2];
        *out++ = table[(p[0] & 0x03) << 4];
        *out++ = '=';
        *out++ = '=';
    } else if (i == 2) {
        *out++ = table[p[0] >> 2];
        *out++ = table[((p[0] & 0x03) << 4) + (p[1] >> 4)];
        *out++ = table[(p[1] & 0x0F) << 2];
        *out++ = '=';
    }
    *out = '\0';

    return out - mark;
}
#endif

#ifndef BASE64_DECODE_DISABLE
static const int8_t reverse_alpabet[2][256] = {{
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
}, {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
}};

size_t base64_decode_buffer_size(size_t len)
{
    return len;
}

size_t base64_decode(void *out, const char *in, size_t n, base64_options_t options)
{
    size_t i;
    size_t o;
    uint8_t val;
    uint8_t a, b, c, d;
    uint8_t A, B, C, D;
    unsigned char *p;
    const int8_t *map = reverse_alpabet[options & Base64UseUrlAlphabet];

    p = (unsigned char *) out;
    for (o = 0, i = 0; i < n;) {
        a = in[0];
        if (a == '\0')
            break;

        b = in[1];
        A = map[a];
        B = map[b];
        if (A < 0 || B < 0)
            return 0;

        p[o++] = (A << 2) + (B >> 4);
        val = B << 4;
        c = in[2];
        d = in[3];
        if (c == '=' || c == '\0') {
            if (c == '=' && d != '=')
                return 0;
            break;
        }

        C = map[c];
        if (C < 0)
            return 0;

        p[o++] = val + (C >> 2);
        if (d == '=' || d == '\0')
            break;

        D = map[d];
        if (D < 0)
            return 0;

        p[o++] = (C << 6) + D;
        in += 4;
        i += 4;
        if (*in == '\n') {
            in++;
            i++;
        }
    }

    return o;

}
#endif
