// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;

using CmdPltVscodeLauncer.Properties;
using CmdPltVscodeLauncer.VSCodeHelper;

using Microsoft.CommandPalette.Extensions;
using Microsoft.CommandPalette.Extensions.Toolkit;

namespace CmdPltVscodeLauncer
{
    public partial class CmdPltVscodeLauncerCommandsProvider : CommandProvider
    {
        private readonly ICommandItem[] _commands;

        public CmdPltVscodeLauncerCommandsProvider()
        {
            // Same timing as PowerToys Run: VS Code instances are detected once, when the plugin is constructed.
            try
            {
                VSCodeInstances.LoadVSCodeInstances();
            }
            catch (Exception ex)
            {
                Log.Exception("Failed to load VS Code instances", ex, typeof(CmdPltVscodeLauncerCommandsProvider));
            }

            Id = "CmdPltVscodeLauncer";
            DisplayName = Resources.PluginTitle;
            Icon = Icons.PluginIcon;
            _commands = [
                new CommandItem(new VSCodeWorkspacesPage())
                {
                    Title = Resources.PluginTitle,
                    Subtitle = Resources.PluginDescription,
                },
            ];
        }

        public override ICommandItem[] TopLevelCommands()
        {
            return _commands;
        }
    }
}
