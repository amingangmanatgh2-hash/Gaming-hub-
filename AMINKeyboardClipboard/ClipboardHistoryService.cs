using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using System.Windows;

namespace AMINKeyboardClipboard
{
    /// <summary>
    /// Monitors the clipboard for <b>text only</b> and keeps a small local history.
    ///
    /// Design notes:
    ///  - Only <see cref="TextDataFormat.UnicodeText"/> is ever read. Images, files and
    ///    custom formats are ignored, so no binary data is ever captured or stored.
    ///  - The history is capped and de-duplicated, and is written to the current
    ///    user's LocalAppData folder. Nothing leaves the machine.
    ///  - Clipboard access can legitimately fail (another app holds the clipboard
    ///    open), so reads retry a few times before giving up silently.
    /// </summary>
    public sealed class ClipboardHistoryService
    {
        public const int MaxEntries = 100;
        public const int MaxTextLength = 20000;

        private readonly string filePath;
        private readonly List<string> history = new List<string>();
        private readonly JavaScriptSerializer serializer = new JavaScriptSerializer { MaxJsonLength = 32 * 1024 * 1024 };

        private string lastKnownClipboardText;
        private bool listening;

        /// <summary>Raised on the UI thread after the history changed.</summary>
        public event Action HistoryChanged;

        public ClipboardHistoryService()
        {
            filePath = Path.Combine(Settings.DataFolder, "clipboard-history.json");
            Load();
        }

        /// <summary>Snapshot copy, safe to enumerate from the UI thread.</summary>
        public IReadOnlyList<string> History
        {
            get { return history.ToList().AsReadOnly(); }
        }

        public int Count
        {
            get { return history.Count; }
        }

        public bool Start(IntPtr hwnd)
        {
            if (listening || hwnd == IntPtr.Zero) return listening;

            listening = NativeMethods.AddClipboardFormatListener(hwnd);
            return listening;
        }

        public void Stop(IntPtr hwnd)
        {
            if (!listening || hwnd == IntPtr.Zero) return;

            NativeMethods.RemoveClipboardFormatListener(hwnd);
            listening = false;
        }

        /// <summary>
        /// Called from the WM_CLIPBOARDUPDATE handler. Reads text, if any, and
        /// appends it to the history.
        /// </summary>
        public void TryReadClipboardText()
        {
            string text = ReadClipboardText();
            if (text == null) return;

            if (string.Equals(text, lastKnownClipboardText, StringComparison.Ordinal))
                return;

            lastKnownClipboardText = text;
            Add(text);
        }

        private static string ReadClipboardText()
        {
            // The clipboard is a shared, OS-wide resource. Another process may hold
            // it open for a few milliseconds; retry instead of losing the entry.
            for (int attempt = 0; attempt < 5; attempt++)
            {
                try
                {
                    if (!Clipboard.ContainsText(TextDataFormat.UnicodeText))
                        return null;

                    string text = Clipboard.GetText(TextDataFormat.UnicodeText);

                    if (string.IsNullOrWhiteSpace(text))
                        return null;

                    if (text.Length > MaxTextLength)
                        text = text.Substring(0, MaxTextLength);

                    return text;
                }
                catch (Exception)
                {
                    System.Threading.Thread.Sleep(60);
                }
            }

            return null;
        }

        public void Add(string text)
        {
            if (string.IsNullOrWhiteSpace(text)) return;

            if (text.Length > MaxTextLength)
                text = text.Substring(0, MaxTextLength);

            history.RemoveAll(x => string.Equals(x, text, StringComparison.Ordinal));
            history.Insert(0, text);

            if (history.Count > MaxEntries)
                history.RemoveRange(MaxEntries, history.Count - MaxEntries);

            Save();
            RaiseChanged();
        }

        public void Remove(string text)
        {
            if (history.RemoveAll(x => string.Equals(x, text, StringComparison.Ordinal)) == 0)
                return;

            Save();
            RaiseChanged();
        }

        public void Clear()
        {
            if (history.Count == 0) return;

            history.Clear();
            lastKnownClipboardText = null;
            Save();
            RaiseChanged();
        }

        /// <summary>
        /// Puts an entry back on the clipboard without re-adding it to the history
        /// again on the WM_CLIPBOARDUPDATE that follows.
        /// </summary>
        public bool CopyToClipboard(string text)
        {
            if (string.IsNullOrEmpty(text)) return false;

            for (int attempt = 0; attempt < 5; attempt++)
            {
                try
                {
                    Clipboard.SetText(text);
                    lastKnownClipboardText = text;
                    return true;
                }
                catch (Exception)
                {
                    System.Threading.Thread.Sleep(60);
                }
            }

            return false;
        }

        private void Load()
        {
            try
            {
                if (!File.Exists(filePath)) return;

                string json = File.ReadAllText(filePath);
                if (string.IsNullOrWhiteSpace(json)) return;

                var entries = serializer.Deserialize<List<string>>(json);
                if (entries == null) return;

                foreach (string entry in entries)
                {
                    if (string.IsNullOrWhiteSpace(entry)) continue;
                    if (history.Contains(entry)) continue;

                    history.Add(entry);
                    if (history.Count >= MaxEntries) break;
                }
            }
            catch
            {
                // A corrupt history file must never stop the app from starting.
                history.Clear();
            }
        }

        private void Save()
        {
            try
            {
                File.WriteAllText(filePath, serializer.Serialize(history));
            }
            catch
            {
                // The app stays usable even if the disk write fails.
            }
        }

        private void RaiseChanged()
        {
            var handler = HistoryChanged;
            if (handler != null) handler();
        }
    }

    /// <summary>
    /// One history row as the UI sees it: a short single-line preview plus the
    /// full text that is put back on the clipboard.
    /// </summary>
    public sealed class ClipboardEntry
    {
        private const int PreviewLength = 400;

        public string Text { get; private set; }
        public string FullText { get; private set; }

        public static ClipboardEntry From(string text)
        {
            if (text == null) text = string.Empty;

            string flat = text
                .Replace("\r\n", " \u21B5 ")
                .Replace('\n', ' ')
                .Replace('\r', ' ')
                .Replace('\t', ' ')
                .Trim();

            if (flat.Length > PreviewLength)
                flat = flat.Substring(0, PreviewLength) + "…";

            return new ClipboardEntry { Text = flat, FullText = text };
        }
    }
}
