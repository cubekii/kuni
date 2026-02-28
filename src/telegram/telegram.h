#pragma once
#include "AUI/Common/AOptional.h"
#include "AUI/Image/AImage.h"
#include "AUI/Json/AJson.h"
#include "AUI/Thread/AFuture.h"

namespace telegram {
    using id_t = int64_t;
    using message_id_t = id_t;
    using chat_id_t = id_t;


    struct Message {
        chat_id_t chatId;
        AOptional<message_id_t> replyToMessageId;
        AOptional<message_id_t> messageThreadId;
        AString text;
        AOptional<AImage> image;
        AString parseMode;

        AJson::Array entities;

        struct ReplyMarkup {
            struct Button {
                AString text;
                AString callbackData;
            };
            AVector<AVector<Button>> buttons;
        };

        AOptional<ReplyMarkup> replyMarkup;
    };
    AFuture<message_id_t> postMessage(Message message);
    AFuture<message_id_t> editMessage(message_id_t messageId, Message message);
    AFuture<> deleteMessage(chat_id_t chatId, message_id_t messageId);
    AFuture<> placeReaction(chat_id_t chatId, message_id_t messageId,
                            AString reaction);

    AFuture<> answerCallbackQuery(AString queryId);

    AFuture<AJson> longPoll();
}
