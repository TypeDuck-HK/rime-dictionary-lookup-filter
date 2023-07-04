//
//  modules.cc
//  rime-dictionary-lookup-filter-objs
//
//  Created by leung tsinam on 28/10/2022.
//

#include <rime/common.h>
#include <rime/registry.h>
#include <rime_api.h>
#include "dictionary_lookup_filter.hpp"
#include "PhonologyUniquifier.hpp"
#include <rime/component.h>

static void rime_multi_reverse_lookup_initialize() {
    using namespace rime;
    
    LOG(INFO) << "registering components from module 'dictionary_lookup_filter'.";
    Registry& r = Registry::instance();
    
    r.Register("dictionary_lookup_filter", new Component<DictionaryLookupFilter>);
    r.Register("phonology_uniquifier", new Component<PhonologyUniquifier>);
}

static void rime_multi_reverse_lookup_finalize() {}

RIME_REGISTER_MODULE(multi_reverse_lookup)
