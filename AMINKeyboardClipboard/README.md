<div dir="rtl">

# ⌨️ AMIN Keyboard &amp; Clipboard

کیبورد مجازی **فارسی/انگلیسی** + تاریخچه **متنی** کلیپ‌بورد برای ویندوز.
WPF روی **.NET Framework 4.8** — بدون نصب، بدون سرور، بدون اینترنت، بدون دسترسی مدیر.

> این یک برنامه دسکتاپ جداست و هیچ ارتباطی با کد Cloudflare Worker موجود در ریشه مخزن (SelfHub) ندارد.

---

## ⚠️ وضعیت واقعی این نسخه (مهم)

| موضوع | وضعیت |
|---|---|
| کامپایل شدن سورس | ✅ تأییدشده — GitHub Actions روی `windows-latest` (`dotnet build -c Release` + کامپایل Inno Setup) |
| نسخه منتشرشده | ✅ [Release kb-v1.0.0](https://github.com/amingangmanatgh2-hash/Gaming-hub-/releases/tag/kb-v1.0.0) — خروجی همان بیلد CI |
| اجرا روی دستگاه واقعی (Dell Latitude / Windows 10 1803) | **تست نشده** — نه در این محیط قابل اجراست و نه ادعایش را می‌کنیم |
| تست دستی کیبورد در Word / Notepad / مرورگر | **تست نشده** — طبق انتظار کار می‌کند، اما باید خودتان امتحان کنید |
| امضای دیجیتال (Code Signing) | ندارد؛ ویندوز SmartScreen ممکن است هشدار بدهد |

نسخه‌ای که در Release گذاشته می‌شود همان خروجی بیلد CI است؛ اگر بیلد CI شکست بخورد، فایلی هم منتشر نمی‌شود.

---

## ✨ قابلیت‌ها

- **کیبورد شناور** فارسی و انگلیسی با چیدمان ردیفی (۱۲/۱۲/۱۱/۱۱ کلید + ردیف عملیات).
- ارسال کاراکتر با `SendInput` و `KEYEVENTF_UNICODE` — مستقل از لی‌آوت فعال ویندوز، پس حروف فارسی همیشه درست تایپ می‌شوند.
- پنجره کیبورد با `WS_EX_NOACTIVATE` ساخته می‌شود: کلیک روی کلیدها **فوکوس پنجره مقصد را نمی‌دزدد**.
- جابه‌جایی پنجره با کشیدن نوار عنوان آن (پیاده‌سازی دستی، چون پنجره فعال نمی‌شود) و ذخیره محل آن.
- تاریخچه کلیپ‌بورد **فقط متن**: حداکثر ۱۰۰ مورد، حذف تکراری، جستجو، کپی مجدد، حذف و پاک‌سازی کامل.
- ترک کردن (de-dup) و محدودیت طول هر مورد (۲۰٬۰۰۰ کاراکتر) تا فایل تاریخچه بی‌نهایت رشد نکند.
- میانبر سراسری: تلاش برای `Win + V` و در صورت اشغال بودن، `Ctrl + Alt + V`.
- تنظیم «اجرای خودکار در ورود به ویندوز» با `HKCU` (بدون UAC).
- پنجره خوش‌آمدگویی و توضیح حریم خصوصی در اجرای اول.
- تک‌نمونه‌ای (Single instance): اجرای دوباره، پنجره موجود را باز می‌کند.
- مدیریت خطا: خطای UI برنامه را نمی‌بندد؛ کلیپ‌بورد قفل‌شده با retry خوانده می‌شود.

## 🔒 حریم خصوصی (دقیقاً چه چیزی ذخیره می‌شود)

- فقط `TextDataFormat.UnicodeText` خوانده می‌شود. تصویر، فایل و فرمت‌های سفارشی **نادیده گرفته می‌شوند**.
- هیچ کد شبکه‌ای در برنامه نیست: نه آمار، نه آپلود، نه به‌روزرسانی خودکار.
- هیچ تشخیص یا ذخیره گذرواژه‌ای وجود ندارد.
- داده‌ها فقط اینجا: `%LocalAppData%\AMINKeyboardClipboard\`

```text
clipboard-history.json   ← متن‌های کپی‌شده (JSON ساده، رمزنگاری نشده)
first-run.done           ← پرچم نمایش‌یک‌باره خوش‌آمد
keyboard-bounds.txt      ← محل و اندازه پنجره کیبورد
```

> تاریخچه **رمزنگاری نمی‌شود**. اگر روی رایانه‌ای مشترک کار می‌کنید، از دکمه «پاک کردن همه» یا تب تنظیمات → «حذف کامل داده‌های محلی» استفاده کنید.

---

## 🧱 ساختار پروژه

```text
AMINKeyboardClipboard/
├─ AMINKeyboardClipboard.csproj   ← SDK-style، net48، UseWPF
├─ AMINKeyboardClipboard.sln
├─ App.xaml / App.xaml.cs         ← منابع استایل + تک‌نمونه‌ای
├─ MainWindow.xaml / .cs          ← سه تب: کیبورد، تاریخچه، تنظیمات
├─ FloatingKeyboardWindow.cs      ← کیبورد شناور (کد کامل، بدون XAML)
├─ ClipboardHistoryService.cs     ← شنود و ذخیره تاریخچه متنی
├─ NativeMethods.cs               ← user32: clipboard، hotkey، SendInput، WinEvent
├─ Settings.cs                    ← HKCU، پرچم اجرای اول، محل کیبورد
├─ FirstRunWindow.xaml / .cs
├─ App.config, app.manifest
├─ Properties/AssemblyInfo.cs
└─ installer/
   ├─ AMINKeyboardClipboard.iss   ← Inno Setup 6
   └─ Farsi.isl                   ← ترجمه فارسی نصب‌کننده (از پروژه Inno Setup)
```

## 🔨 ساخت (روی ویندوز)

```powershell
# ۱) پیش‌نیاز: .NET Framework 4.8 Developer Pack + (برای نصب‌کننده) Inno Setup 6
dotnet build .\AMINKeyboardClipboard\AMINKeyboardClipboard.csproj -c Release
# خروجی: AMINKeyboardClipboard\bin\Release\net48\AMIN Keyboard Clipboard.exe

# ۲) ساخت نصب‌کننده
& "$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe" .\AMINKeyboardClipboard\installer\AMINKeyboardClipboard.iss
# خروجی: AMINKeyboardClipboard\installer\Output\AMINKeyboardClipboardSetup.exe
```

با Visual Studio: فایل `AMINKeyboardClipboard.sln` را باز کنید و `Release | Any CPU` را بیلد کنید.

## 🖱️ استفاده

1. برنامه مقصد (Notepad/Word/مرورگر) را باز کنید و داخل محل تایپ کلیک کنید.
2. AMIN را باز کنید → تب «کیبورد مجازی» → «نمایش کیبورد شناور».
3. روی کلیدها کلیک کنید؛ کاراکتر به همان پنجره مقصد می‌رود.
4. `فا / EN` زبان را عوض می‌کند. در حالت انگلیسی `Shift` حروف بزرگ/کوچک را جابه‌جا می‌کند.
5. برای متن‌های کپی‌شده: تب «تاریخچه کلیپ‌بورد» → دوبار کلیک یا `Enter` روی مورد → در برنامه مقصد `Ctrl + V`.
6. برای جابه‌جایی کیبورد، نوار عنوان آن را با ماوس بکشید (محل ذخیره می‌شود).

## ⚠️ محدودیت‌های شناخته‌شده

1. **برنامه‌های Administrator**: ویندوز اجازه تزریق ورودی از برنامه با دسترسی معمولی به پنجره Elevated را نمی‌دهد (مرز امنیتی UIPI). راه‌حل: خود AMIN را هم «Run as administrator» اجرا کنید.
2. **`Win + V`** تقریباً در همه نسخه‌های Windows 10 توسط خود ویندوز گرفته شده؛ برنامه خودکار به `Ctrl + Alt + V` سوییچ می‌کند و این را در تب تنظیمات نشان می‌دهد.
3. **کنترل‌های غیراستاندارد** (بعضی بازی‌ها، ریموت‌دسکتاپ بعضی برنامه‌ها) ممکن است ورودی Unicode را نپذیرند.
4. **آیکن سینی (Tray)** ندارد؛ پنجره اصلی را نبندید اگر می‌خواهید میانبر کار کند. بستن پنجره = خروج کامل.
5. تاریخچه رمزنگاری‌شده نیست و فقط متن است (نه عکس/فایل).
6. روی Windows 10 1803 با **.NET Framework 4.8** تست نشده؛ نصب‌کننده وجود 4.8 را بررسی و هشدار می‌دهد.
7. نصب‌کننده امضای دیجیتال ندارد، پس SmartScreen ممکن است پیام «Windows protected your PC» بدهد.

---

</div>

## English summary

A small, dependency-free WPF (.NET Framework 4.8) Windows app:

- **Floating Persian/English on-screen keyboard** that injects Unicode text with `SendInput`, and is created with `WS_EX_NOACTIVATE` so clicking keys never steals focus from the target window. Draggable, remembers its position.
- **Text-only clipboard history** (max 100 entries, de-duplicated, max 20,000 chars each), stored locally as JSON in `%LocalAppData%\AMINKeyboardClipboard\`. Search, re-copy, delete, clear.
- Global hot key: tries `Win+V`, falls back to `Ctrl+Alt+V`.
- Optional per-user autostart via `HKCU\...\Run` (no UAC). Single-instance. Inno Setup 6 installer with Persian and English UI, per-user install (`PrivilegesRequired=lowest`), and a .NET 4.8 presence check.

**Honest status:** the source is compiled and the installer is produced by GitHub Actions (`.github/workflows/build-windows.yml`) on every push touching this folder. It has **not** been run on physical hardware — in particular not on a Dell Latitude with Windows 10 1803. Source-level review, CI compile, and real-machine testing are different things.

### What was fixed compared with the original draft

| # | Problem in the draft | Fix |
|---|---|---|
| 1 | `Microsoft.CSharp.targets` imported twice (top and bottom of the `.csproj`) — MSBuild error | Single SDK-style `<Project Sdk="Microsoft.NET.Sdk">`, no duplicate import |
| 2 | Only `System.Windows.Forms.Keys` gave access to `V`; reference was missing | Raw virtual-key constant `0x56`, and the Windows Forms dependency is gone |
| 3 | `INPUT` union contained only `KEYBDINPUT`, so `sizeof(INPUT)` was smaller than the native `INPUT` and `SendInput` could keep returning 0 | Full `MOUSEINPUT`/`KEYBDINPUT`/`HARDWAREINPUT` union at correct offsets |
| 4 | `Shift` mutated the static `englishKeys` array it was iterating over while rebuilding the layout | Immutable base layouts plus a `shift` flag, upper case derived on demand |
| 5 | `GetForegroundWindow`/hook reported **foreground** windows only, so focusing the app could make it type into itself | `WINEVENT_SKIPOWNPROCESS` hook, own-process filtering, `IsWindow` validity check |
| 6 | `WndProc` used `clipboardService` before it was created, and kept reading clipboard after `Stop()` | Null-guarded hook, subscription removed on close, listener state tracked |
| 7 | Two synchronous `Clipboard` reads swallowed all failures silently | Bounded retry loop, single read per update, `lastKnownClipboardText` reset on clear |
| 8 | Closing the app with the keyboard window open left an unhooked `SetWinEventHook` | Unhook in `OnClosing`, window closed from `MainWindow_Closed` |
| 9 | `Settings.SetStartupEnabled` wrote `Assembly.Location`, which can be a shadow-copy path | Same, plus `IsStartupEnabled` now compares against the current exe path so stale entries are not shown as enabled |
| 10 | Installer referenced `compiler:Languages\Persian.isl`, which Inno Setup 6 does not ship (Persian exists only as an unofficial translation `Farsi.isl`) — compile error | `Farsi.isl` shipped in `installer/`, referenced locally |
| 11 | Installer source path `..\bin\Release\...` and `*.dll` glob did not match the modern `net48` output folder | `..\bin\Release\net48` with `skipifsourcedoesntexist` on the DLL glob |
| 12 | The `.iss` `[UninstallDelete]`/data handling and `.NET` requirement were undocumented | Explicit `.NET 4.8` pre-check in `[Code]`, data-folder behaviour documented |
| 13 | Every keystroke stole focus, and the whole window flickered | Non-activating window + manual drag + per-key `SendInput` error reporting (including `ERROR_ACCESS_DENIED` → elevation hint) |
| 14 | Hard-coded `TaskbarIcon`/tray expectations with no tray support | Documented: closing the main window exits; hot key restores it |
| 15 | `System.Web.Extensions` / `JavaScriptSerializer` made the **WPF markup compiler fail** (`MC1000: Could not find assembly 'System.Web'`) — found by CI, not by inspection | Removed the dependency; history now uses `DataContractJsonSerializer` with an atomic temp-file write |
| 16 | History was written directly over the live file, so a crash mid-write could truncate it | Write to `.tmp` then `File.Replace`; malformed JSON resets cleanly instead of crashing |

> Items 1–14 came from reading the draft; **item 15 was only found by actually compiling it in CI**, which is the point: a plausible-looking project is not a building one. There was also no `System.Web` reference in the draft's `.csproj` at all, so it failed immediately.
