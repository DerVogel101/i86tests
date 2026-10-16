#include "lib/hardware.h"
#include "lib/irq_handler.h"
#include "lib/strings.h"

typedef unsigned long uint32;

static const uint8 channels[] = {0, 32, 64, 96};
static IrqBinding bindings[11];
static volatile uint16 events[11], last_id[11], sign_flags;
static volatile uint16 unexpected, unexpected_id;
static uint16 passed, failed, skipped, stage;
static bool real_ok, tick_ok, irq_ok;

static void number(uint16 value)
{
    char text[6];
    uint16_to_str(value, text);
    log_text(text);
}

static void value(const char *label, uint16 n)
{
    log_text(label);
    number(n);
    log_text("\n");
}

static void heading(const char *name)
{
    log_text("\n["); number(++stage); log_text("] "); log_text(name); log_text("\n");
}

static void result(const char *name, bool ok)
{
    if (ok) passed++; else failed++;
    log_text(ok ? "PASS " : "FAIL "); log_text(name); log_text("\n");
}

static void skip(const char *name, const char *reason)
{
    skipped++;
    log_text("SKIP "); log_text(name); log_text(": "); log_text(reason); log_text("\n");
}

static bool expect(const char *name, uint16 actual, uint16 expected)
{
    if (actual == expected) return true;
    log_text("  "); log_text(name); log_text(" expected="); number(expected);
    log_text(" actual="); number(actual); log_text("\n");
    return false;
}

static bool same_clock(Realtime a, Realtime b)
{
    return b.milliseconds >= a.milliseconds && a.second == b.second &&
        a.minute == b.minute && a.hour == b.hour && a.day_of_month == b.day_of_month &&
        a.month == b.month && a.year == b.year;
}

static bool read_clock(Realtime *time)
{
    for (uint8 attempt = 0; attempt < 16; attempt++) {
        Realtime first = get_realtime();
        *time = get_realtime();
        if (same_clock(first, *time)) return true;
    }
    return false;
}

static bool read_ticks(TickTime *time)
{
    for (uint8 attempt = 0; attempt < 16; attempt++) {
        TickTime first = get_tick_time();
        *time = get_tick_time();
        if (first.uptime_ticks == time->uptime_ticks && first.minecraft_day == time->minecraft_day)
            return true;
    }
    return false;
}

static bool calendar_valid(Realtime time)
{
    return time.milliseconds < 1000 && time.second < 60 && time.minute < 60 &&
        time.hour < 24 && time.day_of_month < 31 && time.month < 12 && time.year != 0;
}

static uint32 clock_ms(Realtime time)
{
    return (((uint32)time.hour * 60UL + time.minute) * 60UL + time.second) * 1000UL +
        time.milliseconds;
}

static uint32 elapsed_ms(uint32 now, uint32 before)
{
    return now >= before ? now - before : 86400000UL - before + now;
}

typedef struct {
    uint32 real_start;
    uint16 tick_start, remaining;
    bool interactive;
} Deadline;

static Deadline deadline(bool interactive)
{
    Realtime real;
    TickTime tick;
    read_clock(&real); read_ticks(&tick);
    Deadline d = {clock_ms(real), tick.uptime_ticks, interactive ? 65535u : 4096u, interactive};
    return d;
}

static bool expired(Deadline *d)
{
    Realtime real;
    TickTime tick;
    if (d->remaining == 0) return true;
    d->remaining--;
    if (real_ok && read_clock(&real) && calendar_valid(real) &&
        elapsed_ms(clock_ms(real), d->real_start) >= (d->interactive ? 60000UL : 10000UL))
        return true;
    return tick_ok && read_ticks(&tick) &&
        (uint16)(tick.uptime_ticks - d->tick_start) >= (d->interactive ? 1200u : 200u);
}

static bool wait_ticks(uint16 count)
{
    TickTime start, now;
    read_ticks(&start);
    Deadline d = deadline(false);
    while (true) {
        if (read_ticks(&now) && (uint16)(now.uptime_ticks - start.uptime_ticks) >= count)
            return true;
        if (expired(&d)) return false;
        i86_yield_batch();
    }
}

static bool quiet(void)
{
    Deadline d = deadline(false);
    while (is_irq_pending()) {
        if (expired(&d)) return false;
        i86_yield_batch();
    }
    return true;
}

static void on_irq(IrqDeviceType type, uint8 index)
{
    uint8 slot = 11;
    if (type == IRQ_DEVICE_REDSTONE) {
        for (uint8 i = 0; i < 4; i++) if (index == channels[i]) slot = i;
    } else if (type == IRQ_DEVICE_REALTIMER && index < 3) slot = 4 + index;
    else if (type == IRQ_DEVICE_TICKTIMER && index < 3) slot = 7 + index;
    else if (type == IRQ_DEVICE_SIGN && index == 1) {
        slot = 10;
        sign_flags = in16(IO_SIGN_CONTROL(1));
    }
    uint16 id = ((uint16)type << 8) | index;
    if (slot < 11) { events[slot]++; last_id[slot] = id; }
    else { unexpected++; unexpected_id = id; }
}

static void setup_irqs(void)
{
    irq_init();
    irq_ok = true;
    for (uint8 i = 0; i < 4; i++)
        if (!irq_register_id(&bindings[i], IRQ_DEVICE_REDSTONE, channels[i], on_irq)) irq_ok = false;
    for (uint8 i = 0; i < 6; i++)
        if (!irq_register_id(&bindings[4 + i], i < 3 ? IRQ_DEVICE_REALTIMER : IRQ_DEVICE_TICKTIMER,
                             i % 3, on_irq)) irq_ok = false;
    if (!irq_register_id(&bindings[10], IRQ_DEVICE_SIGN, 1, on_irq)) irq_ok = false;
    irq_set_unhandled_handler(on_irq);
    clear_irq_overflow();
    enable_intr();
    result("IRQ registration", irq_ok && is_intr_enabled());
}

static void terminal_test(void)
{
    static const uint8 lengths[] = {0, 1, 31, 32, 33, 64, 65};
    char text[66];
    heading("Terminal chunking probes");
    for (uint8 test = 0; test < 7; test++) {
        uint8 length = lengths[test];
        for (uint8 i = 0; i < length; i++) text[i] = 'A' + i % 26;
        text[length] = '\0';
        log_text("LOG["); number(length); log_text("]="); log_text(text); log_text("\n");
    }
    result("terminal send commands serviced", (in8(IO_TERMINAL_CONTROL) & TERMINAL_CONTROL_SEND) == 0);
    bool masks = expect("clicked mask", SIGN_CONTROL_ALL, 0x0100);
    masks = expect("front mask", SIGN_CONTROL_FRONT, 0x0200) && masks;
    masks = expect("back mask", SIGN_CONTROL_BACK, 0x0400) && masks;
    result("16-bit sign masks", masks);
}

static void clocks_and_yield(void)
{
    heading("Clocks and yield");
    Realtime start, now;
    TickTime tick_start, tick_now;
    real_ok = read_clock(&start) && calendar_valid(start);
    read_ticks(&tick_start);
    log_text("Host date "); number(start.year); log_text("-"); number(start.month + 1);
    log_text("-"); number(start.day_of_month + 1); log_text(" "); number(start.hour);
    log_text(":"); number(start.minute); log_text(":"); number(start.second);
    log_text("."); number(start.milliseconds); log_text("\n");
    value("Minecraft day=", tick_start.minecraft_day);
    value("Tick uptime=", tick_start.uptime_ticks);
    result("calendar ranges", real_ok);
    Deadline d = deadline(false);
    bool real_advanced = false;
    while (!expired(&d)) {
        i86_yield_batch();
        if (real_ok && read_clock(&now) && elapsed_ms(clock_ms(now), clock_ms(start)) > 0)
            real_advanced = true;
        if (read_ticks(&tick_now) && (uint16)(tick_now.uptime_ticks - tick_start.uptime_ticks) > 0)
            tick_ok = true;
        if (real_advanced && tick_ok) break;
    }
    real_ok = real_ok && real_advanced;
    result("real clock advances", real_ok);
    result("tick clock advances", tick_ok);
    i86_yield_batch();
    result("batch yield resumes with IF enabled", is_intr_enabled());
    if (tick_ok) {
        read_ticks(&tick_start);
        i86_yield_tick();
        read_ticks(&tick_now);
        result("tick yield advances tick and preserves IF", is_intr_enabled() &&
               (uint16)(tick_now.uptime_ticks - tick_start.uptime_ticks) > 0);
    } else skip("tick yield timing", "tick clock failed");
}

static void redstone(void)
{
    heading("Redstone loopbacks: 0 HIGH, 32 LOW, 64 BOTH, 96 ANALOG");
    if (!tick_ok) { skip("redstone", "tick clock failed; cannot allow world propagation"); return; }
    for (uint8 i = 0; i < 4; i++) {
        uint8 id = channels[i];
        value("Channel ", id);
        out8(IO_REDSTONE_OUT(id), 0);
        bool ok = wait_ticks(4) && quiet();
        ok = expect("baseline input", in8(IO_REDSTONE_IN(id)), 0) && ok;
        if (!ok) { result("redstone baseline", false); continue; }
        for (uint8 step = 1; step <= 17; step++) {
            uint8 strength = step <= 15 ? step : step == 16 ? 15 : 0;
            uint16 before = events[i];
            uint16 other[4];
            for (uint8 j = 0; j < 4; j++) other[j] = events[j];
            out8(IO_REDSTONE_OUT(id), strength);
            bool settled = wait_ticks(4) && quiet();
            bool input_ok = expect("input strength", in8(IO_REDSTONE_IN(id)), strength);
            ok = expect("output strength", in8(IO_REDSTONE_OUT(id)), strength) && ok;
            ok = settled && input_ok && ok;
            if (irq_ok) {
                uint16 expected = i == 0 ? step == 1 : i == 1 ? step == 17 :
                    i == 2 ? step == 1 || step == 17 : step != 16;
                if (events[i] != before + expected) value("  at strength=", strength);
                ok = expect("IRQ count", (uint16)(events[i] - before), expected) && ok;
                if (expected) ok = expect("IRQ ID", last_id[i], id) && ok;
                for (uint8 j = 0; j < 4; j++)
                    if (j != i) ok = expect("other channel IRQs", events[j], other[j]) && ok;
            }
            if (!input_ok || !settled) break;
        }
        out8(IO_REDSTONE_OUT(id), 0);
        if (!wait_ticks(4) || !quiet()) ok = false;
        result("analog loopback and edge IRQs", ok && irq_ok);
        if (!irq_ok) skip("redstone IRQ assertions", "IRQ registration failed");
    }
}

static void timer_start(uint8 timer, uint16 wait, uint8 control)
{
    out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
    out16(IO_TIMER_WAIT(timer), wait);
    out8(IO_TIMER_CONTROL(timer), control);
}

static bool timer_finished(uint8 timer)
{
    Deadline d = deadline(false);
    while ((in8(IO_TIMER_CONTROL(timer)) & TIMER_CONTROL_ENABLE) || is_irq_pending()) {
        if (expired(&d)) return false;
        i86_yield_batch();
    }
    return true;
}

static void timer_registers(uint8 timer)
{
    bool ok;
    timer_start(timer, 100, TIMER_CONTROL_ENABLE);
    out8(IO_TIMER_CONTROL(timer), 0);
    uint16 frozen = in16(IO_TIMER_COUNTDOWN(timer));
    out16(IO_TIMER_COUNTDOWN(timer), 0xFFFF);
    ok = expect("countdown is read-only", in16(IO_TIMER_COUNTDOWN(timer)), frozen);
    ok = wait_ticks(4) && ok;
    ok = expect("frozen countdown", in16(IO_TIMER_COUNTDOWN(timer)), frozen) && ok;
    out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET | TIMER_CONTROL_ENABLE);
    ok = expect("reset control", in8(IO_TIMER_CONTROL(timer)), 0) && ok;
    ok = expect("reset countdown", in16(IO_TIMER_COUNTDOWN(timer)), 0) && ok;
    ok = expect("reset reload", in16(IO_TIMER_WAIT(timer)), 0) && ok;
    result("timer freeze and reset", ok);

    uint16 before = events[4 + timer];
    timer_start(timer, 0, TIMER_CONTROL_ENABLE | TIMER_CONTROL_INTR);
    ok = wait_ticks(4) && quiet();
    ok = expect("zero wait countdown", in16(IO_TIMER_COUNTDOWN(timer)), 0) && ok;
    ok = expect("zero wait stays enabled", in8(IO_TIMER_CONTROL(timer)),
                TIMER_CONTROL_ENABLE | TIMER_CONTROL_INTR) && ok;
    ok = expect("zero wait IRQs", (uint16)(events[4 + timer] - before), 0) && ok;
    result("zero wait stays idle", ok);
    timer_start(timer, 2, TIMER_CONTROL_ENABLE);
    ok = timer_finished(timer);
    ok = expect("IRQ-disabled countdown", in16(IO_TIMER_COUNTDOWN(timer)), 0) && ok;
    ok = expect("IRQ-disabled events", (uint16)(events[4 + timer] - before), 0) && ok;
    result("expiry without IRQ generation", ok);
    out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
}

static void timers(void)
{
    static const uint8 real_units[] = {1, 5, 10, 50};
    static const uint8 tick_units[] = {1, 5, 10, 20};
    heading("Six timers: one-shot, resolutions, periodic IRQs");
    for (uint8 timer = 0; timer < 6; timer++) {
        value("Timer ", timer);
        if (!tick_ok || (timer < 3 && !real_ok)) {
            skip("timer", "required clock failed"); continue;
        }
        for (uint8 resolution = 0; resolution < 4; resolution++) {
            quiet();
            uint16 before = events[4 + timer];
            Realtime real_start, real_end;
            TickTime tick_start, tick_end;
            read_clock(&real_start); read_ticks(&tick_start);
            timer_start(timer, 2, TIMER_CONTROL_ENABLE | TIMER_CONTROL_INTR | (resolution << 4));
            bool ok = timer_finished(timer);
            read_clock(&real_end); read_ticks(&tick_end);
            uint16 duration = 2u * (timer < 3 ? real_units[resolution] : tick_units[resolution]);
            uint32 elapsed = timer < 3 ? elapsed_ms(clock_ms(real_end), clock_ms(real_start)) :
                (uint16)(tick_end.uptime_ticks - tick_start.uptime_ticks);
            if (elapsed < duration) { value("  early expiry elapsed=", (uint16)elapsed); ok = false; }
            ok = expect("countdown", in16(IO_TIMER_COUNTDOWN(timer)), 0) && ok;
            ok = expect("one-shot enable", in8(IO_TIMER_CONTROL(timer)) & TIMER_CONTROL_ENABLE, 0) && ok;
            ok = expect("timer IRQ count", (uint16)(events[4 + timer] - before), 1) && ok;
            ok = expect("timer IRQ ID", last_id[4 + timer],
                ((uint16)(timer < 3 ? IRQ_DEVICE_REALTIMER : IRQ_DEVICE_TICKTIMER) << 8) | (timer % 3)) && ok;
            value("  resolution=", resolution);
            result("one-shot timer", ok && irq_ok);
            out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
        }
        uint16 before = events[4 + timer];
        timer_start(timer, timer < 3 ? 100 : 2,
                    TIMER_CONTROL_ENABLE | TIMER_CONTROL_PERIODIC | TIMER_CONTROL_INTR);
        Deadline d = deadline(false);
        while ((uint16)(events[4 + timer] - before) < 3 && !expired(&d)) {
            if (timer & 1) i86_yield_tick(); else i86_yield_batch();
        }
        bool ok = (uint16)(events[4 + timer] - before) >= 3 && is_intr_enabled();
        out8(IO_TIMER_CONTROL(timer), 0);
        ok = quiet() && ok;
        before = events[4 + timer];
        ok = wait_ticks(4) && quiet() && ok;
        ok = expect("disabled periodic IRQs", events[4 + timer], before) && ok;
        result("periodic IRQs while yielding, then disable", ok && irq_ok);
        out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
        if (timer == 0 || timer == 3) timer_registers(timer);
    }
}

static bool sign_line(uint8 line, const char *text)
{
    uint16 port = IO_SIGN_FRONT_1(1) + (uint16)line * 16u;
    out8(port, SIGN_FORMAT_DISABLE_COLOR);
    bool ok = true;
    for (uint8 i = 1; i < 16; i++) {
        uint8 ch = (uint8)*text;
        if (ch) text++;
        out8(port + i, ch);
        if (in8(port + i) != ch) ok = false;
    }
    return in8(port) == SIGN_FORMAT_DISABLE_COLOR && ok;
}

static void sign_test(void)
{
    heading("Sign 1 front text and click");
    bool ok = sign_line(0, "Hello World");
    ok = sign_line(1, "click test") && ok;
    ok = sign_line(2, "") && ok;
    ok = sign_line(3, "") && ok;
    result("sign text readback", ok);
    if (!ok || !irq_ok || (!real_ok && !tick_ok)) {
        skip("sign click", "text, IRQ registration or clock failed"); return;
    }
    out16(IO_SIGN_CONTROL(1), 0);
    quiet();
    sign_flags = 0;
    uint16 before = events[10];
    log_text("Click front of sign 1 within 60 seconds.\n");
    Deadline d = deadline(true);
    while ((events[10] == before || !(sign_flags & SIGN_CONTROL_FRONT)) && !expired(&d))
        i86_yield_batch();
    ok = events[10] != before;
    ok = expect("sign IRQ ID", last_id[10], 0x0101) && ok;
    ok = expect("front click flags", sign_flags & (SIGN_CONTROL_ALL | SIGN_CONTROL_FRONT),
                SIGN_CONTROL_ALL | SIGN_CONTROL_FRONT) && ok;
    result("front click IRQ", ok);
    out16(IO_SIGN_CONTROL(1), 0);
    result("sign flags clear separately from IRQ ACK", in16(IO_SIGN_CONTROL(1)) == 0);
}

static bool disk_read(uint16 *word)
{
    if (!is_nv_store_connected()) return false;
    out16(IO_NV_STORE_ADR, 0);
    out8(IO_NV_STORE_CONTROL, NV_STORE_CONTROL_READ);
    Deadline d = deadline(false);
    while (in8(IO_NV_STORE_CONTROL) & NV_STORE_CONTROL_READ) {
        if (expired(&d)) return false;
        i86_yield_batch();
    }
    *word = in16(IO_NV_STORE_DATA);
    return is_nv_store_connected();
}

static bool disk_write(uint16 word)
{
    if (!is_nv_store_connected()) return false;
    out16(IO_NV_STORE_ADR, 0);
    out16(IO_NV_STORE_DATA, word);
    out8(IO_NV_STORE_CONTROL, NV_STORE_CONTROL_WRITE);
    Deadline d = deadline(false);
    while (in8(IO_NV_STORE_CONTROL) & NV_STORE_CONTROL_WRITE) {
        if (expired(&d)) return false;
        i86_yield_batch();
    }
    return is_nv_store_connected();
}

static void storage(void)
{
    heading("Persistent first words: overwritten intentionally");
    uint16 original, actual = 0;
    if (!disk_read(&original)) {
        result("I/O storage connected/readable", false);
        skip("I/O overwrite", "storage disconnected or command timed out");
    } else if (tick_ok) {
        value("I/O original=", original);
        uint16 pattern = (uint16)~original;
        bool ok = disk_write(pattern) && disk_read(&actual);
        ok = expect("I/O complement", actual, pattern) && ok;
        bool second = disk_write(0xA55A) && disk_read(&actual);
        ok = expect("I/O pattern", actual, 0xA55A) && second && ok;
        result("I/O overwrite/readback", ok);
    } else { value("I/O original=", original); skip("I/O overwrite", "tick clock failed; preserve original data"); }
    if (is_nv_store_connected()) {
        if (tick_ok) {
            TickTime tick; read_ticks(&tick);
            bool ok = disk_write(tick.uptime_ticks) && disk_read(&actual);
            result("I/O final tick readback", expect("I/O tick", actual, tick.uptime_ticks) && ok);
            value("I/O final tick=", tick.uptime_ticks);
        } else skip("I/O final tick", "tick clock failed; refusing to replace data with a false timestamp");
    }

    volatile uint16 __far *nv = (volatile uint16 __far *)0xA0000000UL;
    original = *nv;
    value("Memory NVRAM original=", original);
    if (!tick_ok) { skip("memory overwrite", "tick clock failed; preserve original data"); return; }
    uint16 pattern = (uint16)~original;
    *nv = pattern;
    bool ok = expect("memory complement", *nv, pattern);
    *nv = 0xA55A;
    ok = expect("memory pattern", *nv, 0xA55A) && ok;
    result("memory NVRAM overwrite/readback", ok);
    if (tick_ok) {
        TickTime tick; read_ticks(&tick);
        *nv = tick.uptime_ticks;
        result("memory final tick readback", expect("memory tick", *nv, tick.uptime_ticks));
        value("Memory final tick=", tick.uptime_ticks);
    } else skip("memory final tick", "tick clock failed; no valid timestamp");
}

void rom_main(void)
{
    log_text("SELFTEST BEGIN\nFirst word of disk and memory NVRAM will be overwritten.\n");
    setup_irqs();
    for (uint8 timer = 0; timer < 6; timer++) out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
    for (uint8 i = 0; i < 4; i++) out8(IO_REDSTONE_OUT(channels[i]), 0);
    terminal_test();
    clocks_and_yield();
    redstone();
    timers();
    sign_test();
    storage();
    heading("Cleanup and IRQ health");
    for (uint8 timer = 0; timer < 6; timer++) out8(IO_TIMER_CONTROL(timer), TIMER_CONTROL_RESET);
    for (uint8 i = 0; i < 4; i++) out8(IO_REDSTONE_OUT(channels[i]), 0);
    bool drained = tick_ok ? wait_ticks(4) && quiet() : quiet();
    result("IRQ queue drained without overflow", drained && !is_irq_overflow());
    if (unexpected) value("Unexpected IRQ ID=", unexpected_id);
    result("no unexpected IRQ IDs", unexpected == 0);
    disable_intr();
    log_text("SELFTEST DONE pass="); number(passed); log_text(" fail="); number(failed);
    log_text(" skip="); number(skipped); log_text("\n");
}
