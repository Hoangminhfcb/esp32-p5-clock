#include "gfx/utf8.h"
#include <string.h>

uint32_t utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t cp;
    int extra;
    if      (s[0] < 0x80)           { cp = s[0];        extra = 0; }
    else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; extra = 1; }
    else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; extra = 2; }
    else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; extra = 3; }
    else { *ps += 1; return '?'; }

    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) { *ps += i; return '?'; }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *ps += 1 + extra;
    return cp;
}

int utf8_truncate(char *s, int max_bytes)
{
    int len = (int)strlen(s);
    if (len <= max_bytes) return len;
    int k = max_bytes;
    while (k > 0 && ((unsigned char)s[k] & 0xC0) == 0x80) k--;   // lùi về đầu ký tự
    s[k] = '\0';
    return k;
}
