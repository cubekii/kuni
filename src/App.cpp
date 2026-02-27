//
// Created by alex2772 on 2/27/26.
//

#include "App.h"

#include "AUI/Logging/ALogger.h"
#include "AUI/Thread/AEventLoop.h"
#include "AUI/Thread/AThreadPool.h"
#include "AUI/Util/kAUI.h"
#include "OpenAIChat.h"
#include "config.h"
#include "telegram/telegram.h"

static constexpr auto LOG_TAG = "App";

App::App() {
    mTools.addTool({
        .name = "get_time",
        .description = "Retrieves the current time.",
        .parameters = {
            .properties = {
                {"timezone", { .type = "string", .description = "The timezone to use for the time." }},
            },
        },
    }, [](const AJson& json) {
        return "12:00 AM";
    });
}
void App::run() {
    AThread::current()->enqueue([this] {
        ALogger::info(LOG_TAG) << "Bot is up and running. Press enter to exit.";

        /*
        telegram::postMessage({
            .chatId = config::TECHNICAL_CHAT_ID,
            .text = "Bot is up and running.",
        });*/
    });

    setupLongPoll();

    AEventLoop loop;
    IEventLoop::Handle h(&loop);
    loop.loop();
}
void App::setupLongPoll() {
    mAsync << telegram::longPoll().onSuccess([this](const AJson& j) {
        mAsync << AUI_THREADPOOL {
            for (const auto& entry  : j.asArray()) {
                const auto& message = entry["message"];
                auto fromId = message["chat"]["id"].asLongInt();
                OpenAIChat chat {
                    .systemPrompt = config::SYSTEM_PROMPT,
                    .tools = mTools.asJson,
                };

                AVector<OpenAIChat::Message> messages {
                    {
                        .role = OpenAIChat::Message::Role::USER,
                        .content = message["text"].asString(),
                    },
                };

                naxyi:
                AFuture<OpenAIChat::Response> botAnswer = chat.chat(messages);

                if (botAnswer->choices.empty()) {
                    return;
                }
                messages << botAnswer->choices.at(0).message;
                if (!botAnswer->choices.at(0).message.tool_calls.empty()) {
                    messages << mTools.handleToolCalls(botAnswer->choices.at(0).message.tool_calls);
                    goto naxyi;
                }

                *telegram::postMessage({
                    .chatId = fromId,
                    .text = botAnswer->choices.at(0).message.content,
                    .parseMode = "",
                });
            }
        };
    }).onFinally([this] { setupLongPoll(); });

}

