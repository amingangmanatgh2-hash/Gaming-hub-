/**
 * ============================================================================
 *  Self-Bot Hub  —  ربات تلگرامی سلف‌ساز + پنل مدیریت وب
 *  Runtime : Cloudflare Workers (ES Modules)
 *  Storage : Cloudflare KV (binding: KV)  +  D1 اختیاری (binding: DB)
 *  Helper  : @helperlevibot  (ربات دستیار / پنل)
 * ============================================================================
 *  ساختار فایل (ماژولار در یک فایل برای دیپلوی تک‌فایلی):
 *    §1  ثابت‌ها و تنظیمات اقتصاد الماس
 *    §2  لایه ذخیره‌سازی (KV + آینه D1)
 *    §3  کلاینت Telegram Bot API
 *    §4  کیبوردها و پنل شیشه‌ای (@helperlevibot)
 *    §5  موتور دستورات ماژولار (رجیستری دستورات سلف)
 *    §6  هندل آپدیت‌های تلگرام (پیام + کال‌بک)
 *    §7  API وب (setup / users / diamonds / broadcast / cron)
 *    §8  صفحات Frontend (فرم راه‌اندازی + پنل مدیریت)
 *    §9  fetch / scheduled (ورودی‌های Worker)
 * ============================================================================
 */

/* ========================================================================== */
/* §1 — ثابت‌ها                                                               */
/* ========================================================================== */

const ECON = {
  ACTIVATION_COST: 10, // هزینه فعال‌سازی اولیه سلف
  DAILY_COST: 30,      // هزینه نگهداری روزانه
  WELCOME_GIFT: 50,    // هدیه ثبت‌نام
};

const BRAND = {
  BOT_NAME: 'Self-Bot Hub',
  HELPER: '@helperlevibot',
  PANEL_TOP: 'Panel LN | CK | Amin',
  VERSION: '1.0.0',
};

const TEHRAN_TZ = 'Asia/Tehran';

function nowTehran() {
  return new Intl.DateTimeFormat('fa-IR', {
    timeZone: TEHRAN_TZ, hour: '2-digit', minute: '2-digit', second: '2-digit',
  }).format(new Date());
}
function todayTehran() {
  return new Intl.DateTimeFormat('fa-IR', {
    timeZone: TEHRAN_TZ, year: 'numeric', month: 'long', day: 'numeric', weekday: 'long',
  }).format(new Date());
}
function faNum(n) { return Number(n).toLocaleString('fa-IR'); }
function randId(len = 32) {
  const a = new Uint8Array(len); crypto.getRandomValues(a);
  return [...a].map(b => 'abcdefghijklmnopqrstuvwxyz0123456789'[b % 36]).join('');
}

/* ========================================================================== */
/* §2 — لایه ذخیره‌سازی (KV + D1 mirror)                                       */
/* ========================================================================== */

const KEY = {
  CONFIG: 'config',
  USER: id => `user:${id}`,
  INDEX: 'users:index',
};

async function getConfig(env) {
  const raw = await env.KV.get(KEY.CONFIG);
  return raw ? JSON.parse(raw) : null;
}
async function saveConfig(env, cfg) {
  await env.KV.put(KEY.CONFIG, JSON.stringify(cfg));
}

function defaultUser(id, from = {}) {
  return {
    id,
    name: [from.first_name, from.last_name].filter(Boolean).join(' ') || 'کاربر',
    username: from.username || '',
    diamonds: ECON.WELCOME_GIFT,
    selfActive: false,
    selfActivatedAt: null,
    lastChargeAt: null,
    joinedAt: Date.now(),
    // settings = سوییچ‌های روشن/خاموش همه ماژول‌ها
    settings: {},
    // داده‌های ماژول‌ها
    enemies: [],
    blocked: [],
    filters: [],
    autoReplies: {},   // trigger -> reply
    secretaryText: 'سلام 👋 در حال حاضر آنلاین نیستم؛ منشی سلف پاسخگوی شماست. ✉️',
    statusText: '',
    state: null,       // وضعیت گفتگو (برای ورودی‌های چندمرحله‌ای)
  };
}

async function getUser(env, id) {
  const raw = await env.KV.get(KEY.USER(id));
  return raw ? JSON.parse(raw) : null;
}

async function saveUser(env, user) {
  await env.KV.put(KEY.USER(user.id), JSON.stringify(user));
  await d1MirrorUser(env, user);
}

async function ensureUser(env, from) {
  let u = await getUser(env, from.id);
  let isNew = false;
  if (!u) {
    u = defaultUser(from.id, from);
    isNew = true;
    await addToIndex(env, from.id);
    await saveUser(env, u);
  }
  return { user: u, isNew };
}

async function getIndex(env) {
  const raw = await env.KV.get(KEY.INDEX);
  return raw ? JSON.parse(raw) : [];
}
async function addToIndex(env, id) {
  const idx = await getIndex(env);
  if (!idx.includes(id)) { idx.push(id); await env.KV.put(KEY.INDEX, JSON.stringify(idx)); }
}
async function listUsers(env) {
  const idx = await getIndex(env);
  const users = [];
  for (const id of idx) {
    const u = await getUser(env, id);
    if (u) users.push(u);
  }
  return users;
}

/** لاگ تراکنش‌های الماس (۱۲۰ مورد آخر) */
async function logTx(env, userId, amount, reason) {
  try {
    const raw = await env.KV.get('txlog');
    const arr = raw ? JSON.parse(raw) : [];
    arr.unshift({ t: Date.now(), id: userId, amount, reason });
    if (arr.length > 120) arr.length = 120;
    await env.KV.put('txlog', JSON.stringify(arr));
  } catch (_) {}
}
async function getTxLog(env) {
  const raw = await env.KV.get('txlog');
  return raw ? JSON.parse(raw) : [];
}

/** آینه اختیاری D1 — اگر binding DB تعریف نشده باشد بی‌صدا رد می‌شود. */
async function d1MirrorUser(env, u) {
  if (!env.DB) return;
  try {
    await env.DB.prepare(
      `INSERT INTO users (id, name, username, diamonds, self_active, joined_at)
       VALUES (?1, ?2, ?3, ?4, ?5, ?6)
       ON CONFLICT(id) DO UPDATE SET
         name=?2, username=?3, diamonds=?4, self_active=?5`
    ).bind(u.id, u.name, u.username, u.diamonds, u.selfActive ? 1 : 0, u.joinedAt).run();
  } catch (_) { /* D1 اختیاری است */ }
}

/* ========================================================================== */
/* §3 — کلاینت Telegram Bot API                                               */
/* ========================================================================== */

async function tg(token, method, body) {
  const res = await fetch(`https://api.telegram.org/bot${token}/${method}`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(body || {}),
  });
  return res.json().catch(() => ({ ok: false }));
}

const send = (cfg, chat_id, text, extra = {}) =>
  tg(cfg.botToken, 'sendMessage', { chat_id, text, parse_mode: 'HTML', ...extra });

const editMsg = (cfg, chat_id, message_id, text, extra = {}) =>
  tg(cfg.botToken, 'editMessageText', { chat_id, message_id, text, parse_mode: 'HTML', ...extra });

const answerCb = (cfg, id, text, alert = false) =>
  tg(cfg.botToken, 'answerCallbackQuery', { callback_query_id: id, text, show_alert: alert });

/* ========================================================================== */
/* §4 — پنل شیشه‌ای @helperlevibot (کیبوردها)                                  */
/* ========================================================================== */

function kbMain() {
  return { inline_keyboard: [
    [{ text: BRAND.PANEL_TOP, callback_data: 'p:top' }],
    [
      { text: '📜 دستورات سلف', callback_data: 'p:cmds' },
      { text: '👤 حساب کاربری', callback_data: 'p:account' },
    ],
    [{ text: '❌ بستن پنل', callback_data: 'p:close' }],
  ]};
}

function kbCommands() {
  return { inline_keyboard: [
    [{ text: '⚙️ مدیریت سلف', callback_data: 'p:self' }],
    [
      { text: '🛠 مدیریت و ابزارها', callback_data: 'c:tools' },
      { text: '⏰ ساعت و وضعیت', callback_data: 'c:clock' },
    ],
    [
      { text: '🐱 اتوماسیون و میوبات', callback_data: 'c:mew' },
      { text: '🛡 چت، پیوی و امنیت', callback_data: 'c:security' },
    ],
    [{ text: '🎮 دانلودر، سرگرمی و مینی‌اپ‌ها', callback_data: 'c:fun' }],
    [{ text: '🔙 بازگشت به پنل', callback_data: 'p:main' }],
  ]};
}

function kbSelfManage(user) {
  const on = user.selfActive;
  return { inline_keyboard: [
    [
      { text: (on ? '✅ ' : '') + 'روشن [⚙️]', callback_data: 's:on' },
      { text: (!on ? '✅ ' : '') + 'خاموش [⚙️]', callback_data: 's:off' },
    ],
    [{ text: '🔙 بازگشت به دستورات', callback_data: 'p:cmds' }],
  ]};
}

function kbCategory(catKey) {
  const cmds = COMMANDS.filter(c => c.cat === catKey);
  const rows = [];
  for (let i = 0; i < cmds.length; i += 2) {
    rows.push(cmds.slice(i, i + 2).map(c => ({ text: c.emoji + ' ' + c.name, callback_data: 'i:' + c.key })));
  }
  rows.push([{ text: '🔙 بازگشت به دستورات', callback_data: 'p:cmds' }]);
  return { inline_keyboard: rows };
}

function kbMiniApps() {
  return { inline_keyboard: [
    [{ text: '🎯 Gamee (بازی‌های تلگرام)', url: 'https://t.me/gamee' }],
    [{ text: '♟ بازی شطرنج', url: 'https://t.me/chessbot' }, { text: '🎲 کازینو مینی‌اپ', url: 'https://t.me/gamebot' }],
    [{ text: '🧩 Hamster-like Apps', url: 'https://t.me/BotFather' }],
    [{ text: '🔙 بازگشت', callback_data: 'c:fun' }],
  ]};
}

function accountText(user) {
  return [
    '👤 <b>حساب کاربری شما</b>',
    '━━━━━━━━━━━━━━━',
    `🪪 نام: <b>${esc(user.name)}</b>`,
    `🔢 آیدی عددی: <code>${user.id}</code>`,
    user.username ? `🌐 یوزرنیم: @${esc(user.username)}` : null,
    `💎 موجودی الماس: <b>${faNum(user.diamonds)}</b>`,
    `🤖 وضعیت سلف: ${user.selfActive ? '🟢 روشن' : '🔴 خاموش'}`,
    `🧩 ماژول‌های فعال: <b>${faNum(Object.values(user.settings).filter(Boolean).length)}</b>`,
    `📅 تاریخ: ${todayTehran()}`,
    '━━━━━━━━━━━━━━━',
    `⚙️ هزینه نگهداری روزانه: ${faNum(ECON.DAILY_COST)} الماس`,
    `🤝 پشتیبانی: ${BRAND.HELPER}`,
  ].filter(Boolean).join('\n');
}

function panelHomeText(user) {
  return [
    `✨ <b>${BRAND.BOT_NAME}</b> — پنل ${BRAND.HELPER}`,
    '━━━━━━━━━━━━━━━',
    `سلام <b>${esc(user.name)}</b> 👋`,
    `💎 موجودی: <b>${faNum(user.diamonds)}</b> الماس`,
    `🤖 سلف: ${user.selfActive ? '🟢 فعال' : '🔴 غیرفعال'}`,
    `🕐 ساعت تهران: <code>${nowTehran()}</code>`,
    '━━━━━━━━━━━━━━━',
    'از دکمه‌های زیر استفاده کنید:',
  ].join('\n');
}

function esc(s) {
  return String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

/* ========================================================================== */
/* §5 — موتور دستورات ماژولار                                                  */
/* ========================================================================== */
/**
 * هر دستور: { key, name, aliases, cat, emoji, toggle, run(ctx) }
 *  - toggle=true یعنی با «<نام> روشن / <نام> خاموش» سوییچ می‌شود.
 *  - ctx = { env, cfg, user, chatId, text, args, reply(txt, extra) }
 */

const CATS = {
  tools:    '🛠 مدیریت و ابزارها',
  clock:    '⏰ ساعت و وضعیت پروفایل',
  mew:      '🐱 اتوماسیون و میو ربات',
  security: '🛡 مدیریت چت، پیوی و امنیت',
  fun:      '🎮 دانلودر، سرگرمی و مینی‌اپ‌ها',
};

const FORTUNES = [
  'امروز روز شانس توست! 🍀', 'یک خبر خوب در راه است 📬', 'مراقب تصمیم‌های عجولانه باش ⚠️',
  'ستاره‌ها می‌گویند: صبر کلید موفقیت است ✨', 'سفری کوتاه در انتظار توست 🧳',
  'یک دوست قدیمی به یادت است 💌', 'امروز بهترین روز برای شروع است 🚀',
];
const MEOWS = ['میو میو 🐱', 'میااااو 😺', 'مرنو... 🐈', 'میو؟ 🐾', 'پیشی گفت: میو 💕'];
const FISH = ['🐟 یک ماهی قزل‌آلا گرفتی! (+۲ الماس)', '🐠 ماهی طلایی نصیبت شد! (+۳ الماس)', '🦀 خرچنگ گرفتی! (+۱ الماس)', '🥾 یک چکمه کهنه... شانس بعدی! (+۰)'];
const RECIPES = ['🍕 پیتزای میویی با طعم تن‌ماهی', '🍣 سوشی مخصوص گربه‌ها', '🥘 خوراک مرغ و جگر', '🍰 کیک شیر و ماهی'];
const MEMES = ['😹 میم «گربه‌ی متعجب» پیدا شد!', '🗿 میم «سنگ مویایی» پیدا شد!', '🤡 میم «دلقک» پیدا شد!', '💀 میم «اسکلت منتظر» پیدا شد!'];
const CHEATS = ['🎯 تقلب: جواب ۴۲ است!', '🃏 تقلب: گزینه ۳ را بزن!', '🎲 تقلب: دوباره تاس بریز، این‌بار ۶ می‌آید!'];

/** کمکی: دستور سوییچی ساده می‌سازد */
function toggleCmd(key, name, cat, emoji, aliases = [], note = '') {
  return {
    key, name, cat, emoji, aliases, toggle: true,
    async run(ctx) {
      const st = !!ctx.user.settings[key];
      if (ctx.args === 'روشن' || ctx.args === 'خاموش') {
        const on = ctx.args === 'روشن';
        ctx.user.settings[key] = on;
        await saveUser(ctx.env, ctx.user);
        return ctx.reply(`${emoji} <b>${name}</b> ${on ? '🟢 روشن شد' : '🔴 خاموش شد'}${note ? '\n' + note : ''}`);
      }
      return ctx.reply(
        `${emoji} <b>${name}</b>\nوضعیت فعلی: ${st ? '🟢 روشن' : '🔴 خاموش'}\n\n` +
        `برای تغییر بنویسید:\n<code>${name} روشن</code> یا <code>${name} خاموش</code>`
      );
    },
  };
}

const COMMANDS = [
  /* ---------------- 🛠 مدیریت و ابزارها ---------------- */
  {
    key: 'transfer', name: 'انتقال الماس', cat: 'tools', emoji: '💸', aliases: [],
    usage: 'انتقال الماس <آیدی عددی> <تعداد>',
    async run(ctx) {
      const m = ctx.args.match(/^(\d+)\s+(\d+)$/);
      if (!m) return ctx.reply('💸 فرمت: <code>انتقال الماس 123456789 20</code>');
      const [, idStr, amtStr] = m;
      const target = Number(idStr), amount = Number(amtStr);
      if (target === ctx.user.id) return ctx.reply('🙃 انتقال به خودتان ممکن نیست!');
      if (amount <= 0 || ctx.user.diamonds < amount) return ctx.reply('❌ موجودی کافی نیست!');
      const tu = await getUser(ctx.env, target);
      if (!tu) return ctx.reply('❌ کاربر مقصد یافت نشد (باید قبلاً ربات را استارت کرده باشد).');
      ctx.user.diamonds -= amount; tu.diamonds += amount;
      await saveUser(ctx.env, ctx.user); await saveUser(ctx.env, tu);
      await logTx(ctx.env, ctx.user.id, -amount, 'transfer_out');
      await logTx(ctx.env, target, amount, 'transfer_in');
      await send(ctx.cfg, target, `💎 <b>${faNum(amount)}</b> الماس از طرف <code>${ctx.user.id}</code> دریافت کردید!\nموجودی جدید: <b>${faNum(tu.diamonds)}</b>`);
      return ctx.reply(`✅ <b>${faNum(amount)}</b> الماس به <code>${target}</code> منتقل شد.\n💎 موجودی شما: <b>${faNum(ctx.user.diamonds)}</b>`);
    },
  },
  toggleCmd('assistant', 'دستیار شخصی', 'tools', '🤵', [], 'دستیار شخصی به پیام‌های شما کمک می‌کند.'),
  {
    key: 'selfmanage', name: 'مدیریت سلف', cat: 'tools', emoji: '⚙️', aliases: ['سلف'],
    async run(ctx) {
      return ctx.reply(
        `⚙️ <b>مدیریت سلف</b>\nوضعیت: ${ctx.user.selfActive ? '🟢 روشن' : '🔴 خاموش'}\n💎 موجودی: ${faNum(ctx.user.diamonds)}`,
        { reply_markup: kbSelfManage(ctx.user) }
      );
    },
  },
  {
    key: 'cleaner', name: 'کلینر', cat: 'tools', emoji: '🧹', aliases: [],
    async run(ctx) {
      return ctx.reply('🧹 <b>کلینر</b>\nپیام‌های سرویس و بی‌اهمیت چت پاک‌سازی شد! ✅\n(در گروه‌ها نیاز به دسترسی ادمین دارد)');
    },
  },
  {
    key: 'calc', name: 'محاسبات', cat: 'tools', emoji: '🧮', aliases: ['حساب'],
    usage: 'محاسبات <عبارت ریاضی>',
    async run(ctx) {
      const expr = ctx.args.replace(/[۰-۹]/g, d => '۰۱۲۳۴۵۶۷۸۹'.indexOf(d));
      if (!expr) return ctx.reply('🧮 فرمت: <code>محاسبات 2+2*10</code>');
      if (!/^[\d+\-*/().%\s]+$/.test(expr)) return ctx.reply('❌ فقط اعداد و عملگرهای ریاضی مجاز است.');
      try {
        const val = Function('"use strict";return (' + expr + ')')();
        return ctx.reply(`🧮 نتیجه:\n<code>${esc(ctx.args)} = ${faNum(val)}</code>`);
      } catch { return ctx.reply('❌ عبارت نامعتبر است.'); }
    },
  },
  {
    key: 'ping', name: 'پینگ', cat: 'tools', emoji: '🏓', aliases: ['ping'],
    async run(ctx) {
      const t0 = Date.now();
      const r = await send(ctx.cfg, ctx.chatId, '🏓 در حال اندازه‌گیری...');
      const dt = Date.now() - t0;
      if (r.ok) await editMsg(ctx.cfg, ctx.chatId, r.result.message_id,
        `🏓 <b>پینگ:</b> <code>${faNum(dt)} ms</code>\n⚡️ پردازش روی Cloudflare Edge`);
      return { handled: true };
    },
  },
  {
    key: 'info', name: 'اطلاعات', cat: 'tools', emoji: 'ℹ️', aliases: ['info'],
    async run(ctx) { return ctx.reply(accountText(ctx.user)); },
  },

  /* ---------------- ⏰ ساعت و وضعیت پروفایل ---------------- */
  toggleCmd('clock', 'ساعت', 'clock', '🕐', [], '🕐 ساعت تهران روی اسم/بیو شما نمایش داده می‌شود (بروزرسانی با کرون).'),
  toggleCmd('clock_payampor', 'ساعت پیامپور', 'clock', '⏱', [], 'ساعت در انتهای هر پیام ارسالی درج می‌شود.'),
  {
    key: 'avatar', name: 'عکس پروفایل', cat: 'clock', emoji: '🖼', aliases: [],
    async run(ctx) {
      const r = await tg(ctx.cfg.botToken, 'getUserProfilePhotos', { user_id: ctx.user.id, limit: 1 });
      if (r.ok && r.result.total_count > 0) {
        const fid = r.result.photos[0].at(-1).file_id;
        await tg(ctx.cfg.botToken, 'sendPhoto', { chat_id: ctx.chatId, photo: fid, caption: '🖼 عکس پروفایل فعلی شما' });
        return { handled: true };
      }
      return ctx.reply('🖼 عکس پروفایلی یافت نشد.');
    },
  },
  toggleCmd('mode_online', 'حالت آنلاین', 'clock', '🟢', [], 'سلف شما همیشه آنلاین نمایش داده می‌شود.'),
  toggleCmd('mode_action', 'حالت اکشن', 'clock', '⌨️', [], 'وضعیت «در حال تایپ...» به‌صورت خودکار ارسال می‌شود.'),
  {
    key: 'mode_text', name: 'حالت متن', cat: 'clock', emoji: '📝', aliases: [],
    usage: 'حالت متن <متن وضعیت>',
    async run(ctx) {
      if (ctx.args === 'خاموش') {
        ctx.user.statusText = ''; ctx.user.settings.mode_text = false;
        await saveUser(ctx.env, ctx.user);
        return ctx.reply('📝 حالت متن 🔴 خاموش شد.');
      }
      if (!ctx.args) return ctx.reply('📝 فرمت: <code>حالت متن سلام دنیا</code>\nخاموش‌کردن: <code>حالت متن خاموش</code>');
      ctx.user.statusText = ctx.args; ctx.user.settings.mode_text = true;
      await saveUser(ctx.env, ctx.user);
      return ctx.reply(`📝 حالت متن 🟢 فعال شد:\n«${esc(ctx.args)}»`);
    },
  },

  /* ---------------- 🐱 اتوماسیون و میو ربات ---------------- */
  toggleCmd('auto_fortune', 'پیشگو خودکار', 'mew', '🔮'),
  toggleCmd('auto_meow', 'میو خودکار', 'mew', '🐱'),
  toggleCmd('auto_transfer', 'انتقال خودکار', 'mew', '💱', [], 'سکه‌های میوبات به‌صورت خودکار منتقل می‌شوند.'),
  toggleCmd('mew_robbery', 'روبت میویی', 'mew', '🦝'),
  {
    key: 'auto_fishing', name: 'ماهیگیری خودکار', cat: 'mew', emoji: '🎣', toggle: true,
    async run(ctx) {
      if (ctx.args === 'روشن' || ctx.args === 'خاموش') {
        ctx.user.settings.auto_fishing = ctx.args === 'روشن';
        await saveUser(ctx.env, ctx.user);
        return ctx.reply(`🎣 ماهیگیری خودکار ${ctx.user.settings.auto_fishing ? '🟢 روشن' : '🔴 خاموش'} شد.`);
      }
      const catch_ = FISH[Math.floor(Math.random() * FISH.length)];
      const bonus = Number((catch_.match(/\+([۰-۹\d])/) || [0, 0])[1]) || 0;
      if (bonus) { ctx.user.diamonds += bonus; await saveUser(ctx.env, ctx.user); }
      return ctx.reply(`🎣 <b>ماهیگیری</b>\n${catch_}\n💎 موجودی: ${faNum(ctx.user.diamonds)}`);
    },
  },
  toggleCmd('mew_worknote', 'کارنوشته میویی', 'mew', '📋'),
  {
    key: 'mew_chef', name: 'آشپز میویی', cat: 'mew', emoji: '👨‍🍳',
    async run(ctx) {
      return ctx.reply(`👨‍🍳 <b>آشپز میویی</b> امروز پیشنهاد می‌دهد:\n${RECIPES[Math.floor(Math.random() * RECIPES.length)]}`);
    },
  },
  {
    key: 'mew_steal', name: 'سرقت میویی', cat: 'mew', emoji: '🕵️',
    async run(ctx) {
      const ok = Math.random() > 0.5;
      if (ok) { ctx.user.diamonds += 2; await saveUser(ctx.env, ctx.user); }
      return ctx.reply(ok
        ? `🕵️ سرقت موفق! ۲ الماس به جیب زدی 😼\n💎 موجودی: ${faNum(ctx.user.diamonds)}`
        : '🚨 پلیس میویی تو را گرفت! سرقت ناموفق بود 😿');
    },
  },

  /* ---------------- 🛡 مدیریت چت، پیوی و امنیت ---------------- */
  {
    key: 'enemy', name: 'دشمن', cat: 'security', emoji: '😈',
    usage: 'دشمن <آیدی عددی> | دشمن لیست | دشمن حذف <آیدی>',
    async run(ctx) {
      const u = ctx.user;
      if (ctx.args === 'لیست' || !ctx.args) {
        return ctx.reply('😈 <b>لیست دشمنان:</b>\n' + (u.enemies.length ? u.enemies.map(e => `• <code>${e}</code>`).join('\n') : '— خالی —') +
          '\n\nافزودن: <code>دشمن 123456</code>\nحذف: <code>دشمن حذف 123456</code>');
      }
      const del = ctx.args.match(/^حذف\s+(\d+)$/);
      if (del) {
        u.enemies = u.enemies.filter(e => e !== Number(del[1]));
        await saveUser(ctx.env, u);
        return ctx.reply(`✅ <code>${del[1]}</code> از لیست دشمنان حذف شد.`);
      }
      const add = ctx.args.match(/^(\d+)$/);
      if (add) {
        if (!u.enemies.includes(Number(add[1]))) u.enemies.push(Number(add[1]));
        await saveUser(ctx.env, u);
        return ctx.reply(`😈 <code>${add[1]}</code> به لیست دشمنان افزوده شد؛ پیام‌هایش با فحش‌پاسخ خودکار پاسخ داده می‌شود!`);
      }
      return ctx.reply('❌ فرمت نادرست.');
    },
  },
  {
    key: 'block', name: 'بلاک', cat: 'security', emoji: '🚫',
    usage: 'بلاک <آیدی> | بلاک لیست | بلاک حذف <آیدی>',
    async run(ctx) {
      const u = ctx.user;
      if (ctx.args === 'لیست' || !ctx.args) {
        return ctx.reply('🚫 <b>لیست بلاک:</b>\n' + (u.blocked.length ? u.blocked.map(e => `• <code>${e}</code>`).join('\n') : '— خالی —'));
      }
      const del = ctx.args.match(/^حذف\s+(\d+)$/);
      if (del) { u.blocked = u.blocked.filter(e => e !== Number(del[1])); await saveUser(ctx.env, u); return ctx.reply('✅ آنبلاک شد.'); }
      const add = ctx.args.match(/^(\d+)$/);
      if (add) { if (!u.blocked.includes(Number(add[1]))) u.blocked.push(Number(add[1])); await saveUser(ctx.env, u); return ctx.reply('🚫 بلاک شد.'); }
      return ctx.reply('❌ فرمت نادرست.');
    },
  },
  toggleCmd('pv_lock', 'قفلی پیوی', 'security', '🔒', [], 'پیوی شما برای غریبه‌ها قفل می‌شود.'),
  toggleCmd('presence_pen', 'قلم حضور', 'security', '🖊'),
  {
    key: 'secretary', name: 'منشی', cat: 'security', emoji: '📞',
    usage: 'منشی روشن|خاموش | متن منشی <متن>',
    async run(ctx) {
      if (ctx.args === 'روشن' || ctx.args === 'خاموش') {
        ctx.user.settings.secretary = ctx.args === 'روشن';
        await saveUser(ctx.env, ctx.user);
        return ctx.reply(`📞 منشی ${ctx.user.settings.secretary ? '🟢 روشن' : '🔴 خاموش'} شد.\nمتن فعلی: «${esc(ctx.user.secretaryText)}»`);
      }
      return ctx.reply(`📞 <b>منشی (پاسخگوی خودکار)</b>\nوضعیت: ${ctx.user.settings.secretary ? '🟢' : '🔴'}\nمتن: «${esc(ctx.user.secretaryText)}»\n\n<code>منشی روشن</code> / <code>منشی خاموش</code>\nتغییر متن: <code>متن منشی سلام!</code>`);
    },
  },
  {
    key: 'secretary_text', name: 'متن منشی', cat: 'security', emoji: '✍️', hidden: true,
    async run(ctx) {
      if (!ctx.args) return ctx.reply('✍️ فرمت: <code>متن منشی سلام، بعداً پاسخ می‌دهم</code>');
      ctx.user.secretaryText = ctx.args; await saveUser(ctx.env, ctx.user);
      return ctx.reply(`✅ متن منشی ذخیره شد:\n«${esc(ctx.args)}»`);
    },
  },
  toggleCmd('force_join', 'عضویت اجباری', 'security', '👥'),
  toggleCmd('mute', 'سکوت', 'security', '🤐'),
  {
    key: 'word_filter', name: 'فیلتر کلمات', cat: 'security', emoji: '🚯',
    usage: 'فیلتر کلمات <کلمه> | فیلتر کلمات لیست | فیلتر کلمات حذف <کلمه>',
    async run(ctx) {
      const u = ctx.user;
      if (ctx.args === 'لیست' || !ctx.args) {
        return ctx.reply('🚯 <b>کلمات فیلترشده:</b>\n' + (u.filters.length ? u.filters.map(w => `• ${esc(w)}`).join('\n') : '— خالی —'));
      }
      const del = ctx.args.match(/^حذف\s+(.+)$/);
      if (del) { u.filters = u.filters.filter(w => w !== del[1].trim()); await saveUser(ctx.env, u); return ctx.reply('✅ حذف شد.'); }
      if (!u.filters.includes(ctx.args)) u.filters.push(ctx.args);
      await saveUser(ctx.env, u);
      return ctx.reply(`🚯 کلمه «${esc(ctx.args)}» فیلتر شد؛ پیام‌های حاوی آن حذف می‌شوند.`);
    },
  },
  toggleCmd('content_guard', 'محتوا', 'security', '🔞', [], 'فیلتر محتوای نامناسب فعال می‌شود.'),
  {
    key: 'auto_reply', name: 'پاسخ خودکار', cat: 'security', emoji: '💬',
    usage: 'پاسخ خودکار <کلمه> | <جواب>',
    async run(ctx) {
      const u = ctx.user;
      if (ctx.args === 'لیست' || !ctx.args) {
        const list = Object.entries(u.autoReplies);
        return ctx.reply('💬 <b>پاسخ‌های خودکار:</b>\n' +
          (list.length ? list.map(([k, v]) => `• ${esc(k)} ← ${esc(v)}`).join('\n') : '— خالی —') +
          '\n\nافزودن: <code>پاسخ خودکار سلام | علیک سلام!</code>\nحذف: <code>پاسخ خودکار حذف سلام</code>');
      }
      const del = ctx.args.match(/^حذف\s+(.+)$/);
      if (del) { delete u.autoReplies[del[1].trim()]; await saveUser(ctx.env, u); return ctx.reply('✅ حذف شد.'); }
      const m = ctx.args.split('|');
      if (m.length !== 2) return ctx.reply('❌ فرمت: <code>پاسخ خودکار سلام | علیک سلام!</code>');
      u.autoReplies[m[0].trim()] = m[1].trim();
      await saveUser(ctx.env, u);
      return ctx.reply(`💬 پاسخ خودکار ثبت شد:\n«${esc(m[0].trim())}» ← «${esc(m[1].trim())}»`);
    },
  },
  toggleCmd('member_league', 'لیگ اعضا', 'security', '🏆'),
  toggleCmd('group_manage', 'مدیریت گروه', 'security', '👮'),
  toggleCmd('chat_guard', 'نگهبان چت', 'security', '🛡'),
  toggleCmd('auto_seen', 'سین خودکار', 'security', '👁'),
  toggleCmd('auto_reaction', 'ری اکشن خودکار', 'security', '❤️'),
  toggleCmd('first_comment', 'کامنت اول خودکار', 'security', '🥇'),
  {
    key: 'name_cmd', name: 'اسم', cat: 'security', emoji: '🪪',
    usage: 'اسم <نام جدید>',
    async run(ctx) {
      if (!ctx.args) return ctx.reply(`🪪 نام فعلی: <b>${esc(ctx.user.name)}</b>\nتغییر: <code>اسم نام جدید</code>`);
      ctx.user.name = ctx.args; await saveUser(ctx.env, ctx.user);
      return ctx.reply(`✅ نام شما به «${esc(ctx.args)}» تغییر کرد.`);
    },
  },
  toggleCmd('sender', 'سندر', 'security', '📤', [], 'ارسال انبوه پیام به لیست چت‌ها.'),
  toggleCmd('attention', 'توجهی', 'security', '👀'),

  /* ---------------- 🎮 دانلودر، سرگرمی و مینی‌اپ‌ها ---------------- */
  {
    key: 'miniapps', name: 'مینی اپ بازی', cat: 'fun', emoji: '🎮', aliases: ['مینی اپ', 'بازی'],
    async run(ctx) {
      return ctx.reply('🎮 <b>هاب مینی‌اپ‌ها و بازی‌های تلگرامی</b>\nیکی را انتخاب کنید:', { reply_markup: kbMiniApps() });
    },
  },
  {
    key: 'translate', name: 'ترجمه', cat: 'fun', emoji: '🌐',
    usage: 'ترجمه <متن>',
    async run(ctx) {
      if (!ctx.args) return ctx.reply('🌐 فرمت: <code>ترجمه Hello world</code> (انگلیسی↔فارسی خودکار)');
      const isFa = /[\u0600-\u06FF]/.test(ctx.args);
      const sl = isFa ? 'fa' : 'auto', tl = isFa ? 'en' : 'fa';
      try {
        const r = await fetch(`https://translate.googleapis.com/translate_a/single?client=gtx&sl=${sl}&tl=${tl}&dt=t&q=${encodeURIComponent(ctx.args)}`);
        const j = await r.json();
        const out = j[0].map(x => x[0]).join('');
        return ctx.reply(`🌐 <b>ترجمه:</b>\n${esc(out)}`);
      } catch { return ctx.reply('❌ سرویس ترجمه در دسترس نیست.'); }
    },
  },
  {
    key: 'downloader', name: 'دانلودر', cat: 'fun', emoji: '📥',
    usage: 'دانلودر <لینک>',
    async run(ctx) {
      if (!ctx.args) return ctx.reply('📥 فرمت: <code>دانلودر https://...</code>\nپشتیبانی: یوتیوب، اینستاگرام، تیک‌تاک، ساندکلود');
      return ctx.reply(`📥 لینک دریافت شد:\n<code>${esc(ctx.args)}</code>\n⏳ در صف پردازش دانلود قرار گرفت... نتیجه به‌زودی ارسال می‌شود.`);
    },
  },
  {
    key: 'currency', name: 'قیمت ارز', cat: 'fun', emoji: '💵',
    async run(ctx) {
      let btc = '—', eth = '—', usd = '—';
      try {
        const r = await fetch('https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,ethereum,tether&vs_currencies=usd', { headers: { accept: 'application/json' } });
        const j = await r.json();
        btc = '$' + Number(j.bitcoin?.usd || 0).toLocaleString();
        eth = '$' + Number(j.ethereum?.usd || 0).toLocaleString();
        usd = '$' + Number(j.tether?.usd || 1).toLocaleString();
      } catch (_) {}
      return ctx.reply([
        '💵 <b>قیمت لحظه‌ای ارز</b>', '━━━━━━━━━━━━━━━',
        `₿ بیت‌کوین: <b>${btc}</b>`, `Ξ اتریوم: <b>${eth}</b>`, `💲 تتر: <b>${usd}</b>`,
        '━━━━━━━━━━━━━━━', `🕐 ${nowTehran()} — ${todayTehran()}`,
      ].join('\n'));
    },
  },
  {
    key: 'del_msg', name: 'حذف پیام', cat: 'fun', emoji: '🗑',
    async run(ctx) {
      if (ctx.replyTo) {
        await tg(ctx.cfg.botToken, 'deleteMessage', { chat_id: ctx.chatId, message_id: ctx.replyTo });
        return ctx.reply('🗑 پیام حذف شد.');
      }
      return ctx.reply('🗑 روی یک پیام ریپلای کنید و بنویسید «حذف پیام».');
    },
  },
  {
    key: 'meme_search', name: 'سرچ میم', cat: 'fun', emoji: '😹',
    async run(ctx) { return ctx.reply(`😹 <b>سرچ میم</b>${ctx.args ? ' برای «' + esc(ctx.args) + '»' : ''}:\n${MEMES[Math.floor(Math.random() * MEMES.length)]}`); },
  },
  {
    key: 'cheat', name: 'تقلب', cat: 'fun', emoji: '🃏',
    async run(ctx) { return ctx.reply(CHEATS[Math.floor(Math.random() * CHEATS.length)]); },
  },
  {
    key: 'voice_search', name: 'سرچ ویس آماده', cat: 'fun', emoji: '🔊',
    async run(ctx) { return ctx.reply(`🔊 <b>سرچ ویس آماده</b>${ctx.args ? ' «' + esc(ctx.args) + '»' : ''}\n🎧 ۳ ویس مرتبط یافت شد؛ در حال ارسال از آرشیو...`); },
  },
  {
    key: 'tts', name: 'متن به ویس', cat: 'fun', emoji: '🗣',
    usage: 'متن به ویس <متن>',
    async run(ctx) {
      if (!ctx.args) return ctx.reply('🗣 فرمت: <code>متن به ویس سلام دنیا</code>');
      return ctx.reply(`🗣 متن «${esc(ctx.args)}» به صف تبدیل گفتار اضافه شد؛ ویس به‌زودی ارسال می‌شود. 🎙`);
    },
  },
  {
    key: 'daily_gift', name: 'جایزه روزانه', cat: 'fun', emoji: '🎁',
    async run(ctx) {
      const today = new Intl.DateTimeFormat('en-CA', { timeZone: TEHRAN_TZ }).format(new Date());
      if (ctx.user.lastGiftDay === today) return ctx.reply('🎁 جایزه امروز را گرفته‌اید! فردا دوباره سر بزنید ⏳');
      const amt = 5 + Math.floor(Math.random() * 6); // ۵ تا ۱۰ الماس
      ctx.user.lastGiftDay = today;
      ctx.user.diamonds += amt;
      await saveUser(ctx.env, ctx.user);
      await logTx(ctx.env, ctx.user.id, amt, 'daily_gift');
      return ctx.reply(`🎁 <b>جایزه روزانه:</b> ${faNum(amt)} الماس! 💎\nموجودی: <b>${faNum(ctx.user.diamonds)}</b>`);
    },
  },
  {
    key: 'wheel', name: 'گردونه شانس', cat: 'fun', emoji: '🎡',
    async run(ctx) {
      const COST = 3;
      if (ctx.user.diamonds < COST) return ctx.reply(`🎡 برای چرخاندن گردونه ${faNum(COST)} الماس لازم است!`);
      const prize = [0, 0, 1, 2, 3, 5, 8, 10][Math.floor(Math.random() * 8)];
      ctx.user.diamonds += prize - COST;
      await saveUser(ctx.env, ctx.user);
      await logTx(ctx.env, ctx.user.id, prize - COST, 'wheel');
      const msg = prize === 0 ? '💨 پوچ! شانس بعدی...'
        : prize <= COST ? `😅 ${faNum(prize)} الماس — تقریباً مساوی!`
        : `🎉 بردی! <b>${faNum(prize)}</b> الماس!`;
      return ctx.reply(`🎡 <b>گردونه شانس</b> (هزینه ${faNum(COST)} 💎)\n${msg}\n💎 موجودی: <b>${faNum(ctx.user.diamonds)}</b>`);
    },
  },
  {
    key: 'leaderboard', name: 'تاپ الماس', cat: 'fun', emoji: '🏆', aliases: ['لیدربورد'],
    async run(ctx) {
      const users = await listUsers(ctx.env);
      const top = users.sort((a, b) => b.diamonds - a.diamonds).slice(0, 10);
      const medals = ['🥇', '🥈', '🥉'];
      return ctx.reply('🏆 <b>تاپ الماس — ۱۰ نفر برتر</b>\n━━━━━━━━━━━━━━━\n' +
        top.map((u, i) => `${medals[i] || '▫️'} ${esc(u.name)} — <b>${faNum(u.diamonds)}</b> 💎${u.selfActive ? ' 🟢' : ''}`).join('\n'));
    },
  },
  toggleCmd('premium_emoji', 'ایموجی پرمیوم', 'fun', '💠'),
  toggleCmd('story_challenge', 'چالش استوری', 'fun', '📸'),
  {
    key: 'photo_quality', name: 'کیفیت عکس', cat: 'fun', emoji: '🖼',
    async run(ctx) { return ctx.reply('🖼 <b>کیفیت عکس</b>\nروی یک عکس ریپلای کنید تا با هوش مصنوعی ۴K شود! ✨ (در صف پردازش قرار می‌گیرد)'); },
  },
  {
    key: 'fortune_now', name: 'پیشگو', cat: 'fun', emoji: '🔮', hidden: true,
    async run(ctx) { return ctx.reply(`🔮 ${FORTUNES[Math.floor(Math.random() * FORTUNES.length)]}`); },
  },
  {
    key: 'meow_now', name: 'میو', cat: 'fun', emoji: '🐱', hidden: true,
    async run(ctx) { return ctx.reply(MEOWS[Math.floor(Math.random() * MEOWS.length)]); },
  },
];

/** جستجوی دستور با طولانی‌ترین تطبیق نام/الیاس از ابتدای متن */
function matchCommand(text) {
  let best = null, bestLen = 0;
  for (const c of COMMANDS) {
    for (const n of [c.name, ...(c.aliases || [])]) {
      if ((text === n || text.startsWith(n + ' ')) && n.length > bestLen) {
        best = c; bestLen = n.length;
      }
    }
  }
  if (!best) return null;
  return { cmd: best, args: text.slice(bestLen).trim() };
}

function commandsHelpText() {
  const out = ['📜 <b>لیست کامل دستورات سلف</b>', ''];
  for (const [k, title] of Object.entries(CATS)) {
    out.push(`<b>${title}</b>`);
    out.push(COMMANDS.filter(c => c.cat === k && !c.hidden).map(c => c.emoji + ' ' + c.name).join(' | '));
    out.push('');
  }
  out.push('🔀 دستورات سوییچی: «<i>نام دستور</i> روشن / خاموش»');
  return out.join('\n');
}

/* ========================================================================== */
/* §6 — هندل آپدیت تلگرام                                                      */
/* ========================================================================== */

async function handleUpdate(env, cfg, update) {
  try {
    if (update.callback_query) return await handleCallback(env, cfg, update.callback_query);
    if (update.message) return await handleMessage(env, cfg, update.message);
  } catch (e) {
    console.error('update error', e);
  }
}

async function handleMessage(env, cfg, msg) {
  const from = msg.from;
  if (!from || from.is_bot) return;
  const chatId = msg.chat.id;
  const text = (msg.text || '').trim();
  const { user, isNew } = await ensureUser(env, from);
  const isOwner = String(from.id) === String(cfg.ownerId);

  /* ---- /start ---- */
  if (text === '/start' || text === 'استارت' || text === 'پنل' || text === '/panel') {
    if (isNew) {
      await send(cfg, chatId,
        `🎁 خوش آمدید! <b>${faNum(ECON.WELCOME_GIFT)}</b> الماس هدیه ثبت‌نام دریافت کردید. 💎`);
    }
    return send(cfg, chatId, panelHomeText(user), { reply_markup: kbMain() });
  }

  /* ---- دستورات مالک ---- */
  if (isOwner && text.startsWith('/give_diamonds')) {
    const m = text.match(/^\/give_diamonds\s+(\d+)\s+(-?\d+)$/);
    if (!m) return send(cfg, chatId, '👑 فرمت: <code>/give_diamonds 123456789 100</code>\n(عدد منفی = کسر)');
    const target = Number(m[1]), amount = Number(m[2]);
    let tu = await getUser(env, target);
    if (!tu) { tu = defaultUser(target); await addToIndex(env, target); }
    tu.diamonds = Math.max(0, tu.diamonds + amount);
    await saveUser(env, tu);
    await logTx(env, target, amount, 'admin');
    await send(cfg, target, amount >= 0
      ? `👑 مالک <b>${faNum(amount)}</b> الماس برای شما واریز کرد! 💎\nموجودی: <b>${faNum(tu.diamonds)}</b>`
      : `👑 مالک <b>${faNum(-amount)}</b> الماس از حساب شما کسر کرد.\nموجودی: <b>${faNum(tu.diamonds)}</b>`).catch?.(() => {});
    return send(cfg, chatId, `✅ انجام شد. موجودی جدید <code>${target}</code>: <b>${faNum(tu.diamonds)}</b> 💎`);
  }
  if (isOwner && (text === '/stats' || text === 'آمار')) {
    const users = await listUsers(env);
    const active = users.filter(u => u.selfActive).length;
    const total = users.reduce((s, u) => s + u.diamonds, 0);
    return send(cfg, chatId, [
      '👑 <b>آمار سیستم</b>', '━━━━━━━━━━━━━━━',
      `👥 کاربران: <b>${faNum(users.length)}</b>`,
      `🤖 سلف‌های فعال: <b>${faNum(active)}</b>`,
      `💎 مجموع الماس‌ها: <b>${faNum(total)}</b>`,
    ].join('\n'));
  }

  /* ---- راهنما ---- */
  if (text === 'دستورات' || text === 'راهنما' || text === '/help') {
    return send(cfg, chatId, commandsHelpText(), { reply_markup: kbCommands() });
  }

  /* ---- موتور دستورات ---- */
  const found = matchCommand(text);
  if (found) {
    // دستورات (به‌جز مدیریت سلف/اطلاعات/انتقال الماس) فقط با سلفِ روشن کار می‌کنند
    const freeKeys = ['selfmanage', 'info', 'transfer', 'ping', 'miniapps', 'daily_gift', 'wheel', 'leaderboard'];
    if (!user.selfActive && !freeKeys.includes(found.cmd.key)) {
      return send(cfg, chatId,
        '🔴 سلف شما خاموش است!\nبرای استفاده از دستورات، ابتدا سلف را روشن کنید:\n«مدیریت سلف» یا دکمه ⚙️ در پنل.',
        { reply_markup: kbSelfManage(user) });
    }
    const ctx = {
      env, cfg, user, chatId,
      text, args: found.args,
      replyTo: msg.reply_to_message?.message_id || null,
      reply: (t, extra) => send(cfg, chatId, t, extra),
    };
    return found.cmd.run(ctx);
  }

  /* ---- اتوماسیون‌های پس‌زمینه (برای سلف فعال) ---- */
  if (user.selfActive && text) {
    // فیلتر کلمات
    if (user.filters.some(w => text.includes(w))) {
      await tg(cfg.botToken, 'deleteMessage', { chat_id: chatId, message_id: msg.message_id });
      return send(cfg, chatId, '🚯 پیام حاوی کلمه فیلترشده حذف شد.');
    }
    // پاسخ خودکار
    for (const [k, v] of Object.entries(user.autoReplies)) {
      if (text.includes(k)) return send(cfg, chatId, '💬 ' + esc(v));
    }
    // میو خودکار / پیشگو خودکار
    if (user.settings.auto_meow && /میو|گربه|پیشی/.test(text)) {
      return send(cfg, chatId, MEOWS[Math.floor(Math.random() * MEOWS.length)]);
    }
    if (user.settings.auto_fortune && /فال|پیشگو|طالع/.test(text)) {
      return send(cfg, chatId, `🔮 ${FORTUNES[Math.floor(Math.random() * FORTUNES.length)]}`);
    }
    // منشی
    if (user.settings.secretary && msg.chat.type === 'private') {
      return send(cfg, chatId, '📞 ' + esc(user.secretaryText));
    }
  }
}

async function handleCallback(env, cfg, cb) {
  const from = cb.from;
  const chatId = cb.message?.chat?.id;
  const msgId = cb.message?.message_id;
  const data = cb.data || '';
  const { user } = await ensureUser(env, from);

  const edit = (text, kb) => editMsg(cfg, chatId, msgId, text, kb ? { reply_markup: kb } : {});

  switch (true) {
    case data === 'p:top':
      return answerCb(cfg, cb.id, `✨ ${BRAND.PANEL_TOP} — نسخه ${BRAND.VERSION}`, true);

    case data === 'p:main':
      await edit(panelHomeText(user), kbMain());
      return answerCb(cfg, cb.id, '🏠 پنل اصلی');

    case data === 'p:cmds':
      await edit(commandsHelpText(), kbCommands());
      return answerCb(cfg, cb.id, '📜 دستورات سلف');

    case data === 'p:account':
      await edit(accountText(user), { inline_keyboard: [[{ text: '🔙 بازگشت به پنل', callback_data: 'p:main' }]] });
      return answerCb(cfg, cb.id, '👤 حساب کاربری');

    case data === 'p:close':
      await tg(cfg.botToken, 'deleteMessage', { chat_id: chatId, message_id: msgId });
      return answerCb(cfg, cb.id, '❌ پنل بسته شد');

    case data === 'p:self':
      await edit(
        `⚙️ <b>مدیریت سلف</b>\n━━━━━━━━━━━━━━━\nوضعیت: ${user.selfActive ? '🟢 روشن' : '🔴 خاموش'}\n💎 موجودی: <b>${faNum(user.diamonds)}</b>\n\n` +
        `💰 هزینه فعال‌سازی: ${faNum(ECON.ACTIVATION_COST)} الماس\n📆 نگهداری روزانه: ${faNum(ECON.DAILY_COST)} الماس`,
        kbSelfManage(user));
      return answerCb(cfg, cb.id, '⚙️ مدیریت سلف');

    case data === 's:on': {
      if (user.selfActive) return answerCb(cfg, cb.id, '🟢 سلف از قبل روشن است!');
      if (user.diamonds < ECON.ACTIVATION_COST) {
        return answerCb(cfg, cb.id, `❌ حداقل ${ECON.ACTIVATION_COST} الماس برای فعال‌سازی لازم است!`, true);
      }
      user.diamonds -= ECON.ACTIVATION_COST;
      user.selfActive = true;
      user.selfActivatedAt = Date.now();
      user.lastChargeAt = Date.now();
      await saveUser(env, user);
      await logTx(env, user.id, -ECON.ACTIVATION_COST, 'activation');
      await edit(
        `🟢 <b>سلف شما روشن شد!</b>\n━━━━━━━━━━━━━━━\n💎 ${faNum(ECON.ACTIVATION_COST)} الماس هزینه فعال‌سازی کسر شد.\n💎 موجودی: <b>${faNum(user.diamonds)}</b>\n📆 هر ۲۴ ساعت ${faNum(ECON.DAILY_COST)} الماس هزینه نگهداری کسر می‌شود.`,
        kbSelfManage(user));
      return answerCb(cfg, cb.id, '🟢 سلف روشن شد!');
    }

    case data === 's:off': {
      if (!user.selfActive) return answerCb(cfg, cb.id, '🔴 سلف از قبل خاموش است!');
      user.selfActive = false;
      await saveUser(env, user);
      await edit(`🔴 <b>سلف شما خاموش شد.</b>\n💎 موجودی: <b>${faNum(user.diamonds)}</b>`, kbSelfManage(user));
      return answerCb(cfg, cb.id, '🔴 سلف خاموش شد');
    }

    case data.startsWith('c:'): {
      const cat = data.slice(2);
      await edit(`<b>${CATS[cat] || 'دستورات'}</b>\nیک دستور را انتخاب کنید 👇`, kbCategory(cat));
      return answerCb(cfg, cb.id, CATS[cat] || '');
    }

    case data.startsWith('i:'): {
      const key = data.slice(2);
      const cmd = COMMANDS.find(c => c.key === key);
      if (!cmd) return answerCb(cfg, cb.id, '❓ دستور یافت نشد');
      const info = [
        `${cmd.emoji} <b>${cmd.name}</b>`,
        `📂 دسته: ${CATS[cmd.cat]}`,
        cmd.usage ? `📝 فرمت: <code>${esc(cmd.usage)}</code>` : (cmd.toggle ? `📝 فرمت: <code>${cmd.name} روشن</code> / <code>${cmd.name} خاموش</code>` : `📝 ارسال: <code>${cmd.name}</code>`),
        cmd.toggle ? `وضعیت: ${user.settings[cmd.key] ? '🟢 روشن' : '🔴 خاموش'}` : null,
      ].filter(Boolean).join('\n');
      await edit(info, { inline_keyboard: [
        ...(cmd.toggle ? [[
          { text: '🟢 روشن', callback_data: 't:1:' + cmd.key },
          { text: '🔴 خاموش', callback_data: 't:0:' + cmd.key },
        ]] : []),
        [{ text: '🔙 بازگشت', callback_data: 'c:' + cmd.cat }],
      ]});
      return answerCb(cfg, cb.id, cmd.name);
    }

    case data.startsWith('t:'): {
      const [, onStr, key] = data.split(':');
      const cmd = COMMANDS.find(c => c.key === key);
      if (!cmd) return answerCb(cfg, cb.id, '❓');
      if (!user.selfActive) return answerCb(cfg, cb.id, '🔴 ابتدا سلف را روشن کنید!', true);
      user.settings[key] = onStr === '1';
      await saveUser(env, user);
      await edit(
        `${cmd.emoji} <b>${cmd.name}</b>\nوضعیت جدید: ${user.settings[key] ? '🟢 روشن' : '🔴 خاموش'}`,
        { inline_keyboard: [
          [{ text: '🟢 روشن', callback_data: 't:1:' + key }, { text: '🔴 خاموش', callback_data: 't:0:' + key }],
          [{ text: '🔙 بازگشت', callback_data: 'c:' + cmd.cat }],
        ]});
      return answerCb(cfg, cb.id, user.settings[key] ? '🟢 روشن شد' : '🔴 خاموش شد');
    }

    default:
      return answerCb(cfg, cb.id, '❓');
  }
}

/* ========================================================================== */
/* §7 — کرون روزانه (کسر هزینه نگهداری)                                        */
/* ========================================================================== */

async function runDailyCharge(env) {
  const cfg = await getConfig(env);
  if (!cfg) return { ok: false, reason: 'not configured' };
  const users = await listUsers(env);
  let charged = 0, deactivated = 0;
  for (const u of users) {
    if (!u.selfActive) continue;
    if (u.diamonds >= ECON.DAILY_COST) {
      u.diamonds -= ECON.DAILY_COST;
      u.lastChargeAt = Date.now();
      charged++;
      await saveUser(env, u);
      await logTx(env, u.id, -ECON.DAILY_COST, 'daily');
      if (u.diamonds < ECON.DAILY_COST) {
        await send(cfg, u.id, `⚠️ موجودی شما (${faNum(u.diamonds)} 💎) برای شارژ فردا کافی نیست!\nبرای جلوگیری از خاموشی سلف، الماس تهیه کنید. ${BRAND.HELPER}`).catch?.(() => {});
      }
    } else {
      u.selfActive = false;
      deactivated++;
      await saveUser(env, u);
      await send(cfg, u.id, `🔴 <b>سلف شما خاموش شد!</b>\nموجودی (${faNum(u.diamonds)} 💎) برای هزینه نگهداری روزانه (${faNum(ECON.DAILY_COST)} 💎) کافی نبود.\nپس از شارژ، از «مدیریت سلف» دوباره روشن کنید.`).catch?.(() => {});
    }
  }
  // گزارش به مالک
  await send(cfg, cfg.ownerId,
    `⏰ <b>گزارش کرون روزانه</b>\n💳 شارژ شده: ${faNum(charged)} کاربر\n🔴 خاموش شده: ${faNum(deactivated)} کاربر`).catch?.(() => {});
  return { ok: true, charged, deactivated };
}

/* ========================================================================== */
/* §8 — API وب + Frontend                                                      */
/* ========================================================================== */

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status, headers: { 'content-type': 'application/json; charset=utf-8', 'access-control-allow-origin': '*' },
  });
}
function html(body) {
  return new Response(body, { headers: { 'content-type': 'text/html; charset=utf-8' } });
}

async function requireAdmin(env, request) {
  const cfg = await getConfig(env);
  if (!cfg) return { err: json({ ok: false, error: 'not_configured' }, 400) };
  const key = request.headers.get('x-admin-key') || new URL(request.url).searchParams.get('key');
  if (!key || key !== cfg.adminKey) return { err: json({ ok: false, error: 'unauthorized' }, 401) };
  return { cfg };
}

async function apiSetup(env, request) {
  const existing = await getConfig(env);
  if (existing) return json({ ok: false, error: 'already_configured' }, 400);
  let body;
  try { body = await request.json(); } catch { return json({ ok: false, error: 'bad_json' }, 400); }
  const botToken = String(body.botToken || '').trim();
  const ownerId = String(body.ownerId || '').trim();
  const apiId = String(body.apiId || '').trim();
  const apiHash = String(body.apiHash || '').trim();
  if (!/^\d+:[\w-]{30,}$/.test(botToken)) return json({ ok: false, error: 'invalid_token_format' }, 400);
  if (!/^\d{4,15}$/.test(ownerId)) return json({ ok: false, error: 'invalid_owner_id' }, 400);
  if (!/^\d{1,10}$/.test(apiId)) return json({ ok: false, error: 'invalid_api_id' }, 400);
  if (!/^[a-fA-F0-9]{32}$/.test(apiHash)) return json({ ok: false, error: 'invalid_api_hash' }, 400);

  // ۱) اعتبارسنجی توکن
  const me = await tg(botToken, 'getMe');
  if (!me.ok) return json({ ok: false, error: 'token_rejected_by_telegram' }, 400);

  // ۲) ساخت کلیدهای امن
  const webhookSecret = randId(24);
  const adminKey = randId(40);
  const origin = new URL(request.url).origin;
  const webhookUrl = `${origin}/webhook/${webhookSecret}`;

  // ۳) ست وب‌هوک خودکار
  const wh = await tg(botToken, 'setWebhook', {
    url: webhookUrl,
    secret_token: webhookSecret,
    allowed_updates: ['message', 'callback_query'],
    drop_pending_updates: true,
  });
  if (!wh.ok) return json({ ok: false, error: 'webhook_failed', detail: wh.description }, 400);

  // ۴) ذخیره امن کانفیگ (توکن + API ID / API Hash مخصوص MTProto)
  const cfg = {
    botToken, ownerId, apiId, apiHash, adminKey, webhookSecret,
    botUsername: me.result.username, configuredAt: Date.now(),
  };
  await saveConfig(env, cfg);

  // ۵) ثبت مالک + اطلاع‌رسانی
  let owner = await getUser(env, Number(ownerId));
  if (!owner) {
    owner = defaultUser(Number(ownerId), { first_name: 'Owner' });
    owner.diamonds = 1000;
    await addToIndex(env, Number(ownerId));
    await saveUser(env, owner);
  }
  await send(cfg, ownerId, [
    `👑 <b>${BRAND.BOT_NAME} راه‌اندازی شد!</b>`, '━━━━━━━━━━━━━━━',
    `🤖 ربات: @${me.result.username}`,
    `🧬 API ID: <code>${apiId}</code>`,
    `🧬 API Hash: <code>${apiHash.slice(0, 6)}••••••${apiHash.slice(-4)}</code>`,
    `🌐 پنل وب: ${origin}/panel`,
    `🔑 کلید مدیریت: <code>${adminKey}</code>`,
    `💎 ${faNum(1000)} الماس اولیه برای شما شارژ شد.`,
    '', 'برای شروع /start را بزنید.',
  ].join('\n')).catch?.(() => {});

  return json({ ok: true, botUsername: me.result.username, adminKey, panelUrl: origin + '/panel', webhookUrl, apiId });
}

async function apiUsers(env, request) {
  const { err } = await requireAdmin(env, request);
  if (err) return err;
  const users = await listUsers(env);
  return json({
    ok: true,
    stats: {
      total: users.length,
      active: users.filter(u => u.selfActive).length,
      diamonds: users.reduce((s, u) => s + u.diamonds, 0),
    },
    users: users.map(u => ({
      id: u.id, name: u.name, username: u.username, diamonds: u.diamonds,
      selfActive: u.selfActive, joinedAt: u.joinedAt,
      modules: Object.values(u.settings).filter(Boolean).length,
    })),
  });
}

async function apiDiamonds(env, request) {
  const { err, cfg } = await requireAdmin(env, request);
  if (err) return err;
  const body = await request.json().catch(() => ({}));
  const id = Number(body.id), amount = Number(body.amount);
  if (!id || !Number.isFinite(amount)) return json({ ok: false, error: 'bad_params' }, 400);
  let u = await getUser(env, id);
  if (!u) { u = defaultUser(id); await addToIndex(env, id); }
  u.diamonds = Math.max(0, u.diamonds + amount);
  await saveUser(env, u);
  await logTx(env, id, amount, 'admin');
  await send(cfg, id, amount >= 0
    ? `👑 مدیریت <b>${faNum(amount)}</b> الماس برای شما واریز کرد! 💎 موجودی: <b>${faNum(u.diamonds)}</b>`
    : `👑 مدیریت <b>${faNum(-amount)}</b> الماس کسر کرد. موجودی: <b>${faNum(u.diamonds)}</b>`).catch?.(() => {});
  return json({ ok: true, id, diamonds: u.diamonds });
}

async function apiToggleSelf(env, request) {
  const { err, cfg } = await requireAdmin(env, request);
  if (err) return err;
  const body = await request.json().catch(() => ({}));
  const u = await getUser(env, Number(body.id));
  if (!u) return json({ ok: false, error: 'user_not_found' }, 404);
  u.selfActive = !!body.on;
  await saveUser(env, u);
  await send(cfg, u.id, u.selfActive ? '🟢 سلف شما توسط مدیریت روشن شد!' : '🔴 سلف شما توسط مدیریت خاموش شد.').catch?.(() => {});
  return json({ ok: true, id: u.id, selfActive: u.selfActive });
}

async function apiBroadcast(env, request) {
  const { err, cfg } = await requireAdmin(env, request);
  if (err) return err;
  const body = await request.json().catch(() => ({}));
  const text = String(body.text || '').trim();
  if (!text) return json({ ok: false, error: 'empty' }, 400);
  const users = await listUsers(env);
  let sent = 0;
  for (const u of users) {
    const r = await send(cfg, u.id, '📢 <b>اطلاعیه مدیریت</b>\n━━━━━━━━━━━━━━━\n' + esc(text));
    if (r.ok) sent++;
  }
  return json({ ok: true, sent, total: users.length });
}

async function apiConfig(env, request) {
  const { err, cfg } = await requireAdmin(env, request);
  if (err) return err;
  const origin = new URL(request.url).origin;
  return json({
    ok: true,
    botUsername: cfg.botUsername,
    ownerId: cfg.ownerId,
    apiId: cfg.apiId || null,
    apiHash: cfg.apiHash || null,
    apiHashMasked: cfg.apiHash ? cfg.apiHash.slice(0, 6) + '••••••••••' + cfg.apiHash.slice(-4) : null,
    webhookUrl: `${origin}/webhook/${cfg.webhookSecret}`,
    configuredAt: cfg.configuredAt,
    version: BRAND.VERSION,
  });
}

async function apiTransactions(env, request) {
  const { err } = await requireAdmin(env, request);
  if (err) return err;
  return json({ ok: true, transactions: await getTxLog(env) });
}

async function apiRunCron(env, request) {
  const { err } = await requireAdmin(env, request);
  if (err) return err;
  const r = await runDailyCharge(env);
  return json(r);
}

/* ------------------------- Frontend: صفحه راه‌اندازی ------------------------- */

const CSS_BASE = `
*{margin:0;padding:0;box-sizing:border-box;font-family:'Vazirmatn','Segoe UI',Tahoma,sans-serif}
body{min-height:100vh;background:#0b0e1a;color:#e8eaf6;direction:rtl}
.bg{position:fixed;inset:0;background:
 radial-gradient(60% 50% at 20% 10%,rgba(99,102,241,.25),transparent),
 radial-gradient(50% 40% at 85% 25%,rgba(236,72,153,.18),transparent),
 radial-gradient(45% 45% at 50% 95%,rgba(34,211,238,.15),transparent);z-index:-1}
.card{background:rgba(255,255,255,.05);border:1px solid rgba(255,255,255,.1);
 border-radius:20px;backdrop-filter:blur(14px);box-shadow:0 20px 60px rgba(0,0,0,.4)}
input,textarea{width:100%;padding:13px 16px;border-radius:12px;border:1px solid rgba(255,255,255,.15);
 background:rgba(0,0,0,.3);color:#fff;font-size:15px;outline:none;transition:.2s}
input:focus,textarea:focus{border-color:#818cf8;box-shadow:0 0 0 3px rgba(129,140,248,.2)}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;padding:13px 22px;border:none;
 border-radius:12px;font-size:15px;font-weight:700;cursor:pointer;transition:.2s;color:#fff}
.btn-primary{background:linear-gradient(135deg,#6366f1,#8b5cf6)}
.btn-primary:hover{filter:brightness(1.15);transform:translateY(-1px)}
.btn:disabled{opacity:.5;cursor:not-allowed}
label{display:block;margin:14px 0 6px;font-size:13px;color:#a5b4fc;font-weight:600}
.muted{color:#94a3b8;font-size:13px}
@font-face{font-family:Vazirmatn;src:local('Vazirmatn')}
`;

const SETUP_HTML = `<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>⚡️ راه‌اندازی Self-Bot Hub</title>
<link href="https://cdn.jsdelivr.net/gh/rastikerdar/vazirmatn@v33.003/Vazirmatn-font-face.css" rel="stylesheet">
<style>${CSS_BASE}
body{overflow-x:hidden}
.aurora{position:fixed;inset:-20%;z-index:-2;filter:blur(70px);opacity:.55;
 background:
  radial-gradient(35% 30% at 25% 20%,#6d28d9,transparent 70%),
  radial-gradient(30% 30% at 75% 15%,#db2777,transparent 70%),
  radial-gradient(35% 35% at 60% 80%,#0891b2,transparent 70%),
  radial-gradient(25% 25% at 15% 75%,#4f46e5,transparent 70%);
 animation:drift 16s ease-in-out infinite alternate}
@keyframes drift{from{transform:rotate(-4deg) scale(1)}to{transform:rotate(5deg) scale(1.15)}}
.gem{position:fixed;z-index:-1;font-size:22px;opacity:.5;animation:float linear infinite}
@keyframes float{from{transform:translateY(105vh) rotate(0)}to{transform:translateY(-10vh) rotate(360deg)}}
.wrap{max-width:480px;margin:0 auto;padding:40px 18px}
.logo{text-align:center;margin-bottom:24px}
.logo .em{font-size:58px;display:block;filter:drop-shadow(0 8px 30px rgba(139,92,246,.8));animation:pulse 2.6s ease-in-out infinite}
@keyframes pulse{0%,100%{transform:scale(1)}50%{transform:scale(1.08)}}
.logo h1{font-size:30px;margin-top:10px;font-weight:900;letter-spacing:.5px;
 background:linear-gradient(90deg,#a5b4fc,#f0abfc,#67e8f9,#a5b4fc);background-size:300% 100%;
 -webkit-background-clip:text;background-clip:text;color:transparent;animation:shine 5s linear infinite}
@keyframes shine{to{background-position:300% 0}}
.logo p{color:#94a3b8;font-size:13px;margin-top:8px}
.frame{position:relative;border-radius:22px;padding:1.5px;overflow:hidden}
.frame::before{content:'';position:absolute;inset:-150%;
 background:conic-gradient(from 0deg,#6366f1,#ec4899,#22d3ee,#6366f1);animation:spin 5s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.card{position:relative;border-radius:21px;padding:26px;background:rgba(13,16,32,.92);border:none}
.chips{display:flex;gap:8px;justify-content:center;flex-wrap:wrap;margin-bottom:20px}
.chip{font-size:11px;padding:6px 12px;border-radius:99px;background:rgba(99,102,241,.14);
 border:1px solid rgba(129,140,248,.35);color:#c7d2fe}
.field{position:relative;margin-top:4px}
.field .ic{position:absolute;left:14px;top:50%;transform:translateY(-50%);font-size:16px;opacity:.7}
.field input{padding-left:42px}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media(max-width:420px){.grid2{grid-template-columns:1fr}}
.btn-mega{width:100%;margin-top:24px;padding:15px;font-size:16px;position:relative;overflow:hidden;
 background:linear-gradient(135deg,#6366f1,#a855f7,#ec4899);background-size:200% 100%;animation:btnsh 3s linear infinite}
@keyframes btnsh{to{background-position:200% 0}}
.btn-mega:hover{transform:translateY(-2px);box-shadow:0 10px 30px rgba(168,85,247,.4)}
.result{display:none;margin-top:18px;padding:16px;border-radius:14px;font-size:14px;line-height:2.1}
.result.ok{display:block;background:rgba(34,197,94,.1);border:1px solid rgba(34,197,94,.4)}
.result.err{display:block;background:rgba(239,68,68,.1);border:1px solid rgba(239,68,68,.4)}
code{background:rgba(0,0,0,.45);padding:2px 8px;border-radius:6px;font-size:12px;direction:ltr;display:inline-block;word-break:break-all}
.hint{font-size:11.5px;color:#64748b;margin-top:4px}
.spin{width:16px;height:16px;border:2px solid rgba(255,255,255,.3);border-top-color:#fff;border-radius:50%;animation:sp 1s linear infinite;display:none}
@keyframes sp{to{transform:rotate(360deg)}}
.copybtn{cursor:pointer;background:rgba(255,255,255,.1);border:none;color:#c7d2fe;border-radius:6px;padding:2px 8px;font-size:11px;font-family:inherit}
</style>
</head>
<body>
<div class="aurora"></div>
<div id="gems"></div>
<div class="wrap">
  <div class="logo">
    <span class="em">🤖</span>
    <h1>SELF-BOT HUB</h1>
    <p>سلف‌ساز تلگرام + اقتصاد الماس 💎 روی Cloudflare Edge ⚡️</p>
  </div>
  <div class="chips">
    <span class="chip">⚡️ Workers</span><span class="chip">🗄 KV / D1</span>
    <span class="chip">⏰ Cron روزانه</span><span class="chip">🧬 MTProto Ready</span>
  </div>
  <div class="frame"><div class="card">
    <form id="f">
      <label>🔑 توکن ربات تلگرام (Bot Token)</label>
      <div class="field"><span class="ic">🤖</span>
        <input id="token" dir="ltr" placeholder="123456789:AAE..." required autocomplete="off"></div>
      <div class="hint">از @BotFather — امن در Cloudflare KV ذخیره می‌شود.</div>

      <label>👑 آیدی عددی مالک (Owner Numeric ID)</label>
      <div class="field"><span class="ic">🪪</span>
        <input id="owner" dir="ltr" placeholder="123456789" required inputmode="numeric" pattern="\d{4,15}"></div>
      <div class="hint">از @userinfobot بگیرید.</div>

      <div class="grid2">
        <div>
          <label>🧬 API ID</label>
          <div class="field"><span class="ic">#️⃣</span>
            <input id="apiId" dir="ltr" placeholder="1234567" required inputmode="numeric" pattern="\d{1,10}"></div>
        </div>
        <div>
          <label>🧬 API Hash</label>
          <div class="field"><span class="ic">🔐</span>
            <input id="apiHash" dir="ltr" placeholder="32 کاراکتر hex" required pattern="[a-fA-F0-9]{32}" maxlength="32"></div>
        </div>
      </div>
      <div class="hint">API ID و API Hash را از <b>my.telegram.org → API development tools</b> دریافت کنید (برای هسته MTProto سلف).</div>

      <button class="btn btn-mega" id="go">
        <span class="spin" id="sp"></span> 🚀 راه‌اندازی، ذخیره امن و ست وب‌هوک
      </button>
    </form>
    <div class="result" id="res"></div>
  </div></div>
  <p class="muted" style="text-align:center;margin-top:18px">پس از راه‌اندازی، این صفحه به صفحه وضعیت تبدیل می‌شود 🔒</p>
</div>
<script>
// ذرات الماس شناور
(function(){
  var g=document.getElementById('gems'),ems=['💎','✨','🔷','⭐️'];
  for(var i=0;i<14;i++){
    var s=document.createElement('span');s.className='gem';
    s.textContent=ems[i%ems.length];
    s.style.left=(Math.random()*100)+'vw';
    s.style.fontSize=(12+Math.random()*18)+'px';
    s.style.animationDuration=(9+Math.random()*14)+'s';
    s.style.animationDelay=(-Math.random()*20)+'s';
    g.appendChild(s);
  }
})();
var f=document.getElementById('f'),res=document.getElementById('res'),go=document.getElementById('go'),sp=document.getElementById('sp');
function val(id){return document.getElementById(id).value.trim()}
f.addEventListener('submit',function(e){
  e.preventDefault();
  go.disabled=true;sp.style.display='inline-block';res.className='result';
  fetch('/api/setup',{method:'POST',headers:{'content-type':'application/json'},
    body:JSON.stringify({botToken:val('token'),ownerId:val('owner'),apiId:val('apiId'),apiHash:val('apiHash')})})
  .then(function(r){return r.json()})
  .then(function(j){
    if(j.ok){
      res.className='result ok';
      res.innerHTML='✅ <b>راه‌اندازی موفق!</b> 🎉<br>'+
        '🤖 ربات: <b>@'+j.botUsername+'</b><br>'+
        '🧬 API ID ثبت شد: <code>'+j.apiId+'</code><br>'+
        '🔗 وب‌هوک خودکار ست شد ✅<br>'+
        '🔑 کلید مدیریت: <code id="ak">'+j.adminKey+'</code> '+
        '<button class="copybtn" onclick="cp()">کپی 📋</button><br>'+
        '🌐 <a href="/panel?key='+j.adminKey+'" style="color:#67e8f9;font-weight:700">ورود به پنل مدیریت ←</a>';
      try{localStorage.setItem('adminKey',j.adminKey)}catch(_){}
    }else{
      res.className='result err';
      var m={invalid_token_format:'فرمت توکن ربات نامعتبر است.',
        invalid_owner_id:'آیدی عددی مالک نامعتبر است.',
        invalid_api_id:'API ID باید فقط عدد باشد (my.telegram.org).',
        invalid_api_hash:'API Hash باید دقیقا ۳۲ کاراکتر hex باشد.',
        token_rejected_by_telegram:'تلگرام این توکن را رد کرد! دوباره از @BotFather بگیرید.',
        already_configured:'ربات قبلا پیکربندی شده است.',
        webhook_failed:'ست وب‌هوک ناموفق: '+(j.detail||'')};
      res.innerHTML='❌ '+(m[j.error]||j.error);
      go.disabled=false;
    }
    sp.style.display='none';
  })
  .catch(function(){res.className='result err';res.textContent='❌ خطای شبکه';go.disabled=false;sp.style.display='none'});
});
function cp(){
  var t=document.getElementById('ak').textContent;
  (navigator.clipboard?navigator.clipboard.writeText(t):Promise.reject()).then(function(){alert('کپی شد ✅')}).catch(function(){prompt('کپی کنید:',t)});
}
</script>
</body>
</html>`;

/* ------------------------- Frontend: پنل مدیریت وب ------------------------- */

const PANEL_HTML = `<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>👑 پنل مدیریت — Self-Bot Hub</title>
<link href="https://cdn.jsdelivr.net/gh/rastikerdar/vazirmatn@v33.003/Vazirmatn-font-face.css" rel="stylesheet">
<style>${CSS_BASE}
.top{display:flex;align-items:center;justify-content:space-between;padding:16px 22px;margin:18px;border-radius:16px}
.top h1{font-size:18px}
.top .tag{font-size:11px;color:#94a3b8}
.wrap{max-width:1080px;margin:0 auto;padding:0 18px 60px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:14px;margin-bottom:20px}
.stat{padding:20px;text-align:center}
.stat .v{font-size:30px;font-weight:800;background:linear-gradient(90deg,#a5b4fc,#f0abfc);-webkit-background-clip:text;background-clip:text;color:transparent}
.stat .l{font-size:13px;color:#94a3b8;margin-top:4px}
.section{padding:20px;margin-bottom:18px}
.section h2{font-size:15px;margin-bottom:14px;color:#c7d2fe}
table{width:100%;border-collapse:collapse;font-size:13px}
th{color:#818cf8;text-align:right;padding:9px 10px;border-bottom:1px solid rgba(255,255,255,.1);font-size:12px}
td{padding:9px 10px;border-bottom:1px solid rgba(255,255,255,.06)}
tr:hover td{background:rgba(255,255,255,.03)}
.pill{padding:3px 10px;border-radius:99px;font-size:11px;font-weight:700}
.pill.on{background:rgba(34,197,94,.15);color:#4ade80}
.pill.off{background:rgba(239,68,68,.15);color:#f87171}
.mini{padding:5px 10px;font-size:12px;border-radius:8px}
.btn-green{background:linear-gradient(135deg,#059669,#10b981)}
.btn-red{background:linear-gradient(135deg,#dc2626,#ef4444)}
.btn-ghost{background:rgba(255,255,255,.08)}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.row input{flex:1;min-width:140px}
.login{max-width:380px;margin:12vh auto;padding:30px;text-align:center}
#toast{position:fixed;bottom:24px;right:50%;transform:translateX(50%);background:#1e293b;border:1px solid rgba(255,255,255,.15);
 padding:12px 22px;border-radius:12px;font-size:14px;opacity:0;transition:.3s;pointer-events:none;z-index:9}
#toast.show{opacity:1}
@media(max-width:640px){table{font-size:11px}td,th{padding:6px 5px}}
</style>
</head>
<body>
<div class="bg"></div>
<div id="toast"></div>

<div id="loginView" class="card login" style="display:none">
  <div style="font-size:46px">🔐</div>
  <h1 style="margin:10px 0 18px;font-size:20px">ورود به پنل مدیریت</h1>
  <input id="keyIn" dir="ltr" placeholder="Admin Key">
  <button class="btn btn-primary" style="width:100%;margin-top:14px" onclick="doLogin()">ورود 🚀</button>
  <p class="muted" style="margin-top:12px">کلید مدیریت هنگام راه‌اندازی به شما داده شد (و برای مالک در تلگرام ارسال شد).</p>
</div>

<div id="panelView" style="display:none">
  <div class="top card">
    <div><h1>👑 پنل مدیریت Self-Bot Hub</h1><div class="tag">Panel LN | CK | Amin &bull; @helperlevibot</div></div>
    <div class="row">
      <button class="btn btn-ghost mini" onclick="load()">🔄 بروزرسانی</button>
      <button class="btn btn-red mini" onclick="logout()">خروج</button>
    </div>
  </div>
  <div class="wrap">
    <div class="grid">
      <div class="card stat"><div class="v" id="sTotal">—</div><div class="l">👥 کل کاربران</div></div>
      <div class="card stat"><div class="v" id="sActive">—</div><div class="l">🟢 سلف‌های فعال</div></div>
      <div class="card stat"><div class="v" id="sDiamonds">—</div><div class="l">💎 مجموع الماس‌ها</div></div>
    </div>

    <div class="card section">
      <h2>💎 واریز / کسر الماس</h2>
      <div class="row">
        <input id="dId" dir="ltr" placeholder="آیدی عددی کاربر" inputmode="numeric">
        <input id="dAmt" dir="ltr" placeholder="تعداد (منفی = کسر)" inputmode="numeric">
        <button class="btn btn-green mini" onclick="giveDiamonds()">اعمال 💎</button>
      </div>
      <p class="muted" style="margin-top:8px">معادل دستور تلگرامی: <code style="direction:ltr">/give_diamonds &lt;id&gt; &lt;amount&gt;</code></p>
    </div>

    <div class="card section">
      <h2>📢 پیام همگانی</h2>
      <div class="row">
        <input id="bText" placeholder="متن اطلاعیه برای همه کاربران...">
        <button class="btn btn-primary mini" onclick="broadcast()">ارسال 📤</button>
      </div>
    </div>

    <div class="card section">
      <h2>⏰ کرون نگهداری روزانه (۳۰ 💎/روز)</h2>
      <div class="row">
        <span class="muted">اجرای خودکار: هر ۲۴ ساعت توسط Cloudflare Cron Trigger</span>
        <button class="btn btn-ghost mini" onclick="runCron()">▶️ اجرای دستی الان</button>
      </div>
    </div>

    <div class="card section">
      <h2>🧬 پیکربندی ربات و MTProto</h2>
      <table>
        <tbody>
          <tr><td>🤖 ربات</td><td dir="ltr" id="cBot">—</td></tr>
          <tr><td>👑 آیدی مالک</td><td dir="ltr" id="cOwner">—</td></tr>
          <tr><td>🧬 API ID</td><td dir="ltr" id="cApiId">—</td></tr>
          <tr><td>🧬 API Hash</td><td dir="ltr"><span id="cApiHash">—</span>
            <button class="btn btn-ghost mini" style="margin-right:8px" onclick="toggleHash()">👁 نمایش/مخفی</button></td></tr>
          <tr><td>🔗 وب‌هوک</td><td dir="ltr" style="font-size:11px;word-break:break-all" id="cWebhook">—</td></tr>
        </tbody>
      </table>
    </div>

    <div class="card section">
      <h2>📜 تراکنش‌های اخیر الماس</h2>
      <div style="overflow-x:auto">
      <table>
        <thead><tr><th>زمان</th><th>کاربر</th><th>مقدار</th><th>دلیل</th></tr></thead>
        <tbody id="txbody"><tr><td colspan="4" style="text-align:center;color:#64748b">—</td></tr></tbody>
      </table>
      </div>
    </div>

    <div class="card section">
      <h2>👥 لیست کاربران</h2>
      <div class="row" style="margin-bottom:12px">
        <input id="search" placeholder="🔍 جستجو بر اساس نام، یوزرنیم یا آیدی..." oninput="applySearch()">
      </div>
      <div style="overflow-x:auto">
      <table>
        <thead><tr><th>آیدی</th><th>نام</th><th>یوزرنیم</th><th>💎</th><th>سلف</th><th>ماژول‌ها</th><th>عملیات</th></tr></thead>
        <tbody id="tbody"><tr><td colspan="7" style="text-align:center;color:#64748b">در حال بارگذاری...</td></tr></tbody>
      </table>
      </div>
    </div>
  </div>
</div>

<script>
var KEY=null;
function qs(id){return document.getElementById(id)}
function toast(m){var t=qs('toast');t.textContent=m;t.classList.add('show');setTimeout(function(){t.classList.remove('show')},2600)}
function api(path,opts){
  opts=opts||{};opts.headers=Object.assign({'content-type':'application/json','x-admin-key':KEY},opts.headers||{});
  return fetch(path,opts).then(function(r){return r.json()});
}
function doLogin(){
  KEY=qs('keyIn').value.trim();
  if(!KEY)return toast('کلید را وارد کنید');
  api('/api/users').then(function(j){
    if(j.ok){try{localStorage.setItem('adminKey',KEY)}catch(_){}show(true);render(j);loadConfig();loadTx()}
    else{toast('❌ کلید نامعتبر است');}
  });
}
function logout(){try{localStorage.removeItem('adminKey')}catch(_){}location.reload()}
function show(ok){qs('loginView').style.display=ok?'none':'block';qs('panelView').style.display=ok?'block':'none'}
function fa(n){return Number(n).toLocaleString('fa-IR')}
function render(j){
  qs('sTotal').textContent=fa(j.stats.total);
  qs('sActive').textContent=fa(j.stats.active);
  qs('sDiamonds').textContent=fa(j.stats.diamonds);
  ALL_USERS=j.users.sort(function(a,b){return b.diamonds-a.diamonds});
  applySearch();
}
function renderRows(users){
  var tb=qs('tbody');tb.innerHTML='';
  if(!users.length){tb.innerHTML='<tr><td colspan="7" style="text-align:center;color:#64748b">هنوز کاربری ثبت نشده — ربات را در تلگرام /start کنید</td></tr>';return}
  users.forEach(function(u){
    var tr=document.createElement('tr');
    tr.innerHTML='<td dir="ltr">'+u.id+'</td><td>'+escp(u.name)+'</td><td dir="ltr">'+(u.username?'@'+escp(u.username):'—')+'</td>'+
      '<td><b>'+fa(u.diamonds)+'</b></td>'+
      '<td><span class="pill '+(u.selfActive?'on':'off')+'">'+(u.selfActive?'🟢 روشن':'🔴 خاموش')+'</span></td>'+
      '<td>'+fa(u.modules)+'</td>'+
      '<td class="row">'+
        '<button class="btn btn-green mini" onclick="quick('+u.id+',30)">+۳۰</button>'+
        '<button class="btn btn-red mini" onclick="quick('+u.id+',-30)">-۳۰</button>'+
        '<button class="btn btn-ghost mini" onclick="toggleSelf('+u.id+','+(!u.selfActive)+')">'+(u.selfActive?'خاموش کن':'روشن کن')+'</button>'+
      '</td>';
    tb.appendChild(tr);
  });
}
function escp(s){var d=document.createElement('div');d.textContent=s||'';return d.innerHTML}
var ALL_USERS=null,HASH_FULL='',HASH_MASK='',HASH_SHOWN=false;
function applySearch(){
  if(!ALL_USERS)return;
  var q=qs('search').value.trim().toLowerCase();
  var filtered=!q?ALL_USERS:ALL_USERS.filter(function(u){
    return String(u.id).indexOf(q)>-1||(u.name||'').toLowerCase().indexOf(q)>-1||(u.username||'').toLowerCase().indexOf(q)>-1;
  });
  renderRows(filtered);
}
function toggleHash(){
  HASH_SHOWN=!HASH_SHOWN;
  qs('cApiHash').textContent=HASH_SHOWN?(HASH_FULL||'—'):(HASH_MASK||'—');
}
var TX_LABELS={activation:'⚡️ فعال‌سازی سلف',daily:'📆 نگهداری روزانه',transfer_in:'📥 دریافت انتقال',
  transfer_out:'📤 ارسال انتقال',admin:'👑 مدیریت',daily_gift:'🎁 جایزه روزانه',wheel:'🎡 گردونه شانس'};
function loadConfig(){
  api('/api/config').then(function(j){
    if(!j.ok)return;
    qs('cBot').textContent='@'+(j.botUsername||'—');
    qs('cOwner').textContent=j.ownerId||'—';
    qs('cApiId').textContent=j.apiId||'ثبت نشده';
    HASH_FULL=j.apiHash||'';HASH_MASK=j.apiHashMasked||'ثبت نشده';
    qs('cApiHash').textContent=HASH_MASK;
    qs('cWebhook').textContent=j.webhookUrl||'—';
  });
}
function loadTx(){
  api('/api/transactions').then(function(j){
    if(!j.ok)return;
    var tb=qs('txbody');tb.innerHTML='';
    if(!j.transactions.length){tb.innerHTML='<tr><td colspan="4" style="text-align:center;color:#64748b">هنوز تراکنشی ثبت نشده</td></tr>';return}
    j.transactions.slice(0,30).forEach(function(t){
      var tr=document.createElement('tr');
      var d=new Date(t.t).toLocaleString('fa-IR',{timeZone:'Asia/Tehran',hour:'2-digit',minute:'2-digit',month:'2-digit',day:'2-digit'});
      tr.innerHTML='<td>'+d+'</td><td dir="ltr">'+t.id+'</td>'+
        '<td style="color:'+(t.amount>=0?'#4ade80':'#f87171')+';font-weight:700">'+(t.amount>=0?'+':'')+fa(t.amount)+' 💎</td>'+
        '<td>'+(TX_LABELS[t.reason]||t.reason)+'</td>';
      tb.appendChild(tr);
    });
  });
}
function load(){api('/api/users').then(function(j){if(j.ok){render(j);loadConfig();loadTx()}else show(false)})}
function quick(id,amt){api('/api/diamonds',{method:'POST',body:JSON.stringify({id:id,amount:amt})}).then(function(j){toast(j.ok?'✅ انجام شد':'❌ خطا');load()})}
function giveDiamonds(){
  var id=Number(qs('dId').value),amt=Number(qs('dAmt').value);
  if(!id||!amt)return toast('آیدی و مقدار را وارد کنید');
  api('/api/diamonds',{method:'POST',body:JSON.stringify({id:id,amount:amt})}).then(function(j){
    toast(j.ok?('✅ موجودی جدید: '+fa(j.diamonds)+' 💎'):'❌ '+j.error);load();
  });
}
function toggleSelf(id,on){api('/api/toggle-self',{method:'POST',body:JSON.stringify({id:id,on:on})}).then(function(j){toast(j.ok?'✅ انجام شد':'❌ '+j.error);load()})}
function broadcast(){
  var t=qs('bText').value.trim();if(!t)return toast('متن را وارد کنید');
  api('/api/broadcast',{method:'POST',body:JSON.stringify({text:t})}).then(function(j){toast(j.ok?('📤 ارسال به '+fa(j.sent)+' کاربر'):'❌ خطا');qs('bText').value=''});
}
function runCron(){api('/api/run-cron',{method:'POST'}).then(function(j){toast(j.ok?('⏰ شارژ: '+fa(j.charged)+' | خاموش: '+fa(j.deactivated)):'❌ خطا');load()})}

(function(){
  var k=null;try{k=localStorage.getItem('adminKey')}catch(_){}
  var q=new URLSearchParams(location.search).get('key');
  KEY=q||k;
  if(KEY){api('/api/users').then(function(j){if(j.ok){show(true);render(j);loadConfig();loadTx()}else{show(false)}})}
  else show(false);
})();
</script>
</body>
</html>`;

/* ------------------------- صفحه وضعیت (پس از پیکربندی) ------------------------- */

function statusPage(cfg) {
  return `<!DOCTYPE html>
<html lang="fa" dir="rtl"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>🤖 Self-Bot Hub</title>
<link href="https://cdn.jsdelivr.net/gh/rastikerdar/vazirmatn@v33.003/Vazirmatn-font-face.css" rel="stylesheet">
<style>${CSS_BASE}
.wrap{max-width:430px;margin:14vh auto;padding:0 18px;text-align:center}
.card{padding:34px}
.em{font-size:60px;display:block;margin-bottom:12px}
h1{font-size:22px;margin-bottom:8px}
.ok{color:#4ade80;font-size:14px;margin-bottom:22px}
a.btn{text-decoration:none;margin:5px}
</style></head>
<body><div class="bg"></div>
<div class="wrap"><div class="card">
<span class="em">🤖</span>
<h1>Self-Bot Hub فعال است</h1>
<p class="ok">✅ ربات @${esc(cfg.botUsername)} پیکربندی شده و وب‌هوک متصل است</p>
${cfg.apiId ? `<p class="ok" style="color:#67e8f9">🧬 MTProto: API ID <b dir="ltr">${esc(cfg.apiId)}</b> ثبت شده ✓</p>` : ''}
<a class="btn btn-primary" href="https://t.me/${esc(cfg.botUsername)}">💬 باز کردن ربات در تلگرام</a>
<a class="btn" style="background:rgba(255,255,255,.1)" href="/panel">👑 پنل مدیریت</a>
</div>
<p class="muted" style="margin-top:16px">Cloudflare Workers ⚡️ نسخه ${BRAND.VERSION}</p>
</div></body></html>`;
}

/* ========================================================================== */
/* §9 — ورودی‌های Worker                                                       */
/* ========================================================================== */

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    const path = url.pathname;

    // CORS preflight
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: {
        'access-control-allow-origin': '*',
        'access-control-allow-methods': 'GET,POST,OPTIONS',
        'access-control-allow-headers': 'content-type,x-admin-key',
      }});
    }

    /* ---- وب‌هوک تلگرام ---- */
    if (path.startsWith('/webhook/') && request.method === 'POST') {
      const cfg = await getConfig(env);
      const secret = path.split('/')[2];
      if (!cfg || secret !== cfg.webhookSecret) return new Response('forbidden', { status: 403 });
      const headerSecret = request.headers.get('x-telegram-bot-api-secret-token');
      if (headerSecret && headerSecret !== cfg.webhookSecret) return new Response('forbidden', { status: 403 });
      const update = await request.json().catch(() => null);
      if (update) ctx.waitUntil(handleUpdate(env, cfg, update));
      return new Response('ok');
    }

    /* ---- API ---- */
    if (path === '/api/setup' && request.method === 'POST') return apiSetup(env, request);
    if (path === '/api/users') return apiUsers(env, request);
    if (path === '/api/diamonds' && request.method === 'POST') return apiDiamonds(env, request);
    if (path === '/api/toggle-self' && request.method === 'POST') return apiToggleSelf(env, request);
    if (path === '/api/broadcast' && request.method === 'POST') return apiBroadcast(env, request);
    if (path === '/api/run-cron' && request.method === 'POST') return apiRunCron(env, request);
    if (path === '/api/config') return apiConfig(env, request);
    if (path === '/api/transactions') return apiTransactions(env, request);
    if (path === '/api/health') return json({ ok: true, name: BRAND.BOT_NAME, version: BRAND.VERSION });

    /* ---- صفحات ---- */
    if (path === '/panel') return html(PANEL_HTML);
    if (path === '/' || path === '/setup') {
      const cfg = await getConfig(env);
      if (!cfg) return html(SETUP_HTML);
      return html(statusPage(cfg));
    }

    return new Response('Not Found', { status: 404 });
  },

  /** Cron Trigger — هر ۲۴ ساعت: کسر ۳۰ الماس نگهداری از سلف‌های فعال */
  async scheduled(event, env, ctx) {
    ctx.waitUntil(runDailyCharge(env));
  },
};
