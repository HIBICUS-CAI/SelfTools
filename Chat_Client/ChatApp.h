#pragma once

#include "AppBasicDef.h"
#include "UserMessage.h"

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

    uint mCurrMebNum = 0;
    std::vector<UserMessage> mAllMess = {};

    std::vector<std::string> mOnlineUser = { "a","ab","abc" };
};
