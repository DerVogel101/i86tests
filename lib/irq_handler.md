# IRQ routing

Include `irq_handler.h`. Call `irq_init()` before enabling interrupts.
The library uses vector `0x20`, reads one queued event per interrupt, invokes
the router, ACKs the event after the callback returns, and restores CPU state.

```c
#include "lib/irq_handler.h"

static IrqBinding timer_zero;
static volatile uint16 expirations;

static void on_timer(IrqDeviceType type, uint8 index)
{
    (void)type;
    (void)index;
    expirations++;
}

/* In foreground initialization, while interrupts are still disabled: */
void setup_irqs(void)
{
    irq_init();
    if (!irq_register_id(&timer_zero, IRQ_DEVICE_REALTIMER, 0, on_timer))
        return; /* Handle registration failure before enabling the source. */
    enable_intr();
}
```

The example installs routing only; configure the timer separately.

## Routing and ownership

Default routing chooses one callback: a matching type override, otherwise an
exact `(type, index)` registration, otherwise the unhandled callback.
A type override hides its existing ID registrations. Unregistering the override
restores them.

`irq_set_handler(callback)` replaces the entire router and returns the previous
effective callback. Passing `NULL` restores `irq_default_handler`.
A replacement router can call `irq_default_handler(type, index)` to delegate.
A type override must not delegate there for its own type, which would recurse.

`irq_set_unhandled_handler(NULL)` silently ignores unmatched events. The library
still ACKs them. All callback arguments are the original raw type and index
bytes, including unnamed types and index 255. `IRQ_DEVICE_TYPE_INVALID` is an
out-of-range sentinel that keeps the enum wide enough for input validation.

Callbacks run with IF and DF cleared. Return promptly: do not ACK, enable
interrupts, halt, wait, or mutate registrations inside a callback. Queue overflow
remains sticky until foreground code calls `clear_irq_overflow()`.

Registration records belong to the application. Keep them writable and alive
until unregistered, and do not modify linked fields. Static zero-initialized
records are recommended. Registration rejects invalid inputs, duplicate keys
and linked records without changing state. Unregistering clears the record so
it can be reused. All mutations preserve the previous IF state and are
foreground-only. Repeated `irq_init()` calls preserve routing and registrations.

## Memory and build requirements

The current IA-16 small model uses six bytes per record and eight bytes of base
registry state, all in RAM. Zero-initialized storage occupies `.bss` without
initialized ROM payload. There is no heap or library-wide capacity array.
Lookup is linear in registered routes; use a custom type handler with a dense
table if IRQ latency requires it.

The entry stub requires CS=C000 and SS=0, uses the application's stack, and
establishes DS/ES=0 while calling C. The linker uses CS-relative code symbols
with physical load address C0000 so near function pointers work. Logical code
and RAM offsets can overlap because they belong to separate segments.
The physical memory layout and 256 KiB ROM format are unchanged.

## Runnable integration checks

From the Windows emulator checkout, build both ROMs through WSL:

```powershell
rtk proxy wsl.exe -d Ubuntu --exec bash -lc 'cd /home/vogel/intel8086_emulator_code/i86tests && ./build.sh && cmake --build build --target irq_handler_test'
$env:IRQ_HANDLER_TEST_ROM = '\\wsl.localhost\Ubuntu\home\vogel\intel8086_emulator_code\i86tests\build\irq_handler_test.bin'
rtk proxy .\gradlew.bat test --offline --tests dervogel101.de.IrqHandlerRomTest
rtk proxy .\gradlew.bat test --offline
```

The integration class needs both `irq_handler_test.bin` and its sibling
`rom.bin`. If the environment variable is absent, only this class is skipped;
an explicitly supplied missing or invalid ROM fails. Gradle tracks the variable
and both images so rebuilding the ROM reruns the checks. The test target is
excluded from the normal build, and all generated ROM artifacts stay in
`build/`. The repository's existing Java dependencies must be cached before
using Gradle's offline mode.
