#include "info_layout.h"
#include "fonts.h"
#include "settings.h"
#include "solarUtils.h"
#include "text_metrics.h"
#include "utils.h"
#include "widgets.h"

#define INFO_SLOT_UPPER_SECONDARY 0
#define INFO_SLOT_UPPER_PRIMARY 1
#define INFO_SLOT_TIME 2
#define INFO_SLOT_LOWER_PRIMARY 3
#define INFO_SLOT_LOWER_SECONDARY 4
#define INFO_SLOT_COUNT 5

#define INFO_GROUP_TOP 0
#define INFO_GROUP_CENTER 1
#define INFO_GROUP_BOTTOM 2

// ============================================================
// Slot descriptor — one per visible row in the layout
// ============================================================
typedef struct {
  const char *text;
  GFont font;
  int height; // true pixel height from metrics
  int offset; // top dead-space offset from metrics
  GColor color;
  uint8_t group;
} SlotDescriptor;

typedef struct {
  uint8_t id;
  uint8_t group;
  uint8_t size;  // INFO_SIZE_S / _M / _L
} InfoLayoutItem;

// Caller-owned time string, set via info_layout_set_time_text().
static const char *s_time_text = "";
static bool s_quick_view_visible = false;

// Buffer storage for each of the 4 widget slots. Populated by
// info_layout_update_widgets() on the minute tick / settings change, then read
// by info_layout_update_proc() on every redraw — so redraws stay
// allocation-free.
static char widgetTextUS[WIDGET_TEXT_LEN]; // upper secondary
static char widgetTextUP[WIDGET_TEXT_LEN]; // upper primary
static char widgetTextLP[WIDGET_TEXT_LEN]; // lower primary
static char widgetTextLS[WIDGET_TEXT_LEN]; // lower secondary

void info_layout_set_time_text(const char *time_text) {
  s_time_text = time_text;
}

void info_layout_set_quick_view(bool visible) {
  s_quick_view_visible = visible;
}

void info_layout_update_widgets(void) {
  if (globalSettings.widgetUpperSecondary[0] != '\0') {
    widget_get_text(globalSettings.widgetUpperSecondary, widgetTextUS,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextUS[0] = '\0';
  }
  if (globalSettings.widgetUpperPrimary[0] != '\0') {
    widget_get_text(globalSettings.widgetUpperPrimary, widgetTextUP,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextUP[0] = '\0';
  }
  if (globalSettings.widgetLowerPrimary[0] != '\0') {
    widget_get_text(globalSettings.widgetLowerPrimary, widgetTextLP,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextLP[0] = '\0';
  }
  if (globalSettings.widgetLowerSecondary[0] != '\0') {
    widget_get_text(globalSettings.widgetLowerSecondary, widgetTextLS,
                    WIDGET_TEXT_LEN);
  } else {
    widgetTextLS[0] = '\0';
  }
}

static bool parse_info_layout(const char *layout, InfoLayoutItem *items,
                              int *count) {
  if (!layout || !layout[0]) return false;

  bool seen[INFO_SLOT_COUNT] = {false};
  bool has_time = false;
  int n = 0;
  const char *p = layout;

  while (*p) {
    if (n >= INFO_SLOT_COUNT) return false;
    if (p[0] < '0' || p[0] > '4' || p[1] != ':' || p[2] < '0' ||
        p[2] > '2') {
      return false;
    }

    uint8_t id = (uint8_t)(p[0] - '0');
    uint8_t group = (uint8_t)(p[2] - '0');
    if (seen[id]) return false;
    seen[id] = true;
    if (id == INFO_SLOT_TIME) has_time = true;

    p += 3;

    // Optional ":size" third field. Older layouts ("id:group") omit it; default
    // those lines to the medium size.
    uint8_t size = INFO_SIZE_DEFAULT;
    if (p[0] == ':' && p[1] >= '0' && p[1] <= '2') {
      size = (uint8_t)(p[1] - '0');
      p += 2;
    }

    items[n].id = id;
    items[n].group = group;
    items[n].size = size;
    n++;

    if (*p == ',') {
      p++;
    } else if (*p != '\0') {
      return false;
    }
  }

  if (!has_time || n == 0) return false;
  *count = n;
  return true;
}

static int get_info_layout(InfoLayoutItem *items) {
  int count = 0;
  if (parse_info_layout(globalSettings.infoLayout, items, &count)) {
    return count;
  }

  parse_info_layout(DEFAULT_INFO_LAYOUT, items, &count);
  return count;
}

static int slot_group_height(SlotDescriptor *slots, int count, uint8_t group) {
  int height = 0;
  int visible = 0;
  for (int i = 0; i < count; i++) {
    if (slots[i].group != group) continue;
    if (visible > 0) height += LINE_PADDING;
    height += slots[i].height;
    visible++;
  }
  return height;
}

// Draw text with a fixed halo so it stays readable over the globe.
// Always white text with a black outline.
static void draw_text_with_halo(GContext *ctx, const char *text, GFont font,
                                GRect frame, GColor color) {
  (void)color;
  GColor halo = GColorBlack;
  GColor text_color = GColorWhite;
  static const int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
  static const int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};
  graphics_context_set_text_color(ctx, halo);
  for (int k = 0; k < 8; k++) {
    graphics_draw_text(ctx, text, font,
                       GRect(frame.origin.x + dx[k], frame.origin.y + dy[k],
                             frame.size.w, frame.size.h),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter,
                       NULL);
  }
  graphics_context_set_text_color(ctx, text_color);
  graphics_draw_text(ctx, text, font, frame, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

static void draw_slot_group(GContext *ctx, SlotDescriptor *slots, int count,
                            uint8_t group, int y, int width) {
  for (int i = 0; i < count; i++) {
    SlotDescriptor *s = &slots[i];
    if (s->group != group) continue;
    draw_text_with_halo(ctx, s->text, s->font,
                        GRect(0, y - s->offset, width, s->height), s->color);
    y += s->height + LINE_PADDING;
  }
}

void info_layout_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  bool useNightColors = false;
  if (globalSettings.useNightTheme) {
    struct tm *timeInfo = getCurrentTime();
    int currentMinutes = timeInfo->tm_hour * 60 + timeInfo->tm_min;
    useNightColors = isNightTime(currentMinutes);
  }

  // Fonts are chosen per line from each entry's size (see info_font_for_size /
  // time_font_for_size); only the colours are decided up front.
  // ---- Color selection ----
  GColor timeColor =
      useNightColors ? globalSettings.nightTimeColor : globalSettings.timeColor;
  GColor primaryColor = useNightColors ? globalSettings.nightSubtextPrimaryColor
                                       : globalSettings.subtextPrimaryColor;
  GColor secondaryColor = useNightColors
                              ? globalSettings.nightSubtextSecondaryColor
                              : globalSettings.subtextSecondaryColor;

  // ---- Build ordered slot list from the configurable layout ----
#define MAX_SLOTS INFO_SLOT_COUNT
  InfoLayoutItem layout_items[INFO_SLOT_COUNT];
  int layout_count = get_info_layout(layout_items);
  SlotDescriptor slots[INFO_SLOT_COUNT];
  int num_slots = 0;
  bool time_pushed = false;

// Helper macro to push a slot
#define PUSH_SLOT(txt, fnt, h, off, col, grp)                                  \
  do {                                                                         \
    if (num_slots < MAX_SLOTS) {                                               \
      slots[num_slots].text = (txt);                                           \
      slots[num_slots].font = (fnt);                                           \
      slots[num_slots].height = (h);                                           \
      slots[num_slots].offset = (off);                                         \
      slots[num_slots].color = (col);                                          \
      slots[num_slots].group = (grp);                                          \
      num_slots++;                                                             \
    }                                                                          \
  } while (0)

  for (int i = 0; i < layout_count; i++) {
    uint8_t group = layout_items[i].group;
    uint8_t size = layout_items[i].size;
    int fh = 0, fo = 0;
    GFont font;
    switch (layout_items[i].id) {
      case INFO_SLOT_UPPER_SECONDARY:
        if (!s_quick_view_visible && widgetTextUS[0] != '\0') {
          font = info_font_for_size(size, &fh, &fo);
          PUSH_SLOT(widgetTextUS, font, fh, fo, secondaryColor, group);
        }
        break;
      case INFO_SLOT_UPPER_PRIMARY:
        if (widgetTextUP[0] != '\0') {
          font = info_font_for_size(size, &fh, &fo);
          PUSH_SLOT(widgetTextUP, font, fh, fo, primaryColor, group);
        }
        break;
      case INFO_SLOT_TIME:
        font = time_font_for_size(size, &fh, &fo);
        PUSH_SLOT(s_time_text, font, fh, fo, timeColor, group);
        time_pushed = true;
        break;
      case INFO_SLOT_LOWER_PRIMARY:
        if (widgetTextLP[0] != '\0') {
          font = info_font_for_size(size, &fh, &fo);
          PUSH_SLOT(widgetTextLP, font, fh, fo, primaryColor, group);
        }
        break;
      case INFO_SLOT_LOWER_SECONDARY:
        if (!s_quick_view_visible && widgetTextLS[0] != '\0') {
          font = info_font_for_size(size, &fh, &fo);
          PUSH_SLOT(widgetTextLS, font, fh, fo, secondaryColor, group);
        }
        break;
    }
  }

  if (!time_pushed) {
    int fh = 0, fo = 0;
    GFont font = time_font_for_size(INFO_SIZE_DEFAULT, &fh, &fo);
    PUSH_SLOT(s_time_text, font, fh, fo, timeColor, INFO_GROUP_CENTER);
  }

#undef PUSH_SLOT
#undef MAX_SLOTS

  int top_height = slot_group_height(slots, num_slots, INFO_GROUP_TOP);
  int center_height = slot_group_height(slots, num_slots, INFO_GROUP_CENTER);
  int bottom_height = slot_group_height(slots, num_slots, INFO_GROUP_BOTTOM);

  int top_y = 2;
  int bottom_y = bounds.size.h - bottom_height - 2;
  int center_top = top_y + top_height + (top_height > 0 ? LINE_PADDING : 0);
  int center_bottom = bottom_y - (bottom_height > 0 ? LINE_PADDING : 0);
  if (bottom_y < center_top) bottom_y = center_top;
  if (center_bottom < center_top) center_bottom = center_top;

  int center_area_height = center_bottom - center_top;
  int center_y = center_top;
  if (center_area_height > center_height) {
    center_y += (center_area_height - center_height) / 2;
  }

  draw_slot_group(ctx, slots, num_slots, INFO_GROUP_TOP, top_y, bounds.size.w);
  draw_slot_group(ctx, slots, num_slots, INFO_GROUP_CENTER, center_y,
                  bounds.size.w);
  draw_slot_group(ctx, slots, num_slots, INFO_GROUP_BOTTOM, bottom_y,
                  bounds.size.w);
}
