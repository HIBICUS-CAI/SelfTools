#pragma once

#include "AppBasicDef.h"
#include "UserMessage.h"
#include "TcpSocket.h"

class ChatApp
{
    DEFINE_SINGLETON(ChatApp);

public:
    bool Init();
    void Stop();

    uint RunLogIn();
    uint RunChatRoom();

private:
    int mRoomID = 0;
    std::string mUserName = "";

    TcpSocketPtr mClientSocket = nullptr;

    std::vector<std::string> mOnlineUser = { "a","ab","abc" };
    std::vector<UserMessage> mAllMess = {};
};
