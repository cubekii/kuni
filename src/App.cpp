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
        .name = "send_telegram_message",
        .description = "Sends a message to a Telegram user.",
        .parameters = {
            .properties = {
                {"chat_id", { .type = "integer", .description = "The ID of the Telegram chat" }},
                {"message", { .type = "string", .description = "Contents of the message" }},
            },
            .required = {"chat_id", "message"},
        },
    }, [](const AJson& args) -> AFuture<AString> {
        const auto& object = args.asObjectOpt().valueOrException("object expected");
        auto chatId = object["chat_id"].asLongIntOpt().valueOrException("`chat_id` integer expected");
        auto message = object["message"].asStringOpt().valueOrException("`message` string expected");
        co_await telegram::postMessage({
            .chatId = chatId,
            .text = message,
        });
        co_return "Message sent successfully.";
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
    mAsync << [](App& app) -> AFuture<> {
        AUI_DEFER { app.setupLongPoll(); };
        auto j = co_await telegram::longPoll();

        for (const auto& entry  : j.asArray()) {
            const auto& message = entry["message"];
            auto fromId = message["chat"]["id"].asLongInt();
            app.passEventToAI("You received a message from {} {} (chat_id = {}):\n\n{}"_format(message["from"]["first_name"].asString(), message["from"]["last_name"].asString(), fromId, message["text"].asString()));
        }
    }(*this);

}

/**
 * @brief Passes an event to the AI to process
 * @param notification notification text message in natural language (i.e., "you received a message from "...": ...; an
 * alarm triggerred, etc...)
 * @details
 * Think of it as your phone's notifications: you receive a notification, read it and (maybe) react to it.
 */
void App::passEventToAI(AString notification) {
    getThread()->enqueue([this, self = shared_from_this(), notification = std::move(notification)] {
        OpenAIChat chat {
            .systemPrompt = config::SYSTEM_PROMPT,
            .tools = mTools.asJson,
        };
        mMessages << OpenAIChat::Message{
            .role = OpenAIChat::Message::Role::USER,
            .content = std::move(notification),
        };

        naxyi:
        OpenAIChat::Response botAnswer = *chat.chat(mMessages);
        mMessages << botAnswer.choices.at(0).message;

        if (botAnswer.choices.empty() || botAnswer.choices.at(0).message.tool_calls.empty()) {
            return;
        }

        mMessages << *mTools.handleToolCalls(botAnswer.choices.at(0).message.tool_calls);
        mMessages.last().content += "\nWhat's your next action? Use a `tool` to act. The following tools available: " + AStringVector(mTools.handlers.keyVector()).join(", ");
        goto naxyi;
    });
}
