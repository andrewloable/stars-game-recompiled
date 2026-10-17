# Stars! in the browser: tools and plan

Goal: decompile `original/stars26i.exe` (Stars! 2.6i, Win16) into C, recompile it to WebAssembly, and
run it in a browser with the Windows 3.11 look and feel preserved. The work is test-driven: every
decompiled function is checked against the original machine code, and every Win16 API against
documented Windows 3.1 behaviour and reference captures.

Work is tracked in beads (`bd ready`, prefix `stars`). This document is the shared reference that
the bd tasks point to.

## 1. What the binary is

Measured on 2026-10-15 from `original/stars26i.exe` (SHA-256 `5770dade…198e`, 3 216 832 bytes).

| Fact | Value |
|------|-------|
| Format | NE (16-bit Windows 3.x EXE). Module `STARS!`, description "Stars! Intergalactic Conquest Game", "Copyright © 1995-1996 Jeff Johnson && Jeff McBride" |
| Toolchain | Microsoft LINK 5.60 (NE header linker version). Microsoft C runtime markers present (`<<NMSG>>`, `R6000`…`R6021`). Exact compiler version not yet confirmed. |
| Segments | 35: segments 1–34 are code, segment 35 is DGROUP (26 248 bytes; heap 8 192, stack 16 384). 715 684 bytes of segment data in total. Alignment shift 6. |
| Entry point | CS:IP = segment 34, offset 0x001A. 53 entry-table ordinals, all exported by name (window and dialog procedures). |
| Imports | 197 functions: KERNEL 44, USER 107, GDI 41, COMMDLG 3, TOOLHELP 1, WIN87EM 1. All by ordinal. Full list in Appendix A. |
| Not imported | CTL3DV2, WAVEMIX, MMSYSTEM, `LoadLibrary`, `GetProcAddress`. The three DLLs in `original/` are not used by the game and are not ported. `hyprfind.dll` is WinHelp's search DLL (it imports CTL3DV2). |
| Relocation records | 390 internal, 1 028 import-by-ordinal, 0 import-by-name, 810 OSFIXUP |
| x87 floating point | The 810 OSFIXUP sites already contain native x87 code (`9B D8..DF`, `9B 26/2E/36 …`, `90 9B`). Windows turns them into emulator interrupts only on machines without an FPU. Decompile them as native x87; loaders must ignore OSFIXUP records. All transcendental ops (FPTAN, FPATAN, F2XM1, FYL2X, FSCALE, FPREM, FRNDINT, FSQRT) and all FLDCW/FNSTCW are in segment 34 (C runtime). Game segments use basic FP ops only. |
| Resources | 38 BITMAP (2 376 000 bytes), 36 DIALOG, 1 MENU (`STARSMENU`), 2 ACCELERATOR (116, 1080), 10 ICON + 10 GROUP_ICON, 11 CURSOR + 11 GROUP_CURSOR, custom types 10000/10002/10004 (IDs 10001/10003/10005; 4 352 / 51 776 / 19 904 bytes; contents unknown) |
| Fonts | All 36 dialogs use "MS Sans Serif" 8 pt. The code also references "Arial Bold". |
| Game files | Strings `%s.xy`, `%s.hst`, `%s.x1` are present. Generated files must stay byte-compatible with the original game. |

## 2. Tools needed to fully decompile

| # | Tool | Role | Status on this Mac |
|---|------|------|--------------------|
| 1 | Ghidra 12.1 (`analyzeHeadless`, `ghidraRun`) | Disassembler and decompiler. Loads this file with the "New Executable (NE)" loader as `x86:LE:16:Protected Mode` (verified). Ships Win16 symbol files for all six imported modules (`Ghidra/Features/Base/data/symbols/win16/*.exports`). | installed |
| 2 | GhidraMCP | Interactive queries and renames from Claude sessions (needs the Ghidra GUI open with the program loaded). | installed |
| 3 | OpenJDK 21 | Runs Ghidra and our Java GhidraScripts. | installed |
| 4 | Unicorn 2.1.4, C library | Executes the original 16-bit code in the hybrid harness (the test oracle). Verified with the Python binding in a scratch venv: real-mode x87 (`FNINIT` control word 0x037F, round-to-even), and 16-bit protected mode with GDT descriptors based above 1 MB, including selector `+8` stepping. | `brew install unicorn` (not installed yet) |
| 5 | Capstone 5.0.9, C library | Disassembly in harness traces and failure reports. | installed |
| 6 | Apple clang, LLVM 20/21 | Native builds of runtime, harness and decompiled code for fast tests. | installed |
| 7 | Emscripten 5.0.2 | C to WebAssembly for the browser build. | installed |
| 8 | Node 24 + npm | Runs wasm builds, the Angular CLI and dev server, `node --test`. | installed |
| 9 | Playwright | Browser end-to-end and screenshot tests. | npm dev dependency (not installed yet) |
| 10 | Wine 16-bit `.spec` files | Ordinal → name, calling convention and argument list for every import. The harness needs the argument bytes to pop for Pascal calls (e.g. `USER.1 MessageBox(word str str word)` pops 12). Paths verified: `dlls/krnl386.exe16/krnl386.exe16.spec`, `dlls/user.exe16/user.exe16.spec`, `dlls/gdi.exe16/gdi.exe16.spec`, `dlls/commdlg.dll16/commdlg.dll16.spec`, `dlls/toolhelp.dll16/toolhelp.dll16.spec`, `dlls/win87em.dll16/win87em.dll16.spec` at wine-mirror/wine commit `5b6b85421c2a2161728b2a55e9f09271770a3eb0` (LGPL-2.1). | to vendor into `tools/winespec/` |
| 11 | Python 3 (stdlib only) | Small code generators, such as spec files → C import table. | installed (3.14.8) |
| 12 | DOSBox-X + Windows 3.11 | Runs the original game on real Windows 3.11, for reference screenshots, font glyphs and golden game files. | DOSBox-X 2026.10.01 installed. **Windows 3.11 install media and licence must come from the user.** |
| 13 | helpdeco 2.1.4 (GPL-2.0, SourceForge) or `winhlp` (PyPI) | Converts `stars!.hlp` to RTF or HTML for the in-browser help viewer. | not installed |
| 14 | Angular, latest stable (CLI via `npx @angular/cli@latest`) | The browser frontend in `web/`: canvas host, input, file import/export, IndexedDB. Static build, no backend (decision 11). | npm dependency of `web/` (not created yet) |
| 14 | Berkeley SoftFloat 3e (BSD-3) | Exact 80-bit x87 arithmetic, needed only if the C runtime runs the FPU at 64-bit mantissa precision (decided by a task). | vendor only if needed |
| 15 | FreeType 2.14.3 (FreeType Licence / GPLv2 dual) | Optional build-time tool only: `tools/genfont.c` uses it to regenerate `runtime/fontdata.c` from the OFL Liberation Sans TTFs (see `third_party/fonts/README.md`). NOT needed for `make` or `make test`, and NOT linked into the runtime or the wasm build; the generated `runtime/fontdata.c` is committed. | `brew install freetype` (only to regenerate) |

Tools we build (each one a bd task):

| Tool | Role |
|------|------|
| `nedump` (C, built from `runtime/ne.c`) | Prints NE headers, segments, relocations, imports and exports. Dumps resources (dialogs and menus as text, bitmaps as `.bmp`). |
| Ghidra scripts (`ghidra/*.java`) | Apply names and types, then export the function inventory, call graph and per-function decompiled C. |
| Hybrid harness (`harness/`, C + Unicorn) | Runs original code on our Win16 runtime, runs decompiled C on the same memory image, and diff-tests the two. |
| Inventory report | Turns `docs/functions.csv` into a count and percentage of functions named, decompiled and diff-tested. |

Not needed:
- IDA Pro / Hex-Rays: the Hex-Rays decompiler does not support 16-bit x86.
- A floating-point "unpatcher": the OSFIXUP sites are already native x87 (see §1).
- Microsoft Visual C++ 1.5x: needed only for a byte-matching recompile, which is not a goal.
- Reko: possible second-opinion decompiler. Its feature list mentions NE, but that is unverified on this binary. Evaluate it only if Ghidra output is blocked.

## 3. Architecture

```
browser
 ├─ web/ (Angular app)           canvas, mouse/keyboard → Win16 messages, file import/export, IndexedDB
 └─ stars.wasm (Emscripten, ASYNCIFY)
      ├─ src/       decompiled game code (C, 16-bit semantics explicit)
      └─ runtime/   Win16 API in C: KERNEL USER GDI COMMDLG TOOLHELP WIN87EM + DOS3Call
                    memory model (selectors), NE loader (resources from the user's exe),
                    software GDI rasteriser → 32-bpp framebuffer → canvas

native only (tests)
 └─ harness/        Unicorn runs the ORIGINAL x86 code against the SAME runtime and memory image
```

Key decisions:

1. **Keep the original segmented memory model.** Far pointers stay 32-bit `selector:offset` values.
   `LIN(fp)` turns one into a host pointer through an emulated descriptor table (GDT, ring 0,
   `__AHINCR` = 8, `__AHSHIFT` = 3). Near pointers stay 16-bit offsets into DGROUP, with the local heap
   inside DGROUP. Handles stay 16-bit. The reasons:
   - Struct layouts, game files and memory images stay identical to the original on every host,
     including 64-bit native test builds, where a C pointer is 8 bytes and could not stand in for a
     4-byte far pointer.
   - The harness maps the runtime's memory arena into Unicorn (`uc_mem_map_ptr`), with the descriptor
     table inside the arena. Original x86 code and decompiled C then run on one shared memory image,
     which is what makes per-function diff tests exact.
2. **Hybrid execution until everything is decompiled.** In native builds, a call to a function not
   decompiled yet falls through to Unicorn, which runs the original code. Segments can be decompiled
   in any order. The browser build contains no emulator: it ships only when all 34 code segments
   are decompiled.
3. **Software rendering.** GDI draws into a framebuffer in wasm memory, and JS only blits it to the
   canvas. Win3.1 raster operations (ROP2, ROP3, pattern brushes, XOR cursors) are exact on a
   framebuffer, and every pixel can be tested without a browser.
4. **Win3.1 chrome is reimplemented in the runtime, not in CSS.** That covers window frames, menus,
   standard controls, MessageBox and the common file dialogs, each matched against DOSBox-X +
   Windows 3.11 reference captures.
5. **Assets come from the user's own copy.** The browser asks for `stars26i.exe`, checks its SHA-256,
   and loads resources and the DGROUP initial image from it. No copyrighted bitmaps, dialogs or fonts
   are built into the wasm.
6. **Blocking Win16 calls use Emscripten ASYNCIFY.** That covers `GetMessage`, `DialogBox`,
   `TrackPopupMenu` and `MessageBox`. JSPI is the upgrade path once all target browsers support it.
7. **Floating point.** The x87 precision-control setting chosen by the C runtime decides the approach
   (task "FP semantics"). With 53-bit precision, C `double` is exact for the basic ops the game
   segments use. With 64-bit precision, use SoftFloat 3e `extF80`. The diff tests settle it either way.

Owner decisions (2026-10-15):

8. **Windows 3.1 assets and display mode.** Fonts (System, MS Sans Serif, Arial Bold) and USER.EXE
   bitmaps, cursors and icons come from the owner's own Windows 3.11 files in `original/win311/`
   (git-ignored), parsed by our NE loader. The emulated display is **800×600, 256 colours**: an 8-bpp
   indexed framebuffer with the 20 static colours plus logical palettes.
   **Amended 2026-10-17 (owner):** the Windows 3.11 files are not available yet, so fonts use
   permissively licensed look-alike fonts (licence file recorded with each font, no Microsoft
   files), and window chrome is hand-drawn to the Windows 3.1 look. Pixel-exact checks against the
   original files wait for the owner to supply them.
9. **No Windows 3.11 media yet.** There is no DOSBox-X reference environment, so the task that builds
   it is blocked. Golden game files come from the original running on our harness. Visual checks are
   the owner's review of frame dumps until reference captures exist. Decision 8's asset files are
   also pending until the media is available.
10. **Public repository, runtime only.** The GitHub repo is public. It holds the plan, tools, Win16
    runtime, harness and web host. Decompiled source (`src/`), diff-test scenarios, the function
    inventory, reference screenshots and golden game files stay local and git-ignored, as do the
    original files, serials and Windows files.
11. **Frontend is Angular (latest stable), browser only, no backend.** `web/` is an Angular app on
    the latest stable Angular release, kept current with `ng update`. It hosts the canvas, input,
    file import/export and IndexedDB persistence, and loads `stars.js` + `stars.wasm` as static
    assets. The game runs entirely in the browser: no application server, no API, no server-side
    storage. The build output is static files any static host can serve.

## 4. Test-driven development

Every task follows red → green → refactor: the test is written first and seen to fail, then the code
makes it pass. There are four kinds of oracle, from cheapest to most expensive:

| Oracle | Checks | Where |
|--------|--------|-------|
| Static facts | Parsers and loaders reproduce the numbers in §1 exactly (segment count, import counts, 810 OSFIXUP records, resource IDs). | `runtime/tests/` |
| Differential (per function) | From one memory snapshot, run the original function in Unicorn and the decompiled C function. Return registers, every byte of memory outside the callee's own stack frame, and the ordered Win16 API call log must all match. Pure functions are also fuzzed with seeded random arguments. A function is "done" only when its diff tests pass. Red step: before the C function exists, the driver reports `MISSING` and fails. | `harness/`, `tests/scenarios/` |
| Golden master (whole program) | Game files produced by the original (under DOSBox-X/Windows 3.11, or under the harness) and by the recompiled build are byte-identical for the same inputs and seeds. | `tests/golden/` |
| Visual | The runtime framebuffer matches reference screenshots captured in DOSBox-X + Windows 3.11, exactly by default. Any tolerance must be written down per test. | `tests/ref/`, `runtime/tests/` |

Win16 API behaviour is specified from the Windows 3.1 SDK documentation, with Wine's 16-bit
implementation as a behavioural reference and DOSBox-X observations where those two disagree or are silent.

Conventions shared by all tasks:
- `make test` at the repo root builds every native test with clang and runs it. A non-zero exit
  means failure. C tests use a minimal `tests/test.h` (CHECK macros) with no framework.
- `make wasm` builds `web/public/wasm/stars.js` + `web/public/wasm/stars.wasm` with emcc; the Angular app in `web/` loads them as static assets.
- Test file names are `test_<topic>.c` next to the module (`runtime/tests/`, `harness/tests/`).
- No commits are made by agents. Changes are left in the working tree.

## 5. Phases and milestones

| Epic | Delivers | Milestone |
|------|----------|-----------|
| A. Foundation and static analysis | Build/test skeleton, NE loader + `nedump`, Ghidra project + function inventory | M1: every function in the inventory, imports/exports named |
| B. Memory model and hybrid harness | Selector memory model, Unicorn harness, diff-test driver, FP decision | M2: first original function executed; first decompiled function green |
| C. Win16 runtime | KERNEL, GDI, fonts, USER (windows, chrome, menus, dialogs, controls), COMMDLG, misc | M3: **the original `stars26i.exe` runs on our runtime under the harness** and renders its main window |
| D. Decompilation | Conventions, shared headers, all 34 code segments diff-tested | M4: native build with zero emulated functions; turn generation byte-identical |
| E. Browser | wasm build, input, files/persistence, full game boot, Playwright | M5: playable in the browser |
| F. Verification | Reference environment, golden corpus, visual regression | runs throughout |

## 6. Risks

| Risk | Mitigation |
|------|------------|
| x87 80-bit precision differs from wasm `double` | Decide from the measured control word; SoftFloat `extF80` if needed. Diff tests catch divergence. |
| Transcendental results (C runtime only) differ between Unicorn, real FPUs and libm | Game segments don't use them directly. Inventory which CRT math functions the game calls and diff-test them. |
| Fonts are Microsoft-owned (MS Sans Serif, Arial) | User decision in the fonts task: user-supplied files from their Windows 3.11, glyphs captured from the reference environment, or a look-alike (inexact). |
| No Windows 3.11 media | Visual fidelity then rests on judgement by eye. Golden game files can still come from the original running on the harness. |
| Copyright: original files, fonts and decompiled source | Keep `original/` and `private/` out of git (already ignored). Load assets from the user's copy at runtime. Publishing the decompiled source or a hosted build is the owner's decision. |
| Ghidra 16-bit output is noisy (`CONCAT22` segment:offset pairs) | Conventions doc with translation rules; diff tests are the arbiter, not the pseudo-C. |

## Directory layout

```
original/      untouched game files (read-only, git-ignored)
private/       registration serials (git-ignored)
docs/          PLAN.md, CONVENTIONS.md, functions.csv (inventory + status)
tools/         generators (Python stdlib) and vendored Wine spec files (tools/winespec/)
ghidra/        Java GhidraScripts (the Ghidra project itself lives in re/ghidra/)
runtime/       Win16 runtime in C; runtime/tests/
harness/       native-only Unicorn harness and diff-test driver; harness/tests/
src/           decompiled game: segNN.c per original code segment, stars.h, globals.h, registry
tests/         test.h, scenarios/ (diff tests), golden/ (game files), ref/ (reference screenshots)
web/           Angular frontend (static, no backend), Playwright tests
third_party/   vendored libraries, only if a task's decision requires one (e.g. SoftFloat 3e)
re/            generated, git-ignored: Ghidra project, dumps, frame captures
build/         build output, git-ignored
```

## Appendix A: imported Win16 functions (197)

Resolved with the Wine spec files listed in §2 (tool #10).

- **KERNEL (44):** FatalExit, GetVersion, LocalAlloc, LocalReAlloc, LocalFree, LocalSize, GlobalAlloc, GlobalReAlloc, GlobalFree, GlobalLock, GlobalUnlock, GlobalSize, LockSegment, UnlockSegment, Yield, WaitEvent, GetModuleFileName, MakeProcInstance, FreeProcInstance, FindResource, LoadResource, LockResource, FreeResource, AccessResource, SizeofResource, AllocResource, OpenFile, _lclose, _lread, _lwrite, lstrcpy, lstrcat, lstrlen, InitTask, DOS3Call, __AHSHIFT, __AHINCR, GetPrivateProfileInt, GetPrivateProfileString, WritePrivateProfileString, GetDOSEnvironment, GetDriveType, FatalAppExit, __WINFLAGS
- **USER (107):** MessageBox, InitApp, PostQuitMessage, ExitWindows, SetTimer, KillTimer, GetTickCount, GetCurrentTime, GetCursorPos, SetCapture, ReleaseCapture, SetFocus, GetFocus, ClientToScreen, ScreenToClient, WindowFromPoint, IsIconic, GetWindowRect, GetClientRect, EnableWindow, GetWindowText, SetWindowText, BeginPaint, EndPaint, CreateWindow, ShowWindow, GetParent, IsWindowVisible, DestroyWindow, MoveWindow, RegisterClass, GetActiveWindow, ScrollWindow, SetScrollPos, GetScrollPos, SetScrollRange, GetDC, ReleaseDC, SetCursor, SetRect, CopyRect, PtInRect, OffsetRect, InflateRect, IntersectRect, FillRect, FrameRect, DrawIcon, DrawText, DialogBox, EndDialog, CreateDialog, GetDlgItem, SetDlgItemText, GetDlgItemText, CheckRadioButton, CheckDlgButton, IsDlgButtonChecked, SendDlgItemMessage, MessageBeep, FlashWindow, GetKeyState, DefWindowProc, GetMessage, PeekMessage, PostMessage, SendMessage, TranslateMessage, DispatchMessage, CallWindowProc, UpdateWindow, InvalidateRect, ValidateRect, GetWindowLong, SetWindowLong, DestroyMenu, CheckMenuItem, EnableMenuItem, GetMenu, GetSubMenu, DrawMenuBar, WinHelp, LoadCursor, LoadIcon, LoadBitmap, LoadAccelerators, TranslateAccelerator, GetSystemMetrics, GetSysColor, SetWindowPos, EqualRect, GetAsyncKeyState, MapWindowPoints, GetWindow, GetMenuItemCount, IsZoomed, SelectPalette, RealizePalette, GetWindowPlacement, InsertMenu, AppendMenu, DeleteMenu, CreatePopupMenu, TrackPopupMenu, _wsprintf, DestroyIcon, DestroyCursor
- **GDI (41):** SetBkColor, SetBkMode, SetROP2, SetTextColor, SetWindowOrg, LineTo, MoveTo, ExcludeClipRect, IntersectClipRect, Ellipse, Rectangle, PatBlt, SetPixel, TextOut, BitBlt, Escape, SelectClipRgn, SelectObject, CreateCompatibleBitmap, CreateCompatibleDC, CreateFontIndirect, CreatePatternBrush, CreatePen, CreateRectRgn, CreateSolidBrush, DeleteDC, DeleteObject, GetBkColor, GetDeviceCaps, GetObject, GetROP2, GetStockObject, GetTextExtent, GetTextMetrics, MulDiv, SetBrushOrg, UnrealizeObject, ExtTextOut, CreatePalette, StretchDIBits, GetDIBits
- **COMMDLG (3):** GetOpenFileName, GetSaveFileName, PrintDlg
- **TOOLHELP (1):** TimerCount
- **WIN87EM (1):** __fpMath
