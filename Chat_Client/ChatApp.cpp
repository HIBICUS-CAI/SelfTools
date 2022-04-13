#include "ChatApp.h"
#include "ScreenBuffer.h"

bool ChatApp::Init()
{
    char cmd[1024] = "";
    int hei = APP_HEIGHT, wid = APP_WIDTH;
    std::sprintf(cmd, "mode con cols=%d lines=%d && cls", wid, hei);
    std::system(cmd);

    if (!ScreenBuffer::Instance()->Init()) { return false; }

    return true;
}

void ChatApp::Stop()
{
    ScreenBuffer::Instance()->Stop();
}
