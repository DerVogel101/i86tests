#include "strings.h"
#include "hardware.h"

void uint16_to_str(uint16 value, char *buffer)
{
    char temp[6]; // max: "65535"
    int i = 0;
    int j = 0;

    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        buffer[j++] = temp[--i];
    }

    buffer[j] = '\0';
}
void str_copy(char *dst, const char *src)
{
    while ((*dst++ = *src++) != '\0') {}
}
void str_concat(char *dst, const char *src)
{
    while (*dst) ++dst;
    str_copy(dst, src);
}
