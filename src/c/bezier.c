#include <pebble.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "gcolor_definitions.h"
#include "trig.h"

#define HANDS_RESOLUTION 20
#define HANDS_SUBPIXELS 2
#define TIME_SUBSTEPS 6
#define TIME_SUBSTEPS_SECONDS (60 / TIME_SUBSTEPS)

static Window *s_main_window;
static Layer *hands_layer;

static int32_t hour_angle, minute_angle, second_angle, battery;

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

// static int32_t shl_signed(int32_t v, int shift) {
  // return ((v & ~0b10000000000000000000000000000000) << shift) | (v & 0b10000000000000000000000000000000);
// }
//
// static int32_t shr_signed(int32_t v, int shift) {
  // return ((v & ~0b10000000000000000000000000000000) >> shift) | (v & 0b10000000000000000000000000000000);
// }

static void draw_hands(GRect layer_bounds, GContext *ctx) {
  GPoint long_pos = from_angle(minute_angle - HALF_PI_LU, 50 * HANDS_SUBPIXELS);
  // long_pos.x <<= 4;
  // long_pos.y <<= 4;
  GPoint short_pos = from_angle(hour_angle - HALF_PI_LU, 35 * HANDS_SUBPIXELS);
  // short_pos.x <<= 4;
  // short_pos.y <<= 4;
  GPoint last_pos = GPointZero;
  graphics_context_set_stroke_width(ctx, 2);
  // GPoint this_pos;
  // GPoint p1;
  // GPoint p2;
  for (int i = 0; i < HANDS_RESOLUTION + 1; i++) {
    GPoint this_pos = bezier(long_pos, GPointZero, short_pos, i, HANDS_RESOLUTION);
    this_pos.x = (this_pos.x + (HANDS_SUBPIXELS + 1) / 2) / HANDS_SUBPIXELS;
    this_pos.y = (this_pos.y + (HANDS_SUBPIXELS + 1) / 2) / HANDS_SUBPIXELS;
    // bool in_battery_range = i * 100 / HANDS_RESOLUTION > battery;
    if (i == 0) {
      goto end;
    }

    graphics_context_set_stroke_color(ctx, GColorWhite);

    // APP_LOG(APP_LOG_LEVEL_DEBUG, "long pos x pre shift %d", this_pos.x);
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "long pos y pre shift %d", this_pos.y);
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "long pos x pos shift %d", this_pos.x);
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "long pos y pos shift %d", this_pos.y);
    graphics_draw_line(ctx, center_gpoint(last_pos, layer_bounds),
                       center_gpoint(this_pos, layer_bounds));
    graphics_context_set_fill_color(ctx, GColorCadetBlue);
    graphics_fill_circle(ctx, center_gpoint(last_pos, layer_bounds), 1);

  end:
    last_pos = this_pos;
  }
}

static void hands_update_handler(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);

  GRect layer_bounds_2 = layer_bounds;
  layer_bounds_2.size.w -= 10;
  layer_bounds_2.size.h -= 10;
  layer_bounds_2.origin.x += 5;
  layer_bounds_2.origin.y += 5;
  graphics_context_set_stroke_width(ctx, 3);
  switch (battery / 10) {
  case 1:
  case 2:
    graphics_context_set_stroke_color(ctx, GColorFolly);
    break;
  case 3:
  case 4:
    graphics_context_set_stroke_color(ctx, GColorChromeYellow);
    break;
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
    graphics_context_set_stroke_color(ctx, GColorScreaminGreen);
  }
  graphics_draw_arc(ctx, layer_bounds_2, GOvalScaleModeFitCircle,
                    -((battery + 5) * 12 / 100) * TRIG_MAX_ANGLE / 12, 0);

  // draw ticks
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 6);
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
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 2);
  for (int32_t i = 0; i < 12; i++) {
    int32_t angle = i * TRIG_MAX_ANGLE / 12;
    graphics_draw_line(
        ctx,
        center_gpoint(from_angle(angle, layer_bounds.size.w / 2 - 10),
                      layer_bounds),
        center_gpoint(from_angle(angle, layer_bounds.size.w / 2 - 5),
                      layer_bounds));
  }

  // draw hands
  draw_hands(layer_bounds, ctx);
}

static void time_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // (tm_min + tm_sec / 60) * max / 60
  // (tm_min * 60 + tm_sec) * max / 60
  minute_angle =
      (tick_time->tm_min * 60 + tick_time->tm_sec) * TRIG_MAX_ANGLE / 3600;
  hour_angle =
      (tick_time->tm_hour % 12) * TRIG_MAX_ANGLE / 12 + minute_angle / 12;
  // minute = (float)tick_time->tm_sec * TAU / 30.0;
  // hour = (float)tick_time->tm_sec * TAU / 10.0;
  if (tick_time->tm_sec % TIME_SUBSTEPS_SECONDS == TIME_SUBSTEPS_SECONDS - 1) {
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "redrawing %d %d %d", (int)(minute_angle *
    // 360 / TRIG_MAX_ANGLE), (int)(hour_angle * 360 / TRIG_MAX_ANGLE),
    // tick_time->tm_sec);
    layer_mark_dirty(hands_layer);
  }
}

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
  time_tick_handler(current_time, HOUR_UNIT & SECOND_UNIT & MINUTE_UNIT);

  battery_handler(battery_state_service_peek());
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});

  window_stack_push(s_main_window, true);
  layer_set_update_proc(hands_layer, hands_update_handler);
  // layer_set_update_proc(seconds_layer, seconds_update_handler);
  tick_timer_service_subscribe(SECOND_UNIT, time_tick_handler);
  battery_state_service_subscribe(battery_handler);
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
