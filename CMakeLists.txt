cmake_minimum_required(VERSION 3.20)
project(i86tests C ASM)

find_program(IA16_OBJCOPY ia16-elf-objcopy REQUIRED)

add_executable(rom main.c lib/hardware.c reset.S lib/strings.c
        lib/sleep.h
        lib/sleep.c
        lib/irq_handler.c
        lib/irq_handler.h lib/keyboard.c lib/punchcard.c)
add_executable(irq_handler_test EXCLUDE_FROM_ALL
        tests/irq_handler_test.c lib/irq_handler.c lib/hardware.c lib/keyboard.c lib/sleep.c reset.S)

add_executable(self_test self_test.c lib/hardware.c lib/strings.c lib/irq_handler.c lib/keyboard.c reset.S)
add_executable(keyboard_test EXCLUDE_FROM_ALL tests/keyboard_test.c lib/hardware.c lib/irq_handler.c lib/keyboard.c reset.S)
add_executable(keyboard_editor keyboard_editor.c lib/hardware.c lib/irq_handler.c lib/keyboard.c reset.S)
add_executable(punchcard_test EXCLUDE_FROM_ALL tests/punchcard_test.c lib/hardware.c lib/irq_handler.c lib/punchcard.c reset.S)

foreach(rom_target IN ITEMS rom irq_handler_test self_test keyboard_test keyboard_editor punchcard_test)
    set_target_properties(${rom_target} PROPERTIES SUFFIX ".elf")
    target_compile_options(${rom_target} PRIVATE -march=i8086 -mcmodel=small -ffreestanding -fno-builtin -finput-charset=UTF-8 -fexec-charset=CP1252)
    # Code and data have separate segments but overlapping logical offsets.
    target_link_options(${rom_target} PRIVATE -nostdlib "-Wl,--no-check-sections" "-T${CMAKE_CURRENT_SOURCE_DIR}/rom.ld")
    set_property(TARGET ${rom_target} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/rom.ld")
    add_custom_command(TARGET ${rom_target} POST_BUILD
        COMMAND "${IA16_OBJCOPY}" -O binary --gap-fill=0xff --pad-to=0x100000
                "$<TARGET_FILE:${rom_target}>" "${CMAKE_CURRENT_BINARY_DIR}/${rom_target}.bin"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${rom_target}.bin"
        VERBATIM)
endforeach()
