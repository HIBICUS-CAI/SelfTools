#pragma once

#include "AppBasicDef.h"

class ChatApp
{
    DEFINE_SINGLETON(ChatApp);

public:
    bool Init();
    void Stop();
};
