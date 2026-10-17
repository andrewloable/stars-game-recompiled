/* Expectations for the EDIT control class (runtime/ctl_edit.c). Task stars-0z4.11, written BEFORE
 * the implementation. Do not edit an expectation to match the code: a disagreement is reported with
 * both values.
 *
 * Sources (the Windows 3.1 SDK text is NOT available here and no reference capture exists):
 *   WINE = wine-mirror/wine commit 5b6b85421c2a2161728b2a55e9f09271770a3eb0
 *   Win16 message numbers  include/wine/winuser16.h lines 455-485: EM_GETSEL16 WM_USER+0, EM_SETSEL16
 *       +1, EM_GETRECT16 +2, EM_SETRECT16 +3, EM_SETRECTNP16 +4, EM_SCROLL16 +5, EM_LINESCROLL16 +6,
 *       EM_SCROLLCARET16 +7, EM_GETMODIFY16 +8, EM_SETMODIFY16 +9, EM_GETLINECOUNT16 +10,
 *       EM_LINEINDEX16 +11, EM_SETHANDLE16 +12, EM_GETHANDLE16 +13, EM_GETTHUMB16 +14,
 *       EM_LINELENGTH16 +17, EM_REPLACESEL16 +18, EM_GETLINE16 +20, EM_LIMITTEXT16 +21,
 *       EM_CANUNDO16 +22, EM_UNDO16 +23, EM_FMTLINES16 +24, EM_LINEFROMCHAR16 +25,
 *       EM_SETTABSTOPS16 +27, EM_SETPASSWORDCHAR16 +28, EM_EMPTYUNDOBUFFER16 +29,
 *       EM_GETFIRSTVISIBLELINE16 +30, EM_SETREADONLY16 +31, EM_SETWORDBREAKPROC16 +32,
 *       EM_GETWORDBREAKPROC16 +33, EM_GETPASSWORDCHAR16 +34.  Win32 uses 0x00B0.. : NOT used.
 *   Win16 packing          EM_GETSEL returns MAKELONG(start, end); EM_SETSEL takes lParam =
 *       MAKELONG(start, end) (issue text); the notification is WM_COMMAND wParam = id, LOWORD(lParam)
 *       = control, HIWORD(lParam) = code (same as part 1).
 *   Styles / codes         include/winuser.rh 1134-1148 (ES_*), include/winuser.h 957-964 (EN_*):
 *       EN_SETFOCUS 0x100, EN_KILLFOCUS 0x200, EN_CHANGE 0x300, EN_UPDATE 0x400, EN_ERRSPACE 0x500,
 *       EN_MAXTEXT 0x501. UNVERIFIED for Windows 3.1 (shared low values).
 *
 * UNVERIFIED (no SDK text, no capture; this runtime's choices):
 *   - Every pixel expectation (text origin, caret width 1 pixel, caret blink 530 ms [WIN.INI
 *     CursorBlinkRate default recalled from memory], selection colours COLOR_HIGHLIGHT /
 *     COLOR_HIGHLIGHTTEXT, a gray label when disabled). Each pixel test names the PNG it waits for.
 *   - EN_UPDATE then EN_CHANGE order per edit (Wine edit.c EDIT_NOTIFY_PARENT after
 *     EDIT_EM_ReplaceSel: EN_UPDATE before the redraw, EN_CHANGE after).
 *   - WM_SETTEXT sends EN_UPDATE and EN_CHANGE to the parent (Wine EDIT_WM_SetText).
 *   - Default text limit 32766 characters; EM_LIMITTEXT(0) restores it.
 *   - Not implemented (documented): ES_MULTILINE (no dialog template uses it), ES_CENTER / ES_RIGHT of
 *     single-line edits, undo (EM_CANUNDO returns 0), EM_SETWORDBREAKPROC, ES_OEMCONVERT, the I-beam
 *     cursor.
 */
#include "test.h"

#include "ctl2_test_common.h"

#define LM 1 /* left margin of the text in pixels (UNVERIFIED) */

static HWND16 E(const char *label, const char *text, uint32_t style, int x, int y, int w, int h, uint16_t id) {
  return btn(label, "EDIT", text, style | WS_TABSTOP, x, y, w, h, id);
}

static void sel_is(HWND16 e, int s, int t) {
  uint32_t r = SM(e, EM_GETSEL, 0, 0);
  CHECK_EQ(r & 0xFFFF, (uint32_t)s);
  CHECK_EQ(r >> 16, (uint32_t)t);
}

static void test_constants(void) {
  CHECK_EQ(EM_GETSEL, 0x0400);
  CHECK_EQ(EM_SETSEL, 0x0401);
  CHECK_EQ(EM_GETRECT, 0x0402);
  CHECK_EQ(EM_SETRECT, 0x0403);
  CHECK_EQ(EM_SETRECTNP, 0x0404);
  CHECK_EQ(EM_SCROLL, 0x0405);
  CHECK_EQ(EM_LINESCROLL, 0x0406);
  CHECK_EQ(EM_SCROLLCARET, 0x0407);
  CHECK_EQ(EM_GETMODIFY, 0x0408);
  CHECK_EQ(EM_SETMODIFY, 0x0409);
  CHECK_EQ(EM_GETLINECOUNT, 0x040A);
  CHECK_EQ(EM_LINEINDEX, 0x040B);
  CHECK_EQ(EM_SETHANDLE, 0x040C);
  CHECK_EQ(EM_GETHANDLE, 0x040D);
  CHECK_EQ(EM_GETTHUMB, 0x040E);
  CHECK_EQ(EM_LINELENGTH, 0x0411);
  CHECK_EQ(EM_REPLACESEL, 0x0412);
  CHECK_EQ(EM_GETLINE, 0x0414);
  CHECK_EQ(EM_LIMITTEXT, 0x0415);
  CHECK_EQ(EM_CANUNDO, 0x0416);
  CHECK_EQ(EM_UNDO, 0x0417);
  CHECK_EQ(EM_FMTLINES, 0x0418);
  CHECK_EQ(EM_LINEFROMCHAR, 0x0419);
  CHECK_EQ(EM_SETTABSTOPS, 0x041B);
  CHECK_EQ(EM_SETPASSWORDCHAR, 0x041C);
  CHECK_EQ(EM_EMPTYUNDOBUFFER, 0x041D);
  CHECK_EQ(EM_GETFIRSTVISIBLELINE, 0x041E);
  CHECK_EQ(EM_SETREADONLY, 0x041F);
  CHECK_EQ(EM_SETWORDBREAKPROC, 0x0420);
  CHECK_EQ(EM_GETWORDBREAKPROC, 0x0421);
  CHECK_EQ(EM_GETPASSWORDCHAR, 0x0422);
  CHECK_EQ(ES_MULTILINE, 4);
  CHECK_EQ(ES_UPPERCASE, 8);
  CHECK_EQ(ES_LOWERCASE, 0x10);
  CHECK_EQ(ES_PASSWORD, 0x20);
  CHECK_EQ(ES_AUTOHSCROLL, 0x80);
  CHECK_EQ(ES_READONLY, 0x800);
  CHECK_EQ(EN_SETFOCUS, 0x100);
  CHECK_EQ(EN_KILLFOCUS, 0x200);
  CHECK_EQ(EN_CHANGE, 0x300);
  CHECK_EQ(EN_UPDATE, 0x400);
  CHECK_EQ(EN_MAXTEXT, 0x501);
  CHECK_EQ(USER_CB_EDIT, MK_FP(MEM_SEL_CALLBACK, 0x50));
}

static void test_class_and_text(void) {
  HWND16 e;
  FARPTR p;
  begin();
  e = E("E", "Hello", ES_AUTOHSCROLL, 10, 10, 120, 16, 101);
  CHECK_EQ(GetWindowLong(e, GWL_WNDPROC), USER_CB_EDIT);
  p = far_alloc(32);
  CHECK_EQ(GetWindowText(e, p, 32), 5);
  CHECK_EQ(strcmp(LIN(p), "Hello"), 0);
  CHECK_EQ(SM(e, WM_GETTEXTLENGTH, 0, 0), 5);
  /* WM_GETTEXT with a small buffer truncates and returns the count copied */
  CHECK_EQ(GetWindowText(e, p, 4), 3);
  CHECK_EQ(strcmp(LIN(p), "Hel"), 0);
  CHECK_EQ(SM(e, WM_GETDLGCODE, 0, 0), DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS);
  end();
}

static void test_settext_notifies_and_long_text(void) {
  HWND16 e;
  char big[300];
  begin();
  e = E("E", "", ES_AUTOHSCROLL, 10, 10, 120, 16, 102);
  log_clear();
  CHECK_EQ(SMS(e, WM_SETTEXT, 0, "abc"), 1);
  CHECK_EQ(strcmp(text_of(e), "abc"), 0);
  CHECK_CODES("0400,0300"); /* EN_UPDATE then EN_CHANGE */
  CHECK_EQ(cmd(0)->wp, 102);
  CHECK_EQ((uint16_t)cmd(0)->lp, e);
  /* the window text is not limited to the 79 characters of the generic window text */
  memset(big, 'x', 250);
  big[250] = 0;
  SMS(e, WM_SETTEXT, 0, big);
  CHECK_EQ(SM(e, WM_GETTEXTLENGTH, 0, 0), 250);
  CHECK_EQ(strlen(text_of(e)), 250u);
  /* the initial text of CreateWindow is the control text */
  end();
}

static void test_typing(void) {
  HWND16 e;
  begin();
  e = E("E", "", ES_AUTOHSCROLL, 10, 10, 120, 16, 103);
  SM(e, WM_CHAR, 'a', 0);
  SM(e, WM_CHAR, 'b', 0);
  SM(e, WM_CHAR, 'c', 0);
  CHECK_EQ(strcmp(text_of(e), "abc"), 0);
  CHECK_EQ(count_code(EN_CHANGE), 3);
  CHECK_EQ(count_code(EN_UPDATE), 3);
  CHECK_CODES("0400,0300,0400,0300,0400,0300");
  sel_is(e, 3, 3);
  CHECK_EQ(SM(e, EM_GETMODIFY, 0, 0), 1);
  end();
}

static void test_real_keyboard(void) {
  HWND16 e;
  begin();
  e = E("E", "", ES_AUTOHSCROLL, 10, 10, 120, 16, 104);
  SetFocus(e);
  log_clear();
  tap('A'); /* WM_KEYDOWN -> TranslateMessage -> WM_CHAR 'a' (no Shift) */
  tap('B');
  tap_with(VK_SHIFT, 'C'); /* 'C' */
  CHECK_EQ(strcmp(text_of(e), "abC"), 0);
  tap(VK_BACK);
  CHECK_EQ(strcmp(text_of(e), "ab"), 0);
  tap(VK_LEFT);
  sel_is(e, 1, 1);
  tap(VK_DELETE); /* deletes the character after the caret */
  CHECK_EQ(strcmp(text_of(e), "a"), 0);
  tap(VK_HOME);
  sel_is(e, 0, 0);
  tap(VK_END);
  sel_is(e, 1, 1);
  end();
}

static void test_limittext_and_maxtext(void) {
  HWND16 e;
  begin();
  e = E("E", "", ES_AUTOHSCROLL, 10, 10, 120, 16, 105);
  CHECK_EQ(SM(e, EM_LIMITTEXT, 2, 0), 0);
  SM(e, WM_CHAR, 'a', 0);
  SM(e, WM_CHAR, 'b', 0);
  log_clear();
  SM(e, WM_CHAR, 'c', 0); /* rejected */
  CHECK_EQ(strcmp(text_of(e), "ab"), 0);
  CHECK_EQ(count_code(EN_MAXTEXT), 1);
  CHECK_EQ(count_code(EN_CHANGE), 0);
  /* EM_REPLACESEL beyond the limit is cut and reports EN_MAXTEXT too */
  SM(e, EM_SETSEL, 0, MKL(0, 2));
  log_clear();
  SMS(e, EM_REPLACESEL, 0, "xyz");
  CHECK_EQ(strcmp(text_of(e), "xy"), 0);
  CHECK_EQ(count_code(EN_MAXTEXT), 1);
  /* limit 0 means the default (large) limit */
  SM(e, EM_LIMITTEXT, 0, 0);
  SM(e, WM_CHAR, 'z', 0);
  CHECK_EQ(strcmp(text_of(e), "xyz"), 0);
  end();
}

static void test_selection_messages(void) {
  HWND16 e;
  begin();
  e = E("E", "hello", ES_AUTOHSCROLL, 10, 10, 120, 16, 106);
  sel_is(e, 0, 0);
  SM(e, EM_SETSEL, 0, MKL(1, 3));
  sel_is(e, 1, 3);
  SMS(e, EM_REPLACESEL, 0, "XY");
  CHECK_EQ(strcmp(text_of(e), "hXYlo"), 0);
  sel_is(e, 3, 3);
  SM(e, EM_SETSEL, 0, MKL(0, 0xFFFF)); /* end 0xFFFF: to the end of the text */
  sel_is(e, 0, 5);
  SM(e, EM_SETSEL, 0, MKL(4, 1)); /* start after end: the selection is normalised in EM_GETSEL */
  {
    uint32_t r = SM(e, EM_GETSEL, 0, 0);
    CHECK(((r & 0xFFFF) == 1 && (r >> 16) == 4) || ((r & 0xFFFF) == 4 && (r >> 16) == 1));
  }
  SM(e, EM_SETSEL, 0, MKL(2, 99)); /* clamped to the length */
  sel_is(e, 2, 5);
  /* typing replaces the selection */
  SM(e, WM_CHAR, 'Q', 0);
  CHECK_EQ(strcmp(text_of(e), "hXQ"), 0);
  sel_is(e, 3, 3);
  /* Backspace on a selection deletes the selection only */
  SM(e, EM_SETSEL, 0, MKL(0, 2));
  SM(e, WM_CHAR, 8, 0);
  CHECK_EQ(strcmp(text_of(e), "Q"), 0);
  end();
}

static void test_shift_selection_and_words(void) {
  HWND16 e;
  begin();
  e = E("E", "one two three", ES_AUTOHSCROLL, 10, 10, 160, 16, 107);
  SetFocus(e);
  SM(e, EM_SETSEL, 0, MKL(0, 0));
  tap_with(VK_SHIFT, VK_RIGHT);
  tap_with(VK_SHIFT, VK_RIGHT);
  sel_is(e, 0, 2);
  tap_with(VK_SHIFT, VK_END);
  sel_is(e, 0, 13);
  tap(VK_LEFT); /* a plain arrow collapses the selection at the caret and moves it */
  sel_is(e, 12, 12);
  tap(VK_HOME);
  tap_with(VK_CONTROL, VK_RIGHT); /* next word */
  sel_is(e, 4, 4);
  end();
}

static void test_clipboard(void) {
  HWND16 e, f;
  begin();
  e = E("E", "hello", ES_AUTOHSCROLL, 10, 10, 120, 16, 108);
  f = E("F", "", ES_AUTOHSCROLL, 10, 40, 120, 16, 109);
  SetFocus(e);
  SM(e, EM_SETSEL, 0, MKL(1, 3));
  tap_with(VK_CONTROL, VK_INSERT); /* copy "el" */
  SM(e, EM_SETSEL, 0, MKL(5, 5));
  tap_with(VK_SHIFT, VK_INSERT); /* paste */
  CHECK_EQ(strcmp(text_of(e), "helloel"), 0);
  SM(e, EM_SETSEL, 0, MKL(0, 2));
  tap_with(VK_SHIFT, VK_DELETE); /* cut "he" */
  CHECK_EQ(strcmp(text_of(e), "lloel"), 0);
  /* the clipboard is shared between edits */
  SetFocus(f);
  tap_with(VK_SHIFT, VK_INSERT);
  CHECK_EQ(strcmp(text_of(f), "he"), 0);
  /* the messages do the same */
  SM(e, EM_SETSEL, 0, MKL(0, 3));
  SM(e, WM_COPY, 0, 0);
  CHECK_EQ(strcmp(text_of(e), "lloel"), 0);
  SM(e, WM_CUT, 0, 0);
  CHECK_EQ(strcmp(text_of(e), "el"), 0);
  SM(f, WM_PASTE, 0, 0);
  CHECK_EQ(strcmp(text_of(f), "hello"), 0);
  SM(f, EM_SETSEL, 0, MKL(0, 1));
  SM(f, WM_CLEAR, 0, 0);
  CHECK_EQ(strcmp(text_of(f), "ello"), 0);
  end();
}

static void test_password_and_case_and_readonly(void) {
  HWND16 p, q, u, l, r;
  begin();
  p = E("P1", "ab", ES_PASSWORD | ES_AUTOHSCROLL, 10, 10, 100, 16, 110);
  q = E("Q1", "**", ES_AUTOHSCROLL, 10, 40, 100, 16, 111);
  (void)q;
  pump();
  PIXEL_TEST("ctl_edit_password");
  CHECK_EQ(strcmp(text_of(p), "ab"), 0); /* the text is the real one */
  CHECK_EQ(SM(p, EM_GETPASSWORDCHAR, 0, 0), '*');
  CHECK(same_pixels(10, 10, 10, 40, 100, 16)); /* ES_PASSWORD renders '*' characters */
  CHECK(cbox(10, 10, 110, 26, BLACK).n > 4);
  SM(p, EM_SETPASSWORDCHAR, '#', 0);
  CHECK_EQ(SM(p, EM_GETPASSWORDCHAR, 0, 0), '#');
  pump();
  CHECK(!same_pixels(10, 10, 10, 40, 100, 16));
  SM(p, EM_SETPASSWORDCHAR, 0, 0); /* 0 shows the text */
  pump();
  CHECK_EQ(SM(p, EM_GETPASSWORDCHAR, 0, 0), 0);
  u = E("U", "", ES_UPPERCASE | ES_AUTOHSCROLL, 10, 70, 100, 16, 112);
  l = E("L", "", ES_LOWERCASE | ES_AUTOHSCROLL, 10, 100, 100, 16, 113);
  SM(u, WM_CHAR, 'a', 0);
  SM(l, WM_CHAR, 'A', 0);
  CHECK_EQ(strcmp(text_of(u), "A"), 0);
  CHECK_EQ(strcmp(text_of(l), "a"), 0);
  r = E("R", "ro", ES_READONLY | ES_AUTOHSCROLL, 10, 130, 100, 16, 114);
  log_clear();
  SM(r, WM_CHAR, 'x', 0);
  SM(r, WM_CHAR, 8, 0);
  CHECK_EQ(strcmp(text_of(r), "ro"), 0);
  CHECK_EQ(ncmd(), 0);
  CHECK_EQ(SM(r, EM_SETREADONLY, 0, 0), 1);
  SM(r, WM_CHAR, 'x', 0); /* the caret of a new edit is at 0 */
  CHECK_EQ(strcmp(text_of(r), "xro"), 0);
  CHECK_EQ(SM(r, EM_SETREADONLY, 1, 0), 1);
  CHECK(GetWindowLong(r, GWL_STYLE) & ES_READONLY);
  end();
}

static void test_focus_notifications(void) {
  HWND16 a, b;
  begin();
  a = E("A", "x", ES_AUTOHSCROLL, 10, 10, 100, 16, 115);
  b = E("B", "y", ES_AUTOHSCROLL, 10, 40, 100, 16, 116);
  log_clear();
  SetFocus(a);
  CHECK_CODES("0100");
  CHECK_EQ(cmd(0)->wp, 115);
  log_clear();
  SetFocus(b);
  CHECK_CODES("0200,0100"); /* a loses it, b gets it */
  CHECK_EQ(cmd(0)->wp, 115);
  CHECK_EQ(cmd(1)->wp, 116);
  end();
}

static void test_line_messages(void) {
  HWND16 e;
  FARPTR p;
  begin();
  e = E("E", "hello", ES_AUTOHSCROLL, 10, 10, 100, 16, 117);
  CHECK_EQ(SM(e, EM_GETLINECOUNT, 0, 0), 1);
  CHECK_EQ(SM(e, EM_LINEINDEX, 0, 0), 0);
  CHECK_EQ(SM(e, EM_LINEINDEX, 0xFFFF, 0), 0);
  CHECK_EQ(SM(e, EM_LINELENGTH, 0, 0), 5);
  CHECK_EQ(SM(e, EM_LINELENGTH, 0xFFFF, 0), 5);
  CHECK_EQ(SM(e, EM_LINEFROMCHAR, 3, 0), 0);
  CHECK_EQ(SM(e, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
  p = far_alloc(32);
  *(uint16_t *)LIN(p) = 32; /* EM_GETLINE: the first word of the buffer is its size */
  CHECK_EQ(SM(e, EM_GETLINE, 0, p), 5);
  CHECK_EQ(memcmp(LIN(p), "hello", 5), 0);
  *(uint16_t *)LIN(p) = 3;
  CHECK_EQ(SM(e, EM_GETLINE, 0, p), 3);
  CHECK_EQ(memcmp(LIN(p), "hel", 3), 0);
  CHECK_EQ(SM(e, EM_GETLINE, 1, p), 0); /* single line: line 1 does not exist */
  CHECK_EQ(SM(e, EM_GETMODIFY, 0, 0), 0);
  SM(e, EM_SETMODIFY, 1, 0);
  CHECK_EQ(SM(e, EM_GETMODIFY, 0, 0), 1);
  SM(e, EM_SETMODIFY, 0, 0);
  CHECK_EQ(SM(e, EM_GETMODIFY, 0, 0), 0);
  CHECK_EQ(SM(e, EM_CANUNDO, 0, 0), 0);
  CHECK_EQ(SM(e, EM_FMTLINES, 1, 0), 0);
  CHECK_EQ(SM(e, EM_GETTHUMB, 0, 0), 0);
  CHECK_EQ(SM(e, EM_SCROLL, 0, 0), 0);
  CHECK_EQ(SM(e, EM_SETHANDLE, 0, 0), 0);
  CHECK_EQ(SM(e, EM_GETHANDLE, 0, 0), 0);
  {
    FARPTR r = far_alloc(sizeof(RECT16));
    SM(e, EM_GETRECT, 0, r);
    CHECK(((RECT16 *)LIN(r))->right > ((RECT16 *)LIN(r))->left);
  }
  end();
}

static void test_mouse_selection(void) {
  HWND16 e;
  int x0 = 10;
  begin();
  e = E("E", "hello world", ES_AUTOHSCROLL, x0, 20, 160, 16, 118);
  underlay(0, 0, 200, 60);
  pump();
  click_at(x0 + 150, 28); /* far right of the text: the caret goes to the end */
  sel_is(e, 11, 11);
  CHECK_EQ(GetFocus(), e);
  click_at(x0 + 0, 28); /* at the left edge */
  sel_is(e, 0, 0);
  /* click between 'l' (index 2) and 'l': just right of the boundary at index 3 -> 3 */
  click_at(x0 + LM + text_w(e, "hello world", 3) + 1, 28);
  sel_is(e, 3, 3);
  /* drag from index 2 to index 6 selects 2..6 */
  mouse(PX0 + x0 + LM + text_w(e, "hello world", 2), PY0 + 28, 1);
  mouse(PX0 + x0 + LM + text_w(e, "hello world", 6), PY0 + 28, 1);
  mouse(PX0 + x0 + LM + text_w(e, "hello world", 6), PY0 + 28, 0);
  sel_is(e, 2, 6);
  /* Shift+click extends from the anchor */
  key(VK_SHIFT, 0x2A, 1);
  click_at(x0 + LM + text_w(e, "hello world", 9), 28);
  key(VK_SHIFT, 0x2A, 0);
  sel_is(e, 2, 9);
  /* a double click selects the word under the pointer */
  dblclick_at(x0 + LM + text_w(e, "hello world", 8), 28);
  sel_is(e, 6, 11);
  end();
}

static void test_pixels_selection_caret(void) {
  HWND16 e;
  CBox b;
  int h;
  begin();
  e = E("E", "hello", ES_AUTOHSCROLL, 10, 10, 120, 20, 119);
  h = text_h(e);
  pump();
  PIXEL_TEST("ctl_edit_basic");
  PIXEL_EQ(60, 25, WHITE_); /* background: the parent's CTLCOLOR_EDIT brush, COLOR_WINDOW by default */
  PIXEL_EQ(10 + 119, 10 + 19, WHITE_);
  b = cbox(10, 10, 130, 30, BLACK);
  CHECK(b.n > 10);
  CHECK(b.l >= 10 + LM && b.l <= 10 + LM + 2);
  CHECK(b.r < 10 + LM + text_w(e, "hello", 5) + 2);
  CHECK(b.b - b.t < h + 2);
  CHECK(b.t >= 10 && b.b < 30); /* vertically inside the control */
  /* unfocused, a selection is not shown */
  SM(e, EM_SETSEL, 0, MKL(1, 3));
  pump();
  CHECK_EQ(cbox(10, 10, 130, 30, HIGHLIGHT_).n, 0);
  /* focused: the selection is highlighted (COLOR_HIGHLIGHT) with COLOR_HIGHLIGHTTEXT text */
  SetFocus(e);
  pump();
  b = cbox(10, 10, 130, 30, HIGHLIGHT_);
  CHECK(b.n > 20);
  CHECK(b.l >= 10 + LM + text_w(e, "hello", 1) - 1 && b.l <= 10 + LM + text_w(e, "hello", 1) + 1);
  CHECK(b.r >= 10 + LM + text_w(e, "hello", 3) - 2 && b.r <= 10 + LM + text_w(e, "hello", 3) + 1);
  CHECK(cbox(10, 10, 130, 30, HIGHLIGHTTXT_).n > 5);
  PIXEL_TEST("ctl_edit_selection");
  dump_frame("ctl_edit_selection", PX0, PY0, 140, 40);
  /* the caret: with no selection and the focus, a 1 pixel wide vertical line at the caret in the
   * text colour, as tall as the text; it is also visible while the selection is empty */
  SM(e, EM_SETSEL, 0, MKL(5, 5));
  pump();
  CHECK_EQ(cbox(10, 10, 130, 30, HIGHLIGHT_).n, 0);
  {
    int cx = 10 + LM + text_w(e, "hello", 5), run = 0, best = 0;
    for (int x = cx - 1; x <= cx + 2; x++) {
      run = 0;
      for (int y = 10; y < 30; y++) {
        run = pc(x, y) == BLACK ? run + 1 : 0;
        if (run > best) best = run;
      }
    }
    CHECK(best >= h - 2);
  }
  /* ES_NOHIDESEL keeps the selection visible without the focus */
  {
    HWND16 n = E("N", "world", ES_AUTOHSCROLL | ES_NOHIDESEL, 10, 40, 120, 20, 120);
    SM(n, EM_SETSEL, 0, MKL(0, 3));
    pump();
    CHECK(cbox(10, 40, 130, 60, HIGHLIGHT_).n > 10);
  }
  /* disabled: gray text */
  EnableWindow(e, 0);
  pump();
  CHECK_EQ(cbox(10, 10, 130, 30, BLACK).n, 0);
  CHECK(cbox(10, 10, 130, 30, GRAYTXT).n > 5);
  end();
}

static void test_no_autohscroll_rejects(void) {
  HWND16 e;
  int n = 0;
  begin();
  e = E("E", "", 0, 10, 10, 40, 16, 121); /* 40 pixels wide, no ES_AUTOHSCROLL */
  for (int i = 0; i < 20; i++) SM(e, WM_CHAR, 'W', 0);
  n = (int)strlen(text_of(e));
  CHECK(n >= 1 && n < 8); /* stops when the text no longer fits */
  CHECK(text_w(e, "WWWWWWWWWWWWWWWWWWWW", n) <= 40);
  {
    HWND16 a = E("A", "", ES_AUTOHSCROLL, 10, 40, 40, 16, 122);
    for (int i = 0; i < 20; i++) SM(a, WM_CHAR, 'W', 0);
    CHECK_EQ(strlen(text_of(a)), 20u); /* scrolls instead */
    pump();
    /* the caret is at the end and scrolled into view: text pixels near the right edge */
    SetFocus(a);
    pump();
    /* scrolled so that the last characters and the caret show: the text ends inside the control
     * (an unscrolled run of 20 'W' would fill it to the last pixel column) */
    {
      CBox bb = cbox(10, 40, 50, 56, BLACK);
      CHECK(bb.n > 10);
      CHECK(bb.r >= 10 + 20 && bb.r <= 10 + 38);
    }
  }
  end();
}

static void test_subclass(void) {
  HWND16 e;
  FARPTR old;
  begin();
  e = E("E", "Hello", ES_AUTOHSCROLL, 10, 10, 100, 16, 123);
  old = SetWindowLong(e, GWL_WNDPROC, FP_PROC_B);
  CHECK_EQ(old, USER_CB_EDIT);
  g_old = old;
  log_clear();
  SMS(e, WM_SETTEXT, 0, "Other");
  CHECK(log_find("E", WM_SETTEXT, 0) != NULL);
  CHECK_EQ(log_find("E", WM_SETTEXT, 0)->tag, 'B');
  CHECK_EQ(strcmp(text_of(e), "Other"), 0);
  /* the subclass sees EM_* and forwards: CallWindowProc reaches the class procedure */
  SM(e, EM_SETSEL, 0, MKL(1, 2));
  CHECK(log_find("E", EM_SETSEL, 0) != NULL);
  sel_is(e, 1, 2);
  SM(e, WM_CHAR, 'z', 0);
  CHECK_EQ(strcmp(text_of(e), "Ozher"), 0);
  SetWindowLong(e, GWL_WNDPROC, old);
  end();
}

static void test_win16_numbers_literal(void) {
  /* literal Win16 numbers, not the macros: the class must answer these and nothing else */
  HWND16 e;
  begin();
  e = E("E", "abcdef", ES_AUTOHSCROLL, 10, 10, 100, 16, 124);
  SM(e, 0x0401, 0, MKL(2, 4)); /* EM_SETSEL */
  CHECK_EQ(SM(e, 0x0400, 0, 0), MKL(2, 4)); /* EM_GETSEL */
  SM(e, 0x0415, 3, 0);                      /* EM_LIMITTEXT */
  SM(e, 0x0401, 0, MKL(6, 6));
  SM(e, WM_CHAR, 'x', 0);
  CHECK_EQ(strcmp(text_of(e), "abcdef"), 0);
  /* the Win32 numbers do nothing (EM_SETSEL = 0x00B1, EM_GETSEL = 0x00B0) */
  SM(e, 0x00B1, 0, 3);
  CHECK_EQ(SM(e, 0x00B0, 0, 0), 0);
  end();
}

int main(void) {
  (void)dump;
  if (load_fixture() != 0) return 1;
  test_constants();
  test_class_and_text();
  test_settext_notifies_and_long_text();
  test_typing();
  test_real_keyboard();
  test_limittext_and_maxtext();
  test_selection_messages();
  test_shift_selection_and_words();
  test_clipboard();
  test_password_and_case_and_readonly();
  test_focus_notifications();
  test_line_messages();
  test_mouse_selection();
  test_pixels_selection_caret();
  test_no_autohscroll_rejects();
  test_subclass();
  test_win16_numbers_literal();
  fprintf(stdout, "pixel tests: %d, all UNVERIFIED against Windows 3.1 (no tests/ref/NAME.png exists)\n", g_pixel_tests);
  fixture_close();
  return TEST_DONE();
}
