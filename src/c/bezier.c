#include <pebble.h>
// #include <stdbool.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <string.h>

// #include "gcolor_definitions.h"
// #include "message_keys.auto.h"
#include "gcolor_definitions.h"
#include "message_keys.auto.h"
#include "trig.h"

// #define HANDS_RESOLUTION 50
#define TIME_SUBSTEPS 6
#define TIME_SUBSTEPS_SECONDS (60 / TIME_SUBSTEPS)
// #define DEBUG_HANDS
#define SETTINGS_KEY 1

static Window *s_main_window;
static Layer *hands_layer;

static int32_t hour_angle, minute_angle, second_angle, battery;

typedef struct ClaySettings {
  GColor background_color;

  int hour_hand_length;
  int minute_hand_length;
  int hands_thickness;
  GColor hands_color;

  int ticks_thickness;
  int ticks_overlay_thickness;
  GColor ticks_color;

  int battery_indicator_thickness;
  GColor high_battery_color;
  GColor medium_battery_color;
  GColor low_battery_color;
} ClaySettings;

static ClaySettings settings;

static void default_settings() {
  settings.background_color = GColorBlack;

  settings.hour_hand_length = 35;
  settings.minute_hand_length = 50;
  settings.hands_thickness = 2;
  settings.hands_color = GColorWhite;

  settings.ticks_thickness = 2;
  settings.ticks_overlay_thickness = 10;
  settings.ticks_color = PBL_IF_BW_ELSE(GColorWhite, GColorDarkGray);

  settings.battery_indicator_thickness = 3;
  settings.high_battery_color = PBL_IF_BW_ELSE(GColorWhite, GColorScreaminGreen);
  settings.medium_battery_color = PBL_IF_BW_ELSE(GColorWhite, GColorChromeYellow);
  settings.low_battery_color = PBL_IF_BW_ELSE(GColorWhite, GColorFolly);
}

static void load_settings() {
  default_settings();
  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void save_settings() {
  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

#define LOAD_INT(name)                                                         \
  Tuple *name = dict_find(iter, MESSAGE_KEY_##name);                           \
  if (name)                                                                    \
  settings.name = name->value->int32

#define LOAD_COLOR(name)                                                       \
  Tuple *name = dict_find(iter, MESSAGE_KEY_##name);                           \
  if (name)                                                                    \
  settings.name = GColorFromHEX(name->value->int32)

#define LOAD_BOOL(name)                                                        \
  Tuple *name = dict_find(iter, MESSAGE_KEY_##name);                           \
  if (name)                                                                    \
  settings.name = name->value->int32 == 1

// AppMessage receive handler
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  LOAD_COLOR(background_color);

  LOAD_INT(hour_hand_length);
  LOAD_INT(minute_hand_length);
  LOAD_INT(hands_thickness);
  LOAD_COLOR(hands_color);

  LOAD_INT(ticks_thickness);
  LOAD_INT(ticks_overlay_thickness);
  LOAD_COLOR(ticks_color);

  LOAD_INT(battery_indicator_thickness);
  LOAD_COLOR(high_battery_color);
  LOAD_COLOR(medium_battery_color);
  LOAD_COLOR(low_battery_color);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Received settings!");

  save_settings();

  window_set_background_color(s_main_window, settings.background_color);
  layer_mark_dirty(hands_layer);
}

static void main_window_unload(Window *window) { layer_destroy(hands_layer); }

static void battery_handler(BatteryChargeState battery_state) {
  battery = battery_state.charge_percent;
  layer_mark_dirty(hands_layer);
}

static GPoint center_gpoint(GPoint p, GRect bounds) {
  return GPoint(p.x + bounds.size.w / 2, p.y + bounds.size.h / 2);
}

GPoint bezier(GPoint start, GPoint control, GPoint end, int t, int max_t) {
  int inv_t = max_t - t;
  return GPoint(
      (inv_t * inv_t * start.x + 2 * inv_t * t * control.x + t * t * end.x) /
          (max_t * max_t),
      (inv_t * inv_t * start.y + 2 * inv_t * t * control.y + t * t * end.y) /
          (max_t * max_t));
}

GPoint from_angle(int32_t angle, int32_t radius) {
  return GPoint(cos_lookup(angle) * radius / TRIG_MAX_RATIO,
                sin_lookup(angle) * radius / TRIG_MAX_RATIO);
}

int32_t umod(int32_t x, int32_t y) { return ((x % y) + y) % y; }

int32_t angle_difference(int32_t a, int32_t b) {
  return umod(b - a - PI_LU, TAU_LU) - PI_LU;
}

static void draw_hands(GRect layer_bounds, GContext *ctx) {
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "%ld", TRIGANGLE_TO_DEG(minute_angle));
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "%ld", TRIGANGLE_TO_DEG(hour_angle));

  GPoint long_pos =
      from_angle(minute_angle - HALF_PI_LU, settings.minute_hand_length);
  GPoint short_pos =
      from_angle(hour_angle - HALF_PI_LU, settings.hour_hand_length);
  int resolution =
      6 + (long_pos.x * short_pos.x + long_pos.y * short_pos.y) * 4 /
              (settings.hour_hand_length * settings.minute_hand_length);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%d", resolution);
  GPoint this_pos;
  GPoint last_pos = GPointZero;
  graphics_context_set_stroke_width(ctx, settings.hands_thickness);
  graphics_context_set_stroke_color(ctx, settings.hands_color);
  for (int i = 0; i < resolution + 1; i++) {
    this_pos = bezier(long_pos, GPointZero, short_pos, i, resolution);
    if (i == 0) {
      goto end;
    }

    graphics_draw_line(ctx, center_gpoint(last_pos, layer_bounds),
                       center_gpoint(this_pos, layer_bounds));

  end:
    last_pos = this_pos;
  }
}

static void hands_update_handler(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);

  GRect layer_bounds_2 = layer_bounds;
  layer_bounds_2.size.w -= 14;
  layer_bounds_2.size.h -= 14;
  layer_bounds_2.origin.x += 7;
  layer_bounds_2.origin.y += 7;
  graphics_context_set_stroke_width(ctx, settings.battery_indicator_thickness);
  switch (battery / 10) {
  case 1:
  case 2:
    graphics_context_set_stroke_color(ctx, settings.low_battery_color);
    break;
  case 3:
  case 4:
    graphics_context_set_stroke_color(ctx, settings.medium_battery_color);
    break;
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
    graphics_context_set_stroke_color(ctx, settings.high_battery_color);
  }
  graphics_draw_arc(ctx, layer_bounds_2, GOvalScaleModeFitCircle,
                    -((battery + 5) * 12 / 100) * TRIG_MAX_ANGLE / 12, 0);

  // draw masking ticks
  graphics_context_set_stroke_color(ctx, settings.background_color);
  graphics_context_set_stroke_width(ctx, settings.ticks_overlay_thickness);
  for (int32_t i = 0; i < 12; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 12;
    graphics_draw_line(
        ctx,
        center_gpoint(from_angle(angle, layer_bounds.size.w / 2 - 10),
                      layer_bounds),
        center_gpoint(from_angle(angle, layer_bounds.size.w / 2),
                      layer_bounds));
  }

  // draw ticks
  graphics_context_set_stroke_color(ctx, settings.ticks_color);
  graphics_context_set_stroke_width(ctx, settings.ticks_thickness);
  for (int32_t i = 0; i < 12; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 12;
    graphics_draw_line(
        ctx,
        center_gpoint(from_angle(angle, layer_bounds.size.w / 2 - 10),
                      layer_bounds),
        center_gpoint(from_angle(angle, layer_bounds.size.w / 2 - 4),
                      layer_bounds));
  }

  // draw hands
  draw_hands(layer_bounds, ctx);
}

static void time_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // #ifdef DEBUG_HANDS
  //   // minute_angle = (minute_angle + DEG_TO_TRIGANGLE(2)) % TRIG_MAX_ANGLE;
  //   // hour_angle = (hour_angle + DEG_TO_TRIGANGLE(1)) % TRIG_MAX_ANGLE;
  //   // layer_mark_dirty(hands_layer);
  //   tick_time->tm_min = tick % 60;
  //   tick_time->tm_hour = tick / 60 % 24;
  // #endif
  // (tm_min + tm_sec / 60) * max / 60
  // (tm_min * 60 + tm_sec) * max / 60
  // minute = (float)tick_time->tm_sec * TAU / 30.0;
  // hour = (float)tick_time->tm_sec * TAU / 10.0;
  if (units_changed & MINUTE_UNIT ||
      tick_time->tm_sec % TIME_SUBSTEPS_SECONDS == TIME_SUBSTEPS_SECONDS - 1) {
    minute_angle =
        (tick_time->tm_min * 60 + tick_time->tm_sec) * TRIG_MAX_ANGLE / 3600;
    minute_angle %= TAU_LU;
    hour_angle =
        (tick_time->tm_hour % 12) * TRIG_MAX_ANGLE / 12 + minute_angle / 12;
    hour_angle %= TAU_LU;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "redrawing %d %d %d", (int)(minute_angle *
    // 360 / TRIG_MAX_ANGLE), (int)(hour_angle * 360 / TRIG_MAX_ANGLE),
    // tick_time->tm_sec);
    layer_mark_dirty(hands_layer);
  }
}

#ifdef DEBUG_HANDS
static int tick = 0;
static void app_timer_callback(void *data) {
  tick += 1;
  tm *tick_time = malloc(sizeof(tm));
  tick_time->tm_sec = TIME_SUBSTEPS_SECONDS - 1;
  tick_time->tm_min = tick % 60;
  tick_time->tm_hour = tick / 60 % 24;
  time_tick_handler(tick_time, 0);
  free(tick_time);
  app_timer_register(50, app_timer_callback, NULL);
}
#endif

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  hands_layer = layer_create(bounds);
  // seconds_layer = layer_create(bounds);

  // Add it as a child layer to the Window's root layer
  // layer_add_child(window_layer, seconds_layer);
  layer_add_child(window_layer, hands_layer);

  layer_mark_dirty(hands_layer);

  time_t now = time(NULL);
  tm *current_time = localtime(&now);
  time_tick_handler(current_time, HOUR_UNIT | SECOND_UNIT | MINUTE_UNIT);

  battery_handler(battery_state_service_peek());

#ifdef DEBUG_HANDS
  app_timer_callback(NULL);
#endif
}

static void init() {
  load_settings();
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, settings.background_color);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});

  window_stack_push(s_main_window, true);
  layer_set_update_proc(hands_layer, hands_update_handler);
  // layer_set_update_proc(seconds_layer, seconds_update_handler);
#ifndef DEBUG_HANDS
  tick_timer_service_subscribe(SECOND_UNIT, time_tick_handler);
#endif
  battery_state_service_subscribe(battery_handler);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(dict_calc_buffer_size(12), 0);
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
