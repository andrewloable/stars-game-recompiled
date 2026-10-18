#include "runtime/fp80.h"

#include <string.h>

static uint16_t g_cw = FP80_CW_DEFAULT;
static uint16_t g_exc;
static Fp80TrapFn g_trap;

void fp80_set_trap_handler(Fp80TrapFn fn) { g_trap = fn; }
void fp80_clear_exceptions(void) { g_exc = 0; }
uint16_t fp80_exceptions(void) { return g_exc; }

/* Folds SoftFloat's flags (cleared before the operation) and a denormal-operand flag into
 * g_exc, and calls the trap handler for every raised flag that is unmasked in g_cw. */
static void raise(uint_fast8_t sf, uint16_t extra) {
  uint16_t e = extra;
  if (sf & softfloat_flag_invalid) e |= FP80_EXC_IE;
  if (sf & softfloat_flag_infinite) e |= FP80_EXC_ZE;
  if (sf & softfloat_flag_overflow) e |= FP80_EXC_OE;
  if (sf & softfloat_flag_underflow) e |= FP80_EXC_UE;
  if (sf & softfloat_flag_inexact) e |= FP80_EXC_PE;
  g_exc |= e;
  if (e & ~g_cw & 0x3Fu) {
    if (g_trap) g_trap((uint16_t)(e & ~g_cw & 0x3Fu));
  }
}

/* DE: a nonzero operand with a zero biased exponent (denormal / pseudo-denormal). */
static uint16_t de(Fp80 x) {
  /* SoftFloat is built little-endian (platform.h, which users of softfloat_types.h do not
   * see): the 64-bit significand is at bytes 0-7, sign and exponent at bytes 8-9. */
  uint64_t signif;
  uint16_t se;
  memcpy(&signif, &x, 8);
  memcpy(&se, (const uint8_t *)&x + 8, 2);
  return ((se & 0x7FFF) == 0 && signif != 0) ? FP80_EXC_DE : 0;
}
/* A binary operation: clear SoftFloat's flags, run, fold, restore the caller's flags. */
#define BINOP(fn, a, b)                                  \
  do {                                                   \
    uint_fast8_t saved = softfloat_exceptionFlags;       \
    Fp80 r_;                                             \
    softfloat_exceptionFlags = 0;                        \
    r_ = fn(a, b);                                       \
    raise(softfloat_exceptionFlags, (uint16_t)(de(a) | de(b))); \
    softfloat_exceptionFlags = saved;                    \
    return r_;                                           \
  } while (0)

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
  uint64_t u;
  Fp80 r;
  memcpy(&f, &d, sizeof f);
  memcpy(&u, &d, sizeof u);
  r = f64_to_extF80(f);
  /* FLD m64 of a denormal double raises DE (the extended result is normal). */
  raise(0, ((u >> 52) & 0x7FF) == 0 && (u & 0xFFFFFFFFFFFFFull) ? FP80_EXC_DE : 0);
  return r;
}

Fp80 fp80_from_i32(int32_t v) { return i32_to_extF80(v); }

double fp80_to_f64(Fp80 x) {
  uint_fast8_t saved = softfloat_exceptionFlags;
  float64_t f;
  double d;
  softfloat_exceptionFlags = 0;
  f = extF80_to_f64(x);
  raise(softfloat_exceptionFlags, de(x));
  softfloat_exceptionFlags = saved;
  memcpy(&d, &f, sizeof d);
  return d;
}

Fp80 fp80_add(Fp80 a, Fp80 b) { BINOP(extF80_add, a, b); }
Fp80 fp80_sub(Fp80 a, Fp80 b) { BINOP(extF80_sub, a, b); }
Fp80 fp80_mul(Fp80 a, Fp80 b) { BINOP(extF80_mul, a, b); }
Fp80 fp80_div(Fp80 a, Fp80 b) { BINOP(extF80_div, a, b); }

Fp80 fp80_sqrt(Fp80 a) {
  uint_fast8_t saved = softfloat_exceptionFlags;
  Fp80 r;
  softfloat_exceptionFlags = 0;
  r = extF80_sqrt(a);
  raise(softfloat_exceptionFlags, de(a));
  softfloat_exceptionFlags = saved;
  return r;
}

int32_t fp80_to_i32(Fp80 x) {
  uint_fast8_t saved = softfloat_exceptionFlags;
  int_fast32_t v;
  softfloat_exceptionFlags = 0;
  v = extF80_to_i32(x, softfloat_roundingMode, false);
  if (softfloat_exceptionFlags & softfloat_flag_invalid) v = INT32_MIN;
  raise(softfloat_exceptionFlags, de(x));
  softfloat_exceptionFlags = saved;
  return (int32_t)v;
}

int16_t fp80_to_i16(Fp80 x) {
  int32_t v = fp80_to_i32(x);
  /* v == INT32_MIN is the indefinite (or a real -2^31): both are out of int16 range. */
  if (v < INT16_MIN || v > INT16_MAX) return INT16_MIN;
  return (int16_t)v;
}

int32_t fp80_to_i32_ftol(Fp80 x) {
  uint_fast8_t saved = softfloat_exceptionFlags;
  int_fast64_t v;
  softfloat_exceptionFlags = 0;
  v = extF80_to_i64(x, softfloat_round_minMag, false);
  if (softfloat_exceptionFlags & softfloat_flag_invalid) v = 0; /* indefinite 2^63: low word 0 */
  raise(softfloat_exceptionFlags, de(x));
  softfloat_exceptionFlags = saved;
  return (int32_t)(uint32_t)((uint64_t)v & 0xFFFFFFFFu);
}
