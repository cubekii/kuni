#pragma once
#include "AUI/Json/AJson.h"
#include "AUI/Thread/AFuture.h"
#include "AUI/Util/APreprocessor.h"

struct OpenAIChat {
    AString systemPrompt;
    AString baseUrl =  "http://" AUI_PP_STRINGIZE(OPENAICHAT_ADDRESS) "/";

    AJson tools;


    struct Message {
        enum class Role {
            ASSISTANT,
            SYSTEM_PROMPT,
            USER,
            TOOL,
          } role;
        AString content;
        AString tool_call_id;
        AString reasoning;
        struct ToolCall {
            AString id;
            int64_t index;
            AString type;
            struct Function {
                AString name;
                AString arguments;
            } function;
        };
        AVector<ToolCall> tool_calls;
    };

    struct Response {
        AString id;
        AString object;
        int64_t created;
        AString model;
        AString system_fingerprint;
        struct Choice {
            int64_t index;
            Message message;
            AString finish_reason;
        };
        AVector<Choice> choices;
        struct Usage {
            int64_t prompt_tokens;
            int64_t completion_tokens;
            int64_t total_tokens;
        } usage;
    };

    AFuture<Response> chat(AString message);
    AFuture<Response> chat(AVector<Message> messages);
};
