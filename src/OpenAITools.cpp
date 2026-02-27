//
// Created by alex2772 on 2/27/26.
//

#include "OpenAITools.h"

AJSON_FIELDS(OpenAITools::Tool::Parameters::Property, AJSON_FIELDS_ENTRY(type) AJSON_FIELDS_ENTRY(description))

AJSON_FIELDS(OpenAITools::Tool::Parameters,
             AJSON_FIELDS_ENTRY(type) AJSON_FIELDS_ENTRY(properties) AJSON_FIELDS_ENTRY(required)
                     AJSON_FIELDS_ENTRY(additionalProperties))

AJSON_FIELDS(OpenAITools::Tool, AJSON_FIELDS_ENTRY(type) AJSON_FIELDS_ENTRY(name) AJSON_FIELDS_ENTRY(description)
                                        AJSON_FIELDS_ENTRY(parameters) AJSON_FIELDS_ENTRY(strict))

void OpenAITools::addTool(const Tool& tool, Handler handler) {
    asJson.asArray().push_back(AJson::Object{
            {"function", aui::to_json(tool)},
    });
    handlers[tool.name] = std::move(handler);
}

AVector<OpenAIChat::Message>OpenAITools::handleToolCalls(const AVector<OpenAIChat::Message::ToolCall>& toolCalls) {
    return toolCalls.map([&](const OpenAIChat::Message::ToolCall& toolCall) {
        return OpenAIChat::Message{
                .role = OpenAIChat::Message::Role::TOOL,
                .content = [&]() -> AString {
                    try {
                        if (auto c = handlers.contains(toolCall.function.name)) {
                            return c->second(AJson::fromString(toolCall.function.arguments));
                        }
                        return "error: no such tool: " + toolCall.function.name;
                    } catch (const AException& e) {
                        return "error: {}"_format(e.getMessage());
                    }
                }(),
                .tool_call_id = toolCall.id,
        };
    });
}
