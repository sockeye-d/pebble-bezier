#include <pebble.h>
#include <stdint.h>
#include <stdio.h>

#include "gcolor_definitions.h"
#include "trig.h"

#define HANDS_RESOLUTION 10
#define TIME_SUBSTEPS 4

static Window *s_main_window;
static Layer *seconds_layer;
static Layer *hands_layer;

static GBitmap *hands_framebuffer;

static int32_t hour_angle, minute_angle, second_angle, battery;
static uint8_t seconds_extra;
static tm last_tm;

static AppTimer *redraw_timer_handle;

static void main_window_unload(Window *window) { layer_destroy(hands_layer); }

static void battery_handler(BatteryChargeState battery_state) {
  battery = battery_state.charge_percent;
  layer_mark_dirty(hands_layer);
}

static GPoint center_gpoint(GPoint p, GRect bounds) {
  return GPoint(p.x + bounds.size.w / 2, p.y + bounds.size.h / 2);
}

const float inv_hands_resolution = 1.0 / (HANDS_RESOLUTION - 1);

GPoint bezier(GPoint start, GPoint control, GPoint end, int t, int max_t) {
  int inv_t = max_t - t;
  // return (vec2){
  // inv_t * inv_t * inv_t * start.x +
  // ((3.0 * inv_t * inv_t * t) + (3.0 * inv_t * t * t)) * control.x +
  // t * t * t * end.x,
  // inv_t * inv_t * inv_t * start.y +
  // ((3.0 * inv_t * inv_t * t) + (3.0 * inv_t * t * t)) * control.y +
  // t * t * t * end.y};
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

static void hands_update_handler(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);

  if (hands_framebuffer) {
    // TODO: do the thing that draws the bitmap
    return;
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

  layer_bounds.size.w -= 10;
  layer_bounds.size.h -= 10;
  layer_bounds.origin.x += 5;
  layer_bounds.origin.y += 5;
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
  graphics_draw_arc(ctx, layer_bounds, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(-battery * 36 / 10), 0);

  // draw hands
  GPoint long_pos = from_angle(minute_angle - HALF_PI_LU, 50);
  GPoint short_pos = from_angle(hour_angle - HALF_PI_LU, 35);
  GPoint last_pos = GPointZero;
  graphics_context_set_stroke_width(ctx, 4);
  for (int i = 0; i < HANDS_RESOLUTION + 1; i++) {
    GPoint this_pos =
        bezier(long_pos, GPointZero, short_pos, i, HANDS_RESOLUTION);
    bool in_battery_range = i * 100 / HANDS_RESOLUTION > battery;
    if (i == 0) {
      goto end;
    }

    // if (in_battery_range) {
    // graphics_context_set_stroke_color(ctx, GColorLightGray);
    // } else {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    // }
    graphics_draw_line(ctx, center_gpoint(last_pos, layer_bounds),
                       center_gpoint(this_pos, layer_bounds));

  end:
    last_pos = this_pos;
  }

  if (hands_framebuffer) {
    gbitmap_destroy(hands_framebuffer);
  }

  GBitmap *framebuffer = graphics_capture_frame_buffer(ctx);

  hands_framebuffer = gbitmap_create_with_data(gbitmap_get_data(framebuffer));
}

static void seconds_update_handler(Layer *layer, GContext *ctx) {
  GRect layer_bounds = layer_get_bounds(layer);
  GPoint p1 = center_gpoint(
      from_angle(second_angle - HALF_PI_LU, layer_bounds.size.w / 2 - 10),
      layer_bounds);
  GPoint p2 = center_gpoint(
      from_angle(second_angle - HALF_PI_LU, layer_bounds.size.w / 2 - 8),
      layer_bounds);
  graphics_context_set_stroke_color(ctx, GColorVividCerulean);
  graphics_context_set_fill_color(ctx, GColorVividCerulean);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, p1, p2);
}

void recalculate_seconds_angle() {
  second_angle = (last_tm.tm_sec * TIME_SUBSTEPS + seconds_extra) *
                 TRIG_MAX_ANGLE / (60 * TIME_SUBSTEPS);
  minute_angle = last_tm.tm_min * 60 + last_tm.tm_sec * TRIG_MAX_ANGLE / 60 + second_angle / 60;
}

static void redraw_timer_handler(void *data) {
  seconds_extra++;
  recalculate_seconds_angle();
  layer_mark_dirty(seconds_layer);
  redraw_timer_handle =
      app_timer_register(1000 / TIME_SUBSTEPS, redraw_timer_handler, NULL);
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  hands_layer = layer_create(bounds);
  seconds_layer = layer_create(bounds);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, seconds_layer);
  layer_add_child(window_layer, hands_layer);

  layer_mark_dirty(hands_layer);
}

static void time_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  last_tm = *tick_time;
  seconds_extra = 0;
  recalculate_seconds_angle();
  hour_angle =
      (tick_time->tm_hour % 12) * TRIG_MAX_ANGLE / 12 + minute_angle / 12;
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "hour: %d", hour_angle * 180 / PI);
  // minute = (float)tick_time->tm_sec * TAU / 30.0;
  // hour = (float)tick_time->tm_sec * TAU / 10.0;

  if (units_changed & SECOND_UNIT) {
    if (redraw_timer_handle)
      app_timer_cancel(redraw_timer_handle);
    redraw_timer_handler(NULL);
  }

  if (units_changed & HOUR_UNIT || units_changed & MINUTE_UNIT) {
    layer_mark_dirty(hands_layer);
  }
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});

  window_stack_push(s_main_window, false);
  layer_set_update_proc(hands_layer, hands_update_handler);
  layer_set_update_proc(seconds_layer, seconds_update_handler);
  tick_timer_service_subscribe(SECOND_UNIT, time_tick_handler);
  battery_state_service_subscribe(battery_handler);

  time_t now = time(NULL);
  tm *current_time = localtime(&now);
  time_tick_handler(current_time, HOUR_UNIT & SECOND_UNIT & MINUTE_UNIT);

  battery_handler(battery_state_service_peek());
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
