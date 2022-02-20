using EncodeConverter;

namespace ScratchPaper_00_CSharp_Temp
{
    internal class Program
    {
        static void Main(string[] args)
        {
            foreach (string arg in args) { Console.WriteLine(arg); }
            Console.WriteLine(EncodingConverter.Instance.ToString());
        }
    }
}
