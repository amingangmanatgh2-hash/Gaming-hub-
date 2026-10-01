-- ============================================================
--  Self-Bot Hub — اسکیمای اختیاری D1 (آینه داده‌ها برای گزارش‌گیری)
--  اجرا:  npx wrangler d1 execute selfbot-db --file=./schema.sql
-- ============================================================

CREATE TABLE IF NOT EXISTS users (
  id          INTEGER PRIMARY KEY,          -- آیدی عددی تلگرام
  name        TEXT,
  username    TEXT,
  diamonds    INTEGER NOT NULL DEFAULT 0,   -- موجودی الماس
  self_active INTEGER NOT NULL DEFAULT 0,   -- 0 = خاموش | 1 = روشن
  joined_at   INTEGER                        -- timestamp (ms)
);

CREATE INDEX IF NOT EXISTS idx_users_active   ON users(self_active);
CREATE INDEX IF NOT EXISTS idx_users_diamonds ON users(diamonds);

-- لاگ تراکنش‌های الماس (در صورت نیاز به توسعه)
CREATE TABLE IF NOT EXISTS diamond_log (
  id         INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id    INTEGER NOT NULL,
  amount     INTEGER NOT NULL,              -- مثبت = واریز | منفی = کسر
  reason     TEXT,                          -- activation / daily / transfer / admin
  created_at INTEGER DEFAULT (strftime('%s','now'))
);
