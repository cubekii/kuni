#include <gmock/gmock.h>
#include <range/v3/algorithm/any_of.hpp>

#include "AppBase.h"
#include "OpenAIChat.h"

#include "AppBase.h"
#include "OpenAITools.h"
#include "config.h"


namespace {
    class AppMock : public AppBase {
    public:
        AppMock() {
        }

        MOCK_METHOD(void, telegramPostMessage, (const AString& message), ());

    protected:
        void updateTools(OpenAITools& actions) override {
            actions.insert({
                .name = "send_telegram_message",
                .description = "Sends a message to the chat",
                .parameters =
                    {
                        .properties =
                            {
                                {"text", {.type = "string", .description = "Contents of the message"}},
                            },
                        .required = {"text"},
                    },
                .handler = [this](OpenAITools::Ctx ctx) -> AFuture<AString> {
                    const auto& object = ctx.args.asObjectOpt().valueOrException("object expected");
                    auto message = object["text"].asStringOpt().valueOrException("`text` string expected");
                    telegramPostMessage(message);
                    co_return "Message sent successfully.";
                },
            });
        }
    };
} // namespace

TEST(Diary, Basic) {
    APath("test_data").removeFileRecursive();
    AEventLoop loop;
    IEventLoop::Handle h(&loop);
    AAsyncHolder async;
    auto app = _new<AppMock>();

    async << app->passNotificationToAI(R"(
Today you read an article. Contents below.

The source character set of C source programs is contained within the 7-bit ASCII character set but is a superset of the
ISO 646-1983 Invariant Code Set. Trigraph sequences allow C programs to be written using only the ISO (International
Standards Organization) Invariant Code Set. Trigraphs are sequences of three characters (introduced by two consecutive
question marks) that the compiler replaces with their corresponding punctuation characters. You can use trigraphs in C
source files with a character set that does not contain convenient graphic representations for some punctuation
characters.

C++17 removes trigraphs from the language. Implementations may continue to support trigraphs as part of the
implementation-defined mapping from the physical source file to the basic source character set, though the standard
encourages implementations not to do so. Through C++14, trigraphs are supported as in C.

Visual C++ continues to support trigraph substitution, but it's disabled by default. For information on how to enable
trigraph substitution, see /Zc:trigraphs (Trigraphs Substitution).
)");
    while (async.size() > 0) {
        loop.iteration();
    }
    async << app->diaryDumpMessages();
    while (async.size() > 0) {
        loop.iteration();
    }

    app->diary().reload();

    if (!ranges::any_of(app->diary().list(), [&](const auto& text) { return text.freeformBody.contains("trigraphs"); })) {
        GTEST_FAIL() << "We expect LLM to save info about c++ trigraphs to the diary.";
    }
}

TEST(Diary, Remember) {
    APath("test_data").removeFileRecursive();
    AEventLoop loop;
    IEventLoop::Handle h(&loop);
    AAsyncHolder async;

    {
        auto app = _new<AppMock>();
        testing::InSequence s;
        ON_CALL(*app, telegramPostMessage(testing::_))
            .WillByDefault([](AString text) -> AFuture<> { co_return; });
        EXPECT_CALL(*app, telegramPostMessage(testing::_)).Times(testing::AtLeast(1));

        async << app->passNotificationToAI(R"(
You received a message from Alex2772 (chat_id=1):

Today I was playing several games of Dota 2. Both times I was playing Arc Warden and both times we lost
:( my teammates weren't bad though.
)");
        while (async.size() > 0) {
            loop.iteration();
        }
        async << app->diaryDumpMessages();
        while (async.size() > 0) {
            loop.iteration();
        }
        app->diary().reload();
        if (!ranges::any_of(app->diary().list(), [](const auto& i) { return i.freeformBody.lowercase().contains("warden"); })) {
            GTEST_FAIL() << "We expect LLM to save info about Arc Warden";
        }
    }

    // at this point, llm context is clean.
    // we'll sent a causal message referring Dota 2 but not referring Arc Warden.
    // we expect AI to remember Alex2772 plays Arc Warden.
    {
        auto app = _new<AppMock>();
        testing::InSequence s;
        bool called = false;
        async << app->passNotificationToAI(R"(
You received a message from Alex2772 (chat_id=1):

Today I won a match in Dota 2

Guess which hero I was playing :)
)");
        ON_CALL(*app, telegramPostMessage(testing::_))
            .WillByDefault([&](AString text) noexcept -> AFuture<> {
                const auto lower = text.lowercase();
                if (!(lower.contains("arc") && lower.contains("warden"))) {
                    throw AException("we expect AI to remember Arc Warden");
                }
                called = true;
                co_return;
            });

        EXPECT_CALL(*app, telegramPostMessage(testing::_)).Times(testing::AtLeast(1));

        while (async.size() > 0) {
            loop.iteration();
        }

        EXPECT_TRUE(called);
    }
}
