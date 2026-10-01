#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/ia16-toolchain.cmake"
cmake --build build
free_hex=$(ia16-elf-nm build/rom.elf | awk '$3 == "__rom_free_bytes" { print $1 }')
rom_capacity=$((0xFFFF0 - 0xC0000))
used_tenths=$(((rom_capacity - 0x$free_hex) * 1000 / rom_capacity))
printf 'ROM remaining before reset vector: %d bytes (%d.%d%% used)\n' "0x$free_hex" "$((used_tenths / 10))" "$((used_tenths % 10))"
