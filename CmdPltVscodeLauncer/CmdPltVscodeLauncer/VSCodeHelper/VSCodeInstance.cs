// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Microsoft.CommandPalette.Extensions.Toolkit;

namespace CmdPltVscodeLauncer.VSCodeHelper
{
    public enum VSCodeVersion
    {
        Stable = 1,
        Insiders = 2,
        Exploration = 3,
    }

    public class VSCodeInstance
    {
        public VSCodeVersion VSCodeVersion { get; set; }

        public string ExecutablePath { get; set; } = string.Empty;

        public string AppData { get; set; } = string.Empty;

        public string SharedStorageDbPath { get; set; } = string.Empty;

        public IconInfo WorkspaceIcon() => WorkspaceIconBitMap;

        public IconInfo RemoteIcon() => RemoteIconBitMap;

        public IconInfo WorkspaceIconBitMap { get; set; }

        public IconInfo RemoteIconBitMap { get; set; }
    }
}
