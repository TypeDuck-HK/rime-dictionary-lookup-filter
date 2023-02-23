//
//  MultiReverseLookupFilter.hpp
//  rime-multi-reverse-lookup-filter-objs
//
//  Created by leung tsinam on 28/10/2022.
//

#ifndef PhonologyUniquefier_hpp
#define PhonologyUniquefier_hpp

#include <rime/common.h>
#include <rime/filter.h>
#include <rime/algo/algebra.h>
#include <rime/gear/filter_commons.h>
#include <rime/ticket.h>
#include <rime/dict/dictionary.h>


namespace rime {

class PhonologyUniquifier : public Filter{
public:
    explicit PhonologyUniquifier(const Ticket& ticket);

    virtual an<Translation> Apply(an<Translation> translation,
                                          CandidateList* candidates);
};
};

#endif /* PhonologyUniquefier_hpp */
