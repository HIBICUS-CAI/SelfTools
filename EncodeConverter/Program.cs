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

            e65001.OutputFileContext();
            e936.OutputFileContext();
            e932.OutputFileContext();

            string[] pat = new string[] { "*.json", "*.hlsl", "*.txt" };
            FolderReader.Instance.GetFilesInFolder("TestData65001", pat, true);
            FolderReader.Instance.OutPutFoundFiles();
            FolderReader.Instance.GetFilesInFolder("TestData936", pat, true);
            FolderReader.Instance.OutPutFoundFiles();
            FolderReader.Instance.GetFilesInFolder("TestData932", pat, false);
            FolderReader.Instance.OutPutFoundFiles();
        }
    }
}
