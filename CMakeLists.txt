# sst-forza-elements top level CMakeLists.txt
# this is not meant to be standalone
# this is triggered via the top-level CMakeLists.txt in forzarev

set(ZEN_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/include" CACHE PATH "ZEN include path")

#add_subdirectory(include)
add_subdirectory(src)
