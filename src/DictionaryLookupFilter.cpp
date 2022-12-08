//
//  MultiReverseLookupFilter.cpp
//  rime-multi-reverse-lookup-filter-objs
//
//  Created by leung tsinam on 28/10/2022.
//

#include "dictionary_lookup_filter.hpp"

#include <rime/candidate.h>
#include <rime/engine.h>
#include <rime/schema.h>
#include <rime/translation.h>
#include <rime/dict/reverse_lookup_dictionary.h>
#include <rime/dict/dictionary.h>
#include <rime/gear/translator_commons.h>
#include <algorithm>
namespace rime {

class MultiReverseLookupFilterTranslation : public CacheTranslation {
public:
    MultiReverseLookupFilterTranslation(an<Translation> translation,
                                        DictionaryLookupFilter* filter)
    : CacheTranslation(translation), filter_(filter) {
    }
    virtual an<Candidate> Peek();
    
protected:
    DictionaryLookupFilter* filter_;
};

an<Candidate> MultiReverseLookupFilterTranslation::Peek() {
    auto cand = CacheTranslation::Peek();
    if (cand) {
        filter_->Process(cand);
    }
    return cand;
}

DictionaryLookupFilter::DictionaryLookupFilter(const Ticket& ticket)
: Filter(ticket), TagMatching(ticket) {
    if (ticket.name_space == "filter") {
        name_space_ = "dictionary_lookup_filter";
    } else {
        name_space_ = ticket.name_space;
    }
    
    if(Config* config = ticket.engine->schema()->config()) {
        config->GetString(name_space_ + "/dictionary", &dictname_);
    }
}

void DictionaryLookupFilter::Initialize() {
    initialized_ = true;
    if (!engine_)
        return;
    
    if(DictionaryComponent* dictionary = dynamic_cast<DictionaryComponent*>( Dictionary::Require("dictionary")))
    {
        
        dict_.reset(dictionary->Create(dictname_, dictname_, {}));
        if (dict_)
            dict_->Load();
    }

    if (Config* config = engine_->schema()->config()) {
        comment_formatter_.Load(config->GetList(name_space_ + "/comment_format"));
    }
}

an<Translation> DictionaryLookupFilter::Apply(
                                                an<Translation> translation, CandidateList* candidates) {
    if (!initialized_) {
        Initialize();
    }
    if(!dict_)
    {
        return translation;
    }
    return New<MultiReverseLookupFilterTranslation>(translation, this);
}

void DictionaryLookupFilter::Process(const an<Candidate>& cand) {
    if(dict_ == nullptr)
    {
        return;
    }

    auto phrase = As<Phrase>(Candidate::GetGenuineCandidate(cand));
    if (!phrase)
        return;
    string spellingCode = phrase->comment();
    const string& text = cand->text();
    rime::DictEntryIterator it;
    
    spellingCode.erase(std::remove(spellingCode.begin(), spellingCode.end(), ' '), spellingCode.end());
    string queryCode = text + "|" + spellingCode;   // to sperate the jyutping and json temporarily
    dict_->LookupWords(&it, queryCode, false);
    if(!it.exhausted())
    {
        string dictRes = it.Peek()->text;
        //comment_formatter_.Apply(&dictRes);
        if(dictRes.empty())
        {
            return;
        }
        phrase->set_comment(dictRes);
    }
}

}  // namespace rime
