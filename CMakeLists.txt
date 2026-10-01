cmake_minimum_required(VERSION 3.20)
project(i86tests C ASM)

find_program(IA16_OBJCOPY ia16-elf-objcopy REQUIRED)

add_executable(rom main.c lib/hardware.c reset.S lib/strings.c
        lib/sleep.h
        lib/sleep.c
        lib/irq_handler.c
        lib/irq_handler.h)
set_target_properties(rom PROPERTIES SUFFIX ".elf")
target_compile_options(rom PRIVATE -march=i8086 -mcmodel=small -ffreestanding -fno-builtin -finput-charset=UTF-8 -fexec-charset=CP1252)
target_link_options(rom PRIVATE -nostdlib "-T${CMAKE_CURRENT_SOURCE_DIR}/rom.ld")
set_property(TARGET rom APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/rom.ld")

add_custom_command(TARGET rom POST_BUILD
        COMMAND "${IA16_OBJCOPY}" -O binary --gap-fill=0xff --pad-to=0x100000
                "$<TARGET_FILE:rom>" "${CMAKE_CURRENT_BINARY_DIR}/rom.bin"
        BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/rom.bin"
        VERBATIM)
