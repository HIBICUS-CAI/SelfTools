﻿using System.Text;

namespace EncodeConverter
{
    public enum DEFAULT_ENCODING
    {
        UTF8, GB2312, SHIFTJIS,

        SIZE
    }

    public class EncodingConverter
    {
        private static readonly EncodingConverter mConverter = new();

        private readonly Encoding[] mDefaultEncodings =
            new Encoding[(int)DEFAULT_ENCODING.SIZE];

        public static EncodingConverter Instance { get { return mConverter; } }

        public EncodingConverter()
        {
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
            mDefaultEncodings[(int)DEFAULT_ENCODING.UTF8] =
                Encoding.GetEncoding(65001);
            mDefaultEncodings[(int)DEFAULT_ENCODING.GB2312] =
                Encoding.GetEncoding(936);
            mDefaultEncodings[(int)DEFAULT_ENCODING.SHIFTJIS] =
                Encoding.GetEncoding(932);
        }

        public string[] ConvertEncodingToList(string[] _texts,
            DEFAULT_ENCODING _srcEncoding, DEFAULT_ENCODING _dstEncoding)
        {
            List<string> result = new();
            Encoding srcEncoding = mDefaultEncodings[(int)_srcEncoding];
            Encoding dstEncoding = mDefaultEncodings[(int)_dstEncoding];

            foreach (string line in _texts)
            {
                byte[] originByte = srcEncoding.GetBytes(line);
                byte[] convertByte = Encoding.Convert(srcEncoding,
                    dstEncoding, originByte);
                char[] convertChars = new char[dstEncoding.GetCharCount(
                    convertByte, 0, convertByte.Length)];
                dstEncoding.GetChars(convertByte, 0, convertByte.Length,
                    convertChars, 0);
                result.Add(new string(convertChars));
            }

            return result.ToArray();
        }
    }
}