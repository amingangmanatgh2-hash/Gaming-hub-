# یادداشت طراحی نسخهٔ ۷

پیش از طراحی، نتایج جست‌وجوهای گستردهٔ فارسی و انگلیسی دربارهٔ ارائهٔ کنفرانسی، Morph، حرکت سینمایی، تایپوگرافی، منوی فصل‌بندی و خوانایی پروژکتور بررسی شد. منابع شاخص:

- Microsoft Support — Morph tips and `!!` object naming
- Microsoft Support — transition timing and performance guidance
- Stephanie Walter — conference slide structure, RTL alignment, large type, one idea per slide
- Editage — back-row readability and conference font sizing
- Deckary — grid, hierarchy, whitespace and projector checks
- 24Slides — distinction between slide transition and in-slide animation
- Presenta (فارسی) — Morph, off-canvas staging and perspective shifts
- مدیر فناوری (فارسی) — سناریو، مسیر ذهنی، ناوبری و استفادهٔ هدفمند از ترنزیشن
- Visme / PPT Productivity — chapter-based agenda and numbered ranges
- Piktochart / Figma — consistent narrative, bold typography and section pacing
- Training Lady / Nuts & Bolts — Morph zoom/pan using shared high-resolution imagery
- University of York / Microsoft — reducing heavy effects on weaker computers

## تکنیک‌های اعمال‌شده

1. فهرست پنج‌فصلی در اسلاید ۲، بدون نام ارائه‌دهندگان، با بازهٔ شمارهٔ اسلایدها.
2. یک پیام اصلی در هر اسلاید و متن‌های کوتاهِ قابل خواندن از انتهای کلاس.
3. سیستم تایپوگرافی ثابت با عنوان، برچسب فصل، متن پشتیبان و اعداد برجسته.
4. پنل‌های شیشه‌ای چندلایه با خط تأکید، هایلایت و سایهٔ نرم؛ نه جعبه‌های تکراری ساده.
5. Morph مبتنی بر اشیای مشترک `!!Scene`، `!!TitlePanel`، `!!Title`، `!!Progress` و `!!KineticRail`.
6. حرکت افقی کنترل‌شده، پن دوربین و ریل متحرک فقط هنگام جابه‌جایی بین اسلایدها.
7. Zoom و Split فقط برای تغییر فصل/ریتم؛ بدون افکت‌های شلوغ و بدون صدا.
8. صفر انیمیشن داخل اسلاید، صفر GIF و فایل سبک برای رایانهٔ مدرسه.
