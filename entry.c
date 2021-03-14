#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "base64.h"

int main(int argc, char *argv[])
{
    FILE *fp;
    void *buf;
    char *b64;
    ssize_t len;
    size_t b64_len;
    struct stat st;

    if (argc < 2)
        return -1;

    if (stat(argv[1], &st) != 0) {
        fprintf(stderr, "get file stat failed!\n");
        return -1;
    }

    b64_len = base64_encode_buffer_size(st.st_size, true);
    buf = malloc(st.st_size + b64_len);
    if (!buf) {
        fprintf(stderr, "fail to malloc memory!\n");
        return -1;
    }

    b64 = buf + st.st_size;
    fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "fail to open %s!\n", argv[1]);
        free(buf);
        return -1;
    }

    len = fread(buf, st.st_size, 1, fp);
    fprintf(stderr, "read block %I64d\n", len);
    fclose(fp);
    b64_len = base64_encode(b64, buf, st.st_size, Base64InsertLineBreaks);
    printf("%s\n", b64);
    free(buf);    

    return 0;
}
