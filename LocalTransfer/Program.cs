using System.CommandLine;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;

var rootCommand = new RootCommand()
{
    Description =
        "a util tool to perform file transfer between LOCAL network devices.\n(maybe work in public network but JUST DON'T DO THAT)"
};

var infoCommand = new Command("info", "show information for specific host or address.");
rootCommand.Add(infoCommand);
var infoTarget = new Argument<string[]>("target");
var infoShowHostName = new Option<bool>("--name", "-n")
{
    Description = "Show host name.",
    DefaultValueFactory = _ => false
};
var infoShowIPv6 = new Option<bool>("--ipv6", "-6")
{
    Description = "Show IPv6 address.",
    DefaultValueFactory = _ => false
};
var infoShowSelf = new Option<bool>("--self", "-s")
{
    Description = "Show self information.",
    DefaultValueFactory = _ => false
};
infoCommand.Add(infoTarget);
infoCommand.Add(infoShowIPv6);
infoCommand.Add(infoShowHostName);
infoCommand.Add(infoShowSelf);
infoCommand.SetAction(result =>
{
    int returnCode = 0;
    bool doesUseV6 = result.GetValue(infoShowIPv6);
    bool doesGetName = result.GetValue(infoShowHostName);
    bool doesShowSelfInfo = result.GetValue(infoShowSelf);
    var addressFamily = doesUseV6 ? AddressFamily.InterNetworkV6 : AddressFamily.InterNetwork;
    var targets = result.GetValue(infoTarget)!.Select((value, index) => (value, index)).ToList();
    int prefixPadding = 0;
    var targetsForPadding = targets.AsEnumerable();
    if (doesShowSelfInfo)
    {
        targetsForPadding = targetsForPadding.Concat([("self", -1)]);
    }

    if (targets.Count > 0 || doesShowSelfInfo)
    {
        prefixPadding = targetsForPadding.Select((str, index) => $"Target[{index}({str})]".Length).Max();
        prefixPadding += 4 - prefixPadding % 4;
    }

    foreach ((string targetName, int index) in result.GetValue(infoTarget)!.Select((value, index) => (value, index)))
    {
        string prefix = $"Target[{index}({targetName})]".PadRight(prefixPadding);
        Console.Write($"{prefix}fetching...\r");
        string output;
        try
        {
            string ip, name;
            if (doesGetName)
            {
                var host = Dns.GetHostEntry(targetName, addressFamily);
                ip = $"IP(s): {string.Join(";", host.AddressList.Select(addr => addr.ToString()))}";
                name = doesGetName ? $"Name: {host.HostName}, " : "";
            }
            else
            {
                ip =
                    $"IP(s): [{string.Join("; ", Dns.GetHostAddresses(targetName, addressFamily).Select(addr => addr.ToString()))}]";
                name = "";
            }

            output = $"{prefix}{name}{ip}";
        }
        catch (SocketException ex)
        {
            output = $"{prefix}Failed: {ex.SocketErrorCode}, {ex.Message}";
            returnCode = -1;
        }

        Console.WriteLine(output);
    }

    if (doesShowSelfInfo)
    {
        string prefix = "Target[-1(self)]".PadRight(prefixPadding);
        string ip =
            $"IP(s): [{string.Join("; ", Dns.GetHostAddresses(Dns.GetHostName(), addressFamily).Select(addr => addr.ToString()))}]";
        string name = doesGetName ? $"Name: {Dns.GetHostName()}, " : "";
        Console.WriteLine($"{prefix}{name}{ip}");
    }

    return returnCode;
});

var scanCommand = new Command("scan", "scan local devices.");
rootCommand.Add(scanCommand);
var scanBaseIP = new Option<IPAddress>("--ip", "-i")
{
    Description = "Scan IP base with this address.",
    DefaultValueFactory = _ => IPAddress.Parse("192.168.0.1"),
    CustomParser = result => IPAddress.Parse(result.Tokens[0].Value),
    Validators =
    {
        result =>
        {
            if (result.Tokens.Any() && !IPAddress.TryParse(result.Tokens[0].Value, out _))
                result.AddError($"Invalid IP address \"{result.Tokens[0].Value}\".");
        }
    }
};
var scanMask = new Option<int>("--mask", "-m")
{
    Description = "Scan IP base with this mask.",
    DefaultValueFactory = _ => 20
};
var scanTimeout = new Option<int>("--timeout", "-t")
{
    Description = "Timeout for ping host.",
    DefaultValueFactory = _ => 1000
};
scanCommand.Add(scanBaseIP);
scanCommand.Add(scanMask);
scanCommand.Add(scanTimeout);
scanCommand.SetAction(result =>
{
    byte[] ipBytes = result.GetValue(scanBaseIP)!.GetAddressBytes();
    uint mask = ~(uint.MaxValue >> result.GetValue(scanMask));
    byte[] maskBytes = BitConverter.GetBytes(mask).Reverse().ToArray();

    byte[] startBytes = new byte[ipBytes.Length];
    byte[] endBytes = new byte[ipBytes.Length];
    for (int i = 0; i < ipBytes.Length; i++)
    {
        startBytes[i] = (byte)(ipBytes[i] & maskBytes[i]);
        endBytes[i] = (byte)(ipBytes[i] | ~maskBytes[i]);
    }

    startBytes[3] += 1;
    endBytes[3] -= 1;
    Console.WriteLine($"IP range: [{new IPAddress(startBytes)} ~ {new IPAddress(endBytes)}]");

    var activeHostList = new List<IPAddress>(2048);

    foreach (int x in Enumerable.Range(startBytes[0], endBytes[0] - startBytes[0] + 1).ToList())
    {
        foreach (int y in Enumerable.Range(startBytes[1], endBytes[1] - startBytes[1] + 1).ToList())
        {
            foreach (int z in Enumerable.Range(startBytes[2], endBytes[2] - startBytes[2] + 1))
            {
                Console.Write($"Checking: {x}.{y}.{z}.x part...\r");
                bool newLine = true;

                var ipArray =
                    from w in Enumerable.Range(startBytes[3], endBytes[3] - startBytes[3] + 1)
                    select new IPAddress(new[] { (byte)x, (byte)y, (byte)z, (byte)w });

                foreach (var ip in ipArray
                             .Select(ip => (new Ping().SendPingAsync(ip, result.GetValue(scanTimeout)), ip))
                             .ToArray()
                             .Where(task => task.Item1.Result.Status == IPStatus.Success)
                             .Select(task => (task.Item1.Result.Status, task.ip)))
                {
                    if (newLine)
                    {
                        newLine = false;
                        Console.WriteLine();
                    }
                    Console.Write($"{(ip.Status == IPStatus.Success ? "Found" : ip.Status.ToString())}: {ip.ip}\n");
                    activeHostList.Add(ip.ip);
                }
            }
        }
    }

    Console.WriteLine($"\nFinished, {activeHostList.Count} Found.");
});

return rootCommand.Parse(args).Invoke();