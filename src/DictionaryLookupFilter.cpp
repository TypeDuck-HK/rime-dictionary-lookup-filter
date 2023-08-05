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
#include <algorithm>
#include <unordered_set>

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
    if (startPos == string::npos)
        return;
    const string& text = cand->text();
    rime::DictEntryIterator it;

    string jyutping = spellingCode.substr(startPos + 1);
    boost::remove_erase_if(jyutping, boost::is_any_of(" \f"));
    std::unordered_set<string> pronunciations;
    boost::split(pronunciations, jyutping, boost::is_any_of(";"));
    string result = "";
    dict_->LookupWords(&it, text, false);
    if (it.exhausted())
        return;
    do {
        string line = it.Peek()->text;
        if (line.empty())
            continue;
        string pronunciation = line.substr(0, line.find(','));
        result += "\r";
        result += pronunciations.find(pronunciation) != pronunciations.end() ? "1," : "0,";
        result += line;
    } while (it.Next());
    if (!result.empty())
        phrase->set_comment(spellingCode.substr(0, startPos + 1) + result);
}

}  // namespace rime
