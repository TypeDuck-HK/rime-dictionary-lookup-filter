project(rime-multi-reverse-lookup-filter)
cmake_minimum_required(VERSION 2.8)

aux_source_directory(src RIME_MULTI_REVERSE_SRC)

add_library(rime-multi-reverse-lookup-filter-objs OBJECT ${RIME_MULTI_REVERSE_SRC})
if(BUILD_SHARED_LIBS)
  set_target_properties(rime-multi-reverse-lookup-filter-objs
    PROPERTIES 
    POSITION_INDEPENDENT_CODE ON)
endif()

set(plugin_name "rime-multi-reverse-lookup-filter" PARENT_SCOPE)
set(plugin_objs $<TARGET_OBJECTS:rime-multi-reverse-lookup-filter-objs> PARENT_SCOPE)
set(plugin_deps ${rime_library} PARENT_SCOPE)
set(plugin_modules "multi_reverse_lookup" PARENT_SCOPE)
