#include "hardware.h"
#include "sleep.h"

void sleep_realtime(uint16 wait, uint8 resolution, bool use_tick) {
    if (wait == 0)
        return;

    uint16 timer = use_tick ? SLEEP_TIMER_TICK_ID : SLEEP_TIMER_REAL_ID;
    out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
    out16(IO_TIMER_WAIT(timer), wait);
    out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_ENABLE | (resolution & 0x30));

    while (in16(IO_TIMER_COUNTDOWN(timer)) != 0)
        i86_yield_batch();
}
