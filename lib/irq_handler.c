#include "hardware.h"
#include "irq_handler.h"
#include <stddef.h>

extern void irq_entry(void);

static IrqBinding *type_handlers;
static IrqBinding *id_handlers;
static IrqCallback routing_handler;
static IrqCallback unhandled_handler;

_Static_assert(sizeof(IrqCallback) == 2, "IRQ callbacks require the small model");
_Static_assert(sizeof(IrqBinding) == 6, "Unexpected IRQ registration size");

static bool lock_irq(void)
{
    bool enabled = is_intr_enabled();
    disable_intr();
    return enabled;
}

static void restore_intr(bool enabled)
{
    if (enabled) enable_intr();
}

void irq_init(void)
{
    bool enabled = lock_irq();
    volatile uint16 *vector = (volatile uint16 *)0x0080;
    vector[0] = (uint16)irq_entry;
    vector[1] = 0xC000;
    restore_intr(enabled);
}

IrqCallback irq_set_handler(IrqCallback callback)
{
    bool enabled = lock_irq();
    IrqCallback previous = routing_handler ? routing_handler : irq_default_handler;
    routing_handler = callback;
    restore_intr(enabled);
    return previous;
}

void irq_set_unhandled_handler(IrqCallback callback)
{
    bool enabled = lock_irq();
    unhandled_handler = callback;
    restore_intr(enabled);
}

/* ponytail: linear lookup; use a custom type handler with a dense table if
 * registered-route counts make IRQ latency too high. */
static IrqCallback find_callback(IrqBinding *head, uint16 key)
{
    for (; head; head = head->next)
        if (head->key == key) return head->callback;
    return NULL;
}

static bool contains_binding(IrqBinding *head, IrqBinding *binding)
{
    for (; head; head = head->next)
        if (head == binding) return true;
    return false;
}

static bool register_binding(IrqBinding **head, IrqBinding *binding,
                             uint16 key, IrqCallback callback)
{
    if (!binding || !callback) return false;
    bool enabled = lock_irq();
    bool registered = false;
    if (!contains_binding(type_handlers, binding) &&
        !contains_binding(id_handlers, binding) && !find_callback(*head, key)) {
        binding->key = key;
        binding->callback = callback;
        binding->next = *head;
        *head = binding;
        registered = true;
    }
    restore_intr(enabled);
    return registered;
}

bool irq_register_type(IrqBinding *binding,
                       IrqDeviceType type, IrqCallback callback)
{
    if ((uint16)type > 255) return false;
    return register_binding(&type_handlers, binding, (uint16)type, callback);
}

bool irq_register_id(IrqBinding *binding,
                     IrqDeviceType type, uint8 index, IrqCallback callback)
{
    if ((uint16)type > 255) return false;
    return register_binding(&id_handlers, binding,
                            ((uint16)type << 8) | index, callback);
}

bool irq_unregister(IrqBinding *binding)
{
    if (!binding) return false;
    bool enabled = lock_irq();
    IrqBinding **link = &type_handlers;
    while (*link && *link != binding) link = &(*link)->next;
    if (!*link) {
        link = &id_handlers;
        while (*link && *link != binding) link = &(*link)->next;
    }
    bool removed = *link != NULL;
    if (removed) {
        *link = binding->next;
        binding->key = 0;
        binding->callback = NULL;
        binding->next = NULL;
    }
    restore_intr(enabled);
    return removed;
}

void irq_default_handler(IrqDeviceType type, uint8 index)
{
    IrqCallback callback = find_callback(type_handlers, (uint16)type);
    if (!callback)
        callback = find_callback(id_handlers, ((uint16)type << 8) | index);
    if (!callback) callback = unhandled_handler;
    if (callback) callback(type, index);
}

/* Called only by the assembly entry; ACK belongs here, never to a router. */
void irq_service(void)
{
    if (!is_irq_pending()) return;
    uint16 id = in16(IO_IRQ);
    IrqCallback callback = routing_handler ? routing_handler : irq_default_handler;
    callback((IrqDeviceType)(id >> 8), (uint8)id);
    ack_irq();
}
