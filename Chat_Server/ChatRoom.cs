using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Sockets;
using System.Runtime.Serialization.Formatters.Binary;

namespace Chat_Server
{
    public class ChatRoom
    {
        public struct ChatMessage
        {
            public string UserName { get; }
            public string Content { get; }

            public ChatMessage(string _userName, string _content)
            {
                UserName = _userName;
                Content = _content;
            }
        }

        public class EventMessage
        {
            public enum SendMessType
            {
                USER_LIST = 0,
                NEW_MESS = 1
            }

            public SendMessType Type { get; }
            public byte[] Data { get; set; }

            public EventMessage(SendMessType _messType, uint _dataSize)
            {
                Type = _messType;
                Data = new byte[_dataSize];
            }

            public static EventMessage SyncUserList(string[] _list)
            {
                int type = (int)SendMessType.USER_LIST;
                byte[] typeData = BitConverter.GetBytes(type);

                int count = _list.Length;
                byte[] countData = BitConverter.GetBytes(count);

                int typeDataSize = typeData.Length;
                int countDataSize = countData.Length;

                int allNameDataSize = 0;
                List<Tuple<int, byte[]>> allName = new();
                allName.Capacity = _list.Length;

                foreach (var name in _list)
                {
                    int nameSize = name.Length;
                    byte[] nameSizeData = BitConverter.GetBytes(nameSize);
                    byte[] nameData = StringUtils.StrToAscByte(name);
                    byte[] aNameData = new byte[
                        nameSizeData.Length + nameData.Length];
                    Buffer.BlockCopy(nameSizeData, 0,
                        aNameData, 0, nameSizeData.Length);
                    Buffer.BlockCopy(nameData, 0,
                        aNameData, nameSizeData.Length, nameData.Length);
                    int aNameDataSize = aNameData.Length;
                    allName.Add(new(aNameDataSize, aNameData));
                    allNameDataSize += aNameDataSize;
                }

                int allDataSize =
                    typeDataSize + countDataSize + allNameDataSize;
                byte[] allData = new byte[allDataSize];

                int dstStart = 0;
                Buffer.BlockCopy(typeData, 0,
                    allData, dstStart, typeDataSize);
                dstStart += typeDataSize;
                Buffer.BlockCopy(countData, 0,
                    allData, dstStart, countDataSize);
                dstStart += countDataSize;
                foreach (var aName in allName)
                {
                    Buffer.BlockCopy(aName.Item2, 0,
                        allData, dstStart, aName.Item2.Length);
                    dstStart += aName.Item1;
                }

                EventMessage @event = new(SendMessType.USER_LIST,
                    (uint)allDataSize);
                @event.Data = allData;

                return @event;
            }

            public static EventMessage CreateNewMess(ChatMessage _content)
            {
                EventMessage @event = new(SendMessType.NEW_MESS,
                    (uint)_content.GetType().Name.ToString().Length);

                return @event;
            }

            public void SendTo(Socket _client)
            {
                _client.Send(Data);
                Console.WriteLine($"send {Data} : {_client}");
            }
        }

        public int Id { get; }
        public List<string> Users { get; set; }
        public SortedDictionary<string, Socket> Clients { get; set; }
        public Thread Thread { get; set; }

        public List<ChatMessage> ChatMessages { get; set; }
        public Queue<EventMessage> EventMessages { get; set; }

        public ChatRoom(int _roomID)
        {
            Id = _roomID;
            Users = new();
            Clients = new();

            Thread = new(RunProcess);
            ChatMessages = new();
            EventMessages = new();
        }

        private void RunProcess()
        {
            List<string> errorUser = new();
            errorUser.Capacity = 64;

            while (Users.Any())
            {
                lock (Clients)
                {
                    foreach (var item in Clients)
                    {
                        if (item.Value.Poll(0, SelectMode.SelectRead))
                        {
                            if (item.Value.Available == 0)
                            {
                                errorUser.Add(item.Key);
                                continue;
                            }

                            byte[] firstFlag = new byte[4];
                            int firstReceive = item.Value.Receive(firstFlag);
                            if (firstReceive > 0)
                            {
                                if (BitConverter.ToInt32(firstFlag, 0) ==
                                    (int)EventMessage.SendMessType.NEW_MESS)
                                {

                                }
                            }
                            else
                            {
                                errorUser.Add(item.Key);
                            }
                        }
                    }

                    foreach (var item in errorUser) { RemoveUser(item); }
                    errorUser.Clear();

                    if (EventMessages.Any())
                    {
                        EventMessage @event = EventMessages.Dequeue();
                        foreach (var item in Clients)
                        {
                            @event.SendTo(item.Value);
                        }
                    }
                }
            }

            ConnectManager.Instance.RemoveChatRoom(Id);
        }

        public void AddUser(string _userName, Socket _socket)
        {
            if (!Users.Contains(_userName))
            {
                Users.Add(_userName);
                Clients.Add(_userName, _socket);
                if (Thread.ThreadState == ThreadState.Unstarted)
                {
                    Thread.Start();
                }
                EventMessages.Enqueue(
                    EventMessage.SyncUserList(Users.ToArray()));
            }
        }

        private void RemoveUser(string _userName)
        {
            if (Users.Contains(_userName))
            {
                ConnectManager.Instance.CloseSocket(Clients[_userName]);
                Clients.Remove(_userName);
                Users.Remove(_userName);
                EventMessages.Enqueue(
                    EventMessage.SyncUserList(Users.ToArray()));
            }
        }
    }
}
