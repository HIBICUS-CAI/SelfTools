using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Media;

namespace Hider
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow
    {
        private static readonly string SettingFilePath = Path.Join(Path.GetTempPath(), "hider_setting", "setting.txt");

        public MainWindow()
        {
            InitializeComponent();
            Closing += OnClosing;
            G.Background = new SolidColorBrush(Color.FromRgb(104, 118, 138));

            if (!File.Exists(SettingFilePath)) return;
            var settings = File.ReadAllLines(SettingFilePath).Select(_line => _line[(_line.IndexOf(':') + 1)..]).ToArray();
            Debug.Assert(Application.Current.MainWindow != null, "Application.Current.MainWindow != null");
            Application.Current.MainWindow.Left = double.Parse(settings[0]);
            Application.Current.MainWindow.Top = double.Parse(settings[1]);
            Application.Current.MainWindow.Width = double.Parse(settings[2]);
            Application.Current.MainWindow.Height = double.Parse(settings[3]);
        }

        private static void OnClosing(object? _sender, CancelEventArgs _e)
        {
            Debug.Assert(Application.Current.MainWindow != null, "Application.Current.MainWindow != null");
            if (!Directory.Exists(Path.GetDirectoryName(SettingFilePath)))
            {
                Directory.CreateDirectory(Path.GetDirectoryName(SettingFilePath) ?? throw new InvalidOperationException());
            }
            File.WriteAllLines(SettingFilePath, new[]
            {
                $"Left:{Application.Current.MainWindow.Left}",
                $"Top:{Application.Current.MainWindow.Top}",
                $"Width:{Application.Current.MainWindow.Width}",
                $"Height:{Application.Current.MainWindow.Height}"
            });
        }
    }
}