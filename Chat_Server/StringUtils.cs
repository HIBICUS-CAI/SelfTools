using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Chat_Server
{
    internal class StringUtils
    {
        public static string AscByteToStr(byte[] _source, uint _start, uint _size)
        {
            char[] buffer = new char[_size];

            for (int i = 0; i < _size; i++)
            {
                buffer[i] = Convert.ToChar(_source[_start + i]);
            }

            return new string(buffer);
        }

        public static byte[] StrToAscByte(string _source)
        {
            byte[] data = new byte[_source.Length + 1];

            for (int i = 0; i < _source.Length; i++)
            {
                data[i] = Convert.ToByte(_source[i]);
            }

            return data;
        }
    }
}
