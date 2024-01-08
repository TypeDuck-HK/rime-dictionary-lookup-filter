//
//  DictionaryLookupFilter.cpp
//  rime-dictionary-lookup-filter-objs
//
//  Created by leung tsinam on 28/10/2022.
//

#include "DictionaryLookupFilter.hpp"

#include <rime/candidate.h>
#include <rime/engine.h>
#include <rime/schema.h>
#include <rime/translation.h>
#include <rime/dict/reverse_lookup_dictionary.h>
#include <rime/dict/dictionary.h>
#include <rime/gear/translator_commons.h>
#include <rime/gear/script_translator.h>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

namespace rime {

class DictionaryLookupFilterTranslation : public CacheTranslation {
  public:
    DictionaryLookupFilterTranslation(an<Translation> translation,
                                      DictionaryLookupFilter* filter)
            : CacheTranslation(translation), filter_(filter) {}
    virtual an<Candidate> Peek();

  protected:
    DictionaryLookupFilter* filter_;
};

an<Candidate> DictionaryLookupFilterTranslation::Peek() {
    auto cand = CacheTranslation::Peek();
    if (cand)
        filter_->Process(cand);
    return cand;
}

DictionaryLookupFilter::DictionaryLookupFilter(const Ticket& ticket)
        : Filter(ticket), TagMatching(ticket) {
    if (ticket.name_space == "filter")
        name_space_ = "dictionary_lookup_filter";
    else
        name_space_ = ticket.name_space;

    if (Config* config = ticket.engine->schema()->config())
        config->GetString(name_space_ + "/dictionary", &dictname_);
}

void DictionaryLookupFilter::Initialize() {
    initialized_ = true;
    if (!engine_)
        return;

    if (DictionaryComponent* dictionary = dynamic_cast<DictionaryComponent*>(
                Dictionary::Require("dictionary"))) {
        dict_.reset(dictionary->Create(dictname_, dictname_, {}));
        if (dict_)
            dict_->Load();
    }
}

an<Translation> DictionaryLookupFilter::Apply(an<Translation> translation,
                                              CandidateList* candidates) {
    if (!initialized_)
        Initialize();
    if (!dict_)
        return translation;
    return New<DictionaryLookupFilterTranslation>(translation, this);
}

bool DictionaryLookupFilter::GetWordsFromUserDictEntry(
    const DictEntry entry,
    vector<pair<string, string>>& words,
    Dictionary* dictionary) {
    bool success = false;
    if (!entry.elements.empty()) {
        vector<string> syllables;
        if (dictionary->Decode(entry.code, &syllables)) {
            size_t i = 0;
            for (const string& element : entry.elements) {
                string pronunciation;
                for (const char* p = element.c_str(); *p; ++p) {
                    if ((*p & 0xc0) != 0x80 && i < syllables.size())
                        pronunciation += syllables[i++];
                }
                words.push_back({element, pronunciation});
                success = true;
            }
        }
    }
    return success;
}

void DictionaryLookupFilter::Process(const an<Candidate>& cand) {
    if (dict_ == nullptr)
        return;
    auto phrase = As<Phrase>(Candidate::GetGenuineCandidate(cand));
    if (!phrase)
        return;
    const string spellingCode = phrase->comment();
    const size_t startPos = spellingCode.find('\f');
    const string prefix = spellingCode.substr(0, startPos);
    string result = ParseEntry(
        cand->text(),
        startPos == string::npos ? "" : spellingCode.substr(startPos + 1),
        false);
    if (!result.empty()) {
        phrase->set_comment(prefix + "\f" + result);
        return;
    }
    Dictionary* dictionary = nullptr;
    if (auto syllabifier = As<ScriptSyllabifier>(phrase->syllabifier()))
        dictionary = syllabifier->translator()->dict();
    vector<pair<string, string>> words;
    if (auto sentence = As<Sentence>(phrase)) {
        const vector<DictEntry>& components = sentence->components();
        for (const DictEntry entry : components) {
            if (!GetWordsFromUserDictEntry(entry, words, dictionary)) {
                string pronunciation;
                if (!entry.comment.empty()) {
                    pronunciation = entry.comment;
                    const size_t pos = pronunciation.find('\f');
                    if (pos != string::npos)
                        pronunciation = pronunciation.substr(pos + 1);
                    boost::remove_erase_if(pronunciation, boost::is_any_of("; "));
                    pronunciation = pronunciation.substr(0, pronunciation.find('\f'));
                } else if (dictionary) {
                    vector<string> syllables;
                    if (dictionary->Decode(entry.code, &syllables))
                        pronunciation = boost::join(syllables, "");
                }
                words.push_back({entry.text, pronunciation});
            }
        }
    } else
        GetWordsFromUserDictEntry(phrase->entry(), words, dictionary);
    if (!words.empty()) {
        string entries;
        std::unordered_set<string> cache;
        for (pair<string, string>& word : words) {
            result += word.second;
            if (cache.find(word.first) == cache.end()) {
              entries += ParseEntry(word.first, word.second, true);
              cache.insert(word.first);
            }
        }
        if (!entries.empty())
            phrase->set_comment(prefix + "\f\r1," + cand->text() + "," + result +
                                ",1,0,,,,composition,,,,,,,,," + entries);
    }
}

string DictionaryLookupFilter::ParseEntry(string text, string jyutping, const bool isSentence) {
    DictEntryIterator it;
    std::unordered_set<string> pronunciations;
    boost::remove_erase_if(jyutping, boost::is_any_of("; "));
    boost::split(pronunciations, jyutping, boost::is_any_of("\f"));
    dict_->LookupWords(&it, text, false);
    if (it.exhausted())
        return "";
    std::multimap<int8_t, string> matchedLines, remainingLines;
    do {
        const string line = it.Peek()->text;
        if (line.empty())
            continue;
        const size_t firstCommaPos = line.find(',');
        const string pronunciation = line.substr(0, firstCommaPos);
        string pronOrder = line.substr(firstCommaPos + 1);
        pronOrder = pronOrder.substr(0, pronOrder.find(','));
        const bool match = pronunciations.find(pronunciation) != pronunciations.end();
        (match ? matchedLines : remainingLines).insert({(int8_t)std::stoi(pronOrder), line});
    } while (it.Next());
    string result, prefix = "\r1," + text + ",";
    if (isSentence || !matchedLines.empty() || remainingLines.empty()) {
        for (const pair<int8_t, string>& line : matchedLines)
            result += prefix + line.second;
        prefix[1] = '0';
    }
    for (const pair<int8_t, string>& line : remainingLines)
        result += prefix + line.second;
    return result;
}

}  // namespace rime
