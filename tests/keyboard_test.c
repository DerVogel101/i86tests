#include "../lib/keyboard.h"
#include <stddef.h>

static volatile uint16 *const state = (volatile uint16 *)0x0500;
static KeyboardBinding first, last, duplicate;

static void received(uint8 slot, uint8 byte)
{
    if (is_intr_enabled()) state[1] |= 1;
    if (slot == 0) { if (state[3] < 2) state[12 + state[3]] = byte; ++state[3]; }
    else if (slot == 7) { ++state[4]; state[6] = byte; }
    else state[1] |= 2;
}

void rom_main(void)
{
    disable_intr();
    for (uint8 i = 0; i < 16; ++i) state[i] = 0;
    uint8 byte = 0;
    if (keyboard_try_read(8, &byte) || keyboard_try_read(0, NULL) || keyboard_unregister(&first)) state[1] |= 4;
    irq_init();
    if (!keyboard_register(&first, 0, received) || is_intr_enabled()) state[1] |= 8;
    if (keyboard_register(&duplicate, 0, received) || keyboard_register(&first, 7, received)
            || keyboard_register(&duplicate, 8, received) || keyboard_register(NULL, 1, received)) state[1] |= 16;
    state[0] = 1;
    while (!keyboard_try_read(0, &byte)) {}
    state[2] = byte;
    state[8] = in8(IO_KEYBOARD_CONTROL(0));
    if (keyboard_try_read(0, &byte) || is_intr_enabled()) state[1] |= 32;
    while (!state[9]) {}
    enable_intr();
    if (!keyboard_register(&last, 7, received) || !is_intr_enabled()
            || !keyboard_unregister(&last) || !is_intr_enabled()
            || !keyboard_register(&last, 7, received) || !is_intr_enabled()) state[1] |= 64;
    state[0] = 2;
    while (state[3] < 2 || state[4] < 1) {}
    disable_intr();
    if (!keyboard_unregister(&first) || !keyboard_unregister(&last) || is_intr_enabled()) state[1] |= 128;
    state[0] = 3;
    for (;;) hlt();
}
