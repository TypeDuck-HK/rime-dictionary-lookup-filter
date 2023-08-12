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
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/adaptor/map.hpp>
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

void DictionaryLookupFilter::Process(const an<Candidate>& cand) {
    if (dict_ == nullptr)
        return;
    auto phrase = As<Phrase>(Candidate::GetGenuineCandidate(cand));
    if (!phrase)
        return;
    string spellingCode = phrase->comment();
    size_t startPos = spellingCode.find('\f');
    string result;
    auto sentence = As<Sentence>(phrase);
    vector<string> words;
    if (sentence) {
        const vector<DictEntry>& components = sentence->components();
        words.resize(components.size());
        std::transform(
            components.begin(), components.end(), words.begin(),
            [](const DictEntry& component) { return component.text; });
    } else if (auto userDictEntry = dynamic_cast<const UserDictEntry*>(&phrase->entry()))
        words = userDictEntry->elements;
    if (!words.empty()) {
        string entries;
        std::unordered_map<string, string> word;
        for (const string& text : words) {
            auto it = word.find(text);
            if (it != word.end()) {
                result += it->second;
                continue;
            }
            string lines = ParseEntry(text, "", false);
            if (!lines.empty()) {
                string pronunciation = lines.substr(lines.find(',') + 1);
                pronunciation = pronunciation.substr(pronunciation.find(',') + 1);
                pronunciation = pronunciation.substr(0, pronunciation.find(','));
                result += pronunciation;
                entries += lines;
                word.insert({text, pronunciation});
            }
        }
        if (!entries.empty())
            phrase->set_comment(spellingCode.substr(0, startPos + 1) +
                                "\r1," + cand->text() + "," + result +
                                ",1,0,,,,composition,,,,,,,,," + entries);
    } else if (startPos != string::npos) {
        result = ParseEntry(cand->text(), spellingCode.substr(startPos + 1), true);
        if (!result.empty())
            phrase->set_comment(spellingCode.substr(0, startPos + 1) + result);
    }
}

string DictionaryLookupFilter::ParseEntry(string text, string jyutping, const bool isNotSentence) {
    rime::DictEntryIterator it;
    std::unordered_set<string> pronunciations;
    if (isNotSentence) {
        boost::remove_erase_if(jyutping, boost::is_any_of("; "));
        boost::split(pronunciations, jyutping, boost::is_any_of("\f"));
    }
    dict_->LookupWords(&it, text, false);
    if (it.exhausted())
        return "";
    std::multimap<int8_t, string> matchedLines, remainingLines;
    do {
        string line = it.Peek()->text;
        if (line.empty())
            continue;
        size_t firstCommaPos = line.find(',');
        string pronunciation = line.substr(0, firstCommaPos);
        string pronOrder = line.substr(firstCommaPos + 1);
        pronOrder = pronOrder.substr(0, pronOrder.find(','));
        bool match = pronunciations.find(pronunciation) != pronunciations.end();
        (match ? matchedLines : remainingLines).insert({(int8_t)std::stoi(pronOrder), line});
    } while (it.Next());
    string result, prefix = "\r1," + text + ",";
    if (isNotSentence && matchedLines.empty() && !remainingLines.empty()) {
        result += prefix;
        result += boost::algorithm::join(remainingLines | boost::adaptors::map_values, prefix);
        return result;
    }
    if (!matchedLines.empty()) {
        result += prefix;
        result += boost::algorithm::join(matchedLines | boost::adaptors::map_values, prefix);
    }
    if (!remainingLines.empty()) {
        prefix[1] = '0';
        result += prefix;
        result += boost::algorithm::join(remainingLines | boost::adaptors::map_values, prefix);
    }
    return result;
}

}  // namespace rime
