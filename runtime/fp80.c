#include "runtime/fp80.h"

#include <string.h>

static uint16_t g_cw = FP80_CW_DEFAULT;

void fp80_set_cw(uint16_t cw) {
  static const uint_fast8_t rc[4] = {softfloat_round_near_even, softfloat_round_min,
                                     softfloat_round_max, softfloat_round_minMag};
  g_cw = cw;
  softfloat_roundingMode = rc[(cw >> 10) & 3];
  switch ((cw >> 8) & 3) { /* 00 = 24-bit, 10 = 53-bit, 11 = 64-bit significand */
    case 0: extF80_roundingPrecision = 32; break;
    case 2: extF80_roundingPrecision = 64; break;
    default: extF80_roundingPrecision = 80; break; /* 01 is reserved: treat as 64-bit */
  }
}

uint16_t fp80_get_cw(void) { return g_cw; }

Fp80 fp80_from_f64(double d) {
  float64_t f;
  memcpy(&f, &d, sizeof f);
  return f64_to_extF80(f);
}

Fp80 fp80_from_i32(int32_t v) { return i32_to_extF80(v); }

double fp80_to_f64(Fp80 x) {
  float64_t f = extF80_to_f64(x);
  double d;
  memcpy(&d, &f, sizeof d);
  return d;
}

Fp80 fp80_add(Fp80 a, Fp80 b) { return extF80_add(a, b); }
Fp80 fp80_sub(Fp80 a, Fp80 b) { return extF80_sub(a, b); }
Fp80 fp80_mul(Fp80 a, Fp80 b) { return extF80_mul(a, b); }
Fp80 fp80_div(Fp80 a, Fp80 b) { return extF80_div(a, b); }

int32_t fp80_to_i32(Fp80 x) {
  uint_fast8_t saved = softfloat_exceptionFlags;
  int_fast32_t v;
  softfloat_exceptionFlags = 0;
  v = extF80_to_i32(x, softfloat_roundingMode, false);
  if (softfloat_exceptionFlags & softfloat_flag_invalid) v = INT32_MIN;
  softfloat_exceptionFlags = saved;
  return (int32_t)v;
}

int16_t fp80_to_i16(Fp80 x) {
  int32_t v = fp80_to_i32(x);
  /* v == INT32_MIN is the indefinite (or a real -2^31): both are out of int16 range. */
  if (v < INT16_MIN || v > INT16_MAX) return INT16_MIN;
  return (int16_t)v;
}
