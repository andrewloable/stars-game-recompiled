Berkeley SoftFloat Release 3e by John R. Hauser (BSD-3-Clause, see LICENSE).
Source: https://github.com/ucb-bar/berkeley-softfloat-3 (master tarball fetched 2026-10-15;
the COPYING.txt and every source header say "Release 3e").
Vendored unmodified: source/ (specialization 8086-SSE only; the other specialization
directories were removed) and platform.h (copied from build/Linux-x86_64-GCC/platform.h).
Build flags: see the Makefile (SOFTFLOAT_FAST_INT64, SOFTFLOAT_ROUND_ODD, INLINE_LEVEL=5).
Used for exact x87 80-bit arithmetic: docs/CONVENTIONS.md, section "Floating point".
