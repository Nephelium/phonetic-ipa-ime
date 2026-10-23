#pragma once
#include "engine/ipa/ipa_readings.h"

namespace PhoneticIpa
{
// Worker-owned state. A reading choice belongs to one composition and one
// word/spelling pair, not a page index which may change after asynchronous results.
class ReadingSession
{
  public:
    void Clear()
    {
        context_.clear();
        choices_.clear();
        Cancel();
    }
    void SetContext(const std::string &context)
    {
        if (context_ != context)
        {
            Clear();
            context_ = context;
        }
    }
    phonetic_ipa::Transcription Resolve(const phonetic_ipa::HanziTable &table, const std::string &word,
                                        const std::string &pinyin) const
    {
        const auto found = choices_.find(Identity(word, pinyin));
        return found == choices_.end() ? table.Convert(word, pinyin) : found->second;
    }
    bool Begin(const phonetic_ipa::HanziTable &table, const std::string &word, const std::string &pinyin)
    {
        if (active_)
            return false;
        draft_ = Resolve(table, word, pinyin);
        if (!draft_.CanChoose())
            return false;
        for (auto &part : draft_.parts)
            if (part.options.size() > 1)
                part.selected = -1;
        draft_.Refresh();
        identity_ = Identity(word, pinyin);
        word_ = word;
        active_ = true;
        return true;
    }
    const phonetic_ipa::ReadingPart *Pending() const
    {
        if (!active_)
            return nullptr;
        for (const auto &part : draft_.parts)
            if (part.selected < 0)
                return &part;
        return nullptr;
    }
    bool Choose(size_t index)
    {
        for (auto &part : draft_.parts)
            if (active_ && part.selected < 0)
            {
                if (index >= part.options.size())
                    return false;
                part.selected = static_cast<int>(index);
                draft_.Refresh();
                if (!draft_.ambiguous)
                {
                    if (choices_.size() >= 256)
                        choices_.clear();
                    choices_[identity_] = draft_;
                    active_ = false;
                }
                return true;
            }
        return false;
    }
    void Cancel()
    {
        active_ = false;
        draft_ = {};
        identity_.clear();
        word_.clear();
    }
    bool Active() const
    {
        return active_;
    }
    const std::string &Word() const
    {
        return word_;
    }

  private:
    std::string context_, identity_, word_;
    bool active_ = false;
    phonetic_ipa::Transcription draft_;
    std::map<std::string, phonetic_ipa::Transcription> choices_;
    static std::string Identity(const std::string &word, const std::string &pinyin)
    {
        return word + '\t' + pinyin;
    }
};
} // namespace PhoneticIpa
