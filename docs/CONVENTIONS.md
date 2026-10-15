# Decompilation and runtime conventions

This file is extended by the decompilation conventions task (stars-bfj.1). The
"Floating point" section below was written by stars-dwf.4.

## Floating point

**Decision: use SoftFloat extF80 (Berkeley SoftFloat, "3e plus master changes", vendored in
`third_party/softfloat/`), not C `double`, for every x87 basic operation in
decompiled code.** The helpers are in `runtime/fp80.h` / `runtime/fp80.c`
(`fp80_from_f64`, `fp80_add/sub/mul/div`, `fp80_to_f64`, `fp80_to_i16/i32`,
`fp80_to_i32_ftol`, `fp80_set_cw`). A decompiled `FLD m64; FADD m64; FSTP m64` becomes
`fp80_to_f64(fp80_add(fp80_from_f64(a), fp80_from_f64(b)))`, and values that
stay on the x87 stack between instructions stay `Fp80`.

### Control word: harness model, not a measurement

- **0x037F is the harness MODEL.** `harness_init` loads it into the Unicorn FPU
  (Unicorn boots with FPCW = 0, 24-bit precision, which no real x87 has). It is the
  FNINIT state: PC = 11b (64-bit significand), RC = 00b (nearest even), all six
  exceptions masked. `harness/tests/test_fp.c` pins it
  (`test_control_word_at_winmain`) and labels it as the model.
- **What the start-up path does (verified by `test_control_word_sentinel`).** The C
  runtime path from `34:0x001A` to WinMain (`3:0x0000`) DOES run FNSTCW `[bp-6]`,
  then FNINIT at `34:0x265a`, then compares, then FNCLEX and FLDCW `[bp-6]` at
  `34:0x26b1`. It is a save / FNINIT / restore probe and is net-neutral: a sentinel
  word of 0x1332 set before the call is still 0x1332 at WinMain (the test also
  asserts the FNINIT and the FLDCW are executed, and that the word is 0x037F between
  them). The earlier premise "no FLDCW/FNINIT on the path" was wrong.
- **`WIN87EM.__fpMath` codes are in BX, not AX**: BX = 0 init, BX = 3 installs an
  exception handler (`0110:1db4` = `34:0x1db4`), BX = 0xB; BX = 2 at `34:0x024a` is
  outside this path. No code calls BX = 4, and the game never sets PC below 64-bit.
  (BX codes: from the 2026-10-15 review, not re-derived by the test.)
- **The real word is most likely PC = 11b with IM/DM/ZM/OM unmasked: 0x1330, or
  0x1332 (the same with DM masked)** (Wine's win87em init gives 0x1330; the C runtime
  uses the 0x1332 family itself). Handler: `34:0x1db4`. This is an inference, not a
  measurement: WIN87EM is not in the repo.
- **Consequence.** `fp80` and the harness model run with exceptions MASKED (overflow
  gives infinity, invalid gives the indefinite NaN). The real machine most likely has
  them unmasked and vectors to the handler. The two diverge only on exceptional
  paths (overflow, invalid, divide by zero, denormal operand). Non-exceptional
  results do not depend on the mask bits, and PC = 11b is the same in both.
- The C runtime only changes the word temporarily, inside its own helpers, in
  segment 34 (Ghidra counts 12 FLDCW and 7 FSTCW there; the issue text said 11 and
  6):
  - `34:0x0de6`: FNSTCW, set RC bits to 11b (truncate), FLDCW, FISTP m64, FLDCW
    (restore). This is the double-to-long cast (`ftol`).
  - `34:0x2068`, `34:0x20aa`: FNSTCW, then low byte |= 0x38 and high byte = 0x13
    (PC = 11b, IC = 1, OM/UM/PM masked), FLDCW, FXAM classification for the
    transcendental error paths, then restore at `34:0x1cc3`.
  - the other sites are in undecoded ranges of segment 34 (`RANGE_34_*` rows of
    `docs/functions.csv`) and were not read individually. The five decoded sites
    (`34:0x0e07`, `0x0e0f`, `0x1cc3`, `0x2078`, `0x20ba`) never select a precision
    below 64-bit.

### Reason

With PC = 64-bit, a basic operation is rounded to a 64-bit significand and then
rounded again to 53 bits by FSTP m64. Plain C `double` rounds once, so it differs
whenever the 64-bit result lies near a 53-bit halfway point. SoftFloat extF80
reproduces both roundings. Measured over 100000 seeded random operand pairs per
operation (xorshift64*, seed `0x5EEDF00DCAFE1234`), Unicorn x87 versus:

| operation | fp80 mismatches | plain C double mismatches (non-NaN) |
|-----------|-----------------|--------------------------------------|
| FADD + FSTP m64 | 0 | 17 |
| FSUB + FSTP m64 | 0 | 15 |
| FMUL + FSTP m64 | 0 | 28 |
| FDIV + FSTP m64 | 0 | 23 |
| FISTP m16, FISTP m32 | 0 | (not compared) |

Example counterexample, FADD, operands `0x40e7bfa39c48756d` and
`0x3f864a1220e0038b`: x87 gives `0x40e7bfa3f570bdf0`, plain double
`0x40e7bfa3f570bdf1`. `test_fp.c` asserts that the plain-double count is above 0,
so the test is known to be able to tell the two apart. Operands are random
doubles (exponents mostly within 2^+-40, 9 percent across the whole finite range,
a few +-0 and +-inf; no NaNs; the second operand is within 60 binary orders of the
first half of the time). Overflow, underflow to subnormal, 0/0 and inf-inf NaN
bits (negative quiet NaN, "real indefinite") are covered and identical.

The helpers also honour the rounding control and the precision control of the
control word given to `fp80_set_cw`: `test_fp.c` checks 24-bit, 53-bit and 64-bit
precision with all four rounding modes (10000 pairs each, 0 mismatches), including
FISTP m16/m32 (ties and out-of-range values give the integer indefinite 0x8000 /
0x80000000).

### What the equivalence tests do not verify

- **NaN operands** are not verified for the arithmetic tests: x87 and SoftFloat can
  differ in SNaN quieting and in which of two NaN operands is returned. The committed
  arithmetic tests (FADD/FSUB/FMUL/FDIV, FISTP m16/m32) generate no NaN and no
  denormal operands, although denormal OUTPUTS occur (underflow). The only NaN
  operands in the committed tests are in the ftol test (result 0 for any NaN) and the
  NaN produced by 0/0 and inf-inf (compared as results).
- Exception masks other than the masked model (see above) are not exercised.

### Rules for decompiled code

1. Basic operations (FADD, FSUB, FMUL, FDIV and the reversed forms) on values that
   pass through the x87 stack: use `Fp80`. Never `double` arithmetic, and never
   `long double` (64-bit on arm64 macOS and in WebAssembly).
2. Loads (FLD m32/m64, FILD) convert exactly to `Fp80`; stores (FSTP m32/m64)
   round with the current rounding control (FSTP m32 double-rounds like m64: 64-bit
   significand first, then 24); FISTP rounds by RC (nearest even by default) and gives
   the indefinite value on overflow. **Helpers pending: stars-4vx.** Only
   `fp80_from_f64`, `fp80_from_i32`, `fp80_to_f64`, `fp80_to_i16/i32` and
   `fp80_to_i32_ftol` exist today; FLD m32, FSTP m32, FILD m16/m64 have none yet.
3. Leave the control word at the default (`FP80_CW_DEFAULT`, 0x037F) unless the
   original code sets it; mirror its FLDCW with `fp80_set_cw`.
4. Compare and test instructions are exact on both representations.
5. Transcendental instructions (F2XM1, FYL2X, FPTAN, FPATAN, FSCALE, FRNDINT,
   FSQRT, FPREM) are not covered by this proof. Their results on a real x87 and on
   SoftFloat can differ in the last bit. Each one needs its own equivalence test
   when a decompiled function uses it. They occur only in segment 34 (see the
   table in `docs/inventory.md`), plus the raw-byte matches that are data.

### C runtime math functions called from the game

Functions in segment 34 with `fp=1` in `docs/functions.csv` that have callers
outside segment 34: exactly one.

| address | name | callers outside seg 34 | meaning |
|---------|------|------------------------|---------|
| `34:0x0de6` | `FUN_1108_0de6` | 42 call sites in 15 segments (5, 7, 8, 9, 10, 11, 16, 18, 21, 22, 23, 28, 30, 32, 33) | double to 32-bit long conversion (`ftol`): FISTP m64 under RC = truncate, result DX:AX. Use `fp80_to_i32_ftol` as the reference for the game's behaviour. A plain C cast `(long)d` is exact only for \|d\| < 2^31: the original wraps out-of-range values (2^32 + 5 gives 5) and gives 0 for \|d\| >= 2^63 or NaN (integer indefinite, low 32 bits). The old control word is restored. Proven by `test_fp.c` against FISTP m64 (opcode DF /7) in Unicorn under RC = 11b, 100000 seeded operands including out-of-range, 2^32 + 5, 2^63 and NaN: 0 mismatches. |

Every other `fp=1` function in segment 34 is called only from inside segment 34
(the transcendental implementations and their helpers). The game segments contain
no calls to them, so no `libm` replacement is needed beyond `ftol`.

### Build and test

- `third_party/softfloat/sources.mk` lists the SoftFloat sources built (the 3e
  FAST_INT64 object list); the Makefile builds them into `build/libsoftfloat.a`
  without `-Werror` and sanitizers. License: `third_party/softfloat/LICENSE`
  (BSD-3-Clause).
- `make test` runs `harness/tests/test_fp.c` (about 1 second).
