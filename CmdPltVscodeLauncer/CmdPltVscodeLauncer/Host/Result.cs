// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Globalization;

using Microsoft.CommandPalette.Extensions.Toolkit;

namespace CmdPltVscodeLauncer
{
    // Subset of Wox.Plugin.Result used by the VS Code Workspaces plugin.
    public class Result
    {
        public string Title { get; set; }

        public string SubTitle { get; set; }

        public Func<IconInfo> Icon { get; set; }

        // Return true to hide the window.
        public Func<bool> Action { get; set; }

        public object ContextData { get; set; }

        public int Score { get; set; }

        public int SelectedCount { get; set; }

        // Result.GetSortOrderScore (WeightBoost defaults to 0)
        public int GetSortOrderScore(int selectedItemMultiplier)
        {
            return Score + (SelectedCount * selectedItemMultiplier);
        }

        public override string ToString()
        {
            // Using CurrentCulture since this is user facing
            return string.Format(CultureInfo.CurrentCulture, "{0} : {1}", Title, SubTitle);
        }
    }

    public class ContextMenuResult
    {
        public string Title { get; set; }

        public string Glyph { get; set; }

        public Microsoft.CommandPalette.Extensions.KeyChord RequestedShortcut { get; set; }

        // Return true to hide the window.
        public Func<bool> Action { get; set; }
    }
}
