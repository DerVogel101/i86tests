// string.h
#pragma once
#include "hardware.h"

unsigned int str_len(const char* str);
int str_equal(const char* a, const char* b);
void str_copy(char* dst, const char* src);
void str_concat(char* dst, const char* src);

void uint16_to_str(uint16 value, char *buffer);