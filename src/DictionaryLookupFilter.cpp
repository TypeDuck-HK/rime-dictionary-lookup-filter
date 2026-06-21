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
#include <map>
#include <unordered_set>
#include <unordered_map>

namespace rime {

namespace {

struct LookupLine {
    string lookupText;
    string text;
    vector<string> columns;
};

vector<string> LeadingColumns(const string& line, const size_t count) {
    vector<string> columns;
    size_t start = 0;
    while (columns.size() < count) {
        const size_t commaPos = line.find(',', start);
        if (commaPos == string::npos) {
            columns.push_back(line.substr(start));
            break;
        }
        columns.push_back(line.substr(start, commaPos - start));
        start = commaPos + 1;
    }
    return columns;
}

int PronOrder(const vector<string>& columns) {
    return columns.size() > 7 && !columns[7].empty()
           ? std::stoi(columns[7])
           : 0;
}

string ParseEntry(Dictionary* dictionary,
                  string text,
                  string jyutping,
                  const bool isSentence,
                  const bool matchedOnly,
                  std::unordered_set<string>& activeLookups);

string ParseRelatedEntries(Dictionary* dictionary,
                           const LookupLine& line,
                           std::unordered_set<string>& activeLookups) {
    string result;
    if (line.columns.size() > 4 &&
        (!line.columns[3].empty() || !line.columns[4].empty())) {
        result += ParseEntry(dictionary,
                             line.columns[3].empty() ? line.lookupText : line.columns[3],
                             line.columns[4].empty() ? line.columns[0] : line.columns[4],
                             true, true, activeLookups);
    }
    if (line.columns.size() <= 6 || line.columns[5].empty() ||
        line.columns[6].empty())
        return result;

    vector<string> componentTexts, componentJyutpings;
    boost::split(componentTexts, line.columns[5], boost::is_any_of("|"));
    boost::split(componentJyutpings, line.columns[6], boost::is_any_of("|"));
    for (size_t i = 0; i < componentTexts.size() && i < componentJyutpings.size(); ++i) {
        if (componentTexts[i].empty() || componentJyutpings[i].empty())
            continue;
        result += ParseEntry(dictionary, componentTexts[i], componentJyutpings[i],
                             true, true, activeLookups);
    }
    return result;
}

void AppendLineWithRelatedEntries(Dictionary* dictionary,
                                  string& result,
                                  const string& prefix,
                                  const LookupLine& line,
                                  std::unordered_set<string>& activeLookups) {
    result += prefix + line.text;
    result += ParseRelatedEntries(dictionary, line, activeLookups);
}

string ParseEntry(Dictionary* dictionary,
                  string text,
                  string jyutping,
                  const bool isSentence,
                  const bool matchedOnly,
                  std::unordered_set<string>& activeLookups) {
    boost::remove_erase_if(jyutping, boost::is_any_of("; "));
    const string lookupKey = text + "\f" + jyutping;
    if (activeLookups.find(lookupKey) != activeLookups.end())
        return "";
    activeLookups.insert(lookupKey);

    DictEntryIterator it;
    std::unordered_set<string> pronunciations;
    boost::split(pronunciations, jyutping, boost::is_any_of("\f"));
    dictionary->LookupWords(&it, text, false);
    if (it.exhausted()) {
        activeLookups.erase(lookupKey);
        return "";
    }

    std::multimap<int, LookupLine> matchedLines, remainingLines;
    do {
        const string lineText = it.Peek()->text;
        if (lineText.empty())
            continue;
        const vector<string> columns = LeadingColumns(lineText, 8);
        if (columns.empty())
            continue;
        const bool match = pronunciations.find(columns[0]) != pronunciations.end();
        (match ? matchedLines : remainingLines).insert(
            {PronOrder(columns), {text, lineText, columns}});
    } while (it.Next());

    string result, prefix = "\r1," + text + ",";
    if (matchedOnly) {
        for (const pair<int, LookupLine>& line : matchedLines)
            AppendLineWithRelatedEntries(dictionary, result, prefix, line.second,
                                         activeLookups);
        activeLookups.erase(lookupKey);
        return result;
    }
    if (isSentence || !matchedLines.empty() || remainingLines.empty()) {
        for (const pair<int, LookupLine>& line : matchedLines)
            AppendLineWithRelatedEntries(dictionary, result, prefix, line.second,
                                         activeLookups);
        prefix[1] = '0';
    }
    for (const pair<int, LookupLine>& line : remainingLines)
        AppendLineWithRelatedEntries(dictionary, result, prefix, line.second,
                                     activeLookups);
    activeLookups.erase(lookupKey);
    return result;
}

}  // namespace

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
                                ",,,,,,,1,0,,,,composition,,,,,,,," + entries);
    }
}

string DictionaryLookupFilter::ParseEntry(string text, string jyutping, const bool isSentence) {
    std::unordered_set<string> activeLookups;
    return rime::ParseEntry(dict_.get(), text, jyutping, isSentence, false,
                            activeLookups);
}

}  // namespace rime
