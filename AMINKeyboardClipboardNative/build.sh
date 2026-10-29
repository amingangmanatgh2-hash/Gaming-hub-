#!/bin/sh
# ---------------------------------------------------------------------------
#  AMIN Keyboard & Clipboard - cross-compile check from Linux/macOS using Zig.
#
#  This exists so the sources can be compiled away from a Windows machine
#  (resources are skipped). The official release build uses build.bat + MSVC.
#
#  Usage:  ./build.sh            (uses `zig` from PATH)
# ---------------------------------------------------------------------------
set -e

mkdir -p build

CC=${CC:-zig cc}
SRC="src/main.c src/keyboard.c src/clipboard.c src/settings.c"
LIBS="-luser32 -lgdi32 -lshell32 -lshlwapi -ladvapi32"
FLAGS="-O2 -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -Wall"

echo "==> x86_64"
$CC -target x86_64-windows-gnu $FLAGS $SRC -o build/AMINKeyboardClipboard-x64.exe \
    -Wl,--subsystem,windows $LIBS

echo "==> x86 (32-bit)"
$CC -target x86-windows-gnu $FLAGS $SRC -o build/AMINKeyboardClipboard-x86.exe \
    -Wl,--subsystem,windows $LIBS

ls -l build/*.exe
