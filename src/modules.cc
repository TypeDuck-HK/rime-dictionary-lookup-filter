//
//  modules.cc
//  rime-dictionary-lookup-filter-objs
//
//  Created by leung tsinam on 28/10/2022.
//

#include <rime/common.h>
#include <rime/registry.h>
#include <rime_api.h>
#include "DictionaryLookupFilter.hpp"
#include <rime/component.h>

static void rime_dictionary_lookup_initialize() {
    using namespace rime;
    
    LOG(INFO) << "registering components from module 'dictionary_lookup_filter'.";
    Registry& r = Registry::instance();
    
    r.Register("dictionary_lookup_filter", new Component<DictionaryLookupFilter>);
}

static void rime_dictionary_lookup_finalize() {}

RIME_REGISTER_MODULE(dictionary_lookup)
