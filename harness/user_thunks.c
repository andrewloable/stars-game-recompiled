/* HCall decoders for the USER part 1 functions of runtime/user.h (task stars-0z4.6). Native
 * builds only. Arguments are decoded from the PASCAL stack words: hcall_word(c, 0) is the LAST
 * pushed word, and a DWORD or far pointer is pushed high word first, so its low word is word n
 * and its high word word n+1. Functions that return void in Win16 leave c->ret at 0. */
#include "harness/user_thunks.h"

#include "harness/harness.h"
#include "runtime/user.h"

static uint32_t w32(const HCall *c, unsigned n) {
  return (uint32_t)hcall_word(c, n) | ((uint32_t)hcall_word(c, n + 1) << 16);
}
#define W(n) hcall_word(c, (n))
#define S(n) ((int16_t)hcall_word(c, (n)))

static void t_RegisterClass(HCall *c) { c->ret = RegisterClass(w32(c, 0)); }
static void t_CreateWindow(HCall *c) {
  c->ret = CreateWindow(w32(c, 13), w32(c, 11), w32(c, 9), S(8), S(7), S(6), S(5), W(4), W(3), W(2), w32(c, 0));
}
static void t_DestroyWindow(HCall *c) { c->ret = DestroyWindow(W(0)); }
static void t_ShowWindow(HCall *c) { c->ret = ShowWindow(W(1), S(0)); }
static void t_MoveWindow(HCall *c) { c->ret = MoveWindow(W(5), S(4), S(3), S(2), S(1), W(0)); }
static void t_SetWindowPos(HCall *c) { c->ret = SetWindowPos(W(6), W(5), S(4), S(3), S(2), S(1), W(0)); }
static void t_GetWindowRect(HCall *c) { GetWindowRect(W(2), w32(c, 0)); }
static void t_GetClientRect(HCall *c) { GetClientRect(W(2), w32(c, 0)); }
static void t_ClientToScreen(HCall *c) { ClientToScreen(W(2), w32(c, 0)); }
static void t_ScreenToClient(HCall *c) { ScreenToClient(W(2), w32(c, 0)); }
static void t_MapWindowPoints(HCall *c) { MapWindowPoints(W(4), W(3), w32(c, 1), W(0)); }
static void t_WindowFromPoint(HCall *c) { c->ret = WindowFromPoint(w32(c, 0)); }
static void t_GetParent(HCall *c) { c->ret = GetParent(W(0)); }
static void t_GetWindow(HCall *c) { c->ret = GetWindow(W(1), W(0)); }
static void t_GetDesktopWindow(HCall *c) { c->ret = GetDesktopWindow(); }
static void t_IsWindow(HCall *c) { c->ret = IsWindow(W(0)); }
static void t_IsWindowVisible(HCall *c) { c->ret = IsWindowVisible(W(0)); }
static void t_IsIconic(HCall *c) { c->ret = IsIconic(W(0)); }
static void t_IsZoomed(HCall *c) { c->ret = IsZoomed(W(0)); }
static void t_IsWindowEnabled(HCall *c) { c->ret = IsWindowEnabled(W(0)); }
static void t_EnableWindow(HCall *c) { c->ret = EnableWindow(W(1), W(0)); }
static void t_GetWindowText(HCall *c) { c->ret = (uint16_t)GetWindowText(W(3), w32(c, 1), S(0)); }
static void t_SetWindowText(HCall *c) { SetWindowText(W(2), w32(c, 0)); }
static void t_GetWindowTextLength(HCall *c) { c->ret = (uint16_t)GetWindowTextLength(W(0)); }
static void t_GetWindowLong(HCall *c) { c->ret = GetWindowLong(W(1), S(0)); }
static void t_SetWindowLong(HCall *c) { c->ret = SetWindowLong(W(3), S(2), w32(c, 0)); }
static void t_GetWindowWord(HCall *c) { c->ret = GetWindowWord(W(1), S(0)); }
static void t_SetWindowWord(HCall *c) { c->ret = SetWindowWord(W(2), S(1), W(0)); }
static void t_CallWindowProc(HCall *c) { c->ret = CallWindowProc(w32(c, 5), W(4), W(3), W(2), w32(c, 0)); }
static void t_DefWindowProc(HCall *c) { c->ret = DefWindowProc(W(4), W(3), W(2), w32(c, 0)); }
static void t_GetWindowPlacement(HCall *c) { c->ret = GetWindowPlacement(W(2), w32(c, 0)); }

static void t_GetMessage(HCall *c) { c->ret = GetMessage(w32(c, 3), W(2), W(1), W(0)); }
static void t_PeekMessage(HCall *c) { c->ret = PeekMessage(w32(c, 4), W(3), W(2), W(1), W(0)); }
static void t_PostMessage(HCall *c) { c->ret = PostMessage(W(4), W(3), W(2), w32(c, 0)); }
static void t_SendMessage(HCall *c) { c->ret = SendMessage(W(4), W(3), W(2), w32(c, 0)); }
static void t_TranslateMessage(HCall *c) { c->ret = TranslateMessage(w32(c, 0)); }
static void t_DispatchMessage(HCall *c) { c->ret = DispatchMessage(w32(c, 0)); }
static void t_PostQuitMessage(HCall *c) { PostQuitMessage(S(0)); }

static void t_BeginPaint(HCall *c) { c->ret = BeginPaint(W(2), w32(c, 0)); }
static void t_EndPaint(HCall *c) { EndPaint(W(2), w32(c, 0)); }
static void t_InvalidateRect(HCall *c) { InvalidateRect(W(3), w32(c, 1), W(0)); }
static void t_ValidateRect(HCall *c) { ValidateRect(W(2), w32(c, 0)); }
static void t_UpdateWindow(HCall *c) { UpdateWindow(W(0)); }
static void t_GetDC(HCall *c) { c->ret = GetDC(W(0)); }
static void t_ReleaseDC(HCall *c) { c->ret = ReleaseDC(W(1), W(0)); }
static void t_ScrollWindow(HCall *c) { ScrollWindow(W(6), S(5), S(4), w32(c, 2), w32(c, 0)); }

static const struct {
  uint16_t ordinal;
  HApiFn fn;
} g_apis[] = {
    {6, t_PostQuitMessage},      {28, t_ClientToScreen},    {29, t_ScreenToClient},
    {30, t_WindowFromPoint},     {31, t_IsIconic},          {32, t_GetWindowRect},
    {33, t_GetClientRect},       {34, t_EnableWindow},      {35, t_IsWindowEnabled},
    {36, t_GetWindowText},       {37, t_SetWindowText},     {38, t_GetWindowTextLength},
    {39, t_BeginPaint},          {40, t_EndPaint},          {41, t_CreateWindow},
    {42, t_ShowWindow},          {46, t_GetParent},         {47, t_IsWindow},
    {49, t_IsWindowVisible},     {53, t_DestroyWindow},     {56, t_MoveWindow},
    {57, t_RegisterClass},       {61, t_ScrollWindow},      {66, t_GetDC},
    {68, t_ReleaseDC},           {107, t_DefWindowProc},    {108, t_GetMessage},
    {109, t_PeekMessage},        {110, t_PostMessage},      {111, t_SendMessage},
    {113, t_TranslateMessage},   {114, t_DispatchMessage},  {122, t_CallWindowProc},
    {124, t_UpdateWindow},       {125, t_InvalidateRect},   {127, t_ValidateRect},
    {133, t_GetWindowWord},      {134, t_SetWindowWord},    {135, t_GetWindowLong},
    {136, t_SetWindowLong},      {232, t_SetWindowPos},     {258, t_MapWindowPoints},
    {262, t_GetWindow},          {272, t_IsZoomed},         {286, t_GetDesktopWindow},
    {370, t_GetWindowPlacement},
};

void user_register(void) {
  for (size_t i = 0; i < sizeof g_apis / sizeof g_apis[0]; i++)
    harness_register("USER", g_apis[i].ordinal, g_apis[i].fn);
}
