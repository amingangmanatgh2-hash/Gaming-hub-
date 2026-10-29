using System;
using System.Windows;
using System.Windows.Input;

namespace AMIN_Keyboard_Clipboard
{
    /// <summary>
    /// Compact, always-on-top clipboard popup. Click an item to copy it.
    /// </summary>
    public partial class ClipboardWindow : Window
    {
        public ClipboardWindow()
        {
            InitializeComponent();
            ClipboardStore.Changed += ClipboardStore_Changed;
            Loaded += (s, e) => Refresh();
        }

        private void Refresh()
        {
            lstItems.ItemsSource = ClipboardStore.GetAll();
            txtHint.Text = "Click any item once to copy it.";
        }

        private void ClipboardStore_Changed(object sender, EventArgs e)
        {
            Dispatcher.BeginInvoke(new Action(Refresh));
        }

        private void CopySelected()
        {
            if (lstItems.SelectedItem is string selected)
            {
                try
                {
                    Clipboard.SetText(selected);
                    txtHint.Text = "Copied: " + (selected.Length > 60 ? selected.Substring(0, 60) + "…" : selected);
                }
                catch (Exception ex)
                {
                    txtHint.Text = ex.Message;
                }
            }
            else
            {
                txtHint.Text = "Select an item first.";
            }
        }

        private void BtnCopy_Click(object sender, RoutedEventArgs e)
        {
            CopySelected();
        }

        private void LstItems_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            CopySelected();
        }

        private void Card_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            try { DragMove(); }
            catch { }
        }

        private void BtnClose_Click(object sender, RoutedEventArgs e)
        {
            Hide();
        }
    }
}
