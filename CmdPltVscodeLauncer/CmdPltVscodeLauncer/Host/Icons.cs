// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Microsoft.CommandPalette.Extensions.Toolkit;

namespace CmdPltVscodeLauncer
{
    internal static class Icons
    {
        // plugin.json: IcoPathLight / IcoPathDark
        public static IconInfo PluginIcon { get; } = IconHelpers.FromRelativePaths("Assets\\Images\\code.light.png", "Assets\\Images\\code.dark.png");
    }
}
