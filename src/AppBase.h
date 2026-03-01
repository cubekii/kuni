#pragma once
#include "AUI/Common/AObject.h"
#include "AUI/Thread/AAsyncHolder.h"
#include "AUI/Thread/AEventLoop.h"
#include "OpenAITools.h"

class AppBase: public AObject {
public:
    AppBase();
    void run();

    void passEventToAI(AString notification);

    void dairyDumpMessages();

protected:
    AAsyncHolder mAsync;
    virtual AFuture<> telegramPostMessage(int64_t chatId, AString text) {
        ALogger::warn("AppBase") << "telegramPostMessage stub (" << chatId << ", " << text << ")";
        co_return;
    }

    virtual void telegramSetupLongPoll() {}

    /**
     * @brief Returns dairy entries that was saved previously by dairySave.
     */
    virtual AVector<AString> dairyRead() const {
        return {};
    }

    virtual void dairySave(const AString& message) {
        ALogger::warn("AppBase") << "dairySave stub (" << message << ")";
    }

    virtual bool dairyEntryIsRelatedToCurrentContext(const AString& dairyEntry);

private:
    OpenAITools mTools;
    aui::lazy<AVector<AString>> mCachedDairy = [this]{ return dairyRead(); };
    AEventLoop mEventLoop;

    AVector<OpenAIChat::Message> mTemporaryContext {};


};

