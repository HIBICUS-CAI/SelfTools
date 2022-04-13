#pragma once

#include "BasicDef.h"
#include <cstdio>

namespace LogUtils
{
    inline std::wstring ConvertToWString(std::string& _source)
    {
        using convert_typeX = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_typeX, wchar_t> converterX;

        return converterX.from_bytes(_source);
    }

    inline std::string ConvertToString(std::wstring& _source)
    {
        using convert_typeX = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_typeX, wchar_t> converterX;

        return converterX.to_bytes(_source);
    }

    inline std::string GetCommandLineArg(int _index)
    {
        if (_index < __argc) { return std::string(__argv[_index]); }
        else { return std::string(); }
    }

    inline std::string Sprintf(const char* _format, ...)
    {
        char temp[1024];

        va_list args;
        va_start(args, _format);
        _vsnprintf_s(temp, 1024, 1024, _format, args);

        return std::string(temp);
    }

    inline void Log(const char* _format, ...)
    {
        char temp[1024];

        va_list args;
        va_start(args, _format);
        _vsnprintf_s(temp, 1024, 1024, _format, args);

        std::string sLog = "";
        std::wstring wLog = L"";
        sLog = Sprintf("%s\n", temp);
        wLog = ConvertToWString(sLog);

        OutputDebugString(wLog.c_str());
        std::printf("%s", sLog.c_str());
    }
}

#define PLOG(...) LogUtils::Log(__VA_ARGS__);
