#pragma once

#include "AppBasicDef.h"

constexpr uint SCRBUF_SIZE = APP_WIDTH * APP_HEIGHT + 1;

class ScreenBuffer
{
    DEFINE_SINGLETON(ScreenBuffer);

public:
    bool Init();
    void Stop();

    void PrintOut();
    void ClearScreen();

    void ClearBuffer();
    void WriteLineTo(const char* _string, uint _lineIndex);
    void SetCursorToLastLineAt(short _position);

private:
    char* mScreenBuffer = nullptr;
};
