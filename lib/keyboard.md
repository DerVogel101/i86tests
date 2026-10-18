# Keyboard library

Include `keyboard.h` and link `keyboard.c`, `irq_handler.c`, the hardware library and the existing reset entry. One optional Keyboard Controller enables slots 0–7. Each slot has an eight-byte FIFO.

`keyboard_try_read(slot, &byte)` returns immediately. It returns false if the slot is invalid, the pointer is null, no byte is ready, or its ACK is pending. A successful read requests the device ACK; it does not ACK the global IRQ queue. All helpers preserve IF.

For interrupts, call `irq_init()` before STI and register caller-owned, zero-initialized `KeyboardBinding` storage. Keep it alive and unchanged until `keyboard_unregister`. Registration and unregistration are foreground-only. Duplicate slots, reused bindings and invalid arguments fail without changing registrations. The manager's existing type-router precedence still applies.

```c
#include "keyboard.h"

static KeyboardBinding keyboard;
static volatile uint16 lines;

static void typed(uint8 slot, uint8 byte)
{
    (void)slot;
    if (byte == KEY_ENTER) ++lines;
}

void rom_main(void)
{
    irq_init();
    if (!keyboard_register(&keyboard, 0, typed))
        for (;;) hlt();
    enable_intr();
    for (;;) i86_yield_tick();
}
```

Callbacks run with maskable interrupts disabled. The helper reads and requests ACK for one byte before calling the application; the IRQ manager subsequently ACKs the global queue. Do not wait, enable interrupts, mutate registrations or ACK the IRQ queue in the callback. Applications that need buffered processing must retain bytes in their own application buffer.

For polling, repeatedly call `keyboard_try_read` in foreground code and yield between attempts. Polling and callbacks compete for the same hardware byte, so normally choose one consumer per slot.

`IO_KEYBOARD_DATA(n)` is `0x8170 + 2*n`; `IO_KEYBOARD_CONTROL(n)` is the following byte. Read control bit 1 is READY and bit 0 is ACK_PENDING. Write bit 0 requests ACK. Confirmation occurs at the next instruction boundary; removal and the next byte/IRQ wait until the next game tick. IRQ IDs are `0x0400–0x0407` (`IRQ_DEVICE_KEYBOARD`).

Text bytes use Windows-1258. Backspace, Tab and Enter are `0x08`, `0x09`, `0x0A`. Constants `KEY_KP_0` through `KEY_PAGE_DOWN` reserve `0x81–0x9B` for numpad and navigation keys; euro remains `0x80`. Num Lock does not alter these codes. Main-section operators remain text.

Build `keyboard_test` to produce the focused test ROM. Set `KEYBOARD_TEST_ROM` to its `.bin` path when running the emulator Java tests. The test checks polling, delayed ACK, exact slot IRQ routes, duplicate registration, special bytes and IF preservation. Existing IRQ integration checks use `IRQ_HANDLER_TEST_ROM`.
