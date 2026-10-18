#pragma once
#include "irq_handler.h"

/* Devices 0..15 are readers; 16..31 are writers. Buffers contain 32 bytes. */
typedef void (*PunchcardCallback)(uint8 device, uint8 events);
typedef struct PunchcardBinding {
    IrqBinding irq;
    PunchcardCallback callback;
    struct PunchcardBinding *next;
    uint8 device;
} PunchcardBinding;

uint8 punchcard_status(uint8 device);
uint8 punchcard_events(uint8 device);
uint8 punchcard_error(uint8 device);
uint16 punchcard_cards(uint8 device);
uint16 punchcard_remaining(uint8 device);
uint16 punchcard_current_card(uint8 device);
uint8 punchcard_current_row(uint8 device);
uint32 punchcard_size(uint8 device);
/* No waiting. Busy/unacknowledged results reject another request. */
bool punchcard_request(uint8 device, uint8 command, const uint8 *row);
bool punchcard_read_row(uint8 device);
bool punchcard_write_row(uint8 device, const uint8 row[32]);
bool punchcard_read_name(uint8 device);
bool punchcard_write_name(uint8 device, const char *name);
bool punchcard_read_description(uint8 device);
bool punchcard_write_description(uint8 device, const char *description);
bool punchcard_next(uint8 device);
bool punchcard_eject(uint8 device);
/* Copies and ACKs a completed result; row may be NULL for write/switch/eject. */
bool punchcard_try_result(uint8 device, uint8 *row);
void punchcard_clear_events(uint8 device, uint8 events);
void punchcard_clear_error(uint8 device);
/* Foreground-only registration; caller-owned zeroed storage must remain alive.
 * IRQ callback clears event reasons, not result ACK. No waiting/STI/IRQ ACK. */
bool punchcard_register(PunchcardBinding *binding, uint8 device, PunchcardCallback callback);
bool punchcard_unregister(PunchcardBinding *binding);
