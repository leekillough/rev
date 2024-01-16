# sst-forza-elements/src/zip CMakeLists.txt
# this is not meant to be standalone
# this is triggered via the top-level CMakeLists.txt in forzarev

set(ZIPSrcs
zip.cc
zip_events.cc
zip_memctrl.cc
zip_nic.cc
zip.h
zip_events.h
zip_memctrl.h
zip_nic.h
zip_sst.h
)

message(STATUS "FORZA ZIP model using ZOPNET from ${REVCPU_INCLUDE_PATH}")

add_subdirectory(../../../common common)
add_library(ForzaZIP SHARED ${ZIPSrcs})
target_include_directories(ForzaZIP PRIVATE ${REVCPU_INCLUDE_PATH} PUBLIC ${SST_INSTALL_DIR}/include)

install(TARGETS ForzaZIP DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
install(CODE "execute_process(COMMAND sst-register ForzaZIP ForzaZIP_LIBDIR=${CMAKE_CURRENT_SOURCE_DIR})")

if( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  install(CODE "execute_process(COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_undefined_symbols.sh ${CMAKE_CURRENT_SOURCE_DIR}/libForzaZIP.dylib)")
else()
  install(CODE "execute_process(COMMAND_ERROR_IS_FATAL ANY COMMAND_ECHO STDERR COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_undefined_symbols.sh ${CMAKE_CURRENT_SOURCE_DIR}/libForzaZIP.so)")
endif()

# EOF
