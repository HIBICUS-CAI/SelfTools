#include "ScreenBuffer.h"

bool ScreenBuffer::Init()
{
    mScreenBuffer = new char[SCRBUF_SIZE];
    if (!mScreenBuffer) { return false; }
    else { ClearBuffer(); return true; }
}

void ScreenBuffer::Stop()
{
    delete[] mScreenBuffer;
    mScreenBuffer = nullptr;
}

void ScreenBuffer::PrintOut()
{
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), { 0,0 });
    std::printf("%s", mScreenBuffer);
}

void ScreenBuffer::ClearScreen()
{
    std::system("cls");
}

void ScreenBuffer::ClearBuffer()
{
    for (uint i = 0; i < SCRBUF_SIZE; i++) { mScreenBuffer[i] = ' '; }
    mScreenBuffer[SCRBUF_SIZE - 1] = '\0';
}

void ScreenBuffer::WriteLineTo(const char* _string, uint _lineIndex)
{
    std::memcpy(
        mScreenBuffer +
        static_cast<int64_t>(APP_WIDTH) * static_cast<int64_t>(_lineIndex),
        _string, strlen(_string));
}

void ScreenBuffer::SetCursorToLastLineAt(short _position)
{
    COORD pos = { _position,static_cast<short>(APP_HEIGHT - 1) };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}
