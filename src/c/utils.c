#include "utils.h"
#include <pebble.h>

void to_uppercase(char *str) {
  while (*str) {
    *str = toupper((unsigned char)*str);
    str++;
  }
}

struct tm *getCurrentTime() {
  static struct tm timeInfo;
  time_t rawTime;

  time(&rawTime);
  timeInfo = *localtime(&rawTime);

  return &timeInfo;
}

uint8_t pixel4_get(uint8_t byte, int sub) {
  return sub == 0 ? (byte >> 4) & 0x0F : byte & 0x0F;
}

uint8_t pixel4_set(uint8_t byte, int sub, uint8_t index) {
  if (sub == 0) return (uint8_t)((byte & 0x0F) | ((index & 0x0F) << 4));
  return (uint8_t)((byte & 0xF0) | (index & 0x0F));
}
