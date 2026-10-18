# Keyboard sign editor ROM

`keyboard_editor.c` is a separate interactive test program beside `main.c` and
`self_test.c`. The normal `./build.sh` also builds **build/keyboard_editor.bin**.
Burn that 256 KiB image to a ROM chip; the main ROM and terminal self-test are separate.

After configuring with `./build.sh`, rebuild just this ROM with:

```sh
cmake --build build --target keyboard_editor
```

Install the required mainframe components, an Interrupt Controller, one Keyboard
Controller, and at least one GPU Array. An Access Terminal is optional for the
startup message. Power off while binding:

| Device | Slot / sign ID | Purpose |
| --- | --- | --- |
| Keyboard | **0** | Editor input, using the keyboard library's IRQ route |
| Sign port | **1** | Four editable lines, 15 characters per line |
| Sign port | **2** | Cursor, character count, format, last key and action |

Place a Minecraft sign in front of each sign port. Sign click IRQs are unnecessary.
Both sides show the same content. Power on, right-click the keyboard to capture,
and press Escape to leave capture. RESET restarts the editor with an empty document.

## Editing

- Ordinary text inserts at the cursor, shifting the rest of that line right.
- A full line wraps subsequent typing to the next line when it has room. Full
  lines reject insertion rather than discarding text. The document holds 60 characters.
- Left/Right move through text and cross line boundaries; Up/Down change lines
  and clamp to the destination line's length. Home/End select the current line's
  start/end; Page Up/Page Down select the first/last line.
- Enter splits the current line and shifts later lines down. It refuses when
  there is no free bottom line or the cursor is already on the last line.
- Backspace removes the preceding character; Delete removes the current one.
  At a line boundary they join adjacent lines if the result fits 15 characters.
  A join keeps the first line's formatting.
- Tab inserts spaces up to the next four-column tab stop if they fit.

The status sign uses one-based row and column numbers; column 16 is the insertion
position after a full line. `Used` counts characters. `Key` is the last byte and
`IRQ` is the processed keyboard event count, both hexadecimal. An error such as
`Line full` or `No free line` leaves the existing text intact.

## Numpad formatting

Formatting applies to the entire current line, as required by the sign I/O format.
Num Lock has no effect. Main-section operators still insert ordinary text.

| Numpad key | Action |
| --- | --- |
| 0–9 | Select Minecraft color index 0–9 |
| + / - | Cycle forward/backward through all 16 colors (0–F) |
| Divide | Toggle italic (`I`) |
| Multiply | Toggle strikethrough (`S`) |
| Decimal | Toggle underline (`U`) |
| Enter | Reset this line to the default color with no styles |
| Equals, if present | Toggle default color; `Col:-` means the default color |

The keyboard emits Windows-1258. The ordinary characters reachable through the
standard DE/US maps share their byte values with the sign's Windows-1252 encoding,
so umlauts, standalone accents, and euro display directly. Reserved key bytes are
handled as commands and never written into the text.

## Emulator regression checks

Build the ROM, then set `KEYBOARD_EDITOR_ROM` to its `keyboard_editor.bin` path
when running the Java emulator's `test` task. `KeyboardEditorRomTest` runs the actual
IA-16 image and checks editing, both sign faces, cursor movement, formatting,
capacity rejection, character bytes, and real keyboard IRQ/ACK delivery.
