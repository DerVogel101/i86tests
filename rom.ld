SECTIONS {
    /* Code symbols are CS-relative offsets; bytes still load at C000:0000. */
    . = 0;
    .text : AT(0xC0000) { KEEP(*(.text.entry)) *(.text*) }

    ASSERT(SIZEOF(.text) <= 0x10000, "Small-model code exceeds one segment")
    .data 0x0600 : AT(LOADADDR(.text) + SIZEOF(.text)) {
        __data_start = .;
        *(.rodata*) *(.data*)
        __data_end = .;
    }
    __data_rom_offset = LOADADDR(.data) - 0xC0000;
    __rom_free_bytes = 0xFFFF0 - (LOADADDR(.data) + SIZEOF(.data));
    ASSERT(LOADADDR(.data) + SIZEOF(.data) <= 0xFFFF0, "ROM exceeds space before reset vector")

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*) *(COMMON)
        __bss_end = .;
    }

    . = 0xFFFF0;
    .reset : AT(0xFFFF0) { KEEP(*(.reset)) }
}