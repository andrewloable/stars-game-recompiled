/* Internal state and helpers shared by runtime/user_win.c, user_msg.c and user_paint.c
 * (task stars-0z4.6). Not part of the public API. All state is one block in the arena. */
#ifndef STARS_RUNTIME_USER_INTERNAL_H
#define STARS_RUNTIME_USER_INTERNAL_H

#include <string.h>

#include "runtime/user.h"

#define UW_MAX 256   /* window slots, slot 0 is the desktop */
#define UC_MAX 48    /* window classes */
#define UQ_MAX 256   /* posted-message queue (Windows 3.1's default is 8: UNVERIFIED choice) */
#define UPD_MAX 8    /* rectangles per update region before they collapse to a bounding box */
#define UTEXT_MAX 80
#define UCLS_NAME 40
#define UHWND_BASE 0x0A00u
#define UHWND_STEP 4u
#define UATOM_BASE 0xC000u
#define UHWND_BROADCAST 0xFFFFu

/* Internal SWP flag: the window changed minimised/maximised state (Wine's SWP_STATECHANGED). */
#define SWP_STATECHANGED 0x8000
#define SWP_AGG_NOCLIENTCHANGE (SWP_NOCLIENTSIZE | SWP_NOCLIENTMOVE)
#define SWP_AGG_NOPOSCHANGE (SWP_NOSIZE | SWP_NOMOVE | SWP_NOCLIENTSIZE | SWP_NOCLIENTMOVE | SWP_NOZORDER)
#define SWP_AGG_STATUSFLAGS (SWP_AGG_NOPOSCHANGE | SWP_FRAMECHANGED | SWP_HIDEWINDOW | SWP_SHOWWINDOW)
#define SWP_AGG_NOGEOMETRYCHANGE (SWP_NOSIZE | SWP_NOCLIENTSIZE | SWP_NOZORDER)

typedef struct UWnd {
  uint8_t used, need_size, erase, nupd;
  uint16_t cls;                       /* class slot */
  HWND16 parent, child, next, owner;  /* parent is the desktop for top-level windows; child is
                                         the topmost child, next the sibling below */
  uint16_t id, hinst, nextra, pad;
  uint32_t style, exstyle, wndproc, extra_off;
  RECT16 wrect, crect;                /* window and client rectangles in the PARENT's client
                                         coordinates */
  RECT16 upd[UPD_MAX];                /* update region, client coordinates */
  char text[UTEXT_MAX];
} UWnd;

typedef struct UClass {
  uint8_t used, pad;
  uint16_t style;
  uint32_t wndproc;
  int16_t cbcls, cbwnd;
  uint16_t hinst, hicon, hcursor, hbr;
  uint32_t menu;
  char name[UCLS_NAME];
} UClass;

typedef struct UMsg {
  HWND16 hwnd;
  uint16_t msg, wp;
  uint32_t lp, time;
  int16_t x, y;
} UMsg;

typedef struct UserState {
  UWnd w[UW_MAX];
  UClass c[UC_MAX];
  UMsg q[UQ_MAX];
  uint16_t qhead, qcount;
  uint8_t quit_pending;
  int16_t quit_code;
  uint32_t clock; /* fake milliseconds: +1 per posted message, for MSG.time */
} UserState;

UserState *user_state(void);
UWnd *user_wnd(HWND16 h);          /* NULL when h is not a live window (the desktop is one) */
HWND16 user_hwnd(const UWnd *w);
HWND16 user_desktop(void);
void user_client_origin(const UWnd *w, int *x, int *y);   /* client origin in screen coordinates */
void user_window_origin(const UWnd *w, int *x, int *y);   /* window origin in screen coordinates */
bool user_visible(HWND16 h);       /* the window and every ancestor have WS_VISIBLE */
HWND16 user_first_paint(HWND16 parent);
void user_queue_purge(HWND16 hwnd);
uint32_t user_def_window_proc(HWND16 h, uint16_t msg, uint16_t wp, uint32_t lp);

/* Far scratch memory for message parameter blocks (global heap, freed by the caller). */
FARPTR user_scratch(uint32_t bytes, uint16_t *handle);
void user_scratch_free(uint16_t handle);

/* Update regions (user_paint.c). Rectangles are client coordinates. */
void user_upd_add(UWnd *w, RECT16 r, bool erase);
void user_upd_sub(UWnd *w, RECT16 r);
bool user_upd_bbox(const UWnd *w, RECT16 *out);
HDC16 user_get_dc(HWND16 h, const RECT16 *paint_clip);
uint32_t user_def_erase(HWND16 h, HDC16 hdc);
uint32_t user_def_paint(HWND16 h);

static inline bool r_empty(RECT16 r) { return r.right <= r.left || r.bottom <= r.top; }
static inline RECT16 r_make(int l, int t, int r, int b) {
  RECT16 x = {(int16_t)l, (int16_t)t, (int16_t)r, (int16_t)b};
  return x;
}
static inline RECT16 r_offset(RECT16 r, int dx, int dy) { return r_make(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy); }
static inline RECT16 r_isect(RECT16 a, RECT16 b) {
  RECT16 r = r_make(a.left > b.left ? a.left : b.left, a.top > b.top ? a.top : b.top,
                    a.right < b.right ? a.right : b.right, a.bottom < b.bottom ? a.bottom : b.bottom);
  if (r_empty(r)) r = r_make(0, 0, 0, 0);
  return r;
}
static inline RECT16 r_union(RECT16 a, RECT16 b) {
  if (r_empty(a)) return b;
  if (r_empty(b)) return a;
  return r_make(a.left < b.left ? a.left : b.left, a.top < b.top ? a.top : b.top,
                a.right > b.right ? a.right : b.right, a.bottom > b.bottom ? a.bottom : b.bottom);
}
/* a minus b as up to 4 disjoint rectangles; returns the count. */
static inline int r_subtract(RECT16 a, RECT16 b, RECT16 out[4]) {
  RECT16 i = r_isect(a, b);
  int n = 0;
  if (r_empty(a)) return 0;
  if (r_empty(i)) {
    out[0] = a;
    return 1;
  }
  if (i.top > a.top) out[n++] = r_make(a.left, a.top, a.right, i.top);
  if (i.bottom < a.bottom) out[n++] = r_make(a.left, i.bottom, a.right, a.bottom);
  if (i.left > a.left) out[n++] = r_make(a.left, i.top, i.left, i.bottom);
  if (i.right < a.right) out[n++] = r_make(i.right, i.top, a.right, i.bottom);
  return n;
}
static inline bool r_contains(RECT16 r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }
static inline uint32_t make_long(uint16_t lo, uint16_t hi) { return (uint32_t)lo | ((uint32_t)hi << 16); }

#endif /* STARS_RUNTIME_USER_INTERNAL_H */
