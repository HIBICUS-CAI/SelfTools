﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace Chat_Server
{
    using su = StringUtils;

    public class ConnectManager
    {
        public static ConnectManager Instance { get; } = new();

        public Thread? ListenThread { get; set; }
        public TcpListener Listener { get; }
        public List<Socket> AllClients { get; }
        public bool ShouldStop { get; set; }

        public SortedDictionary<int, ChatRoom> ChatRooms { get; set; }

        private ConnectManager()
        {
            int port = 32580;
            IPAddress address = IPAddress.Any;
            Listener = new(address, port);
            AllClients = new();
            ShouldStop = false;
            ChatRooms = new();
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
                        int roomID = BitConverter.ToInt32(userInfo, 0);
                        char nameLenth = Convert.ToChar(
                            userInfo[sizeof(int)]);
                        string userName = su.AscByteToStr(
                            userInfo, sizeof(int) + 1, nameLenth);

                        Console.WriteLine(
                            $"username : {userName}, room : {roomID}, " +
                            $"has accept by {Listener}");

                        byte[] data = su.StrToAscByte(userName);
                        client.Send(data);

                        if (!ChatRooms.ContainsKey(roomID))
                        {
                            ChatRooms.Add(roomID, new ChatRoom(roomID));
                            Console.WriteLine(
                            $"room : {roomID}, has been created");
                        }
                        lock (ChatRooms[roomID].Clients)
                        {
                            ChatRooms[roomID].AddUser(userName, client);
                        }
                        Console.WriteLine(
                            $"username : {userName}, " +
                            $"has been added to room : {roomID}");
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

        public void RemoveChatRoom(int _id)
        {
            ChatRooms.Remove(_id);
        }
    }
}
