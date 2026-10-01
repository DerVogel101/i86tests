.code16
.section .text.entry, "ax"
.global rom_start
rom_start:
    cli
    xor %ax, %ax
    mov %ax, %ss
    mov $0x8000, %sp
    mov %ax, %es

    push %cs
    pop %ds
    mov $__data_rom_offset, %si
    mov $__data_start, %di
    mov $__data_end, %cx
    sub %di, %cx
    cld
    rep movsb

    xor %ax, %ax
    mov %ax, %ds
    mov $__bss_start, %di
    mov $__bss_end, %cx
    sub %di, %cx
    rep stosb

    call rom_main
1:
    hlt
    jmp 1b

.section .reset, "ax"
    ljmp $0xC000, $0x0000