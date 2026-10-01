#pragma once
#include "hardware.h"

#define SLEEP_TIMER_REAL_ID 2
#define SLEEP_TIMER_TICK_ID 5

void sleep_realtime(uint16 wait, uint8 resolution, bool use_tick);