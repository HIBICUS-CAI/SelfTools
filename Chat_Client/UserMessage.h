#pragma once

#include "AppBasicDef.h"
#include "ScreenBuffer.h"

#define sb ScreenBuffer::Instance()

class UserMessage
{
public:
    UserMessage(std::string _user, std::string _message) :
        mUserName(_user)
    {
        size_t length = _message.length();
        size_t end = (size_t)APP_WIDTH - 22;
        std::string sub = "";

        for (size_t start = 0; start < length;)
        {
            if (end > length)
            {
                sub = _message.substr(start, length - start);
                mMessageVec.push_back(sub);

                break;
            }
            else
            {
                sub = _message.substr(start, (size_t)APP_WIDTH - 22);
                mMessageVec.push_back(sub);

                start = end;
                end = end + (size_t)APP_WIDTH - 22;
            }
        }

        std::reverse(mMessageVec.begin(), mMessageVec.end());
    }

    ~UserMessage() {}

    uint PrintToBuffer(uint _endLineIndex)
    {
        std::string sendFromStr = "[" + mUserName + "]:";
        for (size_t i = sendFromStr.length(); i < 20; i++)
        {
            if (i % 2)
            {
                sendFromStr = " " + sendFromStr;
            }
            else
            {
                sendFromStr = sendFromStr + " ";
            }
        }

        std::string mess = "";
        size_t i = 0;
        for (; i < mMessageVec.size(); i++)
        {
            if ((_endLineIndex - i) <= 2)
            {
                mess = sendFromStr + mMessageVec[i];
                sb->WriteLineTo(mess.c_str(), 2);
                break;
            }
            else if (i == (mMessageVec.size() - 1))
            {
                mess = sendFromStr + mMessageVec[i];
                sb->WriteLineTo(mess.c_str(), _endLineIndex - (uint)i);
            }
            else
            {
                mess = "                    " + mMessageVec[i];
                sb->WriteLineTo(mess.c_str(), _endLineIndex - (uint)i);
            }
        }

        return static_cast<uint>(_endLineIndex - i);
    }

private:
    std::string mUserName;
    std::vector<std::string> mMessageVec;
};
