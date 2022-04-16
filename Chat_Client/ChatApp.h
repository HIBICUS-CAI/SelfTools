#pragma once

#include "AppBasicDef.h"
#include "UserMessage.h"
#include "TcpSocket.h"

enum class MESS
{
    USER_LIST = 0,
    NEW_MESS = 1
};

class ChatApp
{
    DEFINE_SINGLETON(ChatApp);

public:
    bool Init();
    void Stop();

    uint RunLogIn();
    uint RunChatRoom();
    std::vector<std::string>& GetUserList();
    std::queue<UserMessage>& GetNeedSend();
    std::vector<UserMessage>& GetAllMess();

private:
    int mRoomID = 0;
    std::string mUserName = "";

    TcpSocketPtr mClientSocket = nullptr;
    HANDLE mChatProcessThread = nullptr;

    std::vector<std::string> mOnlineUser = { "a","ab","abc" };
    std::vector<UserMessage> mAllMess = {};
    std::queue<UserMessage> mForSendMess = {};
};

unsigned __stdcall ChatSocketProcess(void* _args);
