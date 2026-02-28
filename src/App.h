#pragma once
#include "AUI/Common/AObject.h"
#include "AUI/Thread/AAsyncHolder.h"
#include "OpenAITools.h"

class App: public AObject {
public:
    App();
    void run();

private:
    AAsyncHolder mAsync;
    OpenAITools mTools;

    AVector<OpenAIChat::Message> mMessages {};

    void setupLongPoll();

    void passEventToAI(AString notification);

};

