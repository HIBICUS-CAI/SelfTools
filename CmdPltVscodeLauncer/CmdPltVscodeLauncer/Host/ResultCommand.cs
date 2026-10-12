// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;

using Microsoft.CommandPalette.Extensions;
using Microsoft.CommandPalette.Extensions.Toolkit;

namespace CmdPltVscodeLauncer
{
    // PowerToys Run actions return "hide window"; true hides the palette, false keeps it open.
    internal sealed partial class ResultCommand : InvokableCommand
    {
        private readonly Func<bool> _action;

        public ResultCommand(Func<bool> action)
        {
            _action = action;
            Name = "Open";
        }

        public override ICommandResult Invoke()
        {
            return _action() ? CommandResult.Hide() : CommandResult.KeepOpen();
        }
    }
}
