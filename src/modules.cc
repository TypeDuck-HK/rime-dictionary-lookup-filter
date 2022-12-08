//
// Copyright CanCLID Developers
// Distributed under GPLv3
//

#include <rime/common.h>
#include <rime/registry.h>
#include <rime_api.h>
#include "dictionary_lookup_filter.hpp"
#include "PholonogyUniquefier.hpp"
#include <rime/component.h>
static void rime_multi_reverse_lookup_initialize() {
    using namespace rime;
    
    LOG(INFO) << "registering components from module 'dictionary_lookup_filter'.";
    Registry& r = Registry::instance();
    
    r.Register("dictionary_lookup_filter", new Component<DictionaryLookupFilter>);
    r.Register("phonology_uniquifier", new Component<PholonogyUniquifier>);
}

static void rime_multi_reverse_lookup_finalize() {}

RIME_REGISTER_MODULE(multi_reverse_lookup)
