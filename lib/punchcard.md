# Nonblocking punchcard library

Include `punchcard.h` and link `punchcard.c`, `hardware.c`, `irq_handler.c`, and
the normal interrupt startup. Device IDs 0–15 select readers, 16–31 writers;
use `PUNCHCARD_READER(slot)` and `PUNCHCARD_WRITER(slot)`.

Call `irq_init()` before enabling interrupts. Register a zero-initialized,
caller-owned `PunchcardBinding` with `punchcard_register`. It must remain alive
until `punchcard_unregister`. Registrations are sparse and preserve IF. The
callback receives `(device, event_bits)`. The wrapper clears observed event
reasons; the IRQ manager owns global queue ACK. Callbacks must not wait, enable
interrupts, mutate registrations, or ACK the IRQ queue. They may inspect status
or copy a completed row with `punchcard_try_result`.

`punchcard_read_row`, `punchcard_write_row`, `punchcard_read_name`,
`punchcard_write_name`, `punchcard_next`, and `punchcard_eject` submit requests
without waiting. Their boolean result reports submission availability; inspect
`punchcard_error` for device-level command errors. `punchcard_request` exposes the
same interface for raw commands. Write buffers must contain exactly 32 bytes;
the name helper accepts a C string of at most 32 Windows-1252 bytes and pads zeros.

`punchcard_try_result(device, row)` copies 32 bytes and requests completion ACK.
Pass NULL when a write/switch/eject has no returned data. Do not submit another
operation before consuming the previous result. Register reads alone never
advance media. Transfer and card-switch requests require a game tick to finish.
After eight rows, explicitly request next-card and consume that completion too.

Status/counter helpers expose total cards, current card/row, and remaining cards.
Readers include the current card until row eight is consumed. Writers report
additional unallocated cards. `punchcard_size` returns `uint32`, including 65536
for a full 256-card deck. The name header uses no card data capacity.

Example pattern (foreground, with the host supplying ticks):

```c
uint8 row[32];
if (punchcard_read_row(PUNCHCARD_READER(0))) {
    /* Continue other work; later: */
    if (punchcard_try_result(PUNCHCARD_READER(0), row)) {
        /* Consume row; check PUNCHCARD_EOC before requesting another. */
    }
}
```

Build the integrated test/example with
`cmake --build build --target punchcard_test`. Set `PUNCHCARD_TEST_ROM` to its
`punchcard_test.bin` when running the Java emulator's `PunchcardRomTest`. The
host inserts a blank writer deck and transfers its ejected output into reader 0.

`punchcard_read_description` and `punchcard_write_description` transfer an
independent 32-byte Windows-1252 description. Name and description can each be
written once before the first data row. Both use the row buffer, `NAME` event,
and normal result ACK without advancing the cursor. Text helpers reject strings
longer than 32 bytes. Helpers select the appropriate field atomically with their
command, preserving IF; `punchcard_request` selects the name by default.
`IO_PUNCHCARD_METADATA(d)` is the selector at offset `2C` (0 name, 1 description).
Flags at `2B` expose sealed, name-written, and description-written in bits 0–2.
The integrated test also checks description writes and reads independently of names.
