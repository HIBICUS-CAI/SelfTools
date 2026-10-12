// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System.ComponentModel;
using System.Diagnostics;

namespace CmdPltVscodeLauncer
{
    // Port of the Wox.Infrastructure.Helper methods used by the plugin.
    internal static class Helper
    {
        public static Process OpenInConsole(string path)
        {
            var processStartInfo = new ProcessStartInfo
            {
                WorkingDirectory = path,
                FileName = "cmd.exe",
            };

            return Process.Start(processStartInfo);
        }

        public static bool OpenInShell(string path, string arguments = null, string workingDir = null)
        {
            using (var process = new Process())
            {
                process.StartInfo.FileName = path;
                process.StartInfo.WorkingDirectory = string.IsNullOrWhiteSpace(workingDir) ? string.Empty : workingDir;
                process.StartInfo.Arguments = string.IsNullOrWhiteSpace(arguments) ? string.Empty : arguments;
                process.StartInfo.WindowStyle = ProcessWindowStyle.Normal;
                process.StartInfo.UseShellExecute = true;

                try
                {
                    process.Start();
                    return true;
                }
                catch (Win32Exception ex)
                {
                    Log.Exception($"Unable to open {path}: {ex.Message}", ex, typeof(Helper));
                    return false;
                }
            }
        }
    }
}
