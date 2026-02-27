#pragma once
#include "AUI/Common/AObject.h"
#include "AUI/Thread/AAsyncHolder.h"
#include "OpenAITools.h"

class App: public AObject {
public:
    App();
    void run();

private:
    void setupLongPoll();

    AAsyncHolder mAsync;
    OpenAITools mTools;
};

