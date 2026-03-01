#include "AUI/IO/AFileInputStream.h"
#include "AUI/Platform/Entry.h"
#include "AUI/Util/kAUI.h"
#include "AppBase.h"
#include "telegram/telegram.h"

namespace {

constexpr auto DAIRY_DIR = "dairy";

class App: public AppBase {
public:
    using AppBase::AppBase;

protected:
    AFuture<> telegramPostMessage(int64_t chatId, AString text) override {
        co_await telegram::postMessage({.chatId = chatId, .text =  std::move(text) });
    }


    void telegramSetupLongPoll() override {
        mAsync << [](App& app) -> AFuture<> {
            AUI_DEFER { app.telegramSetupLongPoll(); };
            auto j = co_await telegram::longPoll();

            for (const auto& entry  : j.asArray()) {
                const auto& message = entry["message"];
                auto fromId = message["chat"]["id"].asLongInt();
                app.passEventToAI("You received a message from {} {} (chat_id = {}):\n\n{}"_format(message["from"]["first_name"].asString(), message["from"]["last_name"].asString(), fromId, message["text"].asString()));
            }
        }(*this);

    }

    AVector<AString> dairyRead() const override {
        APath dairyDir(DAIRY_DIR);
        if (!dairyDir.isDirectoryExists()) {
            return {};
        }
        AVector<AString> dairy;
        for (const auto& file: dairyDir.listDir()) {
            if (file.isRegularFileExists() && file.extension() == "md") {
                dairy << AString::fromUtf8(AByteBuffer::fromStream(AFileInputStream(file)));
            }
        }
        return dairy;
    }

    void dairySave(const AString& message) override {
        APath dairyDir(DAIRY_DIR);
        dairyDir.makeDirs();
        auto dairyFile = dairyDir / "{}.md"_format(std::chrono::system_clock::now().time_since_epoch().count());
        AFileOutputStream(dairyFile) << message;
    }
};
}


AUI_ENTRY {
    using namespace std::chrono_literals;
    auto app = _new<App>();
    app->run();

    return 0;
}
