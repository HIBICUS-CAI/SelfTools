using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net.Sockets;

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
                ROOM_LOGIN = 0,
                ROOM_QUIT = 1,
                NEW_MESS = 2
            }

            public SendMessType Type { get; }
            public byte[] Data { get; set; }

            public EventMessage(SendMessType _messType, uint _dataSize)
            {
                Type = _messType;
                Data = new byte[_dataSize];
            }

            public static EventMessage CreateLogIn(string _userName)
            {
                EventMessage @event = new(SendMessType.ROOM_LOGIN,
                    (uint)_userName.Length);

                return @event;
            }

            public static EventMessage CreateQuit(string _userName)
            {
                EventMessage @event = new(SendMessType.ROOM_QUIT,
                    (uint)_userName.Length);

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
                //_client.Send(Data);
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
            while (Users.Any())
            {
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
                EventMessages.Enqueue(EventMessage.CreateLogIn(_userName));
            }
        }

        private void RemoveUser(string _userName)
        {
            if (Users.Contains(_userName))
            {
                ConnectManager.Instance.CloseSocket(Clients[_userName]);
                Clients.Remove(_userName);
                Users.Remove(_userName);
                if (!Users.Any())
                {
                    Thread.Join();
                    ConnectManager.Instance.RemoveChatRoom(Id);
                }
                EventMessages.Enqueue(EventMessage.CreateQuit(_userName));
            }
        }
    }
}
