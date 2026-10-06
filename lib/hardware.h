#pragma once
#include <stdbool.h>
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef signed char  int8;
typedef signed short int16;

/* A000:0000 is persistent NVRAM; B000:0000 is volatile shared memory. Each bank is 64 KiB. */
typedef struct { uint8 data[16]; } Nvram;
#define NVRAM ((volatile Nvram __far *)0xA0000000UL)
#define NVRAM_BYTES ((volatile uint8 __far *)0xA0000000UL)
#define SHARED_MEMORY ((volatile uint8 __far *)0xB0000000UL)

#define IO_REDSTONE_IN(id) ((uint16)(id) << 1)
#define IO_REDSTONE_OUT(id) ((uint16)((id) << 1) + 1)

#define IO_SIGN(id) ((uint16)((id) * 0x100) + 0x0100)
#define IO_SIGN_FRONT_1(id) ((uint16)((id) * 0x100) + 0x0100)
#define IO_SIGN_FRONT_2(id) ((uint16)((id) * 0x100) + 0x0110)
#define IO_SIGN_FRONT_3(id) ((uint16)((id) * 0x100) + 0x0120)
#define IO_SIGN_FRONT_4(id) ((uint16)((id) * 0x100) + 0x0130)
#define IO_SIGN_BACK_1(id) ((uint16)((id) * 0x100) + 0x0140)
#define IO_SIGN_BACK_2(id) ((uint16)((id) * 0x100) + 0x0150)
#define IO_SIGN_BACK_3(id) ((uint16)((id) * 0x100) + 0x0160)
#define IO_SIGN_BACK_4(id) ((uint16)((id) * 0x100) + 0x0170)
#define IO_SIGN_CONTROL(id) ((uint16)((id) * 0x100) + 0x0180)

typedef enum {
    SIGN_FORMAT_BLACK = 0x0u,
    SIGN_FORMAT_DARK_BLUE = 0x1u,
    SIGN_FORMAT_DARK_GREEN = 0x2u,
    SIGN_FORMAT_DARK_AQUA = 0x3u,
    SIGN_FORMAT_DARK_RED = 0x4u,
    SIGN_FORMAT_DARK_PURPLE = 0x5u,
    SIGN_FORMAT_GOLD = 0x6u,
    SIGN_FORMAT_GRAY = 0x7u,
    SIGN_FORMAT_DARK_GRAY = 0x8u,
    SIGN_FORMAT_BLUE = 0x9u,
    SIGN_FORMAT_GREEN = 0xAu,
    SIGN_FORMAT_AQUA = 0xBu,
    SIGN_FORMAT_RED = 0xCu,
    SIGN_FORMAT_LIGHT_PURPLE = 0xDu,
    SIGN_FORMAT_YELLOW = 0xEu,
    SIGN_FORMAT_WHITE = 0xFu,
} IOSignColors;

#define SIGN_FORMAT_DISABLE_COLOR ((uint8)0x10)
#define SIGN_FORMAT_STRIKETHROUGH ((uint8)0x20)
#define SIGN_FORMAT_ITALIC ((uint8)0x40)
#define SIGN_FORMAT_UNDERLINED ((uint8)0x80)

#define SIGN_CONTROL_ALL ((uint8)0x0100)
#define SIGN_CONTROL_FRONT ((uint8)0x0200)
#define SIGN_CONTROL_BACK ((uint8)0x0400)

#define IO_TIMER(id) ((uint16)(id) * 0x0008 + 0x8100)
#define IO_TIMER_COUNTDOWN(id) ((uint16)(id) * 0x0008 + 0x8100)
#define IO_TIMER_WAIT(id) ((uint16)(id) * 0x0008 + 0x8102)
#define IO_TIMER_CONTROL(id) ((uint16)(id) * 0x0008 + 0x8104)

#define TIMER_CONTROL_ENABLE ((uint8)0x01)
#define TIMER_CONTROL_PERIODIC ((uint8)0x02)
#define TIMER_CONTROL_INTR ((uint8)0x04)
#define TIMER_CONTROL_RESET ((uint8)0x08)

#define TIMER_CONTROL_RES_1 ((uint8)0x00) // Realtime 1ms / MC 1 Tick
#define TIMER_CONTROL_RES_2 ((uint8)0x10) // Realtime 5ms / MC 5 Ticks
#define TIMER_CONTROL_RES_3 ((uint8)0x20) // Realtime 10ms / MC 10 Ticks
#define TIMER_CONTROL_RES_4 ((uint8)0x30) // Realtime 50ms / MC 20 Ticks

#define IO_TERMINAL ((uint16)0x8130)
#define IO_TERMINAL_SIZE ((uint16)32)
#define IO_TERMINAL_CONTROL ((uint16)0x8150)
#define TERMINAL_CONTROL_SEND ((uint8)0x01)

#define IO_NV_STORE ((uint16)0x8160)
#define IO_NV_STORE_DATA ((uint16)0x8160)
#define IO_NV_STORE_ADR ((uint16)0x8162)
#define IO_NV_STORE_CONTROL ((uint16)0x8164)

#define NV_STORE_CONTROL_READ ((uint8)0x01)
#define NV_STORE_CONTROL_WRITE ((uint8)0x02)
#define NV_STORE_CONTROL_STATUS ((uint8)0x04)

#define IO_IRQ ((uint16)0xFF00)
#define IO_IRQ_DEVICE_ID ((uint16)0xFF00)
#define IO_IRQ_DEVICE_TYPE ((uint16)0xFF01)
#define IO_IRQ_CONTROL ((uint16)0xFF02)

#define IRQ_CONTROL_EVENT_ACK ((uint8)0x01)
#define IRQ_CONTROL_EVENT_PENDING ((uint8)0x02)
#define IRQ_CONTROL_OVERFLOW ((uint8)0x04)

typedef enum {
    IRQ_DEVICE_REDSTONE = 0x00u,
    IRQ_DEVICE_SIGN = 0x01u,
    IRQ_DEVICE_REALTIMER = 0x02u,
    IRQ_DEVICE_TICKTIMER = 0x03u,
    /* Keep the enum wide enough to reject invalid types before byte packing. */
    IRQ_DEVICE_TYPE_INVALID = 0x100u
} IrqDeviceType;

static inline void enable_intr(void) {
    __asm__ volatile ("sti" ::: "memory");
}

static inline void disable_intr(void) {
    __asm__ volatile ("cli" ::: "memory");
}

static inline bool is_intr_enabled(void) {
    uint16 flags;
    __asm__ volatile ("pushf\n\tpop %0" : "=r"(flags));
    return (flags & 0x0200) != 0;
}

typedef struct {
    uint16 milliseconds;
    uint8 hour, minute, second;
    uint8 day_of_month, month; /* zero-based */
    uint16 year;
} Realtime;

typedef struct {
    uint16 minecraft_day;
    uint16 uptime_ticks; /* low 16 bits */
} TickTime;

Realtime get_realtime(void);
TickTime get_tick_time(void);

static inline void hlt(void) {
    __asm__ volatile ("hlt" ::: "memory");
}

/* Write 8 bits to an I/O port */
static inline void out8(uint16 port, uint8 value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/* Write 16 bits to an I/O port */
static inline void out16(uint16 port, uint16 value)
{
    __asm__ volatile (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/* Read 8 bits from an I/O port */
static inline uint8 in8(uint16 port)
{
    uint8 value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

/* Read 16 bits from an I/O port */
static inline uint16 in16(uint16 port)
{
    uint16 value;

    __asm__ volatile (
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

/* Returns 1 if storage is connected, otherwise 0. */
static inline bool is_nv_store_connected(void)
{
    return (in8(IO_NV_STORE_CONTROL) & NV_STORE_CONTROL_STATUS) != 0;
}

static inline bool is_irq_pending(void)
{
    return (in8(IO_IRQ_CONTROL) & IRQ_CONTROL_EVENT_PENDING) != 0;
}

static inline bool is_irq_overflow(void)
{
    return (in8(IO_IRQ_CONTROL) & IRQ_CONTROL_OVERFLOW) != 0;
}

static inline void ack_irq(void)
{
    out8(IO_IRQ_CONTROL, IRQ_CONTROL_EVENT_ACK);
}

static inline void clear_irq_overflow(void)
{
    out8(IO_IRQ_CONTROL, IRQ_CONTROL_OVERFLOW);
}

void log_text(const char *text);