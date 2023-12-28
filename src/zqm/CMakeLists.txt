# sst-forza-elements/src/zqm CMakeLists.txt
# this is not meant to be standalone
# this is triggered via the top-level CMakeLists.txt in forzarev

set(ZQMSrcs
src/zqm.cc
src/zqm.h
src/zqm_sst.h
)

add_subdirectory(../../../common common)
add_library(forzazqm SHARED ${ZQMSrcs})
target_include_directories(forzazqm PRIVATE ${REVCPU_INCLUDE_PATH} PUBLIC ${SST_INSTALL_DIR}/include)

install(TARGETS forzazqm DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
install(CODE "execute_process(COMMAND forzazqm forzazqm_LIBDIR=${CMAKE_CURRENT_SOURCE_DIR})")

if( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  install(CODE "execute_process(COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_undefined_symbols.sh ${CMAKE_CURRENT_SOURCE_DIR}/libforzazqm.dylib)")
else()
  install(CODE "execute_process(COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_undefined_symbols.sh ${CMAKE_CURRENT_SOURCE_DIR}/libforzazqm.so)")
endif()

# EOF
