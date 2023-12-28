# sst-forza-elements/src/zen CMakeLists.txt
# this is not meant to be standalone
# this is triggered via the top-level CMakeLists.txt in forzarev

set(ZQMSrcs
zqm.cc
zqm.h
zen_sst.h
)

add_subdirectory(../../../common common)
add_library(forza SHARED ${ZQMSrcs})
target_include_directories(forza PRIVATE ${REVCPU_INCLUDE_PATH} PUBLIC ${SST_INSTALL_DIR}/include)

install(TARGETS forza DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
install(CODE "execute_process(COMMAND forza forza_LIBDIR=${CMAKE_CURRENT_SOURCE_DIR})")

# EOF
