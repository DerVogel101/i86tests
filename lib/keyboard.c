#include "keyboard.h"
#include <stddef.h>

static KeyboardBinding *bindings[KEYBOARD_SLOTS];

bool keyboard_try_read(uint8 slot, uint8 *byte)
{
    if (slot >= KEYBOARD_SLOTS || !byte) return false;
    bool enabled = is_intr_enabled();
    disable_intr();
    bool ready = (in8(IO_KEYBOARD_CONTROL(slot)) & (KEYBOARD_READY | KEYBOARD_ACK_PENDING)) == KEYBOARD_READY;
    if (ready) {
        *byte = in8(IO_KEYBOARD_DATA(slot));
        out8(IO_KEYBOARD_CONTROL(slot), KEYBOARD_ACK);
    }
    if (enabled) enable_intr();
    return ready;
}

static void keyboard_irq(IrqDeviceType type, uint8 slot)
{
    (void)type;
    uint8 byte;
    if (slot < KEYBOARD_SLOTS && bindings[slot] && keyboard_try_read(slot, &byte))
        bindings[slot]->callback(slot, byte);
}

bool keyboard_register(KeyboardBinding *binding, uint8 slot, KeyboardCallback callback)
{
    if (!binding || !callback || slot >= KEYBOARD_SLOTS) return false;
    bool enabled = is_intr_enabled();
    disable_intr();
    bool available = !bindings[slot];
    for (uint8 i = 0; i < KEYBOARD_SLOTS; ++i)
        if (bindings[i] == binding) available = false;
    bool registered = available && irq_register_id(&binding->irq, IRQ_DEVICE_KEYBOARD, slot, keyboard_irq);
    if (registered) {
        binding->slot = slot;
        binding->callback = callback;
        bindings[slot] = binding;
    }
    if (enabled) enable_intr();
    return registered;
}

bool keyboard_unregister(KeyboardBinding *binding)
{
    if (!binding) return false;
    bool enabled = is_intr_enabled();
    disable_intr();
    bool removed = false;
    for (uint8 slot = 0; slot < KEYBOARD_SLOTS; ++slot) {
        if (bindings[slot] == binding && irq_unregister(&binding->irq)) {
            bindings[slot] = NULL;
            binding->callback = NULL;
            removed = true;
            break;
        }
    }
    if (enabled) enable_intr();
    return removed;
}
