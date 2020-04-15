#ifndef _UTF8_H
#define _UTF8_H

#include <stdint.h>

extern int32_t from_utf8(const unsigned char **inptr, int cnt, int *err);

#define UTF8_BAD (-1)
#define UTF8_SHORT (-2)

#endif /* _UTF8_H */