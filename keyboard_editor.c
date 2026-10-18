#include "lib/keyboard.h"

#define ROWS 4
#define COLS 15
#define TEXT_SIGN 1
#define STATUS_SIGN 2

static KeyboardBinding keyboard;
static uint8 text[ROWS][COLS], length[ROWS], format[ROWS];
static uint8 row, column, last_key;
static uint16 keys;
static volatile bool dirty;
static const char *action = "Ready";

static void copy_line(uint8 destination, uint8 source)
{
    length[destination] = length[source];
    format[destination] = format[source];
    for (uint8 i = 0; i < COLS; ++i) text[destination][i] = text[source][i];
}

static void erase_at_cursor(void)
{
    for (uint8 i = column; i + 1 < length[row]; ++i) text[row][i] = text[row][i + 1];
    --length[row];
}

static void join_next(void)
{
    if (row + 1 == ROWS) { action = "At end"; return; }
    if (length[row] + length[row + 1] > COLS) { action = "Line too long"; return; }
    for (uint8 i = 0; i < length[row + 1]; ++i) text[row][length[row] + i] = text[row + 1][i];
    length[row] += length[row + 1];
    for (uint8 i = row + 1; i + 1 < ROWS; ++i) copy_line(i, i + 1);
    length[ROWS - 1] = 0;
    format[ROWS - 1] = SIGN_FORMAT_DISABLE_COLOR;
    action = "Join lines";
}

static void newline(void)
{
    if (row + 1 == ROWS) { action = "Last line"; return; }
    if (length[ROWS - 1]) { action = "No free line"; return; }
    for (uint8 i = ROWS - 1; i > row + 1; --i) copy_line(i, i - 1);
    length[row + 1] = length[row] - column;
    format[row + 1] = format[row];
    for (uint8 i = 0; i < length[row + 1]; ++i) text[row + 1][i] = text[row][column + i];
    length[row] = column;
    ++row; column = 0;
    action = "Split line";
}

static void insert(uint8 byte)
{
    if (column == COLS && row + 1 < ROWS && length[row + 1] < COLS) { ++row; column = 0; }
    if (length[row] == COLS) { action = "Line full"; return; }
    for (uint8 i = length[row]; i > column; --i) text[row][i] = text[row][i - 1];
    text[row][column++] = byte;
    ++length[row];
    action = "Insert";
}

static void typed(uint8 slot, uint8 byte)
{
    (void)slot;
    last_key = byte; ++keys; dirty = true;
    if (byte >= KEY_KP_0 && byte <= KEY_KP_9) {
        format[row] = (format[row] & 0xe0) | (byte - KEY_KP_0);
        action = "Color";
        return;
    }
    switch (byte) {
    case KEY_LEFT:
        if (column) --column;
        else if (row) column = length[--row];
        action = "Left"; break;
    case KEY_RIGHT:
        if (column < length[row]) ++column;
        else if (row + 1 < ROWS) { ++row; column = 0; }
        action = "Right"; break;
    case KEY_UP:
        if (row) --row;
        if (column > length[row]) column = length[row];
        action = "Up"; break;
    case KEY_DOWN:
        if (row + 1 < ROWS) ++row;
        if (column > length[row]) column = length[row];
        action = "Down"; break;
    case KEY_HOME: column = 0; action = "Home"; break;
    case KEY_END: column = length[row]; action = "End"; break;
    case KEY_PAGE_UP: row = 0; if (column > length[row]) column = length[row]; action = "First line"; break;
    case KEY_PAGE_DOWN: row = ROWS - 1; if (column > length[row]) column = length[row]; action = "Last line"; break;
    case KEY_ENTER: newline(); break;
    case KEY_BACKSPACE:
        if (column) { --column; erase_at_cursor(); action = "Backspace"; }
        else if (row && length[row - 1] + length[row] <= COLS) { --row; column = length[row]; join_next(); }
        else action = row ? "Line too long" : "At start";
        break;
    case KEY_DELETE:
        if (column < length[row]) { erase_at_cursor(); action = "Delete"; }
        else join_next();
        break;
    case KEY_TAB: {
        uint8 spaces = 4 - column % 4;
        if (length[row] + spaces > COLS) { action = "Line full"; break; }
        while (spaces--) insert(' ');
        action = "Tab"; break;
    }
    case KEY_KP_ADD:
    case KEY_KP_SUBTRACT:
        format[row] = (format[row] & 0xe0) | ((format[row] + (byte == KEY_KP_ADD ? 1 : 15)) & 15);
        action = "Color"; break;
    case KEY_KP_DIVIDE: format[row] ^= SIGN_FORMAT_ITALIC; action = "Italic"; break;
    case KEY_KP_MULTIPLY: format[row] ^= SIGN_FORMAT_STRIKETHROUGH; action = "Strike"; break;
    case KEY_KP_DECIMAL: format[row] ^= SIGN_FORMAT_UNDERLINED; action = "Underline"; break;
    case KEY_KP_ENTER: format[row] = SIGN_FORMAT_DISABLE_COLOR; action = "Format reset"; break;
    case KEY_KP_EQUAL: format[row] ^= SIGN_FORMAT_DISABLE_COLOR; action = "Default color"; break;
    default:
        if (byte >= 0x20 && (byte < 0x81 || byte > 0x9f)) insert(byte);
        else action = "Unused key";
    }
}

static void write_line(uint8 sign, uint8 line, uint8 style, const uint8 *bytes, uint8 count)
{
    for (uint8 side = 0; side < 2; ++side) {
        uint16 port = IO_SIGN(sign) + (uint16)side * 64 + (uint16)line * 16;
        out8(port, style);
        for (uint8 i = 0; i < COLS; ++i) out8(port + 1 + i, i < count ? bytes[i] : 0);
    }
}

static void status(uint8 line, const char *message)
{
    uint8 count = 0;
    while (count < COLS && message[count]) ++count;
    write_line(STATUS_SIGN, line, SIGN_FORMAT_DISABLE_COLOR, (const uint8 *)message, count);
}

static char hex(uint8 value) { return "0123456789ABCDEF"[value & 15]; }

static void render(void)
{
    uint8 used = 0;
    for (uint8 i = 0; i < ROWS; ++i) {
        used += length[i];
        write_line(TEXT_SIGN, i, format[i], text[i], length[i]);
    }
    char position[] = "R1 C01 Used00";
    position[1] += row;
    position[4] = '0' + (column + 1) / 10; position[5] = '0' + (column + 1) % 10;
    position[11] = '0' + used / 10; position[12] = '0' + used % 10;
    status(0, position);
    char styles[] = "Col:- I0 U0 S0";
    if (!(format[row] & SIGN_FORMAT_DISABLE_COLOR)) styles[4] = hex(format[row]);
    styles[7] += !!(format[row] & SIGN_FORMAT_ITALIC);
    styles[10] += !!(format[row] & SIGN_FORMAT_UNDERLINED);
    styles[13] += !!(format[row] & SIGN_FORMAT_STRIKETHROUGH);
    status(1, styles);
    char event[] = "Key:00 IRQ:0000";
    event[4] = hex(last_key >> 4); event[5] = hex(last_key);
    for (uint8 i = 0; i < 4; ++i) event[11 + i] = hex(keys >> (12 - i * 4));
    status(2, event);
    status(3, action);
}

void rom_main(void)
{
    for (uint8 i = 0; i < ROWS; ++i) format[i] = SIGN_FORMAT_DISABLE_COLOR;
    irq_init();
    if (!keyboard_register(&keyboard, 0, typed)) {
        action = "IRQ route error";
        render();
        for (;;) i86_yield_tick();
    }
    dirty = true;
    log_text("Keyboard editor: keyboard 0, text sign 1, status sign 2.\n"
             "Numpad 0-9 color, +/- cycle, / italic, * strike, . underline, Enter reset.\n");
    for (;;) {
        disable_intr();
        if (dirty) { render(); dirty = false; }
        enable_intr();
        i86_yield_tick();
    }
}
