namespace EncodeConverter
{
    internal class Program
    {
        static void Main(string[] args)
        {
            foreach (string arg in args) { Console.WriteLine(arg); }
            Console.WriteLine(EncodingConverter.Instance.ToString());

            FileContext e65001 =
                new("TestData65001\\", "a.txt", DEFAULT_ENCODING.UTF8BOM);
            FileContext e936 =
                new("TestData936\\", "a.txt", DEFAULT_ENCODING.GB2312);
            FileContext e932 =
                new("TestData932\\", "a.txt", DEFAULT_ENCODING.SHIFTJIS);

            Console.WriteLine(
                $"{e65001}\nfile : {e65001.Directory}{e65001.FileName}\nwith :\n");
            foreach (string line in e65001.FileContent)
            {
                Console.WriteLine(line);
            }
            Console.WriteLine("\n\n\n");

            Console.WriteLine(
                $"{e936}\nfile : {e936.Directory}{e936.FileName}\nwith :\n");
            foreach (string line in e936.FileContent)
            {
                Console.WriteLine(line);
            }
            Console.WriteLine("\n\n\n");

            Console.WriteLine(
                $"{e932}\nfile : {e932.Directory}{e932.FileName}\nwith :\n");
            foreach (string line in e932.FileContent)
            {
                Console.WriteLine(line);
            }
            Console.WriteLine("\n\n\n");
        }
    }
}
