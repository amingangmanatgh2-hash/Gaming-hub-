#!/usr/bin/env bash
# =============================================================================
#  Cross-compile AMIN Keyboard & Clipboard (x64 + x86) from Linux / macOS.
#
#  Requirements: Zig 0.13+ (it ships Clang + MinGW-w64 + lld and a resource
#  compiler). Install with:  python3 -m pip install ziglang
#
#  Usage:
#     ./build/build_cross.sh              # builds dist/*.exe and verifies them
#     ZIG=/path/to/zig ./build/build_cross.sh
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

ZIG="${ZIG:-zig}"
if ! command -v "$ZIG" >/dev/null 2>&1; then
  if [ -x "$HOME/.local/bin/zig" ]; then ZIG="$HOME/.local/bin/zig"
  elif python3 -c "import ziglang,os,sys;sys.stdout.write(os.path.dirname(ziglang.__file__)+'/zig')" >/dev/null 2>&1; then
    ZIG="$(python3 -c "import ziglang,os,sys;sys.stdout.write(os.path.dirname(ziglang.__file__)+'/zig')")"
  else
    echo "ERROR: zig not found. Install it with: python3 -m pip install ziglang" >&2
    exit 1
  fi
fi
echo "Using Zig: $($ZIG version)"

SOURCES=(
  src/core/text.cpp
  src/core/json.cpp
  src/core/store.cpp
  src/core/settings.cpp
  src/core/hotkey.cpp
  src/core/layout.cpp
  src/core/selftest_core.cpp
  src/win/gfx.cpp
  src/win/util_win.cpp
  src/win/platform.cpp
  src/win/app.cpp
  src/win/ui_keyboard.cpp
  src/win/ui_clipboard.cpp
  src/win/ui_dashboard.cpp
  src/win/selftest_win.cpp
  src/win/main.cpp
)

COMMON=(
  -std=c++17 -O2 -municode -static
  # -s: no debug info, no PDB, no build-machine paths inside the binary
  -s
  -Wall -Wextra -Wno-unused-parameter
  -finput-charset=UTF-8
  -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX
  -Wl,--subsystem,windows
  -lusp10 -lcomctl32 -lshell32 -lole32 -luser32 -lgdi32 -ladvapi32
)

mkdir -p build dist

echo "==> compiling resources"
"$ZIG" rc src/res/app.rc build/app.res

build_one() {
  local triple="$1" out="$2"
  echo "==> building $out ($triple)"
  "$ZIG" c++ -target "$triple" "${COMMON[@]}" -o "$out" "${SOURCES[@]}" build/app.res
}

build_one x86_64-windows-gnu dist/AminKeyboardClipboard-x64.exe
build_one x86-windows-gnu    dist/AminKeyboardClipboard-x86.exe

echo "==> done"
ls -la dist/
