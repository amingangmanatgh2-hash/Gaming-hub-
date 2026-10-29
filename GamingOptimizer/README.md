<div dir="rtl">

# 🎮 Gaming Hub Optimizer — بهینه‌ساز ویندوز (FPS + کیفیت + دیسپلی)

اپ ویندوزی (WPF / .NET Framework 4.8) برای **ویندوز ۱۰/۱۱** که سیستم را برای بازی بهینه می‌کند.

## 📥 دانلود و نصب
از بخش [**Releases**](../../releases) فایل **`GamingHubOptimizer_Setup_1.0.0.exe`** را بگیرید و اجرا کنید (ادمین لازم است).
نسخه‌ی پرتابل: `GamingHubOptimizer.exe`

## ✨ قابلیت‌ها
| بخش | امکانات |
|---|---|
| 📊 داشبورد | اسکن CPU/GPU/RAM/رزولوشن/هرتز، امتیاز گیمینگ، پاک‌سازی Temp + Shader Cache (NVIDIA/AMD/DirectX) |
| 🚀 FPS | Game Mode، خاموش‌کردن Game DVR، HAGS، پلن Ultimate Performance، اولویت GPU/CPU بازی، جلوه‌های بصریِ عملکردی |
| 💎 Quality | جلوه‌های بصری بهترین ظاهر، ClearType، خاموش‌کردن شتاب موس (ایم خام) |
| 🖥 Display | گزارش هرتز/رزولوشن، خاموشی Fullscreen Optimizations، چک‌لیست G-Sync/FreeSync، میانبر تنظیمات دیسپلی |
| 🛡 ایمنی | بکاپ خودکار رجیستری قبل از هر اعمال + Restore داخل برنامه + ساخت Restore Point ویندوز |

سه پروفایل آماده: **🏆 Max FPS** · **⚖ متعادل** · **💎 Ultra Quality + Display**

## 🔨 ساخت از سورس (Setup.exe)

```bat
:: ۱) بیلد Release با Visual Studio 2022 / MSBuild
msbuild GamingOptimizer\GamingOptimizer\GamingOptimizer.csproj /p:Configuration=Release

:: ۲) ساخت اینستالر با Inno Setup 6
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" GamingOptimizer\installer\GamingOptimizer.iss
:: خروجی: GamingOptimizer\installer\Output\GamingHubOptimizer_Setup_1.0.0.exe
```

یا فقط از GitHub Actions (workflow: `build-optimizer.yml` → Run workflow) — خودش **Setup.exe** می‌سازد و در **Release `optimizer-v1.0.0`** منتشر می‌کند.

## ⚠ نکته
بعضی تغییرات (HAGS و پلن پاور) بعد از **ری‌استارت** کامل اثر می‌کنند. برنامه هیچ تله‌متری ندارد و همه‌چیز لوکال است.

</div>
