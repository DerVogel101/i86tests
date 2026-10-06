#pragma once
#include "hardware.h"

typedef void (*IrqCallback)(IrqDeviceType type, uint8 index);

/* Caller-owned, writable storage. Keep alive until unregistered and do not
 * modify linked fields. Static, zero-initialized records are recommended. */
typedef struct IrqBinding {
    uint16 key;
    IrqCallback callback;
    struct IrqBinding *next;
} IrqBinding;

/* Installs vector 0x20; preserves IF, routing and registrations. Call before STI.
 * The library requires the current small-model ABI (CS=C000, DS=SS=0). */
void irq_init(void);

/* Replaces event routing only; the library still reads and ACKs the queue.
 * NULL restores default routing. Returns the previous effective callback. */
IrqCallback irq_set_handler(IrqCallback callback);

/* Routing only: type override, exact ID, then unhandled callback. A replacement
 * router may delegate here. A type override must not delegate here for its own
 * type, since that would call itself again. */
void irq_default_handler(IrqDeviceType type, uint8 index);

/* Foreground-only mutations preserve IF. Types 0..255 and indices 0..255 are
 * supported. NULL inputs, invalid types, duplicate keys and linked records fail
 * without changing state. Removing a type override restores its ID routes. */
bool irq_register_type(IrqBinding *binding,
                       IrqDeviceType type, IrqCallback callback);
bool irq_register_id(IrqBinding *binding,
                     IrqDeviceType type, uint8 index, IrqCallback callback);
bool irq_unregister(IrqBinding *binding);
void irq_set_unhandled_handler(IrqCallback callback);

/* Callbacks run with maskable interrupts disabled on the application's stack.
 * Return promptly: no ACK, STI, HLT, waiting or registry mutation. A NULL
 * unhandled callback ignores unmatched events; the library still ACKs them. */
