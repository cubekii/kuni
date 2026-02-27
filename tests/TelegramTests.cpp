#include "config.h"
#include "telegram/telegram.h"
#include <gmock/gmock.h>


TEST(Telegram, PostMessage) {
  *telegram::postMessage({
    .chatId = config::PAPIK_CHAT_ID,
    .text = "PostMessage",
  });
}

TEST(Telegram, PostMessageInlineKeyboard) {
  *telegram::postMessage({
    .chatId = config::PAPIK_CHAT_ID,
    .text = "PostMessage",
    .replyMarkup = telegram::Message::ReplyMarkup{
      {
        {
          { "Test", "Test" },
        }
      }
    },
  });
}

