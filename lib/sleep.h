#pragma once
#include "hardware.h"

#define SLEEP_TIMER_REAL_ID 2
#define SLEEP_TIMER_TICK_ID 5

/* Foreground only: exclusively owns timer 2 (real) or 5 (tick) during the wait.
 * Resolution is TIMER_CONTROL_RES_1..4; wait counts units of that resolution.
 * Batch-yields until expiry, preserving IF; enabled IRQs can run normally.
 * Call irq_init() before enabling IRQs. A zero wait does nothing.
 * Do not use from IRQ callbacks or reconfigure the selected timer while waiting.
 */
void sleep_realtime(uint16 wait, uint8 resolution, bool use_tick);
