/* USER part 1 (task stars-0z4.6): window classes, window tree, messages, painting, as plain C
 * functions named and typed like the Win16 API. No harness dependency: the native harness
 * decodes the PASCAL stack into these calls in harness/user_thunks.c (user_register). Far
 * pointers are FARPTR (selector:offset); handles are 16 bits.
 *
 * Model (docs/PLAN.md section 3):
 *  - All state (window table, classes, message queue, update regions) lives in the arena
 *    (runtime/mem.h rt_alloc), so a snapshot of the arena is a snapshot of USER.
 *  - A window procedure is a far pointer: to original code (through rt_call_far), to
 *    decompiled C (registry, through rt_call_far), or to a runtime class procedure in the
 *    callback segment MEM_SEL_CALLBACK (USER_CB_DEFWNDPROC), which GetWindowLong(GWL_WNDPROC)
 *    can return so that subclassing works.
 *  - Messages are delivered with rt_call_far(proc, FAR_PASCAL, {hwnd, msg, wParam, lParam hi,
 *    lParam lo}). Message numbers are the Windows 3.1 (Win16) ones, never the Win32 ones.
 *
 * Message order and the other behaviours that no reference capture backs are listed in
 * docs/inventory.md section 13, each marked verified (SDK or Wine, with source) or UNVERIFIED.
 * Non-client painting and hit-testing belong to the Windows 3.1 look task (stars-0z4.9): here
 * the client rectangle equals the window rectangle unless a window procedure changes it in
 * WM_NCCALCSIZE. Input, focus, capture and timers belong to USER part 2 (stars-0z4.7).
 */
#ifndef STARS_RUNTIME_USER_H
#define STARS_RUNTIME_USER_H

#include <stdbool.h>
#include <stdint.h>

#include "runtime/gdi.h"
#include "runtime/include/win16.h"
#include "runtime/mem.h"

typedef uint16_t HWND16;

#pragma pack(push, 1)
typedef struct POINT16 {
  int16_t x, y;
} POINT16;

/* Win16 MSG: 18 bytes. */
typedef struct MSG16 {
  HWND16 hwnd;
  uint16_t message;
  uint16_t wParam;
  uint32_t lParam;
  uint32_t time;
  POINT16 pt;
} MSG16;

/* Win16 PAINTSTRUCT: 32 bytes. */
typedef struct PAINTSTRUCT16 {
  HDC16 hdc;
  uint16_t fErase;
  RECT16 rcPaint;
  uint16_t fRestore;
  uint16_t fIncUpdate;
  uint8_t rgbReserved[16];
} PAINTSTRUCT16;

/* Win16 CREATESTRUCT: 34 bytes. */
typedef struct CREATESTRUCT16 {
  FARPTR lpCreateParams;
  HANDLE16 hInstance;
  HANDLE16 hMenu;
  HWND16 hwndParent;
  int16_t cy, cx, y, x;
  uint32_t style;
  FARPTR lpszName;
  FARPTR lpszClass;
  uint32_t dwExStyle;
} CREATESTRUCT16;

/* Win16 WNDCLASS: 26 bytes. */
typedef struct WNDCLASS16 {
  uint16_t style;
  FARPTR lpfnWndProc;
  int16_t cbClsExtra, cbWndExtra;
  HANDLE16 hInstance;
  HANDLE16 hIcon, hCursor, hbrBackground;
  FARPTR lpszMenuName;
  FARPTR lpszClassName;
} WNDCLASS16;

/* Win16 WINDOWPOS: 14 bytes. */
typedef struct WINDOWPOS16 {
  HWND16 hwnd;
  HWND16 hwndInsertAfter;
  int16_t x, y, cx, cy;
  uint16_t flags;
} WINDOWPOS16;

/* Win16 NCCALCSIZE_PARAMS: 28 bytes. */
typedef struct NCCALCSIZE_PARAMS16 {
  RECT16 rgrc[3];
  FARPTR lppos;
} NCCALCSIZE_PARAMS16;

/* Win16 MINMAXINFO: 20 bytes. */
typedef struct MINMAXINFO16 {
  POINT16 ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
} MINMAXINFO16;

/* Win16 WINDOWPLACEMENT: 22 bytes. */
typedef struct WINDOWPLACEMENT16 {
  uint16_t length, flags, showCmd;
  POINT16 ptMinPosition, ptMaxPosition;
  RECT16 rcNormalPosition;
} WINDOWPLACEMENT16;
#pragma pack(pop)

/* ---- Windows 3.1 constants ---- */

/* Window messages (Win16 numbering; WM_CTLCOLOR is 0x0019 here, the Win32 WM_CTLCOLORxxx
 * messages do not exist). */
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_ENABLE 0x000A
#define WM_SETREDRAW 0x000B
#define WM_SETTEXT 0x000C
#define WM_GETTEXT 0x000D
#define WM_GETTEXTLENGTH 0x000E
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_SHOWWINDOW 0x0018
#define WM_CTLCOLOR 0x0019
#define WM_CANCELMODE 0x001F
#define WM_SETCURSOR 0x0020
#define WM_CHILDACTIVATE 0x0022
#define WM_GETMINMAXINFO 0x0024
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_NCCREATE 0x0081
#define WM_NCDESTROY 0x0082
#define WM_NCCALCSIZE 0x0083
#define WM_NCHITTEST 0x0084
#define WM_NCPAINT 0x0085
#define WM_NCACTIVATE 0x0086
#define WM_KEYFIRST 0x0100
#define WM_TIMER 0x0113
#define WM_MOUSEFIRST 0x0200
#define WM_PARENTNOTIFY 0x0210
#define WM_USER 0x0400

#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2

#define HTERROR (-2)
#define HTTRANSPARENT (-1)
#define HTNOWHERE 0
#define HTCLIENT 1

/* Window styles. */
#define WS_OVERLAPPED 0x00000000UL
#define WS_POPUP 0x80000000UL
#define WS_CHILD 0x40000000UL
#define WS_MINIMIZE 0x20000000UL
#define WS_VISIBLE 0x10000000UL
#define WS_DISABLED 0x08000000UL
#define WS_CLIPSIBLINGS 0x04000000UL
#define WS_CLIPCHILDREN 0x02000000UL
#define WS_MAXIMIZE 0x01000000UL
#define WS_CAPTION 0x00C00000UL
#define WS_BORDER 0x00800000UL
#define WS_DLGFRAME 0x00400000UL
#define WS_VSCROLL 0x00200000UL
#define WS_HSCROLL 0x00100000UL
#define WS_SYSMENU 0x00080000UL
#define WS_THICKFRAME 0x00040000UL
#define WS_MINIMIZEBOX 0x00020000UL
#define WS_MAXIMIZEBOX 0x00010000UL
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_DLGMODALFRAME 0x00000001UL
#define WS_EX_NOPARENTNOTIFY 0x00000004UL
#define WS_EX_TOPMOST 0x00000008UL
#define WS_EX_TRANSPARENT 0x00000020UL

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CW_USEDEFAULT ((int16_t)0x8000)

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_NORMAL 1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_MAXIMIZE 3
#define SW_SHOWNOACTIVATE 4
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWMINNOACTIVE 7
#define SW_SHOWNA 8
#define SW_RESTORE 9

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080
#define SWP_NOCOPYBITS 0x0100
#define SWP_NOOWNERZORDER 0x0200
#define SWP_NOSENDCHANGING 0x0400
/* Runtime-internal flags, as the WINDOWPOS flags Wine passes to WM_WINDOWPOSCHANGED. */
#define SWP_NOCLIENTSIZE 0x0800
#define SWP_NOCLIENTMOVE 0x1000

#define HWND_TOP ((HWND16)0)
#define HWND_BOTTOM ((HWND16)1)
#define HWND_TOPMOST ((HWND16)0xFFFF)
#define HWND_NOTOPMOST ((HWND16)0xFFFE)

#define GW_HWNDFIRST 0
#define GW_HWNDLAST 1
#define GW_HWNDNEXT 2
#define GW_HWNDPREV 3
#define GW_OWNER 4
#define GW_CHILD 5

#define GWL_WNDPROC (-4)
#define GWL_HINSTANCE (-6)
#define GWL_HWNDPARENT (-8)
#define GWL_ID (-12)
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWW_ID GWL_ID

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001
#define PM_NOYIELD 0x0002

#define SW_PARENTCLOSING 1
#define SW_PARENTOPENING 3

/* ---- lifecycle ---- */

/* Builds the USER state in the arena (call after mem_init and gdi_init). Safe to call again
 * after user_shutdown. Returns 0 on success, negative on failure. */
int user_init(void);
/* Destroys every window silently (no messages) and frees the USER blocks. Call while the arena
 * is alive. */
void user_shutdown(void);

/* ---- runtime window procedures ---- */

/* Offsets in the callback segment (MEM_SEL_CALLBACK) of the runtime class procedures. */
#define USER_CB_DEFWNDPROC MK_FP(MEM_SEL_CALLBACK, 0x0000)

/* Calls a window procedure (runtime class procedure or rt_call_far). */
uint32_t user_call_wndproc(FARPTR proc, HWND16 hwnd, uint16_t msg, uint16_t wParam, uint32_t lParam);

/* GetMessage, when nothing is queued and nothing can be synthesised, sets this and returns 0:
 * in native tests "no more input" ends the run. The host (browser) yields instead through
 * user_wait_hook, which returns true when it produced input. */
extern int user_input_exhausted;
extern bool (*user_wait_hook)(void);

/* ---- classes and windows ---- */

uint16_t RegisterClass(FARPTR lpWndClass); /* returns the class atom, 0 on failure */
HWND16 CreateWindow(FARPTR lpClassName, FARPTR lpWindowName, uint32_t dwStyle, int16_t x, int16_t y,
                    int16_t cx, int16_t cy, HWND16 hwndParent, HANDLE16 hMenu, HANDLE16 hInstance,
                    FARPTR lpParam);
uint16_t DestroyWindow(HWND16 hwnd);
uint16_t ShowWindow(HWND16 hwnd, int16_t nCmdShow); /* returns whether the window was visible */
uint16_t MoveWindow(HWND16 hwnd, int16_t x, int16_t y, int16_t cx, int16_t cy, uint16_t fRepaint);
uint16_t SetWindowPos(HWND16 hwnd, HWND16 hwndInsertAfter, int16_t x, int16_t y, int16_t cx, int16_t cy,
                      uint16_t flags);
void GetWindowRect(HWND16 hwnd, FARPTR lpRect);
void GetClientRect(HWND16 hwnd, FARPTR lpRect);
void ClientToScreen(HWND16 hwnd, FARPTR lpPoint);
void ScreenToClient(HWND16 hwnd, FARPTR lpPoint);
void MapWindowPoints(HWND16 hwndFrom, HWND16 hwndTo, FARPTR lpPoints, uint16_t cPoints);
HWND16 WindowFromPoint(uint32_t pt); /* x in the low word, y in the high word (screen) */
HWND16 GetParent(HWND16 hwnd);
HWND16 GetWindow(HWND16 hwnd, uint16_t uCmd);
HWND16 GetDesktopWindow(void);
uint16_t IsWindow(HWND16 hwnd);
uint16_t IsWindowVisible(HWND16 hwnd);
uint16_t IsIconic(HWND16 hwnd);
uint16_t IsZoomed(HWND16 hwnd);
uint16_t IsWindowEnabled(HWND16 hwnd);
uint16_t EnableWindow(HWND16 hwnd, uint16_t bEnable); /* returns the previous disabled state */
int16_t GetWindowText(HWND16 hwnd, FARPTR lpString, int16_t nMaxCount);
void SetWindowText(HWND16 hwnd, FARPTR lpString);
int16_t GetWindowTextLength(HWND16 hwnd);
uint32_t GetWindowLong(HWND16 hwnd, int16_t nIndex);
uint32_t SetWindowLong(HWND16 hwnd, int16_t nIndex, uint32_t dwNewLong);
uint16_t GetWindowWord(HWND16 hwnd, int16_t nIndex);
uint16_t SetWindowWord(HWND16 hwnd, int16_t nIndex, uint16_t wNewWord);
uint32_t CallWindowProc(FARPTR lpPrevWndFunc, HWND16 hwnd, uint16_t msg, uint16_t wParam, uint32_t lParam);
uint32_t DefWindowProc(HWND16 hwnd, uint16_t msg, uint16_t wParam, uint32_t lParam);
uint16_t GetWindowPlacement(HWND16 hwnd, FARPTR lpwndpl);

/* ---- messages ---- */

uint16_t GetMessage(FARPTR lpMsg, HWND16 hwnd, uint16_t wMsgFilterMin, uint16_t wMsgFilterMax);
uint16_t PeekMessage(FARPTR lpMsg, HWND16 hwnd, uint16_t wMsgFilterMin, uint16_t wMsgFilterMax,
                     uint16_t wRemoveMsg);
uint16_t PostMessage(HWND16 hwnd, uint16_t msg, uint16_t wParam, uint32_t lParam);
uint32_t SendMessage(HWND16 hwnd, uint16_t msg, uint16_t wParam, uint32_t lParam);
uint16_t TranslateMessage(FARPTR lpMsg); /* WM_CHAR generation is USER part 2: returns 0 */
uint32_t DispatchMessage(FARPTR lpMsg);
void PostQuitMessage(int16_t nExitCode);

/* ---- painting ---- */

HDC16 BeginPaint(HWND16 hwnd, FARPTR lpPaint);
void EndPaint(HWND16 hwnd, FARPTR lpPaint);
void InvalidateRect(HWND16 hwnd, FARPTR lpRect, uint16_t bErase); /* lpRect 0: whole client area */
void ValidateRect(HWND16 hwnd, FARPTR lpRect);
void UpdateWindow(HWND16 hwnd);
HDC16 GetDC(HWND16 hwnd);
uint16_t ReleaseDC(HWND16 hwnd, HDC16 hdc);
void ScrollWindow(HWND16 hwnd, int16_t dx, int16_t dy, FARPTR lpRect, FARPTR lpClipRect);

/* ---- helpers for later USER tasks and for tests ---- */

/* Screen rectangle of the client area of hwnd; false when hwnd is not a window. */
bool user_client_rect_screen(HWND16 hwnd, RECT16 *out);
/* Number of live windows, the desktop excluded. */
unsigned user_window_count(void);
/* Number of rectangles in hwnd's update region (0 when it needs no painting); test hook. */
unsigned user_window_nupd(HWND16 hwnd);

#endif /* STARS_RUNTIME_USER_H */
