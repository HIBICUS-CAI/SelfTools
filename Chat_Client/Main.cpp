#include "AppBasicDef.h"
#include "ChatApp.h"
#include "ScreenBuffer.h"

int main()
{
    if (ChatApp::Instance()->Init())
    {
        while (true)
        {
            ScreenBuffer::Instance()->ClearBuffer();
            static int i = 0;
            ScreenBuffer::Instance()->WriteLineTo("wocao hahaha", i++);
            if (i == 30) { i = 0; }
            ScreenBuffer::Instance()->PrintOut();
            ScreenBuffer::Instance()->SetCursorToLastLineAt(20);
            Sleep(100);
        }

        ChatApp::Instance()->Stop();
    }

    return 0;
}
