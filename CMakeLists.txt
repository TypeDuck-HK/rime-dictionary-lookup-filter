project(rime-dictionary-lookup-filter)
cmake_minimum_required(VERSION 3.12)

aux_source_directory(src RIME_DICTIONARY_SRC)

add_library(rime-dictionary-lookup-filter-objs OBJECT ${RIME_DICTIONARY_SRC})
if(BUILD_SHARED_LIBS)
  set_target_properties(rime-dictionary-lookup-filter-objs
    PROPERTIES 
    POSITION_INDEPENDENT_CODE ON)
endif()

set(plugin_name "rime-dictionary-lookup-filter" PARENT_SCOPE)
set(plugin_objs $<TARGET_OBJECTS:rime-dictionary-lookup-filter-objs> PARENT_SCOPE)
set(plugin_deps ${rime_library} PARENT_SCOPE)
set(plugin_modules "dictionary_lookup" PARENT_SCOPE)
