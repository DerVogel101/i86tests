#include "hardware.h"
#include <stddef.h>

#define CLOCK_BYTES ((volatile uint8 __far *)0x9FFF0003UL)

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (count--) *d++ = *s++;
    return dest;
}

void log_text(const char *text)
{
    for (uint16 port = IO_TERMINAL; port <= IO_TERMINAL + 0x1Eu; port += 2) {
        uint8 low = 0;
        uint8 high = 0;

        if (*text != '\0') {
            low = (uint8)*text++;
        }

        if (*text != '\0') {
            high = (uint8)*text++;
        }

        const uint16 value = (uint16)low | ((uint16)high << 8);

        out16(port, value);
    }

    out8(IO_TERMINAL_CONTROL, TERMINAL_CONTROL_SEND);
}

static uint16 clock_word(uint16 offset)
{
    return (uint16)CLOCK_BYTES[offset] | ((uint16)CLOCK_BYTES[offset + 1] << 8);
}

Realtime get_realtime(void)
{
    Realtime time;
    time.milliseconds = clock_word(0);
    time.hour = CLOCK_BYTES[6];
    time.minute = CLOCK_BYTES[7];
    time.second = CLOCK_BYTES[8];
    time.day_of_month = CLOCK_BYTES[9];
    time.month = CLOCK_BYTES[10];
    time.year = clock_word(11);
    return time;
}

TickTime get_tick_time(void)
{
    TickTime time;
    time.minecraft_day = clock_word(2);
    time.uptime_ticks = clock_word(4);
    return time;
}
