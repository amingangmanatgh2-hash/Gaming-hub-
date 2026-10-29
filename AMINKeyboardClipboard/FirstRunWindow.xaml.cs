using System.Windows;

namespace AMINKeyboardClipboard
{
    public partial class FirstRunWindow : Window
    {
        public FirstRunWindow()
        {
            InitializeComponent();
        }

        private void Start_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
