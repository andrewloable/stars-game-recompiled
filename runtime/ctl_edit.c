/* The EDIT control class (task stars-0z4.11): a single-line edit control with ES_AUTOHSCROLL,
 * ES_PASSWORD, ES_READONLY, ES_UPPERCASE / ES_LOWERCASE and ES_NOHIDESEL. Message numbers are the
 * Win16 ones (include/wine/winuser16.h at commit 5b6b85421c2a2161728b2a55e9f09271770a3eb0 lines
 * 455-485, cited in runtime/tests/test_ctl_edit.c); the notification is WM_COMMAND with wParam = id,
 * LOWORD(lParam) = control, HIWORD(lParam) = EN_*. Behaviour follows Wine's edit control and the
 * issue text; all of it is UNVERIFIED for Windows 3.1 (docs/inventory.md section 16).
 *
 * State: the control's extra bytes (EdX, in the arena) and one text block (rt_alloc, arena
 * offset kept in EdX). Single line only: ES_MULTILINE is not implemented (no dialog template of the
 * game uses it); the multi-line messages answer as a one-line control. The caret blinks with a
 * 530 ms timer (WIN.INI CursorBlinkRate default, from memory); the internal clipboard is shared by
 * all edit controls (runtime/ctl.c). Plain C, no harness dependency. */
#include <ctype.h>
#include <string.h>

#include "runtime/ctl.h"
#include "runtime/text.h"
#include "runtime/user_internal.h"

#define COL(i) user_syscolor(i)

#define ED_MAXLEN 32766u  /* default and largest text limit (UNVERIFIED) */
#define ED_LM 1           /* left margin of the text */
#define ED_TIMER 1
#define ED_BLINK_MS 530

#define EF_FOCUS 1
#define EF_MODIFIED 2
#define EF_CARET 4 /* caret currently visible (blink phase) */
#define EF_DRAG 8
#define EF_QUIET 16 /* programmatic change from a combo box: no notifications */
#define EF_COMBO 32 /* the edit field of a combo box: some keys go to the combo box */

typedef struct EdX {
  uint32_t buf;              /* arena offset of the text block (cap bytes) */
  uint16_t len, cap, limit;
  uint16_t left;             /* first visible character */
  uint16_t anchor, caret;    /* the selection is [min, max) */
  uint16_t font, flags, pwchar;
} EdX;
_Static_assert(sizeof(EdX) <= USER_EDIT_EXTRA, "EdX does not fit the EDIT cbWndExtra");

static EdX *ex(HWND16 h) { return user_ctl_extra(h, sizeof(EdX)); }
static char *tb(const EdX *x) { return (char *)(mem_arena() + x->buf); }
static unsigned selmin(const EdX *x) { return x->anchor < x->caret ? x->anchor : x->caret; }
static unsigned selmax(const EdX *x) { return x->anchor < x->caret ? x->caret : x->anchor; }

/* ---- the text block ---- */

static bool ensure(EdX *x, unsigned need) {
  unsigned cap;
  void *b;
  if (x->buf && need + 1u <= x->cap) return true;
  cap = (need + 1u + 63u) & ~63u;
  if (cap > 0xFFFFu) cap = 0xFFFFu;
  if (need + 1u > cap) return false;
  b = rt_alloc(cap);
  if (!b) return false;
  memset(b, 0, cap);
  if (x->buf) {
    memcpy(b, tb(x), (size_t)x->len + 1);
    rt_free(tb(x));
  }
  x->buf = mem_off(b);
  x->cap = (uint16_t)cap;
  return true;
}

static void sync_wtext(HWND16 h) { /* the generic window text keeps the first characters (dumps, DefWindowProc) */
  UWnd *w = user_wnd(h);
  EdX *x = ex(h);
  size_t n;
  if (!w || !x || !x->buf) return;
  n = x->len < UTEXT_MAX - 1 ? x->len : UTEXT_MAX - 1;
  memcpy(w->text, tb(x), n);
  w->text[n] = 0;
}

void user_edit_release(HWND16 h) {
  EdX *x = ex(h);
  if (x && x->buf) rt_free(tb(x));
  if (x) x->buf = 0;
}

/* ---- geometry and drawing ---- */

static RECT16 client_of(const UWnd *w) { return r_make(0, 0, w->crect.right - w->crect.left, w->crect.bottom - w->crect.top); }

/* The characters as displayed (the password character replaces each one); returns a pointer to
 * either the text itself or a scratch buffer in out (size ED_MAXLEN + 1). */
static const char *shown(const EdX *x, char *out) {
  if (!x->pwchar) return tb(x);
  memset(out, (int)x->pwchar, x->len);
  out[x->len] = 0;
  return out;
}

static char g_pw[ED_MAXLEN + 2]; /* scratch for shown(): host memory, not state */

static int prefix_w(HDC16 dc, const EdX *x, unsigned n) { return user_ctl_text_width(dc, shown(x, g_pw), (int)n); }

static uint16_t pick_font(const EdX *x) { return user_ctl_pick_font(x->font); }

static bool sel_visible(const UWnd *w, const EdX *x) {
  return x->anchor != x->caret && ((x->flags & EF_FOCUS) || (w->style & ES_NOHIDESEL));
}

static void edit_draw(HWND16 h, HDC16 dc) {
  const UWnd *w = user_wnd(h);
  const EdX *x = ex(h);
  RECT16 rc;
  uint16_t brush, oldfont;
  int th, y;
  unsigned s0, s1, left;
  const char *t;
  bool enabled;
  if (!w || !x) return;
  rc = client_of(w);
  enabled = !(w->style & WS_DISABLED);
  brush = user_ctl_color(h, dc, CTLCOLOR_EDIT);
  user_ctl_fill_brush(dc, rc, brush);
  oldfont = SelectObject(dc, pick_font(x));
  th = user_ctl_text_height(dc);
  y = (rc.bottom - rc.top - th) / 2;
  if (y < 0) y = 0;
  t = shown(x, g_pw);
  left = x->left < x->len ? x->left : x->len;
  SetBkMode(dc, TRANSPARENT);
  if (!enabled) SetTextColor(dc, COL(17));
  s0 = s1 = 0;
  if (sel_visible(w, x)) {
    s0 = selmin(x);
    s1 = selmax(x);
  }
  {
    /* pieces: [left, s0) plain, [s0, s1) selected, [s1, len) plain */
    unsigned a = left, b;
    int base = prefix_w(dc, x, left);
    unsigned cuts[3] = {s0 > left ? s0 : left, s1 > left ? s1 : left, x->len};
    for (int piece = 0; piece < 3; piece++) {
      bool selp = piece == 1;
      int x0, x1;
      b = cuts[piece];
      if (b < a) b = a;
      if (b > a) {
        x0 = ED_LM + prefix_w(dc, x, a) - base;
        x1 = ED_LM + prefix_w(dc, x, b) - base;
        if (selp) {
          user_fill(dc, r_make(x0, y, x1, y + th), COL(13));
          SetTextColor(dc, COL(14));
        }
        user_ctl_text_out(dc, x0, y, t + a, (int)(b - a));
        if (selp) SetTextColor(dc, enabled ? COL(8) : COL(17));
      }
      a = b;
    }
    /* the caret: 1 pixel wide, as tall as the text, in the window text colour */
    if ((x->flags & EF_FOCUS) && (x->flags & EF_CARET) && x->anchor == x->caret && enabled) {
      unsigned c = x->caret < left ? left : x->caret;
      int cx = ED_LM + prefix_w(dc, x, c) - base;
      user_fill(dc, r_make(cx, y, cx + 1, y + th), COL(8));
    }
  }
  SelectObject(dc, oldfont);
}

static void paint_now(HWND16 h) {
  HDC16 dc;
  if (!user_wnd(h) || !IsWindowVisible(h)) return;
  dc = GetDC(h);
  if (!dc) return;
  edit_draw(h, dc);
  ReleaseDC(h, dc);
}

/* Scrolls horizontally so that the caret is visible (ES_AUTOHSCROLL only). */
static void scroll_caret(HWND16 h) {
  const UWnd *w = user_wnd(h);
  EdX *x = ex(h);
  HDC16 dc;
  uint16_t oldfont;
  int avail;
  if (!w || !x) return;
  if (!(w->style & ES_AUTOHSCROLL)) {
    x->left = 0;
    return;
  }
  avail = (w->crect.right - w->crect.left) - 2 * ED_LM - 1;
  dc = GetDC(h);
  if (!dc) return;
  oldfont = SelectObject(dc, pick_font(x));
  if (x->caret < x->left) x->left = x->caret;
  else
    while (x->left < x->caret && prefix_w(dc, x, x->caret) - prefix_w(dc, x, x->left) > avail) x->left++;
  /* use free room at the right when the text got shorter */
  while (x->left > 0 && prefix_w(dc, x, x->len) - prefix_w(dc, x, x->left - 1u) <= avail) x->left--;
  SelectObject(dc, oldfont);
  ReleaseDC(h, dc);
}

/* ---- notification ---- */

static void notify(HWND16 h, uint16_t code) {
  EdX *x = ex(h);
  if (x && (x->flags & EF_QUIET)) return;
  user_ctl_notify(h, code);
}

/* ---- editing ---- */

static void set_sel(HWND16 h, unsigned a, unsigned b) {
  EdX *x = ex(h);
  if (!x) return;
  if (a > x->len) a = x->len;
  if (b > x->len) b = x->len;
  x->anchor = (uint16_t)a;
  x->caret = (uint16_t)b;
}

/* Replaces [a, b) with n characters of ins, within the text limit; returns false when nothing
 * could be inserted. EN_UPDATE before the redraw, EN_CHANGE after (Wine edit.c). */
static void replace_range(HWND16 h, unsigned a, unsigned b, const char *ins, unsigned n, bool allow_notify) {
  EdX *x = ex(h);
  UWnd *w = user_wnd(h);
  unsigned room;
  bool maxtext = false;
  char *t;
  if (!x || !w) return;
  if (a > x->len) a = x->len;
  if (b > x->len) b = x->len;
  if (b < a) b = a;
  room = x->limit > x->len - (b - a) ? x->limit - (x->len - (b - a)) : 0;
  if (n > room) {
    n = room;
    maxtext = true;
  }
  /* without ES_AUTOHSCROLL the new text must fit the window (Windows beeps and ignores the input) */
  if (n && !(w->style & ES_AUTOHSCROLL)) {
    HDC16 dc = GetDC(h);
    if (dc) {
      uint16_t oldfont = SelectObject(dc, pick_font(x));
      int avail = (w->crect.right - w->crect.left) - 2 * ED_LM - 1;
      char *probe = rt_alloc(x->len + n + 2u);
      bool fits = true;
      if (probe) {
        const char *src = tb(x);
        unsigned tot = x->len - (b - a) + n;
        memcpy(probe, src, a);
        memcpy(probe + a, ins, n);
        memcpy(probe + a + n, src + b, x->len - b);
        if (x->pwchar) memset(probe, (int)x->pwchar, tot);
        fits = user_ctl_text_width(dc, probe, (int)tot) <= avail;
        rt_free(probe);
      }
      SelectObject(dc, oldfont);
      ReleaseDC(h, dc);
      if (!fits) {
        n = 0;
        maxtext = false;
        if (a == b) return;
      }
    }
  }
  if (!ensure(x, x->len - (b - a) + n)) {
    if (allow_notify) notify(h, EN_ERRSPACE);
    return;
  }
  t = tb(x);
  if (b - a != n) memmove(t + a + n, t + b, (size_t)x->len - b + 1u);
  memcpy(t + a, ins, n);
  x->len = (uint16_t)(x->len - (b - a) + n);
  t[x->len] = 0;
  x->anchor = x->caret = (uint16_t)(a + n);
  if (n || b > a) {
    x->flags |= EF_MODIFIED;
    sync_wtext(h);
    scroll_caret(h);
    if (allow_notify) notify(h, EN_UPDATE);
    if (!user_wnd(h)) return;
    paint_now(h);
    if (allow_notify) notify(h, EN_CHANGE);
    if (!user_wnd(h)) return;
  }
  if (maxtext && allow_notify) notify(h, EN_MAXTEXT);
}

static void set_text(HWND16 h, const char *s, unsigned n) {
  EdX *x = ex(h);
  if (!x) return;
  if (n > x->limit) n = x->limit;
  if (!ensure(x, n)) return;
  memcpy(tb(x), s, n);
  tb(x)[n] = 0;
  x->len = (uint16_t)n;
  x->anchor = x->caret = 0;
  x->left = 0;
  x->flags &= ~EF_MODIFIED;
  sync_wtext(h);
}

static void copy_sel(HWND16 h) {
  EdX *x = ex(h);
  if (x && x->anchor != x->caret) user_edit_clip_set(tb(x) + selmin(x), selmax(x) - selmin(x));
}

static bool readonly(const UWnd *w) { return (w->style & ES_READONLY) != 0; }

static void do_paste(HWND16 h) {
  const UWnd *w = user_wnd(h);
  EdX *x = ex(h);
  const char *s;
  unsigned n;
  if (!w || !x || readonly(w)) return;
  n = user_edit_clip_get(&s);
  if (!n) return;
  /* a single-line edit takes the clipboard up to the first line break */
  for (unsigned i = 0; i < n; i++)
    if (s[i] == '\r' || s[i] == '\n') {
      n = i;
      break;
    }
  {
    char *copy = rt_alloc(n + 1u);
    if (!copy) return;
    memcpy(copy, s, n);
    replace_range(h, selmin(x), selmax(x), copy, n, true);
    rt_free(copy);
  }
}

static void do_cut(HWND16 h) {
  const UWnd *w = user_wnd(h);
  EdX *x = ex(h);
  if (!w || !x || x->anchor == x->caret) return;
  copy_sel(h);
  if (!readonly(w)) replace_range(h, selmin(x), selmax(x), "", 0, true);
}

static void do_clear(HWND16 h) {
  const UWnd *w = user_wnd(h);
  EdX *x = ex(h);
  if (!w || !x || x->anchor == x->caret || readonly(w)) return;
  replace_range(h, selmin(x), selmax(x), "", 0, true);
}

/* ---- words ---- */

static unsigned word_start(const EdX *x, unsigned i) { /* Ctrl+Left */
  const char *t = tb(x);
  while (i > 0 && t[i - 1] == ' ') i--;
  while (i > 0 && t[i - 1] != ' ') i--;
  return i;
}
static unsigned word_next(const EdX *x, unsigned i) { /* Ctrl+Right: the start of the next word */
  const char *t = tb(x);
  while (i < x->len && t[i] != ' ') i++;
  while (i < x->len && t[i] == ' ') i++;
  return i;
}

/* ---- mouse ---- */

static unsigned index_at(HWND16 h, int px) {
  const EdX *x = ex(h);
  HDC16 dc;
  uint16_t oldfont;
  unsigned i, best;
  int base, prev;
  if (!x) return 0;
  dc = GetDC(h);
  if (!dc) return 0;
  oldfont = SelectObject(dc, pick_font(x));
  base = prefix_w(dc, x, x->left);
  prev = 0;
  best = x->len;
  for (i = x->left; i < x->len; i++) {
    int nxt = prefix_w(dc, x, i + 1) - base, rel = px - ED_LM;
    if (rel < (prev + nxt + 1) / 2) {
      best = i;
      break;
    }
    prev = nxt;
  }
  SelectObject(dc, oldfont);
  ReleaseDC(h, dc);
  return best;
}

/* ---- the window procedure ---- */

static void focus_changed(HWND16 h, bool on) {
  EdX *x = ex(h);
  if (!x) return;
  if (on) {
    x->flags |= EF_FOCUS | EF_CARET;
    SetTimer(h, ED_TIMER, ED_BLINK_MS, 0);
  } else {
    x->flags &= ~(uint16_t)(EF_FOCUS | EF_CARET);
    KillTimer(h, ED_TIMER);
  }
  paint_now(h);
}

static uint32_t em_message(HWND16 h, UWnd *w, EdX *x, uint16_t msg, uint16_t wp, uint32_t lp) {
  switch (msg) {
    case EM_GETSEL: return make_long((uint16_t)selmin(x), (uint16_t)selmax(x));
    case EM_SETSEL: {
      /* wParam is ignored. Wine's EM_SETSEL16 treats it as "do not scroll the caret into view"
       * (from memory, UNVERIFIED); this runtime always scrolls. The game is believed to pass 0
       * there, but no call site was checked (the ghidra server was not reachable when this was
       * written): docs/inventory.md section 8 lists 0x401 senders at 5:0x3400, 9:0x43ab ... */
      unsigned a = lp & 0xFFFF, b = lp >> 16;
      if (a == 0xFFFF) { /* start -1: no selection, the caret stays */
        set_sel(h, x->caret, x->caret);
      } else {
        if (b == 0xFFFF) b = x->len;
        set_sel(h, a, b);
      }
      scroll_caret(h);
      paint_now(h);
      return 0;
    }
    case EM_GETRECT:
    case EM_SETRECT:
    case EM_SETRECTNP:
      if (msg == EM_GETRECT && lp && mem_check(lp, sizeof(RECT16))) {
        RECT16 rc = client_of(w);
        rc.left = (int16_t)(rc.left + ED_LM);
        rc.right = (int16_t)(rc.right - ED_LM);
        *(RECT16 *)LIN(lp) = rc;
      }
      return 0;
    case EM_SCROLL: return 0;
    case EM_LINESCROLL: { /* horizontal columns in the high word (single line: no vertical scroll) */
      int dx = (int16_t)(lp >> 16), left = (int)x->left + dx;
      if (left < 0) left = 0;
      if (left > x->len) left = x->len;
      x->left = (uint16_t)left;
      paint_now(h);
      return 0;
    }
    case EM_SCROLLCARET:
      scroll_caret(h);
      paint_now(h);
      return 0;
    case EM_GETMODIFY: return (x->flags & EF_MODIFIED) ? 1 : 0;
    case EM_SETMODIFY:
      x->flags = (uint16_t)(wp ? (x->flags | EF_MODIFIED) : (x->flags & ~EF_MODIFIED));
      return 0;
    case EM_GETLINECOUNT: return 1;
    case EM_LINEINDEX: return (wp == 0 || wp == 0xFFFF) ? 0 : (uint32_t)-1;
    case EM_SETHANDLE:
    case EM_GETHANDLE:
    case EM_GETTHUMB:
    case EM_FMTLINES:
    case EM_SETTABSTOPS:
    case EM_SETWORDBREAKPROC:
    case EM_GETWORDBREAKPROC:
    case EM_EMPTYUNDOBUFFER:
    case EM_CANUNDO:
    case EM_UNDO:
      return 0;
    case EM_LINELENGTH: return x->len;
    case EM_REPLACESEL: {
      unsigned n = 0;
      const char *s = user_ctl_far_str((FARPTR)lp, ED_MAXLEN, &n);
      char *copy;
      if (!s) {
        s = "";
        n = 0;
      }
      copy = rt_alloc(n + 1u);
      if (!copy) return 0;
      memcpy(copy, s, n);
      replace_range(h, selmin(x), selmax(x), copy, n, true);
      rt_free(copy);
      return 0;
    }
    case EM_GETLINE: {
      unsigned cap, n;
      if (wp != 0 || !lp || !mem_check(lp, 2)) return 0;
      cap = *(uint16_t *)LIN(lp);
      n = x->len < cap ? x->len : cap;
      if (!mem_check(lp, n)) return 0;
      memcpy(LIN(lp), tb(x), n);
      return n;
    }
    case EM_LIMITTEXT:
      x->limit = (uint16_t)(wp && wp < ED_MAXLEN ? wp : ED_MAXLEN);
      return 0;
    case EM_LINEFROMCHAR: return 0;
    case EM_SETPASSWORDCHAR:
      x->pwchar = (uint16_t)(wp & 0xFF);
      if (x->pwchar) w->style |= ES_PASSWORD;
      else w->style &= ~(uint32_t)ES_PASSWORD;
      paint_now(h);
      return 0;
    case EM_GETPASSWORDCHAR: return x->pwchar;
    case EM_GETFIRSTVISIBLELINE: return 0;
    case EM_SETREADONLY:
      if (wp) w->style |= ES_READONLY;
      else w->style &= ~(uint32_t)ES_READONLY;
      return 1;
    default: return user_def_window_proc(h, msg, wp, lp);
  }
}

static void key_down(HWND16 h, UWnd *w, EdX *x, uint16_t vk) {
  bool shift = GetKeyState(VK_SHIFT) < 0, ctrl = GetKeyState(VK_CONTROL) < 0;
  unsigned c = x->caret;
  if ((x->flags & EF_COMBO) && (vk == VK_F4 || vk == VK_UP || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT ||
                                vk == VK_RETURN || vk == VK_ESCAPE)) {
    SendMessage(GetParent(h), WM_KEYDOWN, vk, 0); /* the combo box handles these */
    return;
  }
  switch (vk) {
    case VK_LEFT:
    case VK_RIGHT:
    case VK_HOME:
    case VK_END: {
      if (vk == VK_LEFT) c = ctrl ? word_start(x, c) : (c ? c - 1 : 0);
      else if (vk == VK_RIGHT) c = ctrl ? word_next(x, c) : (c < x->len ? c + 1 : c);
      else if (vk == VK_HOME) c = 0;
      else c = x->len;
      x->caret = (uint16_t)c;
      if (!shift) x->anchor = x->caret;
      scroll_caret(h);
      paint_now(h);
      return;
    }
    case VK_DELETE:
      if (shift) {
        do_cut(h);
      } else if (!readonly(w)) {
        if (x->anchor != x->caret) replace_range(h, selmin(x), selmax(x), "", 0, true);
        else if (c < x->len) replace_range(h, c, c + 1, "", 0, true);
      }
      return;
    case VK_INSERT:
      if (ctrl) copy_sel(h);
      else if (shift) do_paste(h);
      return;
    default: return;
  }
}

static void char_in(HWND16 h, UWnd *w, EdX *x, uint16_t ch) {
  if (ch == 8) { /* Backspace */
    if (readonly(w)) return;
    if (x->anchor != x->caret) replace_range(h, selmin(x), selmax(x), "", 0, true);
    else if (x->caret > 0) replace_range(h, x->caret - 1u, x->caret, "", 0, true);
    return;
  }
  if (ch < 0x20 || ch == 0x7F) return; /* Tab, Return, Escape, Ctrl+letter: nothing in a single-line edit */
  if (readonly(w)) return;
  {
    char c = (char)ch;
    if (w->style & ES_UPPERCASE) c = (char)toupper((unsigned char)c);
    else if (w->style & ES_LOWERCASE) c = (char)tolower((unsigned char)c);
    replace_range(h, selmin(x), selmax(x), &c, 1, true);
  }
}

uint32_t user_edit_proc(HWND16 h, uint16_t msg, uint16_t wp, uint32_t lp) {
  UWnd *w = user_wnd(h);
  EdX *x = ex(h);
  if (!w || !x) return user_def_window_proc(h, msg, wp, lp);
  switch (msg) {
    case WM_CREATE: {
      const CREATESTRUCT16 *cs = lp && mem_check(lp, sizeof *cs) ? LIN(lp) : NULL;
      unsigned n = 0;
      const char *s = cs ? user_ctl_far_str(cs->lpszName, ED_MAXLEN, &n) : NULL;
      x->limit = ED_MAXLEN;
      x->pwchar = (w->style & ES_PASSWORD) ? '*' : 0;
      if (!ensure(x, s ? n : 0)) return (uint32_t)-1;
      if (s) set_text(h, s, n);
      else set_text(h, "", 0);
      return 0;
    }
    case WM_NCDESTROY:
      user_edit_release(h);
      return user_def_window_proc(h, msg, wp, lp);
    case WM_DESTROY:
      KillTimer(h, ED_TIMER);
      return user_def_window_proc(h, msg, wp, lp);
    case WM_GETDLGCODE: return DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      uint16_t hp = 0;
      FARPTR pp = user_scratch(sizeof(PAINTSTRUCT16), &hp);
      HDC16 dc;
      if (!pp) return 0;
      dc = BeginPaint(h, pp);
      if (dc) edit_draw(h, dc);
      EndPaint(h, pp);
      user_scratch_free(hp);
      return 0;
    }
    case WM_ENABLE:
    case WM_SYSCOLORCHANGE:
      paint_now(h);
      return 0;
    case WM_GETTEXTLENGTH: return x->len;
    case WM_GETTEXT: {
      unsigned n = x->len;
      if (!wp || !lp) return 0;
      if (n > (unsigned)wp - 1u) n = (unsigned)wp - 1u;
      if (!mem_check(lp, n + 1u)) return 0;
      memcpy(LIN(lp), tb(x), n);
      ((char *)LIN(lp))[n] = 0;
      return n;
    }
    case WM_SETTEXT: {
      unsigned n = 0;
      const char *s = user_ctl_far_str((FARPTR)lp, ED_MAXLEN, &n);
      char *copy;
      if (!s) {
        s = "";
        n = 0;
      }
      copy = rt_alloc(n + 1u);
      if (!copy) return 0;
      memcpy(copy, s, n);
      set_text(h, copy, n);
      rt_free(copy);
      scroll_caret(h);
      notify(h, EN_UPDATE);
      if (!user_wnd(h)) return 1;
      paint_now(h);
      notify(h, EN_CHANGE);
      return 1;
    }
    case WM_SETFONT:
      x->font = wp;
      if (lp & 0xFFFF) InvalidateRect(h, 0, 1);
      return 0;
    case WM_GETFONT: return x->font;
    case WM_SETFOCUS:
      focus_changed(h, true);
      notify(h, EN_SETFOCUS);
      return 0;
    case WM_KILLFOCUS:
      if (x->flags & EF_DRAG) {
        x->flags &= ~(uint16_t)EF_DRAG;
        if (GetCapture() == h) ReleaseCapture();
      }
      focus_changed(h, false);
      notify(h, EN_KILLFOCUS);
      return 0;
    case WM_TIMER:
      if (wp == ED_TIMER) {
        x->flags ^= EF_CARET;
        if (x->anchor == x->caret) paint_now(h);
        return 0;
      }
      return user_def_window_proc(h, msg, wp, lp);
    case WM_KEYDOWN:
      key_down(h, w, x, wp);
      return 0;
    case WM_CHAR:
      char_in(h, w, x, wp);
      return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
      unsigned i = index_at(h, (int16_t)(lp & 0xFFFF));
      SetFocus(h);
      if (!user_wnd(h)) return 0;
      if (msg == WM_LBUTTONDBLCLK) { /* the word under the pointer */
        unsigned a = i, b = i;
        const char *t = tb(x);
        bool sp = i < x->len && t[i] == ' ';
        while (a > 0 && (t[a - 1] == ' ') == sp) a--;
        while (b < x->len && (t[b] == ' ') == sp) b++;
        set_sel(h, a, b);
      } else {
        x->caret = (uint16_t)i;
        if (!(wp & MK_SHIFT)) x->anchor = x->caret;
        x->flags |= EF_DRAG;
        SetCapture(h);
      }
      scroll_caret(h);
      paint_now(h);
      return 0;
    }
    case WM_MOUSEMOVE:
      if ((x->flags & EF_DRAG) && (wp & MK_LBUTTON)) {
        x->caret = (uint16_t)index_at(h, (int16_t)(lp & 0xFFFF));
        scroll_caret(h);
        paint_now(h);
      }
      return 0;
    case WM_LBUTTONUP:
      if (x->flags & EF_DRAG) {
        x->flags &= ~(uint16_t)EF_DRAG;
        if (GetCapture() == h) ReleaseCapture();
      }
      return 0;
    case WM_CANCELMODE:
      x->flags &= ~(uint16_t)EF_DRAG;
      return user_def_window_proc(h, msg, wp, lp);
    case WM_CUT:
      do_cut(h);
      return 0;
    case WM_COPY:
      copy_sel(h);
      return 0;
    case WM_PASTE:
      do_paste(h);
      return 0;
    case WM_CLEAR:
      do_clear(h);
      return 0;
    case WM_UNDO: return 0;
    default:
      if (msg >= WM_USER && msg <= EM_GETPASSWORDCHAR) return em_message(h, w, x, msg, wp, lp);
      return user_def_window_proc(h, msg, wp, lp);
  }
}

/* For the combo box: replaces the text and selects it all without notifying the parent. */
void user_edit_set_quiet(HWND16 h, const char *s, unsigned n, bool select_all) {
  EdX *x = ex(h);
  if (!x) return;
  x->flags |= EF_QUIET;
  set_text(h, s, n);
  if (select_all) set_sel(h, 0, n);
  scroll_caret(h);
  paint_now(h);
  x = ex(h);
  if (x) x->flags &= ~(uint16_t)EF_QUIET;
}

void user_edit_set_combo(HWND16 h) {
  EdX *x = ex(h);
  if (x) x->flags |= EF_COMBO;
}
