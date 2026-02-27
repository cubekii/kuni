#pragma once
#include "AUI/Common/AString.h"
#include "AUI/Common/AVector.h"
#include "AUI/Json/AJson.h"
#include "OpenAIChat.h"


struct OpenAITools {
    AJson asJson = AJson::Array{};
    using Handler = std::function<AString(const AJson&)>;
    AMap<AString, Handler> handlers;

    struct Tool {
        AString type = "function";
        AString name;
        AString description;
        struct Parameters {
            AString type = "object";
            struct Property {
                AString type = "string";
                AString description;
            };
            AMap<AString, Property> properties;
            AVector<AString> required; // required properties
            bool additionalProperties = false;
        } parameters;
        bool strict = true;
    };
    void addTool(const Tool& tool, Handler handler);
    AVector<OpenAIChat::Message> handleToolCalls(const AVector<OpenAIChat::Message::ToolCall>& toolCalls);
};
