# sst-forza-elements/src/zen CMakeLists.txt
# this is not meant to be standalone
# this is triggered via the top-level CMakeLists.txt in forzarev

set(ZENSrcs
zen.cc
zen.h
zen_sst.h
)

add_subdirectory(../../../common common)
add_library(forzazen SHARED ${ZENSrcs})
target_include_directories(forzazen PRIVATE ${REVCPU_INCLUDE_PATH} PUBLIC ${SST_INSTALL_DIR}/include)

install(TARGETS forzazen DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
install(CODE "execute_process(COMMAND forzazen forzazen_LIBDIR=${CMAKE_CURRENT_SOURCE_DIR})")

if( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  install(CODE "execute_process(COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_undefined_symbols.sh ${CMAKE_CURRENT_SOURCE_DIR}/libforzazen.dylib)")
else()
  install(CODE "execute_process(COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_undefined_symbols.sh ${CMAKE_CURRENT_SOURCE_DIR}/libforzazen.so)")
endif()

# EOF
