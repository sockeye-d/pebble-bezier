#pragma once

#include <pebble.h>

#define HALF_PI 1.57079632675
#define PI 3.1415926535
#define TAU 6.283185307

#define TAU_LU TRIG_MAX_ANGLE
#define PI_LU (TAU_LU >> 1)
#define HALF_PI_LU (TAU_LU >> 2)

#define PHI 2.39996322972865332

float pebble_sin(float angle) {
  return (float)sin_lookup(angle * TRIG_MAX_ANGLE / TAU) / TRIG_MAX_RATIO;
}

float pebble_cos(float angle) {
  return (float)cos_lookup(angle * TRIG_MAX_ANGLE / TAU) / TRIG_MAX_RATIO;
}

float pebble_tan(float angle) {
  return pebble_sin(angle) / pebble_cos(angle);
}
