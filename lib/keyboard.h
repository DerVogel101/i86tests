#pragma once
#include "irq_handler.h"

typedef enum {
    KEY_BACKSPACE = 0x08, KEY_TAB = 0x09, KEY_ENTER = 0x0a,
    KEY_KP_0 = 0x81, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_DECIMAL, KEY_KP_DIVIDE, KEY_KP_MULTIPLY, KEY_KP_SUBTRACT,
    KEY_KP_ADD, KEY_KP_ENTER, KEY_KP_EQUAL,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_INSERT, KEY_DELETE, KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN
} KeyboardKey;

typedef void (*KeyboardCallback)(uint8 slot, uint8 byte);
typedef struct {
    IrqBinding irq;
    KeyboardCallback callback;
    uint8 slot;
} KeyboardBinding;

/* Returns one byte and requests its ACK. Pending ACKs are not read twice.
 * The hardware consumes the ACK at the next game tick. Preserves IF. */
bool keyboard_try_read(uint8 slot, uint8 *byte);

/* Foreground only. Caller-owned, zero-initialized storage must remain alive
 * until unregistered. Call irq_init before STI; do not edit the binding.
 * Callbacks run in IRQ context: no waiting, STI, registry mutation or IRQ ACK. */
bool keyboard_register(KeyboardBinding *binding, uint8 slot, KeyboardCallback callback);
bool keyboard_unregister(KeyboardBinding *binding);
