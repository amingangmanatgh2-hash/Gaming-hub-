using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;

namespace AMIN_Keyboard_Clipboard
{
    [DataContract]
    internal class HistoryData
    {
        [DataMember] public List<string> Items { get; set; }
        public HistoryData() { Items = new List<string>(); }
    }

    /// <summary>
    /// Thread-safe, JSON-persisted clipboard history (built into .NET Framework 4.8
    /// via DataContractJsonSerializer — no NuGet dependencies).
    /// </summary>
    public static class ClipboardStore
    {
        public const int MaxItems = 100;

        private static readonly object Sync = new object();
        private static readonly List<string> Items = new List<string>();

        private static string DirPath
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                                      "AMIN_Keyboard_Clipboard"); }
        }

        private static string FilePath { get { return Path.Combine(DirPath, "history.json"); } }

        public static event EventHandler Changed;

        public static void Load()
        {
            lock (Sync)
            {
                Items.Clear();
                if (File.Exists(FilePath))
                {
                    try
                    {
                        var ser = new DataContractJsonSerializer(typeof(HistoryData));
                        using (var fs = File.OpenRead(FilePath))
                        {
                            var data = (HistoryData)ser.ReadObject(fs);
                            if (data != null && data.Items != null)
                            {
                                foreach (var s in data.Items)
                                    if (s != null) Items.Add(s);
                            }
                        }
                    }
                    catch
                    {
                        Items.Clear();
                    }
                }
                else
                {
                    Directory.CreateDirectory(DirPath);
                }
                RaiseChanged();
            }
        }

        public static List<string> GetAll()
        {
            lock (Sync) { return new List<string>(Items); }
        }

        /// <summary>Adds text at the front; returns false if it was a duplicate.</summary>
        public static bool Add(string text)
        {
            if (string.IsNullOrEmpty(text)) return false;
            lock (Sync)
            {
                if (Items.Count > 0 && Items[0] == text) return false;
                Items.Insert(0, text);
                while (Items.Count > MaxItems) Items.RemoveAt(Items.Count - 1);
                SaveLocked();
                RaiseChanged();
                return true;
            }
        }

        public static bool Remove(string item)
        {
            lock (Sync)
            {
                bool removed = Items.Remove(item);
                if (removed)
                {
                    SaveLocked();
                    RaiseChanged();
                }
                return removed;
            }
        }

        public static void Clear()
        {
            lock (Sync)
            {
                Items.Clear();
                SaveLocked();
                RaiseChanged();
            }
        }

        private static void SaveLocked()
        {
            try
            {
                Directory.CreateDirectory(DirPath);
                var ser = new DataContractJsonSerializer(typeof(HistoryData));
                using (var ms = new MemoryStream())
                {
                    ser.WriteObject(ms, new HistoryData { Items = new List<string>(Items) });
                    File.WriteAllBytes(FilePath, ms.ToArray());
                }
            }
            catch
            {
                // Never crash the app over history persistence.
            }
        }

        private static void RaiseChanged()
        {
            EventHandler handler = Changed;
            if (handler != null) handler(null, EventArgs.Empty);
        }
    }
}
