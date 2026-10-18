/* By-name Win16 API calls with API-log entries (native builds only). stars-4a6.
 *
 * Decompiled code in src/ calls Windows APIs by name (docs/CONVENTIONS.md rule e). The harness API
 * log used by the diff tests is written at the import thunk, so a by-name call would be missing from
 * the DECOMP side of the log. This header renames each listed runtime function to a wrapper that
 * records the call the way the thunk does (module, ordinal, name, raw argument bytes, return value)
 * around the real call. The runtime itself is unchanged and does not include this header
 * (make check-split); src/stars.h includes it behind #ifndef __EMSCRIPTEN__, so the wasm build
 * calls the runtime directly.
 *
 * To log another API, add one wrapper below: list its argument words in PUSH order (the order the
 * original pushes them: first argument first, a 32-bit value as high word then low word), and put
 * the module name and ordinal from runtime/win16_api.inc. Define the #define after the wrapper. */
#ifndef STARS_HARNESS_BYNAME_H
#define STARS_HARNESS_BYNAME_H

#ifdef __EMSCRIPTEN__
#error "harness/byname.h is for native builds only"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "harness/harness.h"
#include "runtime/gdi.h"
#include "runtime/kernel.h"
#include "runtime/mem.h"
#include "runtime/text.h"
#include "runtime/user.h"

#define BYNAME_HI(v) ((uint16_t)((uint32_t)(v) >> 16))
#define BYNAME_LO(v) ((uint16_t)(v))

/* KERNEL global memory (runtime/mem.h). */
static inline uint16_t byname_GlobalAlloc(uint16_t flags, uint32_t bytes) {
  const uint16_t w[] = {flags, BYNAME_HI(bytes), BYNAME_LO(bytes)};
  size_t i = harness_log_begin("KERNEL", 15, w, 3);
  uint16_t r = (GlobalAlloc)(flags, bytes);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_GlobalReAlloc(uint16_t h, uint32_t bytes, uint16_t flags) {
  const uint16_t w[] = {h, BYNAME_HI(bytes), BYNAME_LO(bytes), flags};
  size_t i = harness_log_begin("KERNEL", 16, w, 4);
  uint16_t r = (GlobalReAlloc)(h, bytes, flags);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_GlobalFree(uint16_t h) {
  size_t i = harness_log_begin("KERNEL", 17, &h, 1);
  uint16_t r = (GlobalFree)(h);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_GlobalLock(uint16_t h) {
  size_t i = harness_log_begin("KERNEL", 18, &h, 1);
  uint32_t r = (GlobalLock)(h);
  harness_log_end(i, r);
  return r;
}
static inline bool byname_GlobalUnlock(uint16_t h) {
  size_t i = harness_log_begin("KERNEL", 19, &h, 1);
  bool r = (GlobalUnlock)(h);
  harness_log_end(i, r ? 1u : 0u); /* the thunk handler returns 1 or 0 */
  return r;
}
static inline uint32_t byname_GlobalSize(uint16_t h) {
  size_t i = harness_log_begin("KERNEL", 20, &h, 1);
  uint32_t r = (GlobalSize)(h);
  harness_log_end(i, r);
  return r;
}

/* GDI (runtime/gdi.h, runtime/text.h) and USER (runtime/user.h). */
static inline uint16_t byname_SelectObject(uint16_t hdc, uint16_t h) {
  const uint16_t w[] = {hdc, h};
  size_t i = harness_log_begin("GDI", 45, w, 2);
  uint16_t r = (SelectObject)(hdc, h);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_SetBkMode(uint16_t hdc, int16_t mode) {
  const uint16_t w[] = {hdc, (uint16_t)mode};
  size_t i = harness_log_begin("GDI", 2, w, 2);
  int16_t r = (SetBkMode)(hdc, mode);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint32_t byname_GetTextExtent(uint16_t hdc, uint32_t str, int16_t count) {
  const uint16_t w[] = {hdc, BYNAME_HI(str), BYNAME_LO(str), (uint16_t)count};
  size_t i = harness_log_begin("GDI", 91, w, 4);
  uint32_t r = (GetTextExtent)(hdc, str, count);
  harness_log_end(i, r);
  return r;
}
/* _wsprintf is VARARGS: the thunk logs the fixed part only, the two far pointers (8 bytes); the
 * variable arguments follow them in memory at lpArgs and are not part of the log. cdecl pushes
 * the format pointer first, then the output pointer. */
static inline int16_t byname__wsprintf(uint32_t out, uint32_t fmt, uint32_t args) {
  const uint16_t w[] = {BYNAME_HI(fmt), BYNAME_LO(fmt), BYNAME_HI(out), BYNAME_LO(out)};
  size_t i = harness_log_begin("USER", 420, w, 4);
  int16_t r = (_wsprintf)(out, fmt, args);
  harness_log_end(i, (uint16_t)r);
  return r;
}

/* Added by stars-bfj.5 (segment 3): the other APIs it calls by name. Each wrapper logs exactly what the
 * harness handler for the thunk logs: a void API leaves ret 0, a signed 16-bit result is logged as a
 * word. */
static inline uint32_t byname_MakeProcInstance(uint32_t proc, uint16_t hinst) {
  const uint16_t w[] = {BYNAME_HI(proc), BYNAME_LO(proc), hinst};
  size_t i = harness_log_begin("KERNEL", 51, w, 3);
  uint32_t r = (MakeProcInstance)(proc, hinst);
  harness_log_end(i, r);
  return r;
}
static inline void byname_FreeProcInstance(uint32_t proc) {
  const uint16_t w[] = {BYNAME_HI(proc), BYNAME_LO(proc)};
  size_t i = harness_log_begin("KERNEL", 52, w, 2);
  (FreeProcInstance)(proc);
  harness_log_end(i, 0);
}
static inline uint16_t byname_FreeResource(uint16_t h) {
  const uint16_t w[] = {h};
  size_t i = harness_log_begin("KERNEL", 63, w, 1);
  uint16_t r = (FreeResource)(h);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_SetBkColor(uint16_t hdc, uint32_t c) {
  const uint16_t w[] = {hdc, BYNAME_HI(c), BYNAME_LO(c)};
  size_t i = harness_log_begin("GDI", 1, w, 3);
  uint32_t r = (SetBkColor)(hdc, c);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_SetTextColor(uint16_t hdc, uint32_t c) {
  const uint16_t w[] = {hdc, BYNAME_HI(c), BYNAME_LO(c)};
  size_t i = harness_log_begin("GDI", 9, w, 3);
  uint32_t r = (SetTextColor)(hdc, c);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_IntersectClipRect(uint16_t hdc, int16_t left, int16_t top, int16_t right, int16_t bottom) {
  const uint16_t w[] = {hdc, (uint16_t)left, (uint16_t)top, (uint16_t)right, (uint16_t)bottom};
  size_t i = harness_log_begin("GDI", 22, w, 5);
  int16_t r = (IntersectClipRect)(hdc, left, top, right, bottom);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline int16_t byname_SelectClipRgn(uint16_t hdc, uint16_t rgn) {
  const uint16_t w[] = {hdc, rgn};
  size_t i = harness_log_begin("GDI", 44, w, 2);
  int16_t r = (SelectClipRgn)(hdc, rgn);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint16_t byname_DeleteObject(uint16_t h) {
  const uint16_t w[] = {h};
  size_t i = harness_log_begin("GDI", 69, w, 1);
  uint16_t r = (DeleteObject)(h);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_GetDeviceCaps(uint16_t hdc, int16_t index) {
  const uint16_t w[] = {hdc, (uint16_t)index};
  size_t i = harness_log_begin("GDI", 80, w, 2);
  int16_t r = (GetDeviceCaps)(hdc, index);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline void byname_GetCursorPos(uint32_t pt) {
  const uint16_t w[] = {BYNAME_HI(pt), BYNAME_LO(pt)};
  size_t i = harness_log_begin("USER", 17, w, 2);
  (GetCursorPos)(pt);
  harness_log_end(i, 0);
}
static inline uint16_t byname_SetTimer(uint16_t hwnd, uint16_t id, uint16_t elapse, uint32_t proc) {
  const uint16_t w[] = {hwnd, id, elapse, BYNAME_HI(proc), BYNAME_LO(proc)};
  size_t i = harness_log_begin("USER", 10, w, 5);
  uint16_t r = (SetTimer)(hwnd, id, elapse, proc);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_KillTimer(uint16_t hwnd, uint16_t id) {
  const uint16_t w[] = {hwnd, id};
  size_t i = harness_log_begin("USER", 12, w, 2);
  uint16_t r = (KillTimer)(hwnd, id);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_GetTickCount(void) {
  size_t i = harness_log_begin("USER", 13, NULL, 0);
  uint32_t r = (GetTickCount)();
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_GetFocus(void) {
  size_t i = harness_log_begin("USER", 23, NULL, 0);
  uint16_t r = (GetFocus)();
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_WindowFromPoint(uint32_t pt) {
  const uint16_t w[] = {BYNAME_HI(pt), BYNAME_LO(pt)};
  size_t i = harness_log_begin("USER", 30, w, 2);
  uint16_t r = (WindowFromPoint)(pt);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_IsIconic(uint16_t hwnd) {
  const uint16_t w[] = {hwnd};
  size_t i = harness_log_begin("USER", 31, w, 1);
  uint16_t r = (IsIconic)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline void byname_GetClientRect(uint16_t hwnd, uint32_t rect) {
  const uint16_t w[] = {hwnd, BYNAME_HI(rect), BYNAME_LO(rect)};
  size_t i = harness_log_begin("USER", 33, w, 3);
  (GetClientRect)(hwnd, rect);
  harness_log_end(i, 0);
}
static inline void byname_SetWindowText(uint16_t hwnd, uint32_t str) {
  const uint16_t w[] = {hwnd, BYNAME_HI(str), BYNAME_LO(str)};
  size_t i = harness_log_begin("USER", 37, w, 3);
  (SetWindowText)(hwnd, str);
  harness_log_end(i, 0);
}
static inline uint16_t byname_GetParent(uint16_t hwnd) {
  const uint16_t w[] = {hwnd};
  size_t i = harness_log_begin("USER", 46, w, 1);
  uint16_t r = (GetParent)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_DestroyWindow(uint16_t hwnd) {
  const uint16_t w[] = {hwnd};
  size_t i = harness_log_begin("USER", 53, w, 1);
  uint16_t r = (DestroyWindow)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_GetActiveWindow(void) {
  size_t i = harness_log_begin("USER", 60, NULL, 0);
  uint16_t r = (GetActiveWindow)();
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_GetDC(uint16_t hwnd) {
  const uint16_t w[] = {hwnd};
  size_t i = harness_log_begin("USER", 66, w, 1);
  uint16_t r = (GetDC)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_ReleaseDC(uint16_t hwnd, uint16_t hdc) {
  const uint16_t w[] = {hwnd, hdc};
  size_t i = harness_log_begin("USER", 68, w, 2);
  uint16_t r = (ReleaseDC)(hwnd, hdc);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_SetCursor(uint16_t h) {
  const uint16_t w[] = {h};
  size_t i = harness_log_begin("USER", 69, w, 1);
  uint16_t r = (SetCursor)(h);
  harness_log_end(i, r);
  return r;
}
static inline void byname_OffsetRect(uint32_t rect, int16_t dx, int16_t dy) {
  const uint16_t w[] = {BYNAME_HI(rect), BYNAME_LO(rect), (uint16_t)dx, (uint16_t)dy};
  size_t i = harness_log_begin("USER", 77, w, 4);
  (OffsetRect)(rect, dx, dy);
  harness_log_end(i, 0);
}
static inline int16_t byname_FillRect(uint16_t hdc, uint32_t rect, uint16_t hbr) {
  const uint16_t w[] = {hdc, BYNAME_HI(rect), BYNAME_LO(rect), hbr};
  size_t i = harness_log_begin("USER", 81, w, 4);
  int16_t r = (FillRect)(hdc, rect, hbr);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline int16_t byname_DialogBox(uint16_t hinst, uint32_t name, uint16_t hwnd, uint32_t proc) {
  const uint16_t w[] = {hinst, BYNAME_HI(name), BYNAME_LO(name), hwnd, BYNAME_HI(proc), BYNAME_LO(proc)};
  size_t i = harness_log_begin("USER", 87, w, 6);
  int16_t r = (DialogBox)(hinst, name, hwnd, proc);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint16_t byname_EndDialog(uint16_t hdlg, int16_t result) {
  const uint16_t w[] = {hdlg, (uint16_t)result};
  size_t i = harness_log_begin("USER", 88, w, 2);
  uint16_t r = (EndDialog)(hdlg, result);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_GetDlgItem(uint16_t hdlg, int16_t id) {
  const uint16_t w[] = {hdlg, (uint16_t)id};
  size_t i = harness_log_begin("USER", 91, w, 2);
  uint16_t r = (GetDlgItem)(hdlg, id);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_GetKeyState(int16_t vk) {
  const uint16_t w[] = {(uint16_t)vk};
  size_t i = harness_log_begin("USER", 106, w, 1);
  int16_t r = (GetKeyState)(vk);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint16_t byname_GetMessage(uint32_t msg, uint16_t hwnd, uint16_t min, uint16_t max) {
  const uint16_t w[] = {BYNAME_HI(msg), BYNAME_LO(msg), hwnd, min, max};
  size_t i = harness_log_begin("USER", 108, w, 5);
  uint16_t r = (GetMessage)(msg, hwnd, min, max);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_PostMessage(uint16_t hwnd, uint16_t msg, uint16_t wparam, uint32_t lparam) {
  const uint16_t w[] = {hwnd, msg, wparam, BYNAME_HI(lparam), BYNAME_LO(lparam)};
  size_t i = harness_log_begin("USER", 110, w, 5);
  uint16_t r = (PostMessage)(hwnd, msg, wparam, lparam);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_SendMessage(uint16_t hwnd, uint16_t msg, uint16_t wparam, uint32_t lparam) {
  const uint16_t w[] = {hwnd, msg, wparam, BYNAME_HI(lparam), BYNAME_LO(lparam)};
  size_t i = harness_log_begin("USER", 111, w, 5);
  uint32_t r = (SendMessage)(hwnd, msg, wparam, lparam);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_TranslateMessage(uint32_t msg) {
  const uint16_t w[] = {BYNAME_HI(msg), BYNAME_LO(msg)};
  size_t i = harness_log_begin("USER", 113, w, 2);
  uint16_t r = (TranslateMessage)(msg);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_DispatchMessage(uint32_t msg) {
  const uint16_t w[] = {BYNAME_HI(msg), BYNAME_LO(msg)};
  size_t i = harness_log_begin("USER", 114, w, 2);
  uint32_t r = (DispatchMessage)(msg);
  harness_log_end(i, r);
  return r;
}
static inline void byname_InvalidateRect(uint16_t hwnd, uint32_t rect, uint16_t erase) {
  const uint16_t w[] = {hwnd, BYNAME_HI(rect), BYNAME_LO(rect), erase};
  size_t i = harness_log_begin("USER", 125, w, 4);
  (InvalidateRect)(hwnd, rect, erase);
  harness_log_end(i, 0);
}
static inline uint16_t byname_LoadCursor(uint16_t hinst, uint32_t name) {
  const uint16_t w[] = {hinst, BYNAME_HI(name), BYNAME_LO(name)};
  size_t i = harness_log_begin("USER", 173, w, 3);
  uint16_t r = (LoadCursor)(hinst, name);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_TranslateAccelerator(uint16_t hwnd, uint16_t haccel, uint32_t msg) {
  const uint16_t w[] = {hwnd, haccel, BYNAME_HI(msg), BYNAME_LO(msg)};
  size_t i = harness_log_begin("USER", 178, w, 4);
  int16_t r = (TranslateAccelerator)(hwnd, haccel, msg);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline int16_t byname_GetSystemMetrics(int16_t index) {
  const uint16_t w[] = {(uint16_t)index};
  size_t i = harness_log_begin("USER", 179, w, 1);
  int16_t r = (GetSystemMetrics)(index);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint32_t byname_GetSysColor(uint16_t index) {
  const uint16_t w[] = {index};
  size_t i = harness_log_begin("USER", 180, w, 1);
  uint32_t r = (GetSysColor)(index);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_DestroyIcon(uint16_t h) {
  const uint16_t w[] = {h};
  size_t i = harness_log_begin("USER", 457, w, 1);
  uint16_t r = (DestroyIcon)(h);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_DestroyCursor(uint16_t h) {
  const uint16_t w[] = {h};
  size_t i = harness_log_begin("USER", 458, w, 1);
  uint16_t r = (DestroyCursor)(h);
  harness_log_end(i, r);
  return r;
}

/* Added by stars-bfj.15 (segment 13): the other APIs it calls by name. Same convention as above:
 * words in push order, a void API leaves ret 0, a signed 16-bit result is logged as a word. */
static inline uint16_t byname_PatBlt(uint16_t hdc, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t rop) {
  const uint16_t a[] = {hdc, (uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h, BYNAME_HI(rop), BYNAME_LO(rop)};
  size_t i = harness_log_begin("GDI", 29, a, 7);
  uint16_t r = (PatBlt)(hdc, x, y, w, h, rop);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_ExtTextOut(uint16_t hdc, int16_t x, int16_t y, uint16_t options, uint32_t rect,
                                         uint32_t str, uint16_t count, uint32_t dx) {
  const uint16_t a[] = {hdc,           (uint16_t)x,    (uint16_t)y,   options,        BYNAME_HI(rect), BYNAME_LO(rect),
                        BYNAME_HI(str), BYNAME_LO(str), count,        BYNAME_HI(dx), BYNAME_LO(dx)};
  size_t i = harness_log_begin("GDI", 351, a, 11);
  uint16_t r = (ExtTextOut)(hdc, x, y, options, rect, str, count, dx);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_BeginPaint(uint16_t hwnd, uint32_t ps) {
  const uint16_t a[] = {hwnd, BYNAME_HI(ps), BYNAME_LO(ps)};
  size_t i = harness_log_begin("USER", 39, a, 3);
  uint16_t r = (BeginPaint)(hwnd, ps);
  harness_log_end(i, r);
  return r;
}
static inline void byname_EndPaint(uint16_t hwnd, uint32_t ps) {
  const uint16_t a[] = {hwnd, BYNAME_HI(ps), BYNAME_LO(ps)};
  size_t i = harness_log_begin("USER", 40, a, 3);
  (EndPaint)(hwnd, ps);
  harness_log_end(i, 0);
}
static inline uint32_t byname_CallWindowProc(uint32_t proc, uint16_t hwnd, uint16_t msg, uint16_t wparam,
                                             uint32_t lparam) {
  const uint16_t a[] = {BYNAME_HI(proc), BYNAME_LO(proc), hwnd, msg, wparam, BYNAME_HI(lparam), BYNAME_LO(lparam)};
  size_t i = harness_log_begin("USER", 122, a, 7);
  uint32_t r = (CallWindowProc)(proc, hwnd, msg, wparam, lparam);
  harness_log_end(i, r);
  return r;
}
static inline void byname_ClientToScreen(uint16_t hwnd, uint32_t pt) {
  const uint16_t a[] = {hwnd, BYNAME_HI(pt), BYNAME_LO(pt)};
  size_t i = harness_log_begin("USER", 28, a, 3);
  (ClientToScreen)(hwnd, pt);
  harness_log_end(i, 0);
}
static inline void byname_ScreenToClient(uint16_t hwnd, uint32_t pt) {
  const uint16_t a[] = {hwnd, BYNAME_HI(pt), BYNAME_LO(pt)};
  size_t i = harness_log_begin("USER", 29, a, 3);
  (ScreenToClient)(hwnd, pt);
  harness_log_end(i, 0);
}
static inline uint16_t byname_CreateWindow(uint32_t cls, uint32_t name, uint32_t style, int16_t x, int16_t y,
                                           int16_t cx, int16_t cy, uint16_t parent, uint16_t menu, uint16_t inst,
                                           uint32_t param) {
  const uint16_t a[] = {BYNAME_HI(cls),   BYNAME_LO(cls), BYNAME_HI(name), BYNAME_LO(name), BYNAME_HI(style),
                        BYNAME_LO(style), (uint16_t)x,    (uint16_t)y,     (uint16_t)cx,    (uint16_t)cy,
                        parent,           menu,           inst,            BYNAME_HI(param), BYNAME_LO(param)};
  size_t i = harness_log_begin("USER", 41, a, 15);
  uint16_t r = (CreateWindow)(cls, name, style, x, y, cx, cy, parent, menu, inst, param);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_DefWindowProc(uint16_t hwnd, uint16_t msg, uint16_t wparam, uint32_t lparam) {
  const uint16_t a[] = {hwnd, msg, wparam, BYNAME_HI(lparam), BYNAME_LO(lparam)};
  size_t i = harness_log_begin("USER", 107, a, 5);
  uint32_t r = (DefWindowProc)(hwnd, msg, wparam, lparam);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_EqualRect(uint32_t r1, uint32_t r2) {
  const uint16_t a[] = {BYNAME_HI(r1), BYNAME_LO(r1), BYNAME_HI(r2), BYNAME_LO(r2)};
  size_t i = harness_log_begin("USER", 244, a, 4);
  uint16_t r = (EqualRect)(r1, r2);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_FrameRect(uint16_t hdc, uint32_t rect, uint16_t hbr) {
  const uint16_t a[] = {hdc, BYNAME_HI(rect), BYNAME_LO(rect), hbr};
  size_t i = harness_log_begin("USER", 83, a, 4);
  int16_t r = (FrameRect)(hdc, rect, hbr);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint16_t byname_GetWindow(uint16_t hwnd, uint16_t cmd) {
  const uint16_t a[] = {hwnd, cmd};
  size_t i = harness_log_begin("USER", 262, a, 2);
  uint16_t r = (GetWindow)(hwnd, cmd);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_GetWindowLong(uint16_t hwnd, int16_t index) {
  const uint16_t a[] = {hwnd, (uint16_t)index};
  size_t i = harness_log_begin("USER", 135, a, 2);
  uint32_t r = (GetWindowLong)(hwnd, index);
  harness_log_end(i, r);
  return r;
}
static inline uint32_t byname_SetWindowLong(uint16_t hwnd, int16_t index, uint32_t value) {
  const uint16_t a[] = {hwnd, (uint16_t)index, BYNAME_HI(value), BYNAME_LO(value)};
  size_t i = harness_log_begin("USER", 136, a, 4);
  uint32_t r = (SetWindowLong)(hwnd, index, value);
  harness_log_end(i, r);
  return r;
}
static inline int16_t byname_GetWindowText(uint16_t hwnd, uint32_t buf, int16_t max) {
  const uint16_t a[] = {hwnd, BYNAME_HI(buf), BYNAME_LO(buf), (uint16_t)max};
  size_t i = harness_log_begin("USER", 36, a, 4);
  int16_t r = (GetWindowText)(hwnd, buf, max);
  harness_log_end(i, (uint16_t)r);
  return r;
}
static inline uint16_t byname_IsWindowVisible(uint16_t hwnd) {
  const uint16_t a[] = {hwnd};
  size_t i = harness_log_begin("USER", 49, a, 1);
  uint16_t r = (IsWindowVisible)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline void byname_MapWindowPoints(uint16_t from, uint16_t to, uint32_t pts, uint16_t count) {
  const uint16_t a[] = {from, to, BYNAME_HI(pts), BYNAME_LO(pts), count};
  size_t i = harness_log_begin("USER", 258, a, 5);
  (MapWindowPoints)(from, to, pts, count);
  harness_log_end(i, 0);
}
static inline void byname_MessageBeep(uint16_t type) {
  const uint16_t a[] = {type};
  size_t i = harness_log_begin("USER", 104, a, 1);
  (MessageBeep)(type);
  harness_log_end(i, 0);
}
static inline uint16_t byname_PtInRect(uint32_t rect, uint32_t pt) {
  const uint16_t a[] = {BYNAME_HI(rect), BYNAME_LO(rect), BYNAME_HI(pt), BYNAME_LO(pt)};
  size_t i = harness_log_begin("USER", 76, a, 4);
  uint16_t r = (PtInRect)(rect, pt);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_RealizePalette(uint16_t hdc) {
  const uint16_t a[] = {hdc};
  size_t i = harness_log_begin("USER", 283, a, 1);
  uint16_t r = (RealizePalette)(hdc);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_SelectPalette(uint16_t hdc, uint16_t hpal, uint16_t background) {
  const uint16_t a[] = {hdc, hpal, background};
  size_t i = harness_log_begin("USER", 282, a, 3);
  uint16_t r = (SelectPalette)(hdc, hpal, background);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_SetCapture(uint16_t hwnd) {
  const uint16_t a[] = {hwnd};
  size_t i = harness_log_begin("USER", 18, a, 1);
  uint16_t r = (SetCapture)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline void byname_ReleaseCapture(void) {
  size_t i = harness_log_begin("USER", 19, NULL, 0);
  (ReleaseCapture)();
  harness_log_end(i, 0);
}
static inline uint16_t byname_SetFocus(uint16_t hwnd) {
  const uint16_t a[] = {hwnd};
  size_t i = harness_log_begin("USER", 22, a, 1);
  uint16_t r = (SetFocus)(hwnd);
  harness_log_end(i, r);
  return r;
}
static inline uint16_t byname_SetWindowPos(uint16_t hwnd, uint16_t after, int16_t x, int16_t y, int16_t cx,
                                           int16_t cy, uint16_t flags) {
  const uint16_t a[] = {hwnd, after, (uint16_t)x, (uint16_t)y, (uint16_t)cx, (uint16_t)cy, flags};
  size_t i = harness_log_begin("USER", 232, a, 7);
  uint16_t r = (SetWindowPos)(hwnd, after, x, y, cx, cy, flags);
  harness_log_end(i, r);
  return r;
}
static inline void byname_UpdateWindow(uint16_t hwnd) {
  const uint16_t a[] = {hwnd};
  size_t i = harness_log_begin("USER", 124, a, 1);
  (UpdateWindow)(hwnd);
  harness_log_end(i, 0);
}


#define GlobalAlloc byname_GlobalAlloc
#define GlobalReAlloc byname_GlobalReAlloc
#define GlobalFree byname_GlobalFree
#define GlobalLock byname_GlobalLock
#define GlobalUnlock byname_GlobalUnlock
#define GlobalSize byname_GlobalSize
#define SelectObject byname_SelectObject
#define SetBkMode byname_SetBkMode
#define GetTextExtent byname_GetTextExtent
#define _wsprintf byname__wsprintf
#define MakeProcInstance byname_MakeProcInstance
#define FreeProcInstance byname_FreeProcInstance
#define FreeResource byname_FreeResource
#define SetBkColor byname_SetBkColor
#define SetTextColor byname_SetTextColor
#define IntersectClipRect byname_IntersectClipRect
#define SelectClipRgn byname_SelectClipRgn
#define DeleteObject byname_DeleteObject
#define GetDeviceCaps byname_GetDeviceCaps
#define GetCursorPos byname_GetCursorPos
#define SetTimer byname_SetTimer
#define KillTimer byname_KillTimer
#define GetTickCount byname_GetTickCount
#define GetFocus byname_GetFocus
#define WindowFromPoint byname_WindowFromPoint
#define IsIconic byname_IsIconic
#define GetClientRect byname_GetClientRect
#define SetWindowText byname_SetWindowText
#define GetParent byname_GetParent
#define DestroyWindow byname_DestroyWindow
#define GetActiveWindow byname_GetActiveWindow
#define GetDC byname_GetDC
#define ReleaseDC byname_ReleaseDC
#define SetCursor byname_SetCursor
#define OffsetRect byname_OffsetRect
#define FillRect byname_FillRect
#define DialogBox byname_DialogBox
#define EndDialog byname_EndDialog
#define GetDlgItem byname_GetDlgItem
#define GetKeyState byname_GetKeyState
#define GetMessage byname_GetMessage
#define PostMessage byname_PostMessage
#define SendMessage byname_SendMessage
#define TranslateMessage byname_TranslateMessage
#define DispatchMessage byname_DispatchMessage
#define InvalidateRect byname_InvalidateRect
#define LoadCursor byname_LoadCursor
#define TranslateAccelerator byname_TranslateAccelerator
#define GetSystemMetrics byname_GetSystemMetrics
#define GetSysColor byname_GetSysColor
#define DestroyIcon byname_DestroyIcon
#define DestroyCursor byname_DestroyCursor
#define PatBlt byname_PatBlt
#define ExtTextOut byname_ExtTextOut
#define BeginPaint byname_BeginPaint
#define EndPaint byname_EndPaint
#define CallWindowProc byname_CallWindowProc
#define ClientToScreen byname_ClientToScreen
#define ScreenToClient byname_ScreenToClient
#define CreateWindow byname_CreateWindow
#define DefWindowProc byname_DefWindowProc
#define EqualRect byname_EqualRect
#define FrameRect byname_FrameRect
#define GetWindow byname_GetWindow
#define GetWindowLong byname_GetWindowLong
#define SetWindowLong byname_SetWindowLong
#define GetWindowText byname_GetWindowText
#define IsWindowVisible byname_IsWindowVisible
#define MapWindowPoints byname_MapWindowPoints
#define MessageBeep byname_MessageBeep
#define PtInRect byname_PtInRect
#define RealizePalette byname_RealizePalette
#define SelectPalette byname_SelectPalette
#define SetCapture byname_SetCapture
#define ReleaseCapture byname_ReleaseCapture
#define SetFocus byname_SetFocus
#define SetWindowPos byname_SetWindowPos
#define UpdateWindow byname_UpdateWindow

#endif /* STARS_HARNESS_BYNAME_H */
