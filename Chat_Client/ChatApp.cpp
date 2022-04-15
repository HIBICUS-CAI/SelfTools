#include "ChatApp.h"
#include "ScreenBuffer.h"
#include "LogUtils.h"
#include "SocketUtils.h"
#include "SocketAddress.h"

#define sb ScreenBuffer::Instance()
#define su SocketUtils

bool ChatApp::Init()
{
    char cmd[1024] = "";
    int hei = APP_HEIGHT, wid = APP_WIDTH;
    std::sprintf(cmd, "mode con cp select=936 && mode con cols=%d lines=%d && cls", wid, hei);
    std::system(cmd);

    if (!sb->Init()) { return false; }
    if (!su::SockInit()) { return false; }

    return true;
}

void ChatApp::Stop()
{
    su::SocketStop();
    sb->Stop();
}

uint ChatApp::RunLogIn()
{
    mAllMess.clear();

    sb->ClearBuffer();
    sb->WriteLineTo("                    于下方输入栏中填入房间号码（无空格）", 13);
    sb->WriteLineTo("--------------------------------------------------"\
        "--------------------------------------------------"\
        "--------------------", 28);
    sb->WriteLineTo("房间ID:", 29);
    sb->PrintOut();
    sb->SetCursorToLastLineAt(7);

    char inputStr[128] = "";
    int input = std::scanf("%s", inputStr);
    if (input == 1) { mRoomID = std::atoi(inputStr); }
    else { return 1; }

    sb->ClearBuffer();
    sb->WriteLineTo("                    于下方输入栏中填入用户名（16byte，无空格）", 13);
    sb->WriteLineTo("--------------------------------------------------"\
        "--------------------------------------------------"\
        "--------------------", 28);
    sb->WriteLineTo("用户名:", 29);
    sb->PrintOut();
    sb->SetCursorToLastLineAt(7);

    input = std::scanf("%s", inputStr);
    if (input == 1)
    {
        mUserName = inputStr;
        mUserName = mUserName.substr(0,
            mUserName.length() > 16 ? 16 : mUserName.length());

        mClientSocket = su::CreateTcpSocket(ADDRFAM::INET);
        auto addr = CreateIPv4FromString("192.168.0.125:32580");
        if (!mClientSocket || !addr) { return 1; }
        if (!mClientSocket->Connect(*addr))
        {
            size_t byteSize = sizeof(mRoomID) + sizeof(char) +
                mUserName.length();
            char buffer[32] = "";
            char nameLen = static_cast<char>(mUserName.length());
            std::memcpy(buffer, &mRoomID, sizeof(int));
            std::memcpy(buffer + sizeof(int), &nameLen, sizeof(char));
            std::memcpy(buffer + sizeof(char) + sizeof(int),
                mUserName.c_str(), nameLen);

            mClientSocket->Send(buffer, byteSize);

            std::strcpy(buffer, "");
            mClientSocket->Receive(buffer, 32);
            printf("%s", buffer);
            Sleep(5000);
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

uint ChatApp::RunChatRoom()
{
    while (true)
    {
        sb->ClearBuffer();

        std::string headStr = "本房间当前共" +
            std::to_string(mOnlineUser.size()) + "人在线";
        sb->WriteLineTo(headStr.c_str(), 0);
        sb->WriteLineTo("--------------------------------------------------"\
            "--------------------------------------------------"\
            "--------------------", 1);

        uint lowStart = 27;
        for (auto& mess : mAllMess)
        {
            lowStart = mess.PrintToBuffer(lowStart);
            if (lowStart == 2) { break; }
        }

        sb->WriteLineTo("--------------------------------------------------"\
            "--------------------------------------------------"\
            "--------------------", 28);

        std::string sendStr = "[" + mUserName + "]" + ":";
        sb->WriteLineTo(sendStr.c_str(), 29);
        sb->PrintOut();
        sb->SetCursorToLastLineAt(static_cast<short>(sendStr.size()));

        char inputStr[256] = "";
        int input = std::scanf("%s", inputStr);

        if (std::string(inputStr) == "/QUIT")
        {
            mClientSocket.reset();
            return 0;
        }
        else if (std::string(inputStr) == "/MEMBER")
        {
            std::string list = "";
            for (auto& name : mOnlineUser)
            {
                list += (name + " ");
            }

            mAllMess.insert(mAllMess.begin(),
                UserMessage("MEMBER LIST", list));
        }
        else
        {
            mAllMess.insert(mAllMess.begin(), UserMessage(mUserName, inputStr));
        }
    }

    return 0;
}
