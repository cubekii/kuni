#include "telegram.h"
#include "AUI/Common/AException.h"
#include "AUI/Common/AString.h"
#include "AUI/IO/AFileInputStream.h"
#include "AUI/Json/AJson.h"
#include "AUI/Json/Conversion.h"
#include "AUI/Logging/ALogger.h"
#include "AUI/Thread/AFuture.h"
#include "AUI/Util/kAUI.h"
#include <AUI/Util/APreprocessor.h>
#include <AUI/Curl/ACurl.h>
#include <AUI/Thread/AAsyncHolder.h>
#include <AUI/Thread/AComplexFutureOperation.h>
#include <AUI/Curl/AFormMultipart.h>
#include <AUI/Image/png/PngImageLoader.h>
#include <AUI/IO/AStrongByteBufferInputStream.h>
#include <cstdint>

using namespace std::chrono_literals;

static constexpr auto LOG_TAG = "Telegram";
static constexpr auto TOKEN = AUI_PP_STRINGIZE(TELEGRAM_API_TOKEN);

static AAsyncHolder& telegramAsync() {
    static AAsyncHolder h;
    return h;
}

static AString formatMethodUrl(const char* method) {
    return "https://api.telegram.org/bot{}/{}"_format(TOKEN, method);
}

struct LongPoll {
public:
    int64_t lastUpdateId;
};
AJSON_FIELDS(LongPoll,
    (lastUpdateId, "lastUpdateId")
)

AJSON_FIELDS(telegram::Message::ReplyMarkup::Button,
    (text, "text")
    (callbackData, "callback_data")
)

AFuture<AJson> telegram::longPoll() {
    static int64_t lastUpdateId = []() -> int64_t {
        try {
            auto lp = aui::from_json<LongPoll>(AJson::fromStream(AFileInputStream("telegram_longpoll.json")));
            return lp.lastUpdateId;
        } catch (const AException& e) {
            ALogger::err(LOG_TAG) << "Error loading longpoll cache: " << e;
        } catch (...) {
        }
        return 0;
    }();
    ACurl::Builder builder(formatMethodUrl("getUpdates"));
    builder.withParams({
        {"offset", AString::number(lastUpdateId)},
        {"timeout", "60"},
    });
    builder.withTimeout(70s);

    auto operation = _new<AComplexFutureOperation<AJson>>();

    operation << builder.runAsync().onSuccess([operation](ACurl::Response r) {
        try {
            ALOG_DEBUG(LOG_TAG) << "getUpdates: " << AString::fromUtf8(r.body);
            auto result = AJson::fromBuffer(r.body)["result"].asArray();
            if (!result.empty()) {
                lastUpdateId = result.last()["update_id"].asLongInt() + 1;
            }
            AFileOutputStream("telegram_longpoll.json") << aui::to_json(LongPoll { .lastUpdateId = lastUpdateId });

            operation->supplyValue(std::move(result));
        } catch (const AException& e) {
            operation->supplyException();
            ALogger::err(LOG_TAG) << "Error processing longpoll: " << e;
        }
    });

    return operation->makeFuture();
}

static AFormMultipart fillMultiparmWithMessage(telegram::Message& message) {
    AFormMultipart params = {
        {"chat_id", { AString::number(message.chatId) }, },
    };
    if (!message.parseMode.empty()) {
        params["parse_mode"] = {message.parseMode};

    }

    if (!message.entities.empty()) {
        params["entities"] = {AJson::toString(message.entities)};
    }

    if (message.replyToMessageId) {
        params["reply_to_message_id"] = {AString::number(*message.replyToMessageId)};
    }

    if (message.messageThreadId) {
        params["message_thread_id"] = {AString::number(*message.messageThreadId)};
    }

    if (message.replyMarkup) {
        auto str = AJson::toString({
            {"inline_keyboard", aui::to_json(message.replyMarkup->buttons)}
        });
        ALOG_DEBUG(LOG_TAG) << "reply_markup " << str;
        params["reply_markup"] = { std::move(str) };
    }

    if (!message.text.empty() && !message.parseMode.empty()) {
        message.text.replaceAll(".", "\\.");
        message.text.replaceAll("-", "\\-");
        message.text.replaceAll("+", "\\+");
    }

    ALogger::debug(LOG_TAG) << "postMessage: " << message.text;
    params[message.image ? "caption" : "text"] = {std::move(message.text)};

    if (message.image) {
        AByteBuffer imageBuffer;
        PngImageLoader::save(imageBuffer, *message.image);
        params["photo"] = {
            .value = _new<AStrongByteBufferInputStream>(std::move(imageBuffer)),
            .filename = "photo.png",
            .mimeType = "image/png",
        };
    }

    return params;
}

AFuture<telegram::message_id_t> telegram::postMessage(telegram::Message message) {
    auto a = AUI_THREADPOOL_X [msg = std::move(message)]() mutable -> telegram::message_id_t {
        auto message = std::move(msg);
        ACurl::Builder builder(formatMethodUrl(message.image ? "sendPhoto" : "sendMessage"));
        builder.withMethod(ACurl::Method::HTTP_POST);


        //std::cout << AString::fromUtf8(AByteBuffer::fromStream(params.makeInputStream())) << '\n';
        builder.withMultipart(fillMultiparmWithMessage(message));

        auto response = std::move(*builder.runAsync());
        if (response.code == ACurl::ResponseCode::HTTP_200_OK) {
            ALOG_DEBUG(LOG_TAG) << "postMessage response " << response.code << " "
                                << AString::fromUtf8(response.body);
        } else {
            ALogger::err(LOG_TAG) << "postMessage failed " << response.code << " "
                                << AString::fromUtf8(response.body);
            throw AException(AString::fromUtf8(response.body));
        }

        return AJson::fromBuffer(response.body)["result"]["message_id"].asLongInt();
    };
    telegramAsync() << a;
    return a;
}

AFuture<telegram::message_id_t> telegram::editMessage(message_id_t messageId, telegram::Message message) {
    auto a = AUI_THREADPOOL_X [messageId, msg = std::move(message)]() mutable -> telegram::message_id_t {
        auto message = std::move(msg);
        ACurl::Builder builder(formatMethodUrl("editMessageText"));
        builder.withMethod(ACurl::Method::HTTP_POST);

        auto params = fillMultiparmWithMessage(message);
        params["message_id"] = { AString::number(messageId) };
        builder.withMultipart(std::move(params));

        auto response = std::move(*builder.runAsync());
        if (response.code == ACurl::ResponseCode::HTTP_200_OK) {
            ALOG_DEBUG(LOG_TAG) << "postMessage response " << response.code << " "
                                << AString::fromUtf8(response.body);
        } else {
            ALogger::err(LOG_TAG) << "postMessage failed " << response.code << " "
                                << AString::fromUtf8(response.body);
            throw AException(AString::fromUtf8(response.body));
        }

        return AJson::fromBuffer(response.body)["result"]["message_id"].asLongInt();
    };
    telegramAsync() << a;
    return a;
}

AFuture<> telegram::deleteMessage(chat_id_t chatId, message_id_t messageId) {
    auto a = AUI_THREADPOOL {
        ACurl::Builder builder(formatMethodUrl("deleteMessage"));
        builder.withMethod(ACurl::Method::HTTP_POST);

        AFormMultipart params;
        params["chat_id"] = { AString::number(chatId) };
        params["message_id"] = { AString::number(messageId) };
        builder.withMultipart(std::move(params));

        auto response = std::move(*builder.runAsync());
        if (response.code == ACurl::ResponseCode::HTTP_200_OK) {
            ALOG_DEBUG(LOG_TAG) << "postMessage response " << response.code << " "
                                << AString::fromUtf8(response.body);
        } else {
            ALogger::err(LOG_TAG) << "postMessage failed " << response.code << " "
                                << AString::fromUtf8(response.body);
            throw AException(AString::fromUtf8(response.body));
        }
    };
    telegramAsync() << a;
    return a;
}

void telegram::answerCallbackQuery(AString queryId) {
    telegramAsync() << ACurl::Builder(formatMethodUrl("answerCallbackQuery"))
        .withMethod(ACurl::Method::HTTP_POST)
        .withParams({
            {"callback_query_id", std::move(queryId)},
        })
        .runAsync();
}
AFuture<> telegram::placeReaction(chat_id_t chatId, message_id_t messageId,
                                  AString reaction) {
    auto a = AUI_THREADPOOL {
        ACurl::Builder builder(formatMethodUrl("setMessageReaction"));
        builder.withMethod(ACurl::Method::HTTP_POST);

        AFormMultipart params;
        params["chat_id"] = { AString::number(chatId) };
        params["message_id"] = { AString::number(messageId) };
        params["reaction"] = { AJson::toString(AJson::Array{ AJson::Object{
            {"type", "emoji"},
            {"emoji", reaction}
        } }) };
        builder.withMultipart(std::move(params));

        auto response = std::move(*builder.runAsync());
        if (response.code == ACurl::ResponseCode::HTTP_200_OK) {
            ALOG_DEBUG(LOG_TAG) << "postMessage response " << response.code << " "
                                << AString::fromUtf8(response.body);
        } else {
            ALogger::err(LOG_TAG) << "postMessage failed " << response.code << " "
                                << AString::fromUtf8(response.body);
            throw AException(AString::fromUtf8(response.body));
        }
    };
    telegramAsync() << a;
    return a;
}
