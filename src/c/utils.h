#pragma once

#include <ctype.h>
#include <pebble.h>

void to_uppercase(char *str);
struct tm *getCurrentTime();

// 4-bit (16-entry palette) pixel packing: two pixels per byte, sub 0 = high
// nibble, sub 1 = low nibble.
uint8_t pixel4_get(uint8_t byte, int sub);
uint8_t pixel4_set(uint8_t byte, int sub, uint8_t index);
