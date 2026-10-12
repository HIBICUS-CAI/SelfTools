// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Resources;
using System.Threading;

using Microsoft.CommandPalette.Extensions;
using Shmuelie.WinRTServer.CsWinRT;

[assembly: NeutralResourcesLanguage("en-US")]

namespace CmdPltVscodeLauncer
{
    public class Program
    {
        [MTAThread]
        public static void Main(string[] args)
        {
            if (args.Length > 0 && args[0] == "-RegisterProcessAsComServer")
            {
                global::Shmuelie.WinRTServer.ComServer server = new();

                ManualResetEvent extensionDisposedEvent = new(false);

                // Single extension instance, returned every time the host asks for the IExtension object.
                VscodeLauncerExtension extensionInstance = new(extensionDisposedEvent);
                server.RegisterClass<VscodeLauncerExtension, IExtension>(() => extensionInstance);
                server.Start();

                // Exit once the extension is disposed.
                extensionDisposedEvent.WaitOne();
                server.Stop();
                server.UnsafeDispose();
            }
            else
            {
                Console.WriteLine("Not being launched as a Extension... exiting.");
            }
        }
    }
}
