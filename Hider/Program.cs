using Avalonia;

namespace Hider
{
    internal static class Program
    {
        [STAThread]
        public static void Main(string[] _args)
        {
            BuildAvaloniaApp().StartWithClassicDesktopLifetime(_args);
        }

        // Also used by the Avalonia previewer.
        public static AppBuilder BuildAvaloniaApp()
        {
            return AppBuilder.Configure<App>()
                .UsePlatformDetect()
                // Fixed WM_CLASS so the .desktop entry (StartupWMClass) matches the running window.
                .With(new X11PlatformOptions { WmClass = "Hider" })
                .LogToTrace();
        }
    }
}
