using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace Chat_Server
{
    public class ConnectManager
    {
        public static ConnectManager Instance { get; } = new();

        public Thread? ListenThread { get; set; }
        public TcpListener Listener { get; }
        public List<Socket> AllClients { get; }
        public bool ShouldStop { get; set; }

        private ConnectManager()
        {
            int port = 32580;
            IPAddress address = IPAddress.Any;
            Listener = new TcpListener(address, port);
            AllClients = new List<Socket>();
            ShouldStop = false;
        }

        public void StartListenThread()
        {
            ListenThread = new(new ThreadStart(StartToListen));
            ListenThread.Start();
        }

        public void JoinListenThread()
        {
            CloseAllSocket();
            if (ListenThread != null) ListenThread.Join();
        }

        private void StartToListen()
        {
            Listener.Start();
            while (!ShouldStop)
            {
                if (Listener.Pending())
                {
                    Socket client = Listener.AcceptSocket();
                    AllClients.Add(client);
                    byte[] userInfo = new byte[1024];
                    int inByte = client.Receive(userInfo);
                    if (inByte > 0)
                    {
                        Int32 roomID = BitConverter.ToInt32(userInfo, 0);
                        char nameLenth = Convert.ToChar(
                            userInfo[sizeof(Int32)]);
                        char[] name = new char[nameLenth];
                        for (int i = 0; i < nameLenth; i++)
                        {
                            name[i] = Convert.ToChar(
                                userInfo[sizeof(Int32) + i + 1]);
                        }
                        string userName = new(name);

                        Console.WriteLine(
                            $"username : {userName}, room : {roomID}, " +
                            $"has accept by {Listener}");

                        byte[] data = new byte[nameLenth + 1];
                        for (int i = 0; i < nameLenth; i++)
                        {
                            data[i] = Convert.ToByte(name[i]);
                        }
                        client.Send(data);
                    }
                }
            }
        }

        public void CloseSocket(Socket _target)
        {
            _ = AllClients.Remove(_target);
            _target.Close();
        }

        private void CloseAllSocket()
        {
            foreach (var socket in AllClients) socket.Close();
            AllClients.Clear();
        }
    }
}
