//
//  MultiReverseLookupFilter.cpp
//  rime-multi-reverse-lookup-filter-objs
//
//  Created by leung tsinam on 28/10/2022.
//

#include "PholonogyUniquefier.hpp"

#include <rime/candidate.h>
#include <rime/engine.h>
#include <rime/schema.h>
#include <rime/translation.h>
#include <rime/dict/reverse_lookup_dictionary.h>
#include <rime/dict/dictionary.h>
#include <rime/gear/translator_commons.h>
#include <algorithm>
namespace rime {

class PholonogyUniquifiedTranslation : public CacheTranslation {
 public:
  PholonogyUniquifiedTranslation(an<Translation> translation,
                        CandidateList* candidates)
      : CacheTranslation(translation), candidates_(candidates) {
    Uniquify();
  }
  virtual bool Next();

 protected:
  bool Uniquify();

  an<Translation> translation_;
  CandidateList* candidates_;
};

bool PholonogyUniquifiedTranslation::Next() {
  return CacheTranslation::Next() && Uniquify();
}

static CandidateList::iterator find_text_match(const an<Candidate>& target,
                                               CandidateList::iterator begin,
                                               CandidateList::iterator end) {
  for (auto iter = begin; iter != end; ++iter) {
    if ((*iter)->text() == target->text() && (*iter)->comment() == target->comment()) {
      return iter;
    }
  }
  return end;
}

bool PholonogyUniquifiedTranslation::Uniquify() {
  while (!exhausted()) {
    auto next = Peek();
    CandidateList::iterator previous = find_text_match(
        next, candidates_->begin(), candidates_->end());
    if (previous == candidates_->end()) {
      // Encountered a unique candidate.
      return true;
    }
    auto uniquified = As<UniquifiedCandidate>(*previous);
    if (!uniquified) {
      *previous = uniquified =
          New<UniquifiedCandidate>(*previous, "uniquified");
    }
    uniquified->Append(next);
    CacheTranslation::Next();
  }
  return false;
}

PholonogyUniquifier::PholonogyUniquifier(const rime::Ticket &ticket) : Filter(ticket){
    
}

an<rime::Translation> PholonogyUniquifier::Apply(an<rime::Translation> translation, rime::CandidateList *candidates) { 
    return New<PholonogyUniquifiedTranslation>(translation, candidates);
}


}  // namespace rime

