//
// Created by alex2772 on 2/27/26.
//

#include "OpenAIChat.h"

#include <chrono>
#include <gtest/gtest-message.h>
#include <optional>
#include <random>
#include <range/v3/algorithm/generate.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>

#include "AUI/Curl/ACurl.h"
#include "AUI/IO/AFileOutputStream.h"
#include "AUI/Json/Conversion.h"
#include "AUI/Logging/ALogger.h"
#include "AUI/Reflect/AEnumerate.h"
#include "AUI/Util/kAUI.h"
#include "config.h"
#include "telegram/telegram.h"

static constexpr auto LOG_TAG = "OpenAIChat";

using namespace std::chrono_literals;

template<>
struct AJsonConv<OpenAIChat::Message::Role> {
    static AJson toJson(OpenAIChat::Message::Role v) {
        switch (v) {
            case OpenAIChat::Message::Role::ASSISTANT: return "assistant";
            case OpenAIChat::Message::Role::USER: return "user";
            case OpenAIChat::Message::Role::SYSTEM_PROMPT: return "system";
            case OpenAIChat::Message::Role::TOOL: return "tool";
        }
    }

    static void fromJson(const AJson& json, OpenAIChat::Message::Role& out) {
        const auto& str = json.asString();
        if (str == "assistant") {
            out = OpenAIChat::Message::Role::ASSISTANT;
            return;
        }
        if (str == "user") {
            out = OpenAIChat::Message::Role::USER;
            return;
        }
        if (str == "system") {
            out = OpenAIChat::Message::Role::SYSTEM_PROMPT;
            return;
        }
        if (str == "tool") {
            out = OpenAIChat::Message::Role::TOOL;
            return;
        }
        throw AException("invalid role: " + str);
    }
};


AJSON_FIELDS(OpenAIChat::Message::ToolCall::Function,
AJSON_FIELDS_ENTRY(name)
AJSON_FIELDS_ENTRY(arguments)
)

AJSON_FIELDS(OpenAIChat::Message::ToolCall,
AJSON_FIELDS_ENTRY(id)
AJSON_FIELDS_ENTRY(index)
AJSON_FIELDS_ENTRY(type)
AJSON_FIELDS_ENTRY(function)
)

AJSON_FIELDS(OpenAIChat::Message,
AJSON_FIELDS_ENTRY(role)
(content, "content", AJsonFieldFlags::OPTIONAL)
(reasoning, "reasoning", AJsonFieldFlags::OPTIONAL)
(tool_call_id, "tool_call_id", AJsonFieldFlags::OPTIONAL)
(tool_calls, "tool_calls", AJsonFieldFlags::OPTIONAL)
)

AJSON_FIELDS(OpenAIChat::Response::Choice,
AJSON_FIELDS_ENTRY(index)
AJSON_FIELDS_ENTRY(message)
AJSON_FIELDS_ENTRY(finish_reason)
)

AJSON_FIELDS(OpenAIChat::Response,
AJSON_FIELDS_ENTRY(id)
AJSON_FIELDS_ENTRY(object)
AJSON_FIELDS_ENTRY(created)
AJSON_FIELDS_ENTRY(model)
AJSON_FIELDS_ENTRY(system_fingerprint)
AJSON_FIELDS_ENTRY(choices)
AJSON_FIELDS_ENTRY(usage)
)

AJSON_FIELDS(OpenAIChat::Response::Usage,
AJSON_FIELDS_ENTRY(prompt_tokens)
AJSON_FIELDS_ENTRY(completion_tokens)
AJSON_FIELDS_ENTRY(total_tokens)

)

AFuture<OpenAIChat::Response> OpenAIChat::chat(AString message) {
  return chat({
    { Message::Role::USER, std::move(message)} ,
  });
}


AFuture<OpenAIChat::Response> OpenAIChat::chat(AVector<Message> messages) {
  messages.insert(messages.begin(), {Message::Role::SYSTEM_PROMPT, systemPrompt});
  return AUI_THREADPOOL -> Response {
    auto query = AJson::toString({
        {
          "messages", aui::to_json(messages),
        },
        { "stream", false },
        { "use_context", false },
        { "include_sources", true },
        { "model", config::MODEL },
        { "tools", tools },
      });
    AFileOutputStream("query.json") << query.toStdString();
    ALOG_DEBUG(LOG_TAG) << "Query: " << query;
    auto response = AJson::fromBuffer(ACurl::Builder(baseUrl + "v1/chat/completions")
      .withMethod(ACurl::Method::HTTP_POST)
      .withTimeout(4h)
      .withHeaders({"Content-Type: application/json"})
      .withBody(query.toStdString()).runAsync()->body);
    ALOG_DEBUG(LOG_TAG) << "Response: " << AJson::toString(response);
    return aui::from_json<Response>(response);
  };
};
