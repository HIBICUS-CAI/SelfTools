using System.Text;

namespace EncodeConverter
{
    public class FileContext
    {
        private readonly string mDirectoryName;
        private readonly string mFileName;
        private readonly DEFAULT_ENCODING mEncoding;
        private readonly string[] mFileContent;

        public string Directory { get { return mDirectoryName; } }
        public string FileName { get { return mFileName; } }
        public DEFAULT_ENCODING Encoding { get { return mEncoding; } }
        public string[] FileContent { get { return mFileContent; } }

        public FileContext(string _directory, string _file,
            DEFAULT_ENCODING _encoding)
        {
            mDirectoryName = _directory;
            mFileName = _file;
            mEncoding = _encoding;

            string filePath = _directory + _file;
            Encoding encoding = EncodingConverter.Instance.
                DEncoding[(int)mEncoding];
            mFileContent = File.ReadAllLines(filePath, encoding);
        }

        public FileContext(string _directory, string _file, string[] _content,
            DEFAULT_ENCODING _encoding)
        {
            mDirectoryName = _directory;
            mFileName = _file;
            mFileContent = _content;
            mEncoding = _encoding;
        }

        public void OutputFileContext()
        {
            Console.WriteLine($"┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" +
                $"┃ {this}\n" +
                $"┃ File : {mDirectoryName}{mFileName}\n" +
                $"┃ Encoding : {mEncoding}\n" +
                $"┃ Content : ↓\n" +
                $"┃ =============================");
            foreach (string line in mFileContent)
            {
                Console.WriteLine($"┃ {line}");
            }
            Console.WriteLine("┃ [EOF]\n┃ =============================\n" +
                "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        }

        public void SaveFileContextToDisk()
        {
            Encoding encoding = EncodingConverter.Instance.
                DEncoding[(int)mEncoding];
            File.WriteAllLines($"{mDirectoryName}{mFileName}",
                mFileContent, encoding);
        }

        public FileContext CreateContextEncodedContent(
            string _newDirectory, DEFAULT_ENCODING _dstEncoding)
        {
            string[] dstContent = EncodingConverter.Instance.ConvertEncoding(
                mFileContent, mEncoding, _dstEncoding);
            FileContext result = new(_newDirectory, mFileName, dstContent,
                _dstEncoding);
            return result;
        }

        public FileContext CreateContextEncodedFileName(
            string _newDirectory, DEFAULT_ENCODING _dstEncoding)
        {
            string dstFileName = EncodingConverter.Instance.ConvertEncoding(
                mFileName, mEncoding, _dstEncoding);
            FileContext result = new(_newDirectory, dstFileName, mFileContent,
                _dstEncoding);
            return result;
        }

        public FileContext CreateContextEncodedAll(
            string _newDirectory, DEFAULT_ENCODING _dstEncoding)
        {
            string dstFileName = EncodingConverter.Instance.ConvertEncoding(
                mFileName, mEncoding, _dstEncoding);
            string[] dstContent = EncodingConverter.Instance.ConvertEncoding(
                mFileContent, mEncoding, _dstEncoding);
            FileContext result = new(_newDirectory, dstFileName, dstContent,
                _dstEncoding);
            return result;
        }
    }
}
