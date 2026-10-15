/* FP semantics (task stars-dwf.4): the x87 control word the original runs under, and
 * proof that runtime/fp80.c (SoftFloat extF80) reproduces the original's x87 basic
 * operations bit for bit. Decision record: docs/CONVENTIONS.md "Floating point". */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "harness/harness.h"
#include "runtime/fp80.h"
#include "runtime/mem.h"
#include "test.h"

#define EXE "original/stars26i.exe"

/* Measured by test_control_word_at_winmain: FNINIT state, PC = 11b (64-bit), RC = 00b. */
#define PINNED_CW 0x037Fu
#define N_PAIRS 100000
#define N_PAIRS_CW 10000

/* ---- 1. Control word when WinMain (3:0x0000) is entered ---- */

static void h_inittask(HCall *c) {
  c->regs->ax = 1; c->regs->cx = 0x2000; c->regs->di = 1; c->regs->si = 0;
  c->regs->bx = 0x80; c->regs->es = MEM_SEL_DGROUP; c->regs->dx = 1;
}
static void h_one(HCall *c) { c->ret = 1; }
static void h_version(HCall *c) { c->ret = 0x0A03; }
static uint16_t g_heap_next = 0x4000;
static void h_localalloc(HCall *c) { c->ret = g_heap_next; g_heap_next += 0x100; }

static unsigned g_winmain_hits;
static uint16_t g_cw_at_winmain;
static void on_winmain(uint32_t lin, void *ud) {
  (void)lin; (void)ud;
  if (g_winmain_hits++ == 0) g_cw_at_winmain = harness_fpcw();
  harness_stop_emu();
}

static void test_control_word_at_winmain(void) {
  uint32_t base = desc_base(MEM_SEL_SEG(3));
  harness_set_quiet(true);
  harness_register("KERNEL", 91, h_inittask);   /* InitTask */
  harness_register("KERNEL", 3, h_version);     /* GetVersion */
  harness_register("KERNEL", 30, h_one);        /* WaitEvent */
  harness_register("USER", 5, h_one);           /* InitApp */
  harness_register("KERNEL", 5, h_localalloc);  /* LocalAlloc */
  harness_register("KERNEL", 23, h_one);        /* LockSegment */
  harness_register("KERNEL", 24, h_one);        /* UnlockSegment */
  harness_trace(base, base + 1, on_winmain, NULL);
  harness_call(MEM_SEL_SEG(34), 0x001A, HCONV_CDECL, NULL, 0, NULL);
  harness_trace(0, 0, NULL, NULL);
  harness_set_quiet(false);
  CHECK_EQ(g_winmain_hits, 1);
  CHECK_EQ(g_cw_at_winmain, PINNED_CW);
  CHECK_EQ((g_cw_at_winmain >> 8) & 3, 3);   /* PC: 64-bit significand */
  CHECK_EQ((g_cw_at_winmain >> 10) & 3, 0);  /* RC: round to nearest even */
  CHECK_EQ(g_cw_at_winmain & 0x3F, 0x3F);    /* all exceptions masked */
}

/* ---- 2. Equivalence of Unicorn x87 and fp80 ---- */

static uint64_t g_rng = 0x5EEDF00DCAFE1234ull;
static uint64_t rnd(void) {
  g_rng ^= g_rng >> 12; g_rng ^= g_rng << 25; g_rng ^= g_rng >> 27;
  return g_rng * 0x2545F4914F6CDD1Dull;
}

static int is_nan(uint64_t u) { return ((u >> 52) & 0x7FF) == 0x7FF && (u << 12) != 0; }
static uint64_t bits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double dbl(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }

/* A random double: mostly moderate exponents, some across the whole range, a few
 * zeros and infinities. NaNs are not generated. */
static uint64_t rnd_bits(void) {
  uint64_t r = rnd();
  uint64_t sign = (r & 1) << 63, mant = (r >> 8) & 0xFFFFFFFFFFFFFull;
  unsigned pick = (unsigned)(r >> 1) % 100, e;
  if (pick < 2) return sign;                                  /* +-0 */
  if (pick < 3) return sign | (0x7FFull << 52);               /* +-inf */
  if (pick < 12) e = 1 + (unsigned)(rnd() % 2046);            /* any finite exponent */
  else e = 1023 - 40 + (unsigned)(rnd() % 81);
  return sign | ((uint64_t)e << 52) | mant;
}

/* Second operand: half of the time within +-60 binary orders of the first, which makes
 * alignment shifts, cancellation and sticky bits common. */
static uint64_t rnd_near(uint64_t a) {
  uint64_t b = rnd_bits();
  if (rnd() & 1) {
    int ea = (int)((a >> 52) & 0x7FF), eb = ea - 60 + (int)(rnd() % 121);
    if (ea != 0 && ea != 0x7FF && eb >= 1 && eb <= 2046)
      b = (b & ~(0x7FFull << 52)) | ((uint64_t)eb << 52);
  }
  return b;
}

#define BATCH 1000u

static uint16_t g_code_arith[4], g_code_fistp16, g_code_fistp32;
static uint16_t g_data; /* data selector: BATCH records of 24 bytes (a, b, result) */

static uint16_t mk(const uint8_t *c, size_t n) {
  uint16_t s = harness_alloc_code(c, n);
  CHECK(s != 0);
  return s;
}

/* Each sequence loops over BATCH records at ES:SI (CX = count), 24 bytes apiece:
 *   fld qword es:[si]; <op> qword es:[si+8]; fstp/fistp es:[si+16]; add si,24; loop */
static void setup_code(void) {
  static const uint8_t op[4] = {0x44, 0x64, 0x4C, 0x74}; /* DC /0 /4 /1 /6: FADD, FSUB, FMUL, FDIV, [si+disp8] */
  for (int i = 0; i < 4; i++) {
    uint8_t c[] = {0x26, 0xDD, 0x04, 0x26, 0xDC, op[i], 0x08, 0x26, 0xDD, 0x5C, 0x10,
                   0x83, 0xC6, 0x18, 0xE2, 0xF0, 0xCB};
    g_code_arith[i] = mk(c, sizeof c);
  }
  {
    uint8_t c[] = {0x26, 0xDD, 0x04, 0x26, 0xDF, 0x5C, 0x10, 0x83, 0xC6, 0x18, 0xE2, 0xF4, 0xCB};
    g_code_fistp16 = mk(c, sizeof c); /* fistp word */
  }
  {
    uint8_t c[] = {0x26, 0xDD, 0x04, 0x26, 0xDB, 0x5C, 0x10, 0x83, 0xC6, 0x18, 0xE2, 0xF4, 0xCB};
    g_code_fistp32 = mk(c, sizeof c); /* fistp dword */
  }
  g_data = GlobalAlloc(GPTR, BATCH * 24u);
  CHECK(g_data != 0);
}

/* Runs a synthetic sequence under control word cw over the BATCH records in rec[]. */
static void run_x87(uint16_t sel, uint16_t cw, uint64_t (*rec)[3]) {
  Regs16 io;
  memcpy(LIN((uint32_t)g_data << 16), rec, BATCH * 24u);
  memset(&io, 0, sizeof io);
  io.es = g_data; io.si = 0; io.cx = BATCH;
  harness_set_fpcw(cw);
  harness_call(sel, 0, HCONV_CDECL, NULL, 0, &io);
  CHECK_EQ(harness_last_error(), 0);
  memcpy(rec, LIN((uint32_t)g_data << 16), BATCH * 24u);
}

typedef struct { unsigned tested, mism_fp80, mism_double; uint64_t ex_a, ex_b, ex_x87, ex_dbl; } Stat;

static Fp80 fp80_op(int i, Fp80 a, Fp80 b) {
  return i == 0 ? fp80_add(a, b) : i == 1 ? fp80_sub(a, b) : i == 2 ? fp80_mul(a, b) : fp80_div(a, b);
}
static double dbl_op(int i, double a, double b) {
  return i == 0 ? a + b : i == 1 ? a - b : i == 2 ? a * b : a / b;
}

static Stat test_arith(int op, uint16_t cw, unsigned n) {
  Stat s;
  uint64_t rec[BATCH][3];
  memset(&s, 0, sizeof s);
  fp80_set_cw(cw);
  for (unsigned done = 0; done < n; done += BATCH) {
    for (unsigned i = 0; i < BATCH; i++) {
      rec[i][0] = rnd_bits(); rec[i][1] = rnd_near(rec[i][0]); rec[i][2] = 0;
    }
    run_x87(g_code_arith[op], cw, rec);
    /* run_x87 overwrote nothing but the result column; operands are intact. */
    for (unsigned i = 0; i < BATCH; i++) {
      uint64_t a = rec[i][0], b = rec[i][1], x = rec[i][2];
      uint64_t f = bits(fp80_to_f64(fp80_op(op, fp80_from_f64(dbl(a)), fp80_from_f64(dbl(b)))));
      uint64_t d = bits(dbl_op(op, dbl(a), dbl(b)));
      s.tested++;
      if (x != f && s.mism_fp80++ == 0)
        fprintf(stderr, "fp80 mismatch op %d cw %04x: %016llx %016llx x87 %016llx fp80 %016llx\n", op, cw,
                (unsigned long long)a, (unsigned long long)b, (unsigned long long)x, (unsigned long long)f);
      if (x != d && !is_nan(x) && s.mism_double++ == 0) { s.ex_a = a; s.ex_b = b; s.ex_x87 = x; s.ex_dbl = d; }
    }
  }
  return s;
}

/* Integer test operand: ties, values around the int16/int32 limits, random magnitudes. */
static uint64_t rnd_int_operand(void) {
  uint64_t r = rnd();
  unsigned k = (unsigned)(r >> 60) & 7;
  double v;
  int64_t n = (int64_t)(rnd() % 200001) - 100000;
  switch (k) {
    case 0: v = (double)n + 0.5; break;                         /* tie */
    case 1: v = (double)(rnd() % 80001) - 40000.5; break;       /* int16 boundary area, tie */
    case 2: v = (double)(int64_t)(rnd() % 4294967296ull) - 2147483648.0 + 0.5; break; /* int32 range */
    case 3: v = dbl(rnd_bits()); break;
    case 4: v = (double)n / 8.0; break;
    case 5: v = (double)((int64_t)(rnd() % 5) - 2) * 2147483648.0 + (double)((int)(rnd() % 5) - 2) * 0.5; break;
    case 6: v = (double)((int)(rnd() % 7) - 3) * 32768.0 + (double)((int)(rnd() % 5) - 2) * 0.5; break;
    default: v = dbl(rnd_bits()) * 1e-3; break;
  }
  return bits(v);
}

static unsigned test_fistp(int wide, uint16_t cw, unsigned n) {
  unsigned bad = 0;
  uint64_t rec[BATCH][3];
  fp80_set_cw(cw);
  for (unsigned done = 0; done < n; done += BATCH) {
    for (unsigned i = 0; i < BATCH; i++) { rec[i][0] = rnd_int_operand(); rec[i][1] = 0; rec[i][2] = 0; }
    run_x87(wide ? g_code_fistp32 : g_code_fistp16, cw, rec);
    for (unsigned i = 0; i < BATCH; i++) {
      uint64_t a = rec[i][0], x = rec[i][2];
      Fp80 v = fp80_from_f64(dbl(a));
      uint64_t f = wide ? (uint32_t)fp80_to_i32(v) : (uint16_t)fp80_to_i16(v);
      uint64_t xm = wide ? (x & 0xFFFFFFFFull) : (x & 0xFFFFull);
      if (xm != f && bad++ == 0)
        fprintf(stderr, "fistp%d mismatch cw %04x: %016llx x87 %llx fp80 %llx\n", wide ? 32 : 16, cw,
                (unsigned long long)a, (unsigned long long)xm, (unsigned long long)f);
    }
  }
  return bad;
}

static void test_equivalence(void) {
  static const char *name[4] = {"FADD", "FSUB", "FMUL", "FDIV"};
  setup_code();
  for (int op = 0; op < 4; op++) {
    Stat s = test_arith(op, PINNED_CW, N_PAIRS);
    printf("%s cw=%04x: %u pairs, fp80 mismatches %u, plain-double mismatches %u", name[op], PINNED_CW,
           s.tested, s.mism_fp80, s.mism_double);
    if (s.mism_double)
      printf(" (e.g. %016llx op %016llx: x87 %016llx, double %016llx)", (unsigned long long)s.ex_a,
             (unsigned long long)s.ex_b, (unsigned long long)s.ex_x87, (unsigned long long)s.ex_dbl);
    printf("\n");
    CHECK_EQ(s.mism_fp80, 0);
    CHECK(s.mism_double > 0); /* the test can tell fp80 from plain double (regression proof) */
  }
  CHECK_EQ(test_fistp(0, PINNED_CW, N_PAIRS), 0);
  CHECK_EQ(test_fistp(1, PINNED_CW, N_PAIRS), 0);
  printf("FISTP m16/m32 cw=%04x: %d pairs each\n", PINNED_CW, N_PAIRS);
}

/* The helpers honour the rounding control and the precision control too. */
static void test_other_control_words(void) {
  for (unsigned pc = 0; pc < 4; pc += 2) {   /* 24-bit and 53-bit; 01b is reserved */
    for (unsigned rc = 0; rc < 4; rc++) {
      uint16_t cw = (uint16_t)(0x037F | (pc << 8) | (rc << 10));
      for (int op = 0; op < 4; op++) CHECK_EQ(test_arith(op, cw, N_PAIRS_CW).mism_fp80, 0);
      CHECK_EQ(test_fistp(0, cw, N_PAIRS_CW), 0);
      CHECK_EQ(test_fistp(1, cw, N_PAIRS_CW), 0);
    }
  }
  for (unsigned rc = 1; rc < 4; rc++) {       /* 64-bit precision, directed rounding */
    uint16_t cw = (uint16_t)(0x037F | (rc << 10));
    for (int op = 0; op < 4; op++) CHECK_EQ(test_arith(op, cw, N_PAIRS_CW).mism_fp80, 0);
    CHECK_EQ(test_fistp(0, cw, N_PAIRS_CW), 0);
    CHECK_EQ(test_fistp(1, cw, N_PAIRS_CW), 0);
  }
}

int main(void) {
  if (harness_load(EXE) != 0) {
    fprintf(stderr, "harness_load failed\n");
    return 1;
  }
  test_control_word_at_winmain();
  test_equivalence();
  test_other_control_words();
  harness_shutdown();
  return TEST_DONE();
}
