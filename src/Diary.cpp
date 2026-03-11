#include "Diary.h"

#include "AUI/IO/AFileInputStream.h"
#include "AUI/IO/AFileOutputStream.h"
#include "AUI/Logging/ALogger.h"
#include "AUI/Thread/AThreadPool.h"
#include "AUI/Util/kAUI.h"
#include "OpenAIChat.h"
#include "util/cosine_similarity.h"

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

Diary::Diary(APath diaryDir)
    : mDiaryDir(std::move(diaryDir)) {
    mDiaryDir.makeDirs();
}

AVector<Diary::Entry> Diary::read(const APath& diaryDir) {
    AVector<Entry> result;
    diaryDir.makeDirs();
    for (const auto& entry : diaryDir.listDir()) {
        if (!entry.isRegularFileExists()) {
            continue;
        }
        if (entry.extension() != "md") {
            continue;
        }
        result << Entry{ .id = entry.filenameWithoutExtension(), .text = AString::fromUtf8(AByteBuffer::fromStream(AFileInputStream(entry))) };
    }
    return result;
}

void Diary::save(const Entry& entry) {
    AFileOutputStream(mDiaryDir / (entry.id + ".md")) << entry.text;
}

void Diary::save(const EntryEx& entry) {
    // inject
    // ---
    // { ... }
    // ---
    //
    // prologue to the md file with metadata.
    save(Entry{
        .id = entry.id,
        .text = "---\n{}\n---\n{}\n"_format(AJson::toString(aui::to_json(entry.metadata)), entry.freeformBody),
    });
}

AFuture<AVector<Diary::EntryExAndRelatedness>> Diary::query(const std::valarray<float>& query) {
    struct DiaryEntryExAndRelatednessF {
        std::list<EntryEx>::iterator entry;
        AFuture<aui::float_within_0_1> relatedness;
    };

    AVector<DiaryEntryExAndRelatednessF> relatednesses;

    for (auto it = mCachedDiary->begin(); it != mCachedDiary->end(); ++it) {
        relatednesses << DiaryEntryExAndRelatednessF{
            .entry = it,
            .relatedness = entryIsRelated(query, *it)
        };
    }
    for (const auto&[_, relatedness] : relatednesses) {
        co_await relatedness;
    }
    AVector<EntryExAndRelatedness> result = relatednesses | ranges::view::transform([](const auto& f) {
        return EntryExAndRelatedness{
            .entry = f.entry,
            .relatedness = *f.relatedness,
        };
    }) | ranges::to<AVector<EntryExAndRelatedness>>;
    ranges::sort(result, [](const auto& a, const auto& b) { return a.relatedness > b.relatedness; });
    co_return result;
}

AFuture<aui::float_within_0_1> Diary::entryIsRelated(const std::valarray<float>& context, EntryEx& entry) {
    if (entry.freeformBody.empty()) {
        co_return 0.f;
    }
    if (entry.freeformBody.contains("<important_note")) {
        co_return 1.f;
    }
    if (entry.metadata.embedding.size() != context.size()) {
        OpenAIChat chat;
        entry.metadata.embedding = co_await chat.embedding(entry.freeformBody);
        save(entry);
    }
    auto task = AUI_THREADPOOL_X [&] {
        return aui::float_within_0_1((util::cosine_similarity(context, entry.metadata.embedding) + 1.f) / 2.f);
    };
    co_return co_await task;
}

std::list<Diary::EntryEx> Diary::parse(AVector<Entry> diary) {
    // parse
    // ---
    // { ... }
    // ---
    //
    // prologue in the md file with metadata.
    return diary | ranges::views::transform([](Entry& entry) {
        entry.text = entry.text.trim('\n');
        try {
            if (!entry.text.startsWith("---")) {
                return EntryEx{.id = std::move(entry.id), .freeformBody = std::move(entry.text)};
            }
            auto end = entry.text.bytes().find("---", 4);
            if (end == std::string::npos) {
                return EntryEx{.id = std::move(entry.id), .freeformBody = std::move(entry.text)};
            }
            auto json = AJson::fromString(entry.text.bytes().substr(4, end - 4));
            return EntryEx{.id = std::move(entry.id), .metadata = aui::from_json<EntryEx::Metadata>(std::move(json)), .freeformBody = std::move(AStringView(entry.text.bytes().substr(end + 4)))};
        } catch (const AException& e) {
            ALogger::err("DiaryManager") << "parse can't parse " << entry.id << ": " << e;
            return EntryEx{.id = std::move(entry.id), .freeformBody = std::move(entry.text)};
        }
    }) | ranges::to<std::list<EntryEx>>;
}

void Diary::unload(std::list<EntryEx>::const_iterator it) {
    save(*it);
    mCachedDiary->erase(it);
}
