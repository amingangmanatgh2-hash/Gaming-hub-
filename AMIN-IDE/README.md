<div dir="rtl">

# 🛠️ AMIN IDE — محیط برنامه‌نویسی Arduino (کپی Arduino IDE)

**AMIN IDE** یک محیط برنامه‌نویسی Arduino مبتنی بر مرورگر است که **کامپایل واقعی AVR را در خود مرورگر انجام می‌دهد** — کاملاً offline، **بدون نیاز به فیلتر شکن**، و با رابط دوزبانه فارسی/انگلیسی.

> این محیط کپی و بازنویسی‌شده‌ی Arduino IDE است: دقیقاً همان کاربردهای اصلی (اسکرت‌نویسی، بررسی و رفع خطا، بارگذاری، مانیتور سری) را دارد — اما کل تورلن کامپایل (avr-gcc) و کتابخانه‌ها داخل自分たち بسته‌بندی شده‌اند و هیچ چیز خارجی‌ای لازم نیست.

---

## ✨ قابلیت‌های اصلی

| # | قابلیت | توضیح |
|---|---------|--------|
| ۱ | ✍️ **ویرایشگر سبک** | ویرایشگر خط‌شماره‌دار با highlighted C++، auto-indent، bracket matching — بدون هیچ وابستگی خارجی |
| ۲ | 🔨 **کامپایل واقعی** | تورلن avr-gcc / avr-as / avr-ld / objcopy به صورت WASM کاملاً در مرورگر — **HEX واقعی** تولید می‌کند |
| ۳ | 📦 **offline، بدون فیلتر شکن** | تمام headers، objects و کتابخانه‌ها bundle شده‌اند؛ اجرای آفلاین کاملاً امکان‌پذیر است |
| ۴ | 💾 **HEX output** |نتيجة کامپایل، فایل .hex قابل بارگذاری روی Uno/Nano/Mini |
| ۵ | 🖥️ **مانیتور سری واقعی** | Web Serial API (Chrome/Edge) + حالت دمو برای تست بدون برد |
| ۶ | 📚 **کتابخانه‌های offline** | Wire، SPI، Servo، DHT، BMP085، BusIO، Firmata + پکیج‌های سنسور OLED/TOF |
| ۷ | 🌍 **دوزبانه** | فارسی (RTL) و انگلیسی (LTR) با switch زبان |
| ۸ | 🚀 **GitHub Actions** | build + release اتوماتیک روی گیت‌هاب |

## 📋 بردهای قابل کامپایل

| برد | میکروکنترلر | فرکانس | فلاش | SRAM |
|------|--------------|---------|-------|------|
| Arduino Uno R3 | ATmega328P | 16 MHz | 32 KB | 2 KB |
| Arduino Nano | ATmega328P | 16 MHz | 32 KB | 2 KB |
| Arduino Mini | ATmega328P | 16 MHz | 32 KB | 2 KB |
| Arduino Fio | ATmega328P | 8 MHz | 32 KB | 2 KB |
| Pro Mini 3.3V | ATmega328P | 8 MHz | 32 KB | 2 KB |

## 🖥️รัน کردن

**راه ساده (رسمی):** از GitHub Releases डाउनलोड کنید، zip را باز کنید و `index.html` را با مرورگر باز کنید.

**راه توسعه‌محور:**
```bash
npm ci
npm run dev      # dev server لوکال روی 5173
npm run build    # dist/ کامل + تورلن offline
```

## 🔬 تست

```bash
node scripts/smoke-toolchain.mjs   # کامپایل واقعی Blink با تورلن اصلی
node scripts/smoke-bundle.mjs      # کامپایل با worker bundled
```

## 🏗️ ساخت

- `src/` —فایل‌های frontend (TypeScript، esbuild)
- `src/compiler-worker.ts` — worker کامپایل (turlen avr-gcc-wasm)
- `dist/vendor/avr-gcc-wasm/` — تورلن offline (avr-libc headers، objects، libc/libm/libgcc)
- `.github/workflows/amin-ide.yml` — build + smoke test + release

## ⚠️ محدودیت‌ها

- کامپایل صرفاً برای خانواده **ATmega328P** (Uno/Nano/Mini/Fio/Pro Mini) — کسانی‌که می‌خواهند ESP32/Mega را بیल्ड کنند باید منتظر نسخه بعدی بمانند.
- بارگذاری مستقیم روی برد از مرورگر ممکن نیست (WebUSB هنوز کامل نشده است) — HEX دانلود شده و با Arduino IDE / uf2conv / avrdude بارگذاری می‌شود.

---

*AMIN IDE — با love در ایران ساخته شده 💜*

</div>
