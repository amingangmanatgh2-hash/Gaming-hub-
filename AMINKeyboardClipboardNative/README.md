<div dir="rtl">

# ⌨️ AMIN Keyboard & Clipboard — نسخه بومی (بدون .NET)

کیبورد مجازی **فارسی/انگلیسی** + تاریخچه **متنی** کلیپ‌بورد برای ویندوز —
نوشته‌شده با **C و Win32 خالص**.

> ## ⚠️ چرا این نسخه ساخته شد
> نسخه قبلی (WPF / .NET Framework 4.8) روی کامپیوتر شما نصب نمی‌شد با پیام:
> «.NET Framework 4.8.1 is not supported on this operating system».
> این نسخه **هیچ رانتایمی لازم ندارد**: نه .NET، نه Visual C++ Redistributable.
> CRT به‌صورت استاتیک داخل خود فایل `.exe` لینک شده است.
> تنها DLLهایی که برنامه استفاده می‌کند DLLهای خود ویندوز هستند:
> `KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `SHLWAPI`, `ADVAPI32`.

---

## 📦 دریافت

از صفحه [Releases](https://github.com/amingangmanatgh2-hash/Gaming-hub-/releases) :

| فایل | توضیح |
|---|---|
| `AMINKeyboardClipboardSetup.exe` | نصب‌کننده — خودش نسخه ۶۴ یا ۳۲ بیتی را انتخاب می‌کند |
| `AMINKeyboardClipboard-win64.zip` | نسخه بدون نصب (۶۴ بیتی) |
| `AMINKeyboardClipboard-win32.zip` | نسخه بدون نصب (۳۲ بیتی) |
| `SHA256SUMS.txt` | چک‌سام |

## ⚙️ پیش‌نیاز

- ویندوز ۷ SP1 یا جدیدتر (شامل Windows 10 نسخه 1803)، ۶۴ یا ۳۲ بیتی.
- **همین.** هیچ چیز دیگری نصب نمی‌شود.

## ✨ قابلیت‌ها

- **کیبورد شناور فارسی/انگلیسی** — ارسال کاراکتر با `SendInput` + `KEYEVENTF_UNICODE`،
  مستقل از لی‌آوت فعال ویندوز (پس حروف فارسی همیشه درست تایپ می‌شوند).
- پنجره کیبورد با `WS_EX_NOACTIVATE` ساخته می‌شود → کلیک روی کلیدها **فوکوس پنجره مقصد را نمی‌دزدد**.
  (کلیدها با GDI کشیده می‌شوند، هیچ کنترل فرزندی وجود ندارد که فوکوس بگیرد.)
- جابه‌جایی با کشیدن نوار عنوان؛ محل ذخیره می‌شود و هنگام باز شدن بررسی می‌شود که روی یک مانیتور موجود باشد.
- **تاریخچه متنی کلیپ‌بورد**: حداکثر ۱۰۰ مورد، حذف تکراری، جستجو، کپی مجدد، حذف، پاک‌سازی کامل.
- ذخیره‌سازی محلی در `%LocalAppData%\AMINKeyboardClipboard\` — هیچ کد شبکه‌ای در برنامه نیست.
- میانبر سراسری: تلاش برای `Win + V`، در صورت اشغال بودن `Ctrl + Alt + V`؛ نتیجه در تب تنظیمات نشان داده می‌شود.
- اجرای خودکار اختیاری در ورود به ویندوز (`HKCU` فقط، بدون UAC).
- **آیکن سینی**: بستن پنجره = رفتن به سینی، تا میانبر فعال بماند. خروج کامل: راست‌کلیک روی آیکن سینی → «خروج».
- تک‌نمونه‌ای: اجرای دوم، پنجره موجود را جلو می‌آورد.
- حریم خصوصی و راهنما در تب تنظیمات + پنجره خوش‌آمد در اجرای اول.

## 🔒 حریم خصوصی (چه چیزی ذخیره می‌شود)

- فقط `CF_UNICODETEXT` خوانده می‌شود. تصویر، فایل لیست‌شده و هر قالب دیگری **نادیده گرفته می‌شود**.
- هیچ اتصال شبکه‌ای، آمار، آپلود یا به‌روزرسانی خودکار وجود ندارد.
- هیچ تشخیص یا ذخیره گذرواژه‌ای وجود ندارد.
- فایل‌های ساخته‌شده:

```text
clipboard-history.dat   ← تاریخچه متن (دودویی، رمزنگاری‌نشده)
settings.ini            ← محل پنجره کیبورد
first-run.done          ← پرچم پنجره خوش‌آمد
```

> تاریخچه **رمزنگاری نمی‌شود**. روی رایانه مشترک از «حذف کامل داده‌های محلی» استفاده کنید.

## 🧱 ساختار

```text
AMINKeyboardClipboardNative/
├─ src/
│  ├─ app.h              ← تعریف‌های مشترک
│  ├─ main.c             ← پنجره اصلی، سینی، میانبر سراسری، شنود کلیپ‌بورد
│  ├─ keyboard.c         ← کیبورد شناور (رسم کامل با GDI)
│  ├─ clipboard.c        ← مدل تاریخچه + ذخیره دودویی اتمیک
│  ├─ settings.c         ← HKCU\\Run، مسیر داده‌ها، محل کیبورد
│  ├─ app.rc             ← آیکن + نسخه + manifest
│  ├─ app.manifest       ← asInvoker، سازگاری Win7..11
│  └─ icon.ico
├─ build.bat             ← بیلد با MSVC  (build.bat x64 | x86)
├─ build.sh              ← بیلد متقاطع با Zig (فقط بررسی کامپایل، بدون منابع)
└─ installer/
   ├─ AMINKeyboardClipboard.iss   ← Inno Setup 6، بدون هیچ پیش‌نیاز .NET
   └─ Farsi.isl                   ← ترجمه فارسی نصب‌کننده
```

## 🔨 ساخت

**ویندوز (روش رسمی، همان چیزی که Release از آن ساخته می‌شود):**

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd AMINKeyboardClipboardNative
build.bat x64
:: و برای 32 بیتی، در یک پنجره دیگر:
:: call "...\vcvars32.bat" && build.bat x86
```

نصب‌کننده:

```bat
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\AMINKeyboardClipboard.iss
:: خروجی: installer\Output\AMINKeyboardClipboardSetup.exe
```

**لینوکس/مک (فقط بررسی کامپایل):**

```sh
./build.sh     # نیاز به zig در PATH
```

## 🔬 وضعیت واقعی

| موضوع | وضعیت |
|---|---|
| کامپایل x64 و x86 | ✅ تأییدشده — MSVC در GitHub Actions (`dumpbin /dependents` در لاگ) |
| وجود وابستگی .NET | ✅ ندارد — فقط DLLهای ویندوز |
| اجرای واقعی روی سخت‌افزار، از جمله Dell Latitude / Windows 10 1803 | ❌ **تست نشده** |
| رفتار تایپ در Word / مرورگر / کنترل غیراستاندارد | ❌ تست نشده |
| اجرای نصب‌کننده روی همان دستگاه | ❌ تست نشده |

صادقانه: این کد در محیط من **اجرا نشده** (نه ویندوز در دسترس بود، نه Wine). فقط کامپایل، لینک و
ساخت نصب‌کننده با موفقیت انجام شده است. اگر جایی رفتار غیرمنتظره دیدید، بگویید تا اصلاح کنم.

## ⚠️ محدودیت‌های شناخته‌شده

1. **پنجره‌های Administrator**: ویندوز (UIPI) اجازه تزریق ورودی از برنامه معمولی را نمی‌دهد.
   برنامه در این حالت پیام راهنما نشان می‌دهد؛ راه‌حل: خود AMIN را هم «Run as administrator» اجرا کنید.
2. **`Win + V`** تقریباً همیشه توسط خود ویندوز گرفته شده؛ برنامه خودکار `Ctrl + Alt + V` می‌گذارد.
3. کنترل‌های غیراستاندارد (بعضی بازی‌ها / برخی برنامه‌های ریموت) ممکن است ورودی Unicode را نپذیرند.
4. بیلد **امضای دیجیتال ندارد** → ممکن است SmartScreen هشدار بدهد (`More info` → `Run anyway`).
5. تاریخچه فقط متن است، رمزنگاری نمی‌شود و با حذف برنامه پاک نمی‌شود.

---

</div>

## English summary

A native Win32 (C, MSVC `/MT`) rewrite of the desktop app, created because the
WPF/.NET Framework 4.8 build could not be installed on the user's machine
("The .NET Framework 4.8.1 is not supported on this operating system").

- **No runtime dependency at all.** Static CRT, only OS DLLs imported
  (`KERNEL32`, `USER32`, `GDI32`, `SHELL32`, `SHLWAPI`, `ADVAPI32`).
  Runs on Windows 7 SP1 → Windows 11, 32- and 64-bit.
- Floating Persian/English keyboard drawn with GDI in a `WS_EX_NOACTIVATE`
  window: clicking keys never steals focus; Unicode injection via `SendInput`.
- Text-only clipboard history (100 entries, de-duplicated, searchable),
  stored locally with atomic writes. No network code anywhere.
- Global hot key with `Win+V` → `Ctrl+Alt+V` fallback, tray icon, single
  instance, optional per-user autostart.
- Inno Setup 6 installer with a Persian and English wizard, per-user install,
  **and no .NET check** — that check was exactly what blocked installation.

### Honest status

CI (`.github/workflows/build-windows.yml`) compiles both architectures with MSVC
and builds the installer, and the release assets are published from that run.
The application has **not been executed on physical Windows hardware** — I have
no Windows machine or Wine in this environment. Source review, a successful
compile, and real-world testing are different claims; only the first two are made here.
