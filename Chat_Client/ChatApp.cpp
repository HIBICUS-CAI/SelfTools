#include "ChatApp.h"
#include "ScreenBuffer.h"
#include "LogUtils.h"
#include "SocketUtils.h"
#include "SocketAddress.h"

#define sb ScreenBuffer::Instance()
#define su SocketUtils

static bool g_IsInRoom = false;
static CRITICAL_SECTION g_DataLock = {};

#define lockon EnterCriticalSection(&g_DataLock)
#define unlock LeaveCriticalSection(&g_DataLock)

#define RECV(_SOCKET_PTR, _START_POS, _HAS_READ, _NEED_SIZE)\
while (_HAS_READ < _NEED_SIZE)\
{\
    _HAS_READ += _SOCKET_PTR->Receive(_START_POS + _HAS_READ,\
        _NEED_SIZE - static_cast<size_t>(_HAS_READ));\
}

bool ChatApp::Init()
{
    char cmd[1024] = "";
    int hei = APP_HEIGHT, wid = APP_WIDTH;
    std::sprintf(cmd, "mode con cp select=936 && mode con cols=%d lines=%d && cls", wid, hei);
    std::system(cmd);

    InitializeCriticalSection(&g_DataLock);

    if (!sb->Init()) { return false; }
    if (!su::SockInit()) { return false; }

    return true;
}

void ChatApp::Stop()
{
    DeleteCriticalSection(&g_DataLock);

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

            g_IsInRoom = true;
            mChatProcessThread = (HANDLE)_beginthreadex(nullptr, 0,
                ChatSocketProcess, &mClientSocket, 0, nullptr);
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

        lockon;
        std::string headStr = "本房间当前共" +
            std::to_string(mOnlineUser.size()) + "人在线";
        unlock;
        sb->WriteLineTo(headStr.c_str(), 0);
        sb->WriteLineTo("--------------------------------------------------"\
            "--------------------------------------------------"\
            "--------------------", 1);

        uint lowStart = 27;
        lockon;
        for (auto& mess : mAllMess)
        {
            lowStart = mess.PrintToBuffer(lowStart);
            if (lowStart == 2) { break; }
        }
        unlock;

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
            g_IsInRoom = false;
            WaitForSingleObject(mChatProcessThread, INFINITE);
            mClientSocket->~TcpSocket();
            return 0;
        }
        else if (std::string(inputStr) == "/MEMBER")
        {
            std::string list = "";
            lockon;
            for (auto& name : mOnlineUser)
            {
                list += (name + " ");
            }

            mAllMess.insert(mAllMess.begin(),
                UserMessage("MEMBER LIST", list));
            unlock;
        }
        else
        {
            //mAllMess.insert(mAllMess.begin(), UserMessage(mUserName, inputStr));
            lockon;
            mForSendMess.push(UserMessage(mUserName, inputStr));
            unlock;
        }
    }

    return 0;
}

std::vector<std::string>& ChatApp::GetUserList()
{
    return mOnlineUser;
}

std::queue<UserMessage>& ChatApp::GetNeedSend()
{
    return mForSendMess;
}

std::vector<UserMessage>& ChatApp::GetAllMess()
{
    return mAllMess;
}

unsigned __stdcall ChatSocketProcess(void* _args)
{
    assert(_args);
    TcpSocketPtr socket = *((TcpSocketPtr*)_args);
    char* recvBuffer = new char[1024];
    char* sendBuffer = new char[1024];
    assert(recvBuffer);
    assert(sendBuffer);

    while (g_IsInRoom)
    {
        lockon;
        auto& sendQueue = ChatApp::Instance()->GetNeedSend();
        while (!sendQueue.empty())
        {
            int messType = (int)(MESS::NEW_MESS);
            auto& mess = sendQueue.front();
            std::string name = mess.GetName();
            int nameLen = static_cast<int>(name.length());
            std::string text = mess.GetText();
            int textLen = static_cast<int>(text.length());
            sendQueue.pop();

            std::memset(sendBuffer, 0, 1024);
            auto startPos = sendBuffer;
            std::memcpy(startPos, &messType, sizeof(int));
            startPos += sizeof(int);
            std::memcpy(startPos, &nameLen, sizeof(int));
            startPos += sizeof(int);
            std::memcpy(startPos, name.c_str(), nameLen);
            startPos += nameLen;
            std::memcpy(startPos, &textLen, sizeof(int));
            startPos += sizeof(int);
            std::memcpy(startPos, text.c_str(), textLen);

            size_t allByte = (sizeof(int) * 3) + nameLen + textLen;
            socket->Send(sendBuffer, allByte);
        }
        unlock;

        std::vector<TcpSocketPtr> checkRecv = {};
        std::vector<TcpSocketPtr> couldRecv = {};
        checkRecv.push_back(socket);
        int result = SocketUtils::Select(&checkRecv, &couldRecv,
            nullptr, nullptr, nullptr, nullptr);
        if (!result) { continue; }

        int firstFlagByte = socket->Receive(recvBuffer, 4);
        if (!firstFlagByte) { continue; }
        RECV(socket, recvBuffer, firstFlagByte, 4);
        MESS messFlg = static_cast<MESS>(*((int*)recvBuffer));
        switch (messFlg)
        {
        case MESS::USER_LIST:
        {
            lockon;
            std::memset(recvBuffer, '\0', 1024);
            ChatApp::Instance()->GetUserList().clear();
            int userSizeByte = socket->Receive(recvBuffer, 4);
            RECV(socket, recvBuffer, userSizeByte, 4);
            int userSize = *((int*)recvBuffer);

            for (int i = 0; i < userSize; i++)
            {
                int nameLenByte = socket->Receive(recvBuffer, 4);
                RECV(socket, recvBuffer, nameLenByte, 4);
                int nameLen = *((int*)recvBuffer);
                char tempName[32] = "";
                int nameByte = socket->Receive(tempName, nameLen);
                RECV(socket, tempName, nameByte, nameLen);
                std::string userName = tempName;
                ChatApp::Instance()->GetUserList().push_back(userName);
                socket->Receive(recvBuffer, 1); // 读取最后1字节
            }
            unlock;
            break;
        }
        case MESS::NEW_MESS:
        {
            lockon;
            auto& allMessVec = ChatApp::Instance()->GetAllMess();
            std::memset(recvBuffer, '\0', 1024);

            int nameLenByte = socket->Receive(recvBuffer, 4);
            RECV(socket, recvBuffer, nameLenByte, 4);
            int nameLen = *((int*)recvBuffer);
            std::memset(recvBuffer, '\0', 1024);
            int nameByte = socket->Receive(recvBuffer, nameLen);
            RECV(socket, recvBuffer, nameByte, nameLen);
            std::string nameStr = std::string(recvBuffer);
            std::memset(recvBuffer, '\0', 1024);

            int textLenByte = socket->Receive(recvBuffer, 4);
            RECV(socket, recvBuffer, textLenByte, 4);
            int textLen = *((int*)recvBuffer);
            std::memset(recvBuffer, '\0', 1024);
            int textByte = socket->Receive(recvBuffer, textLen);
            RECV(socket, recvBuffer, textByte, textLen);
            std::string textStr = std::string(recvBuffer);
            std::memset(recvBuffer, '\0', 1024);

            allMessVec.insert(allMessVec.begin(), UserMessage(nameStr, textStr));
            unlock;
            break;
        }
        default:
            break;
        }
    }

    delete[] sendBuffer;
    delete[] recvBuffer;
    return 0;
}
