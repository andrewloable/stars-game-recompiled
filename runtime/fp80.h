/* Exact x87 arithmetic on 80-bit extended values (Berkeley SoftFloat 3e extF80).
 * The original C runtime runs the FPU with 64-bit precision control (control word
 * 0x037F, docs/CONVENTIONS.md "Floating point"), so a basic operation is rounded
 * to a 64-bit significand and only then to the 53-bit double the game stores
 * (FSTP m64). Plain C double does the second rounding only, and differs.
 *
 * The control word set by fp80_set_cw drives the precision (PC, bits 8-9) and the
 * rounding mode (RC, bits 10-11) of every operation below. Exceptions are masked
 * (as in 0x037F): overflow gives infinity, invalid gives the indefinite value.
 */
#ifndef STARS_RUNTIME_FP80_H
#define STARS_RUNTIME_FP80_H

#include <stdint.h>

#include "softfloat.h"

typedef extFloat80_t Fp80;

#define FP80_CW_DEFAULT 0x037Fu /* FNINIT value: PC = 64-bit, RC = nearest, all masked */

void fp80_set_cw(uint16_t cw);
uint16_t fp80_get_cw(void);

Fp80 fp80_from_f64(double d); /* FLD m64: exact */
Fp80 fp80_from_i32(int32_t v);
double fp80_to_f64(Fp80 x);   /* FSTP m64: rounds by RC */

Fp80 fp80_add(Fp80 a, Fp80 b); /* FADD, FSUB, FMUL, FDIV */
Fp80 fp80_sub(Fp80 a, Fp80 b);
Fp80 fp80_mul(Fp80 a, Fp80 b);
Fp80 fp80_div(Fp80 a, Fp80 b);

/* FISTP m16 / m32: round by RC; out of range or NaN gives the integer indefinite
 * (0x8000 / 0x80000000). */
int16_t fp80_to_i16(Fp80 x);
int32_t fp80_to_i32(Fp80 x);

#endif /* STARS_RUNTIME_FP80_H */
