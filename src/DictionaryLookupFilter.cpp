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
    string lookupHonzi;
    string rawLine;
    vector<string> columns;
    string displayHonzi;
    string displayJyutping;
    string tail;
};

struct EmittedLine {
    string commentLine;
    string dedupeKey;
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

size_t ColumnStart(const string& line, const size_t column) {
    size_t pos = 0;
    for (size_t i = 0; i < column; ++i) {
        pos = line.find(',', pos);
        if (pos == string::npos)
            return string::npos;
        ++pos;
    }
    return pos;
}

string OutputTailWithoutPronOrder(const string& rawLine,
                                  const vector<string>& columns) {
    vector<string> outputColumns;
    for (size_t i = 3; i <= 6; ++i)
        outputColumns.push_back(i < columns.size() ? columns[i] : "");

    string tail = boost::join(outputColumns, ",");
    const size_t start = ColumnStart(rawLine, 8);
    if (start != string::npos)
        tail += "," + rawLine.substr(start);
    return tail;
}

LookupLine MakeLookupLine(const string& honzi,
                          const string& rawLine,
                          const vector<string>& columns) {
    return {
        honzi,
        rawLine,
        columns,
        columns.size() > 1 && !columns[1].empty() ? columns[1] : honzi,
        columns.size() > 2 && !columns[2].empty() ? columns[2] : columns[0],
        OutputTailWithoutPronOrder(rawLine, columns),
    };
}

string BuildOutputLine(const char matchInputBuffer, const LookupLine& line) {
    return "\r" + string(1, matchInputBuffer) + "," + line.displayHonzi + "," +
           line.displayJyutping + "," + line.tail;
}

string DedupeKey(const LookupLine& line) {
    return line.displayHonzi + "," + line.displayJyutping + "," + line.tail;
}

vector<EmittedLine> DeduplicateRows(
    const vector<EmittedLine>& rows,
    const std::unordered_set<string>* skippedKeys = nullptr) {
    std::unordered_map<string, size_t> lastIndex;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (skippedKeys && skippedKeys->find(rows[i].dedupeKey) != skippedKeys->end())
            continue;
        lastIndex[rows[i].dedupeKey] = i;
    }
    vector<EmittedLine> result;
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto last = lastIndex.find(rows[i].dedupeKey);
        if (last != lastIndex.end() && last->second == i)
            result.push_back(rows[i]);
    }
    return result;
}

typedef std::multimap<int, LookupLine> OrderedLookupLines;

bool LookupLines(Dictionary* dictionary,
                 const string& honzi,
                 const std::unordered_set<string>& pronunciations,
                 OrderedLookupLines& matchedLines,
                 OrderedLookupLines& remainingLines) {
    DictEntryIterator it;
    dictionary->LookupWords(&it, honzi, false);
    if (it.exhausted())
        return false;

    do {
        const string rawLine = it.Peek()->text;
        if (rawLine.empty())
            continue;
        const vector<string> columns = LeadingColumns(rawLine, 8);
        if (columns.empty())
            continue;
        const bool match = pronunciations.find(columns[0]) != pronunciations.end();
        (match ? matchedLines : remainingLines).insert(
            {PronOrder(columns), MakeLookupLine(honzi, rawLine, columns)});
    } while (it.Next());
    return true;
}

vector<EmittedLine> CollectMatchedRows(Dictionary* dictionary,
                                       string honzi,
                                       string jyutping,
                                       const char matchInputBuffer,
                                       const char componentMatchInputBuffer,
                                       const bool includeComponentEntries,
                                       std::unordered_set<string>& activeLookups);

void AppendCanonicalRedirects(Dictionary* dictionary,
                              vector<EmittedLine>& rows,
                              const LookupLine& line,
                              const char componentMatchInputBuffer,
                              const bool includeComponentEntries,
                              std::unordered_set<string>& activeLookups) {
    if (line.columns.size() <= 4 ||
        (line.columns[3].empty() && line.columns[4].empty()))
        return;

    const string canonicalHonzi =
        line.columns[3].empty() ? line.lookupHonzi : line.columns[3];
    const string canonicalJyutping =
        line.columns[4].empty() ? line.columns[0] : line.columns[4];
    vector<EmittedLine> canonicalRows = CollectMatchedRows(
        dictionary, canonicalHonzi, canonicalJyutping, '0',
        componentMatchInputBuffer, includeComponentEntries, activeLookups);
    rows.insert(rows.end(), canonicalRows.begin(), canonicalRows.end());
}

void AppendCanonicalEntryOrRedirect(Dictionary* dictionary,
                                    vector<EmittedLine>& rows,
                                    const LookupLine& line,
                                    const char matchInputBuffer,
                                    const char componentMatchInputBuffer,
                                    const bool includeComponentEntries,
                                    std::unordered_set<string>& activeLookups) {
    if (line.columns.size() > 4 && line.columns[3].empty() &&
        line.columns[4].empty()) {
        rows.push_back({BuildOutputLine(matchInputBuffer, line), DedupeKey(line)});
        return;
    }
    AppendCanonicalRedirects(dictionary, rows, line, componentMatchInputBuffer,
                             includeComponentEntries, activeLookups);
}

void AppendLineWithRelatedEntries(Dictionary* dictionary,
                                  vector<EmittedLine>& rows,
                                  const LookupLine& line,
                                  const char matchInputBuffer,
                                  const char componentMatchInputBuffer,
                                  const bool includeComponentEntries,
                                  std::unordered_set<string>& activeLookups) {
    rows.push_back({BuildOutputLine(matchInputBuffer, line), DedupeKey(line)});

    // Related rows are collected immediately after their parent row;
    // later deduplication may keep a duplicate at its last collected position.
    AppendCanonicalRedirects(dictionary, rows, line, componentMatchInputBuffer,
                             includeComponentEntries, activeLookups);

    if (!includeComponentEntries || line.columns.size() <= 6 || line.columns[5].empty() ||
        line.columns[6].empty())
        return;

    vector<string> componentTexts, componentJyutpings;
    boost::split(componentTexts, line.columns[5], boost::is_any_of("|"));
    boost::split(componentJyutpings, line.columns[6], boost::is_any_of("|"));
    for (size_t i = 0; i < componentTexts.size() && i < componentJyutpings.size(); ++i) {
        if (componentTexts[i].empty() || componentJyutpings[i].empty())
            continue;
        vector<EmittedLine> componentRows = CollectMatchedRows(
            dictionary, componentTexts[i], componentJyutpings[i],
            componentMatchInputBuffer, componentMatchInputBuffer,
            includeComponentEntries, activeLookups);
        rows.insert(rows.end(), componentRows.begin(), componentRows.end());
    }
}

vector<EmittedLine> CollectMatchedRows(Dictionary* dictionary,
                                       string honzi,
                                       string jyutping,
                                       const char matchInputBuffer,
                                       const char componentMatchInputBuffer,
                                       const bool includeComponentEntries,
                                       std::unordered_set<string>& activeLookups) {
    boost::remove_erase_if(jyutping, boost::is_any_of("; "));
    const string lookupKey = honzi + "\f" + jyutping;
    if (activeLookups.find(lookupKey) != activeLookups.end())
        return {};
    activeLookups.insert(lookupKey);

    std::unordered_set<string> pronunciations;
    boost::split(pronunciations, jyutping, boost::is_any_of("\f"));
    OrderedLookupLines matchedLines, remainingLines;
    if (!LookupLines(dictionary, honzi, pronunciations, matchedLines, remainingLines)) {
        activeLookups.erase(lookupKey);
        return {};
    }

    vector<EmittedLine> rows;
    for (const pair<int, LookupLine>& line : matchedLines)
        AppendLineWithRelatedEntries(dictionary, rows, line.second,
                                     matchInputBuffer, componentMatchInputBuffer,
                                     includeComponentEntries, activeLookups);
    activeLookups.erase(lookupKey);
    return rows;
}

string ParseEntry(Dictionary* dictionary,
                  string honzi,
                  string jyutping,
                  const bool isSentence,
                  std::unordered_set<string>& activeLookups) {
    boost::remove_erase_if(jyutping, boost::is_any_of("; "));
    const string lookupKey = honzi + "\f" + jyutping;
    if (activeLookups.find(lookupKey) != activeLookups.end())
        return "";
    activeLookups.insert(lookupKey);

    std::unordered_set<string> pronunciations;
    boost::split(pronunciations, jyutping, boost::is_any_of("\f"));
    OrderedLookupLines matchedLines, remainingLines;
    if (!LookupLines(dictionary, honzi, pronunciations, matchedLines, remainingLines)) {
        activeLookups.erase(lookupKey);
        return "";
    }

    // Emission rules:
    // - Direct pronunciation matches and their component entries use
    //   match_input_buffer=1, so they appear in both candidate selection and
    //   dictionary panels.
    // - Canonical redirects use match_input_buffer=0 even when collected while
    //   building the 1 group. Empty canonical columns mean the row is already
    //   canonical.
    // - If there are no direct matches for a non-sentence lookup, unmatched
    //   dictionary rows and their related entries are used as the fallback 1 group.
    // - If direct matches exist, unmatched canonical rows are kept with 0;
    //   unmatched noncanonical rows are kept only through their 0 canonical
    //   redirects.
    // - Deduplication ignores match_input_buffer and pronOrder, keeps the last
    //   collected position within each group, and lets the 1 group win over the
    //   0 group.
    // - pronOrder is not emitted; it only sorts within one honzi lookup.
    //   Related canonical/component lookups keep discovery order across honzi.
    vector<EmittedLine> candidateAndDictionaryRows, dictionaryOnlyRows;
    const bool hasOnlyUnmatchedWordEntries =
        !isSentence && matchedLines.empty() && !remainingLines.empty();
    if (hasOnlyUnmatchedWordEntries) {
        for (const pair<int, LookupLine>& line : remainingLines)
            AppendLineWithRelatedEntries(dictionary, candidateAndDictionaryRows, line.second,
                                         '1', '1', true, activeLookups);
    } else {
        for (const pair<int, LookupLine>& line : matchedLines)
            AppendLineWithRelatedEntries(dictionary, candidateAndDictionaryRows, line.second,
                                         '1', '1', true, activeLookups);
        for (const pair<int, LookupLine>& line : remainingLines)
            AppendCanonicalEntryOrRedirect(dictionary, dictionaryOnlyRows, line.second,
                                           '0', '0', false, activeLookups);
    }

    candidateAndDictionaryRows = DeduplicateRows(candidateAndDictionaryRows);
    std::unordered_set<string> insertedKeys;
    for (const EmittedLine& line : candidateAndDictionaryRows)
        insertedKeys.insert(line.dedupeKey);
    dictionaryOnlyRows = DeduplicateRows(dictionaryOnlyRows, &insertedKeys);

    string result;
    for (const EmittedLine& line : candidateAndDictionaryRows)
        result += line.commentLine;
    for (const EmittedLine& line : dictionaryOnlyRows)
        result += line.commentLine;
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
                                ",,,,,,,,,composition,,,,,,,," + entries);
    }
}

string DictionaryLookupFilter::ParseEntry(string honzi, string jyutping, const bool isSentence) {
    std::unordered_set<string> activeLookups;
    return rime::ParseEntry(dict_.get(), honzi, jyutping, isSentence,
                            activeLookups);
}

}  // namespace rime
