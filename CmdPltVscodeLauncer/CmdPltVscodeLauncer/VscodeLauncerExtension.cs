// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Runtime.InteropServices;
using System.Threading;

using Microsoft.CommandPalette.Extensions;

namespace CmdPltVscodeLauncer
{
    [Guid("2D0DD4AE-B110-49AC-993E-2AF077AB7CA0")]
    public sealed partial class VscodeLauncerExtension : IExtension, IDisposable
    {
        private readonly ManualResetEvent _extensionDisposedEvent;

        private readonly CmdPltVscodeLauncerCommandsProvider _provider = new();

        public VscodeLauncerExtension(ManualResetEvent extensionDisposedEvent)
        {
            this._extensionDisposedEvent = extensionDisposedEvent;
        }

        public object GetProvider(ProviderType providerType)
        {
            return providerType switch
            {
                ProviderType.Commands => _provider,
                _ => null,
            };
        }

        public void Dispose() => this._extensionDisposedEvent.Set();
    }
}
