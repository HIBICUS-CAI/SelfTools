using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EncodeConverter;
using System.Threading.Tasks;

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
    }
}
