using System.Globalization;
using Avalonia;
using Avalonia.Controls;

namespace Hider
{
    public partial class MainWindow : Window
    {
        // %LOCALAPPDATA% on Windows, ~/.local/share on Linux. The WPF version used the temp folder,
        // which Linux wipes on reboot.
        private static readonly string SettingFilePath = Path.Join(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "hider_setting", "setting.txt");

        public MainWindow()
        {
            InitializeComponent();
            LoadSetting();
        }

        protected override void OnClosing(WindowClosingEventArgs _e)
        {
            base.OnClosing(_e);
            SaveSetting();
        }

        private void LoadSetting()
        {
            if (!File.Exists(SettingFilePath)) return;
            var settings = File.ReadAllLines(SettingFilePath).Select(_line => _line[(_line.IndexOf(':') + 1)..]).ToArray();
            if (settings.Length < 4 ||
                !int.TryParse(settings[0], CultureInfo.InvariantCulture, out var left) ||
                !int.TryParse(settings[1], CultureInfo.InvariantCulture, out var top) ||
                !double.TryParse(settings[2], CultureInfo.InvariantCulture, out var width) ||
                !double.TryParse(settings[3], CultureInfo.InvariantCulture, out var height))
            {
                return;
            }

            WindowStartupLocation = WindowStartupLocation.Manual;
            Position = new PixelPoint(left, top);
            Width = width;
            Height = height;
        }

        private void SaveSetting()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(SettingFilePath) ?? throw new InvalidOperationException());
            File.WriteAllLines(SettingFilePath, new[]
            {
                // Position is in physical pixels, size in device-independent units (Avalonia's native units).
                FormattableString.Invariant($"Left:{Position.X}"),
                FormattableString.Invariant($"Top:{Position.Y}"),
                FormattableString.Invariant($"Width:{Width}"),
                FormattableString.Invariant($"Height:{Height}")
            });
        }
    }
}
