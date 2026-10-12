// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using CmdPltVscodeLauncer.VSCodeHelper;

namespace CmdPltVscodeLauncer.RemoteMachinesHelper
{
    public class VSCodeRemoteMachine
    {
        public string Host { get; set; }

        public string User { get; set; }

        public string HostName { get; set; }

        public VSCodeInstance VSCodeInstance { get; set; }
    }
}
