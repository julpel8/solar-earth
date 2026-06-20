#include "fonts.h"
#include "settings.h"
#include "text_metrics.h"

GFont time_font_for_size(uint8_t size, int *height, int *offset) {
  switch (size) {
    case INFO_SIZE_S:
      *height = FONT_TIME_SIZE_S_HEIGHT;
      *offset = FONT_TIME_SIZE_S_OFFSET;
      return fonts_get_system_font(FONT_TIME_SIZE_S);
    case INFO_SIZE_L:
      *height = FONT_TIME_SIZE_L_HEIGHT;
      *offset = FONT_TIME_SIZE_L_OFFSET;
      return fonts_get_system_font(FONT_TIME_SIZE_L);
    default:
      *height = FONT_TIME_SIZE_M_HEIGHT;
      *offset = FONT_TIME_SIZE_M_OFFSET;
      return fonts_get_system_font(FONT_TIME_SIZE_M);
  }
}

GFont info_font_for_size(uint8_t size, int *height, int *offset) {
  switch (size) {
    case INFO_SIZE_S:
      *height = FONT_INFO_SIZE_S_HEIGHT;
      *offset = FONT_INFO_SIZE_S_OFFSET;
      return fonts_get_system_font(FONT_INFO_SIZE_S);
    case INFO_SIZE_L:
      *height = FONT_INFO_SIZE_L_HEIGHT;
      *offset = FONT_INFO_SIZE_L_OFFSET;
      return fonts_get_system_font(FONT_INFO_SIZE_L);
    default:
      *height = FONT_INFO_SIZE_M_HEIGHT;
      *offset = FONT_INFO_SIZE_M_OFFSET;
      return fonts_get_system_font(FONT_INFO_SIZE_M);
  }
}

GFont ampm_font(int *height, int *offset) {
  *height = FONT_INFO_SIZE_S_HEIGHT;
  *offset = FONT_INFO_SIZE_S_OFFSET;
  return fonts_get_system_font(FONT_INFO_SIZE_S);
}
