using System.Windows;
using System.Windows.Media;

namespace Hider
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow
    {
        public MainWindow()
        {
            InitializeComponent();
            G.Background = new SolidColorBrush(Color.FromRgb(104, 118, 138));
        }
    }
}