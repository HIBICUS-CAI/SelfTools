// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.IO;

namespace CmdPltVscodeLauncer
{
    internal static class StoragePaths
    {
        private static readonly Lazy<string> _localDataDirectory = new(GetLocalDataDirectory);

        // Packaged: the package LocalState folder (a real path, readable by the Command Palette process).
        // Unpackaged (debug): %LOCALAPPDATA%\CmdPltVscodeLauncer.
        public static string LocalDataDirectory => _localDataDirectory.Value;

        private static string GetLocalDataDirectory()
        {
            string directory;
            try
            {
                directory = Windows.Storage.ApplicationData.Current.LocalFolder.Path;
            }
            catch (Exception)
            {
                directory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "CmdPltVscodeLauncer");
            }

            Directory.CreateDirectory(directory);
            return directory;
        }
    }
}
