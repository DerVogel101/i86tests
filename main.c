#include "lib/hardware.h"
#include "lib/strings.h"

static volatile unsigned char initialized = 0x42;
static volatile char t = 'T';

static volatile unsigned long long int schlong;

void rom_main(void) {
    log_text("Beginning\n");
    uint16 now = get_realtime().milliseconds;
    *(volatile unsigned char *)0x0500 = initialized;
    *(volatile unsigned char *)0x0501 = t;
    uint16 now2 = get_realtime().milliseconds;
    log_text("Done\n");
    uint16 result = now2 - now;
    char buffer[sizeof("Time taken: ") + 6];
    str_copy(buffer, "Time taken: ");
    uint16_to_str(result, buffer + sizeof("Time taken: ") - 1);
    str_concat(buffer, "\n");
    log_text(buffer);
    log_text("123456789012345678901\n");
}