// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;

using Microsoft.CommandPalette.Extensions;
using Microsoft.CommandPalette.Extensions.Toolkit;

namespace CmdPltVscodeLauncer
{
    // Replacement for Wox.Plugin.Logger.Log: messages go to the Command Palette log.
    internal static class Log
    {
        public static void Exception(string message, Exception ex, Type fullClassName)
        {
            Write(MessageState.Error, $"[{fullClassName?.FullName}] {message}{Environment.NewLine}{ex}");
        }

        public static void Error(string message, Type fullClassName)
        {
            Write(MessageState.Error, $"[{fullClassName?.FullName}] {message}");
        }

        private static void Write(MessageState state, string message)
        {
            System.Diagnostics.Debug.WriteLine(message);
            ExtensionHost.LogMessage(new LogMessage(message) { State = state });
        }
    }
}
