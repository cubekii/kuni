//
// Created by alex2772 on 2/27/26.
//

#include "AppBase.h"

#include <range/v3/algorithm/remove_if.hpp>

#include "AUI/Logging/ALogger.h"
#include "AUI/Thread/AEventLoop.h"
#include "AUI/Thread/AThreadPool.h"
#include "AUI/Util/kAUI.h"
#include "OpenAIChat.h"
#include "config.h"

static constexpr auto LOG_TAG = "App";

AppBase::AppBase() {
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
    }, [this](const AJson& args) -> AFuture<AString> {
        const auto& object = args.asObjectOpt().valueOrException("object expected");
        auto chatId = object["chat_id"].asLongIntOpt().valueOrException("`chat_id` integer expected");
        auto message = object["message"].asStringOpt().valueOrException("`message` string expected");
        co_await telegramPostMessage(chatId, message);
        co_return "Message sent successfully.";
    });
}

void AppBase::run() {
    AThread::current()->enqueue([this] {
        ALogger::info(LOG_TAG) << "Bot is up and running. Press enter to exit.";
    });
    _new<AThread>([this, self = shared_from_this()] {
        std::cin.get();
        mEventLoop.stop();
        mEventLoop.notifyProcessMessages();
    })->start();

    telegramSetupLongPoll();

    IEventLoop::Handle h(&mEventLoop);
    mEventLoop.loop();

    ALogger::info(LOG_TAG) << "Bot is shutting down. Please give some time to dump remaining context";
    dairyDumpMessages();
    AThread::processMessages();
}

/**
 * @brief Passes an event to the AI to process
 * @param notification notification text message in natural language (i.e., "you received a message from "...": ...; an
 * alarm triggerred, etc...)
 * @details
 * Think of it as your phone's notifications: you receive a notification, read it and (maybe) react to it.
 */
void AppBase::passEventToAI(AString notification) {
    getThread()->enqueue([this, self = shared_from_this(), notification = std::move(notification)]() mutable {
        mTemporaryContext << OpenAIChat::Message{
            .role = OpenAIChat::Message::Role::USER,
            .content = std::move(notification),
        };

        ranges::remove_if(*mCachedDairy, [&](const AString& entry) {
            if (dairyEntryIsRelatedToCurrentContext(entry)) {
                mTemporaryContext.last().content += "\n<kumi>\nAfter reading this notification, it reminded me my dairy page:\n" + entry + "\n</kumi>\n";
                return true;
            }
            return false;
        });

        OpenAIChat chat {
            .systemPrompt = config::SYSTEM_PROMPT,
            .tools = mTools.asJson,
        };

        naxyi:
        OpenAIChat::Response botAnswer = *chat.chat(mTemporaryContext);
        mTemporaryContext << botAnswer.choices.at(0).message;

        if (botAnswer.choices.empty() || botAnswer.choices.at(0).message.tool_calls.empty()) {
            if (botAnswer.usage.total_tokens >= config::DAIRY_TOKEN_COUNT_TRIGGER) {
                dairyDumpMessages();
            }
            return;
        }

        mTemporaryContext << *mTools.handleToolCalls(botAnswer.choices.at(0).message.tool_calls);
        mTemporaryContext.last().content += "\nWhat's your next action? Use a `tool` to act. The following tools available: " + AStringVector(mTools.handlers.keyVector()).join(", ");
        goto naxyi;
    });
}

void AppBase::dairyDumpMessages() {
    getThread()->enqueue([this, self = shared_from_this()] {
        AUI_DEFER { mCachedDairy.reset(); };
        if (mTemporaryContext.empty()) {
            return;
        }
        mTemporaryContext << OpenAIChat::Message{
            .role = OpenAIChat::Message::Role::USER,
            .content = config::DAIRY_PROMPT,
        };

        OpenAIChat chat {
            .systemPrompt = config::SYSTEM_PROMPT,
            // .tools = mTools.asJson, // no tools should be involved.
        };
        naxyi:
        OpenAIChat::Response botAnswer = *chat.chat(mTemporaryContext);
        if (botAnswer.choices.at(0).message.content.empty()) {
            goto naxyi;
        }
        dairySave(botAnswer.choices.at(0).message.content);
        mTemporaryContext.clear();
    });
}

bool AppBase::dairyEntryIsRelatedToCurrentContext(const AString& dairyEntry) {
    if (dairyEntry.empty()) {
        return false;
    }
    AString basePrompt = config::DAIRY_IS_RELATED_PROMPT;
    basePrompt += "\n<context>\n";
    AUI_ASSERT(!mTemporaryContext.empty());
    for (const auto& message: mTemporaryContext) {
        basePrompt += message.content + "\n\n";
    }
    basePrompt += "</context>\n";
    OpenAIChat chat {
        .systemPrompt = std::move(basePrompt),
    };
    auto decition = chat.chat(dairyEntry)->choices.at(0).message.content.lowercase();
    return decition.contains("yes") || decition.contains("y") || decition.contains("true") || decition.contains("1") || decition.contains("maybe");
}
