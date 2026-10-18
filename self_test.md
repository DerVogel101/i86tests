# Terminal self-test ROM

`self_test.c` is a separate program beside `main.c`. The normal build produces
both `build/rom.bin` and `build/self_test.bin`; each is a 256 KiB ROM with the
existing reset vector and small-model startup. Burn **self_test.bin** to run
the diagnostic. `main.c` is unchanged. The normal build also produces
`build/keyboard_editor.bin`, a separate [keyboard sign editor](keyboard_editor.md)
using keyboard slot 0 and sign IDs 1 (text) and 2 (status).

## Build

In Ubuntu, from this project:

```sh
./build.sh
```

From Windows:

```bat
wsl -d Ubuntu --cd /home/vogel/intel8086_emulator_code/i86tests --exec bash build.sh
```

To rebuild only the diagnostic after configuring the project:

```sh
cmake --build build --target self_test
```

## Hardware and wiring

Power off before installing components or attaching ports. Use the required
mainframe components and a Clock Crystal, plus:

- Four GPIO Controllers (to expose slots 0 through 127).
- At least one GPU Array and a sign port bound to sign ID **1**.
- Atomic Clock, Interrupt Controller, and Access Terminal.
- Harddisk Array and Integrated Storage, with usable persistent storage.

Create one output port and one input port for each ID below. Connect each output
to the input with the **same** ID. Keep the four circuits independent and preserve
the exact strength: ordinary redstone dust can attenuate the signal. Check the
ports' facing directions and use a direct connection or another lossless path.

| Output ID | Input ID | Input interrupt setting |
|---:|---:|---|
| 0 | 0 | HIGH / Rising edge |
| 32 | 32 | LOW / Falling edge |
| 64 | 64 | BOTH / Both edges |
| 96 | 96 | ANALOG / Every level change |

Bind the sign port to ID 1 and enable **front** click interrupts. Leave timer IRQ
delivery automatic. These settings belong to the host/ports; the ROM cannot
select redstone IRQ modes or sign sides through I/O registers.

Shared memory is not tested, and an Ender Tunnel Array is not required.

## Running and results

Open the terminal and power on. Keep the machine and its ports loaded and running;
do not pause the debugger, change the wiring, or change IRQ settings during a run.
The ROM runs once. Reset the CPU to run it again.

The terminal prints numbered stages, `PASS`, `FAIL`, and `SKIP` lines. A mismatch
includes expected and actual values. Terminal chunking probes include complete
`LOG[0]`, `LOG[1]`, `LOG[31]`, `LOG[32]`, `LOG[33]`, `LOG[64]`, and `LOG[65]` lines;
the number is the payload length. The ROM can check that send commands complete;
inspect the terminal lines to verify the complete text.

The diagnostic checks:

1. Terminal delivery and the 16-bit sign-control masks.
2. Calendar ranges, advancing real time and tick uptime, and both yield modes.
3. Exact redstone strengths 0 through 15, repeated-value silence, return to zero,
   and each configured edge/analog IRQ mode. Outputs are held for four game ticks.
4. All six timers, all four resolutions, one-shot expiry and IRQ identity,
   periodic IRQs during batch/tick yielding, disable, reset, countdown write
   protection, zero waits, and expiry without IRQ generation.
5. Sign readback and a front-click IRQ. The front displays `Hello World` on line 1
   and `click test` on line 2. **Wait for `Click front of sign 1 within 60 seconds.`
   before clicking its front.** The ROM also clears the sign's click flags after
   the IRQ router has acknowledged the queued event.
6. The first 16 bits of I/O storage and mapped NVRAM.
7. Output/timer cleanup, IRQ queue drainage, unexpected IRQs, and overflow.

IRQ callbacks only record counters and flags; printing and waiting stay in the
foreground. The IRQ library handles queue acknowledgement. Diagnostic waits yield
the current batch. Some periodic tests deliberately yield the rest of the tick.

Automatic waits stop after 10 real seconds or 200 game ticks, with a 4,096-yield
fallback if the clocks fail. The sign wait allows 60 real seconds or 1,200 game
ticks, with a 65,535-yield fallback. Expiration is checked when the CPU next runs,
so a paused or unloaded machine cannot print a timeout until it resumes.
Missing prerequisites cause a failure followed by skips of dependent tests;
independent tests continue. `sleep_realtime()` is deliberately not used for these
waits because its API has no diagnostic timeout.

A successful run ends with:

```text
SELFTEST DONE pass=... fail=0 skip=0
```

The CPU then halts with interrupts disabled. Tested outputs are zero, timers are
reset, and the sign text remains visible. A pass proves the observations made by
the ROM; it does not replace checking the visible sign or persistence after a
world restart.

## Persistent data changes

**This test intentionally destroys the original first word of both stores.**
Back up valuable images before running it.

I/O storage uses word address 0. Mapped NVRAM uses bytes 0 and 1 at `A000:0000`.
For each device the ROM prints the original decimal value, verifies its bitwise
complement and `0xA55A`, then writes and verifies the current low 16 bits of
powered-on Minecraft **tick uptime**. The two final timestamps may differ.
No other persistent address is written. The originals are not restored.

If the tick clock fails, originals are printed but overwrite tests are skipped
to avoid leaving scratch patterns instead of a valid timestamp. If a device
disconnects or stops accepting writes during the overwrite sequence, the final
timestamp cannot be guaranteed; the reported failure identifies that limit.

For a manual persistence check, record the final values, save and restart the
world, then rerun: the printed originals should equal the previous final values.

## Library fixes

The library fixes accompanying this ROM preserve the existing public API:
`log_text()` sends all chunks, sign flags retain their 16-bit values, and mapped
clock words use single 8086 word reads to avoid torn values between host updates.
Full calendar snapshots still need repeated reads; the ROM compares calendar
fields while allowing milliseconds to advance. A future library helper for
coherent clock snapshots or bounded sleep would remove that diagnostic boilerplate.
