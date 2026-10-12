// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;

namespace CmdPltVscodeLauncer
{
    // Port of Wox.Plugin.Query + PowerLauncher.Plugin.QueryBuilder for the "{" action keyword.
    // Command Palette strips the alias from SearchText, so the keyword is put back to reproduce
    // the exact Search string PowerToys Run hands to the plugin.
    public class Query
    {
        public const string ActionKeyword = "{";

        private const string TermSeparator = " ";

        public Query(string searchText)
        {
            // QueryBuilder.Build
            var text = (ActionKeyword + (searchText ?? string.Empty)).Trim();

            // Query.RawQuery
            RawQuery = string.Join(TermSeparator, text.Split(new[] { TermSeparator }, StringSplitOptions.RemoveEmptyEntries));

            // Query.Search
            Search = RawQuery.Substring(ActionKeyword.Length).Trim();
        }

        public string RawQuery { get; }

        public string Search { get; }
    }
}
