namespace Chat_Server
{
    internal class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Hello, World!");
            foreach (string arg in args) { Console.WriteLine(arg); }

            ConnectManager.Instance.StartListenThread();

            //Thread.Sleep(1000);
            while (true)
            {

            }

            ConnectManager.Instance.ShouldStop = true;
            ConnectManager.Instance.JoinListenThread();
        }
    }
}
