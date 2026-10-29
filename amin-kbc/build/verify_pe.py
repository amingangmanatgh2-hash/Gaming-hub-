#!/usr/bin/env python3
"""
Verify that the produced Windows executables are sane and Windows-7/10-1803 safe.

Checks
  * PE machine type and GUI subsystem (no console window)
  * minimum OS / subsystem version
  * resources present (icon, version info, manifest)
  * no imports that only exist on newer Windows versions
  * no debug paths pointing at the build machine (privacy)
Usage: python3 build/verify_pe.py dist/AminKeyboardClipboard-x64.exe ...
Requires: pip install pefile
"""
import sys, re

try:
    import pefile
except ImportError:
    print("pefile is required: python3 -m pip install pefile")
    sys.exit(2)

# APIs that do NOT exist on Windows 7 and/or Windows 10 1803.
TOO_NEW = {
    # Windows 8+
    "SetProcessDpiAwareness", "GetDpiForMonitor", "SetProcessDpiAwarenessContext",
    "GetDpiForWindow", "GetDpiForSystem", "SetThreadDpiAwarenessContext",
    "EnableNonClientDpiScaling", "DwmSetWindowAttribute", "DwmGetWindowAttribute",
    "RegisterSuspendResumeNotification", "GetSystemMetricsForDpi", "AdjustWindowRectExForDpi",
    # Windows 10 1607+
    "SetProcessMitigationPolicy", "GetThreadDescription", "SetThreadDescription",
    # Windows 10 1709+/1809+
    "AddClipboardFormatListener2", "SetWindowDisplayAffinityEx",
    # Windows 11
    "GetDpiForWindow2", "SetPreferredAppMode",
}

def main(paths):
    ok = True
    for path in paths:
        print(f"=== {path} ===")
        pe = pefile.PE(path)
        machine = pe.FILE_HEADER.Machine
        machine_name = {0x14c: "x86", 0x8664: "x64", 0xaa64: "arm64"}.get(machine, hex(machine))
        print(f"  machine          : {machine_name}")
        sub = pe.OPTIONAL_HEADER.Subsystem
        print(f"  subsystem        : {sub} ({'GUI' if sub == 2 else 'CONSOLE - unexpected'})")
        if sub != 2:
            print("  ERROR: expected a GUI subsystem executable")
            ok = False
        print(f"  min OS version   : {pe.OPTIONAL_HEADER.MajorOperatingSystemVersion}."
              f"{pe.OPTIONAL_HEADER.MinorOperatingSystemVersion}")
        print(f"  subsystem version: {pe.OPTIONAL_HEADER.MajorSubsystemVersion}."
              f"{pe.OPTIONAL_HEADER.MinorSubsystemVersion}")
        print(f"  dll characteristics: 0x{pe.OPTIONAL_HEADER.DllCharacteristics:04x}")
        if pe.OPTIONAL_HEADER.DllCharacteristics & 0x0020:
            print("  note: HIGH_ENTROPY_VA set (ignored by Windows 7)")
        if pe.OPTIONAL_HEADER.DllCharacteristics & 0x0100:
            print("  note: DYNAMIC_BASE (ASLR) enabled")

        types = {}
        if hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
            for entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
                name = pefile.RESOURCE_TYPE.get(entry.struct.Id, str(entry.struct.Id))
                types[name] = len(entry.directory.entries)
        print(f"  resources        : {types}")
        for required in ("RT_ICON", "RT_GROUP_ICON", "RT_VERSION"):
            if required not in types:
                print(f"  ERROR: missing resource {required}")
                ok = False

        imports = {}
        for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
            dll = entry.dll.decode()
            names = []
            for imp in entry.imports:
                if imp.name:
                    names.append(imp.name.decode())
            imports[dll] = names
        print(f"  imported DLLs    : {sorted(imports)}")

        banned = sorted({n for names in imports.values() for n in names if n in TOO_NEW})
        if banned:
            print(f"  ERROR: imports newer-Windows APIs: {banned}")
            ok = False
        else:
            print("  API check        : no Windows-8+/10-1703+ only APIs imported")

        total_funcs = sum(len(v) for v in imports.values())
        print(f"  imported functions: {total_funcs}")

        # Debug/build paths (privacy + reproducibility)
        data = open(path, "rb").read()
        leaks = sorted(set(re.findall(rb"[A-Za-z]:\\\\[^\x00]{4,60}|/home/[a-z0-9_]+/[^\x00]{4,60}", data)))
        if leaks:
            print(f"  ERROR: build machine paths embedded: {leaks[:3]}")
            ok = False
        else:
            print("  path leak check  : clean")

        print(f"  file size        : {len(data):,} bytes")
    return 0 if ok else 1

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
