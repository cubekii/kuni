# kuni (くに)

LLM character AI. It interacts with the world through a text-based Telegram Client optimized for LLM (tdlib; not to be
confused with Telegram Bot API). It features RAG (persistent memory storage with ANN search) and nightly sanity
checks.

## Goals

- Prove C++20 can be used for AI and backend development.
- Prove AI psychosis is a thing.
- Make an AI character that can remember people, events, read news and create emotional bond with people.
- Make interaction interface between AI <-> Telegram as close to human as possible. I.e., LLM sees list of their chats,
  unread messages, replies, forwards, photos, stickers, etc.
- Bot's online state, open/close chat API calls and read marks (in TG it is two checkmarks) are carefully handled to
  make it feel like you are talking to a real human.

## Technical details

- C++20 CMake-based project with heavy usage of modern C++ features such as coroutines
- Uses [tdlib](https://core.telegram.org/tdlib) for Telegram API access
- Used with a self-hosted Ollama server through OpenAI API.
