#include "../lib/irq_handler.h"
#include "../lib/sleep.h"
#include <stddef.h>

/* Sleep commands 16..19 take wait/resolution at 051A/051C. Even commands
 * use real time, odd commands ticks; 18/19 also check IF stays disabled. */

/* Mailbox words at 0500: ready, error line, command, last ID, last route,
 * callback count, callback flags, completed commands, binding size, callback
 * DS, callback ES, pending inside callback, first ID. The last 32 event IDs
 * are recorded at 0540. Commands are driven by IrqHandlerRomTest. */
#define MB ((volatile uint16 *)0x0500)
#define LOG ((volatile uint16 *)0x0540)
#define CHECK(condition) do { if (!(condition)) { \
    MB[1] = __LINE__; disable_intr(); for (;;) hlt(); \
} } while (0)
#define CHECK_IF(condition) do { CHECK(condition); \
    CHECK(is_intr_enabled() == enabled); } while (0)

static IrqBinding zero, sign_zero, sign_seven, timer_zero, edge, sign_edge;
static IrqBinding sign_type, edge_type, scratch_a, scratch_b, scratch_c;
static IrqCallback saved_router;

static void record(IrqDeviceType type, uint8 index, uint16 route)
{
    uint16 flags, ds, es;
    __asm__ volatile ("pushf\n\tpop %0" : "=r"(flags));
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile ("mov %%es, %0" : "=r"(es));
    uint16 id = ((uint16)type << 8) | index;
    MB[3] = id;
    MB[4] = route;
    MB[6] = flags;
    MB[9] = ds;
    MB[10] = es;
    MB[11] = is_irq_pending();
    if (MB[5] == 0) MB[12] = id;
    LOG[MB[5] & 31] = id;
    MB[5]++;
}

static void on_zero(IrqDeviceType type, uint8 index) { record(type, index, 1); }
static void on_sign_zero(IrqDeviceType type, uint8 index) { record(type, index, 2); }
static void on_sign_seven(IrqDeviceType type, uint8 index) { record(type, index, 3); }
static void on_type(IrqDeviceType type, uint8 index) { record(type, index, 4); }
static void on_timer(IrqDeviceType type, uint8 index) { record(type, index, 5); }
static void on_edge(IrqDeviceType type, uint8 index) { record(type, index, 6); }
static void on_unhandled(IrqDeviceType type, uint8 index) { record(type, index, 8); }
static void router(IrqDeviceType type, uint8 index)
{
    if (type == IRQ_DEVICE_REALTIMER) irq_default_handler(type, index);
    else record(type, index, 7);
}

static void registry_checks(bool enabled)
{
    if (enabled) enable_intr();
    else disable_intr();
    irq_init();
    CHECK(is_intr_enabled() == enabled);
    scratch_b.key = 0x1234;
    scratch_b.callback = on_edge;
    scratch_b.next = &scratch_c;

    CHECK_IF(!irq_register_id(NULL, IRQ_DEVICE_SIGN, 5, on_zero));
    CHECK_IF(!irq_register_type(NULL, IRQ_DEVICE_SIGN, on_zero));
    CHECK_IF(!irq_register_id(&scratch_b, IRQ_DEVICE_SIGN, 5, NULL));
    CHECK_IF(!irq_register_type(&scratch_b, IRQ_DEVICE_SIGN, NULL));
    CHECK_IF(!irq_register_type(&scratch_b, (IrqDeviceType)256, on_zero));
    CHECK_IF(!irq_register_type(&scratch_b, (IrqDeviceType)-1, on_zero));
    CHECK_IF(!irq_register_id(&scratch_b, (IrqDeviceType)256, 5, on_zero));
    CHECK_IF(!irq_register_id(&scratch_b, (IrqDeviceType)-1, 5, on_zero));
    CHECK_IF(!irq_unregister(NULL));
    CHECK_IF(!irq_unregister(&scratch_b));
    CHECK(scratch_b.key == 0x1234 && scratch_b.callback == on_edge &&
          scratch_b.next == &scratch_c);

    CHECK_IF(irq_register_id(&scratch_a, IRQ_DEVICE_SIGN, 5, on_zero));
    CHECK_IF(!irq_register_id(&scratch_b, IRQ_DEVICE_SIGN, 5, on_edge));
    CHECK(scratch_b.key == 0x1234 && scratch_b.callback == on_edge &&
          scratch_b.next == &scratch_c);
    CHECK_IF(!irq_register_id(&scratch_a, IRQ_DEVICE_SIGN, 6, on_edge));
    CHECK_IF(!irq_register_type(&scratch_a, IRQ_DEVICE_SIGN, on_edge));
    CHECK(scratch_a.key == 0x0105 && scratch_a.callback == on_zero &&
          scratch_a.next == NULL);
    CHECK_IF(irq_register_id(&scratch_b, IRQ_DEVICE_REALTIMER, 5, on_timer));
    CHECK_IF(irq_unregister(&scratch_a)); /* Tail removal. */
    CHECK(scratch_a.key == 0 && scratch_a.callback == NULL && scratch_a.next == NULL);
    CHECK_IF(!irq_unregister(&scratch_a));
    CHECK_IF(irq_unregister(&scratch_b)); /* Head removal. */

    CHECK_IF(irq_register_type(&scratch_a, IRQ_DEVICE_SIGN, on_type));
    CHECK_IF(!irq_register_type(&scratch_b, IRQ_DEVICE_SIGN, on_zero));
    CHECK_IF(!irq_register_id(&scratch_a, IRQ_DEVICE_REALTIMER, 5, on_zero));
    CHECK_IF(irq_register_type(&scratch_b, IRQ_DEVICE_REDSTONE, on_type));
    CHECK_IF(!irq_register_type(&scratch_b, IRQ_DEVICE_REALTIMER, on_zero));
    CHECK_IF(irq_unregister(&scratch_a));
    CHECK_IF(irq_unregister(&scratch_b));
    CHECK(scratch_b.key == 0 && scratch_b.callback == NULL && scratch_b.next == NULL);

    /* Equal numeric keys in the two lists are distinct; FF is not a wildcard. */
    CHECK_IF(irq_register_type(&scratch_a, (IrqDeviceType)255, on_type));
    CHECK_IF(irq_register_id(&scratch_b, IRQ_DEVICE_REDSTONE, 255, on_zero));
    CHECK_IF(irq_register_id(&scratch_c, (IrqDeviceType)255, 255, on_edge));
    irq_set_unhandled_handler(on_unhandled);
    CHECK(is_intr_enabled() == enabled);
    CHECK_IF(irq_set_handler(router) == irq_default_handler);
    irq_init();
    CHECK(is_intr_enabled() == enabled);
    CHECK_IF(!irq_register_type(&scratch_b, (IrqDeviceType)255, on_type));
    CHECK_IF(irq_set_handler(NULL) == router);
    CHECK_IF(irq_set_handler(router) == irq_default_handler);
    CHECK_IF(irq_set_handler(irq_default_handler) == router);
    CHECK_IF(irq_set_handler(NULL) == irq_default_handler);
    irq_set_unhandled_handler(NULL);
    CHECK(is_intr_enabled() == enabled);
    CHECK_IF(irq_unregister(&scratch_b));
    CHECK_IF(irq_unregister(&scratch_c));
    CHECK_IF(irq_unregister(&scratch_a));
}

void rom_main(void)
{
    for (uint16 i = 0; i < 13; i++) MB[i] = 0;
    for (uint16 i = 0; i < 32; i++) LOG[i] = 0;
    registry_checks(false);
    registry_checks(true);
    disable_intr();
    CHECK(irq_register_id(&zero, IRQ_DEVICE_REDSTONE, 0, on_zero));
    CHECK(irq_register_id(&sign_zero, IRQ_DEVICE_SIGN, 0, on_sign_zero));
    CHECK(irq_register_id(&sign_seven, IRQ_DEVICE_SIGN, 7, on_sign_seven));
    CHECK(irq_register_id(&timer_zero, IRQ_DEVICE_REALTIMER, 0, on_timer));
    CHECK(irq_register_id(&edge, (IrqDeviceType)255, 255, on_edge));
    CHECK(irq_register_id(&sign_edge, IRQ_DEVICE_SIGN, 255, on_edge));
    irq_set_unhandled_handler(on_unhandled);
    MB[8] = sizeof(IrqBinding);
    enable_intr();
    MB[0] = 0x4952;

    for (;;) {
        uint16 command = MB[2];
        if (!command) continue;
        switch (command) {
            case 1: CHECK(irq_register_type(&sign_type, IRQ_DEVICE_SIGN, on_type)); break;
            case 2: CHECK(irq_unregister(&sign_type)); break;
            case 3: saved_router = irq_set_handler(router); break;
            case 4: CHECK(irq_set_handler(saved_router) == router); break;
            case 5: CHECK(irq_set_handler(NULL) == router); break;
            case 6: irq_set_unhandled_handler(NULL); break;
            case 7: irq_set_unhandled_handler(on_unhandled); break;
            case 8: CHECK(irq_unregister(&sign_zero)); break;
            case 9: CHECK(irq_register_id(&sign_zero, IRQ_DEVICE_SIGN, 0, on_sign_zero)); break;
            case 10: irq_init(); break;
            case 11: __asm__ volatile ("int $0x20" ::: "memory"); break;
            case 14: CHECK(irq_register_type(&edge_type, (IrqDeviceType)255, on_type)); break;
            case 15: CHECK(irq_unregister(&edge_type)); break;
            case 16:
            case 17:
            case 18:
            case 19:
                if (command >= 18) disable_intr();
                sleep_realtime(MB[13], (uint8)MB[14], (command & 1) != 0);
                CHECK(is_intr_enabled() == (command < 18));
                enable_intr();
                break;
            default: CHECK(false);
        }
        CHECK(is_intr_enabled());
        MB[2] = 0;
        MB[7]++;
    }
}
