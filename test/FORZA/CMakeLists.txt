
# Macro to build and configure tests in rev based on the labels the test was specified with
# Config vals is a list of options to be passed to the forza config script
# in the order of hartsperzap, zaps, zones, precicnts, program, and shape
macro(add_forza_test test_name test_dir timeout labels config_vals)
  string(TOLOWER ${test_dir} test_dir_lower)
  string(TOLOWER ${test_name} test_name_lower)

  # Parse arguments for an optional script
  set(optional_script "")
  cmake_parse_arguments(ARG "" "SCRIPT" "" ${ARGN})
  if(ARG_SCRIPT)
    set(optional_script ${ARG_SCRIPT})
  endif()

  # Build the test and generate disassembly
  add_custom_target(build_${test_name_lower}
    COMMAND ${CMAKE_MAKE_PROGRAM} clean
    COMMAND ${CMAKE_MAKE_PROGRAM}
    COMMAND ${RVOBJDUMP} -dC -Mno-aliases --source *.exe > ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir_lower}/${test_name_lower}.asm
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir_lower}
    COMMENT "Building and disassembling ${test_name_lower} test"
  )

  # Initialize configuration variables
  #set(machine_args "\"[CORES:RV64GC]\"")
  #set(numHartsList 1)
  #set(numCoresList 1)
  #set(add_memh_test FALSE)
  #set(startSymbol "\"[0:main]\"")

  set(test_level 1)
  string(FIND "${labels}" "test_level=2" test_level_2_index)
  if(NOT ${test_level_2_index} EQUAL -1)
    set(test_level 2)
  endif()
  string(FIND "${labels}" "test_level=3" test_level_3_index)
  if(NOT ${test_level_3_index} EQUAL -1)
    set(test_level 3)
  endif()

  # set empty strings as forza defaults
  # the real defaults will be in the config script
  set(hpz_option "")        # Harts per zap
  set(hpz_suffix "")
  set(zaps_option "")       # Zaps per zone
  set(zaps_suffix "")
  set(zones_option "")      # Zones per precinct
  set(zones_suffix "")
  set(precincts_option "")  # Number of precincts
  set(precincts_suffix "")
  set(program_option "")    # Exeuctable program
  set(program_suffix "")
  set(shape_option "")      # System network shape
  set(shape_suffix "")

  string(REPLACE "=" ";" config_list "${config_vals}")
  list(LENGTH config_list config_len)
  list(FIND config_list "hartsperzap" hpz_index)
  if(NOT ${hpz_index} EQUAL -1)
    math(EXPR hpz_index "${hpz_index} + 1")
    list(GET config_list "${hpz_index}" hpz_val)
  else()
    set(hpz_val "1")
  endif()
  list(FIND config_list "zaps" zaps_index)
  if(NOT ${zaps_index} EQUAL -1)
    math(EXPR zaps_index "${zaps_index} + 1")
    list(GET config_list "${zaps_index}" zaps_val)
  else()
    set(zaps_val "1")
  endif()
  list(FIND config_list "zones" zones_index)
  if(NOT ${zones_index} EQUAL -1)
    math(EXPR zones_index "${zones_index} + 1")
    list(GET config_list "${zones_index}" zones_val)
  else()
    set(zones_val "1")
  endif()
  list(FIND config_list "precincts" precincts_index)
  if(NOT ${precincts_index} EQUAL -1)
    math(EXPR precincts_index "${precincts_index} + 1")
    list(GET config_list "${precincts_index}" precincts_val)
  else()
    set(precincts_val "1")
  endif()
  list(FIND config_list "program" program_index)
  if(NOT ${program_index} EQUAL -1)
    math(EXPR program_index "${program_index} + 1")
    list(GET config_list "${program_index}" program_val)
  else()
    # keep cmake default if not set
    set(program_val "${test_name_lower}")
  endif()
  list(FIND config_list "shape" shape_index)
  if(NOT ${shape_index} EQUAL -1)
    math(EXPR shape_index "${shape_index} + 1")
    list(GET config_list "${shape_index}" shape_val)
  else()
    set(shape_val "2,2:4")
  endif()
  list(FIND config_list "progargs" args_index)
  if(NOT ${args_index} EQUAL -1)
    math(EXPR args_index "${args_index} + 1")
    list(GET config_list "${args_index}" args_val)
    string(REPLACE " " ";" args_val ${args_val})
  else()
    set(args_val "")
  endif()
  set(test_name_suffix "${test_name_lower}_${hpz_val}hpz_${zaps_val}zaps_${zones_val}zones_${precincts_val}precincts")

  if(NOT optional_script)

    # Define forza target with the new naming convention
    add_custom_target(run_${test_name_lower}_${test_name_suffix}
      COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir_lower} && sst --add-lib-path=${CMAKE_SOURCE_DIR}/build/src/ ${CMAKE_SOURCE_DIR}/test/FORZA/forza-test-config.py -- --program="${program_val}.exe" --hartsperzap="${hpz_val}" --zaps="${zaps_val}" --zones="${zones_val}" --precincts="${precincts_val}" --shape="${shape_val}" --progargs="${args_val}"
      DEPENDS build_${test_name_lower}
      COMMENT "Running ${test_name_lower} test with ${hpz_val} harts per ZAP, ${zaps_val} ZAPs, ${zones_val} zones, and ${precincts_val} precincts
      executing program ${program_val} with shape ${shape_val}"
    )
    add_test(NAME ${test_name_lower}_${test_name_suffix} COMMAND ${CMAKE_COMMAND} --build . --target run_${test_name_lower}_${test_name_suffix})

    if(((NOT ${rv32_label_index} EQUAL -1) AND DEFINED RV32_DISABLED) OR test_level GREATER TEST_LEVEL)
      set_tests_properties(${test_name_lower}_${test_name_suffix} PROPERTIES DISABLED TRUE)
    endif()

    # NOTE: every forza test uses memHierarchy so it's now hardcoded in the config

    set_tests_properties(${test_name_lower}_${test_name_suffix}
      PROPERTIES
      ENVIRONMENT "RVCC=${RVCC};RVCXX=${RVCXX}"
      TIMEOUT ${timeout}
      PASS_REGULAR_EXPRESSION "${passRegex}"
      LABELS "${labels}"
)
  else()
    # Custom target for running the specified script
    #add_custom_target(run_${test_name_lower}_script
   #   COMMAND ${CMAKE_COMMAND} -E echo "Executing script ${optional_script}"
   #   COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/${test_name_lower} && bash ${optional_script}
   #   DEPENDS build_${test_name_lower}
   #   COMMENT "Running script for ${test_name_lower} test"
   # )
    string(FIND "${labels}" "security" security_label_index)

    if(NOT ${security_label_index} EQUAL -1)
      set(passRegex "Invalid Physical Address Access")
    else()
      set(passRegex "Simulation is complete")
    endif()

    add_custom_target(run_${test_name_lower}_script
      COMMAND ${CMAKE_COMMAND} -E echo "Executing script ${optional_script}"
      COMMAND bash ${optional_script}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir_lower}
      DEPENDS build_${test_name_lower}
      COMMENT "Running script for ${test_name_lower} test"
    )

    # Add the custom target as a test
    add_test(NAME ${test_name_lower}_script COMMAND ${CMAKE_COMMAND} --build . --target run_${test_name_lower}_script)
    # Set test properties for the script test
    set_tests_properties(${test_name_lower}_script
      PROPERTIES
      ENVIRONMENT "RVCC=${RVCC};RVCXX=${RVCXX}"
      TIMEOUT ${timeout}
      PASS_REGULAR_EXPRESSION "${passRegex}"
      LABELS "${labels}"
    )
    if(((NOT ${rv32_label_index} EQUAL -1) AND DEFINED RV32_DISABLED) OR test_level GREATER TEST_LEVEL)
      set_tests_properties(${test_name_lower}_script PROPERTIES DISABLED TRUE)
    endif()
  endif()
endmacro()

#TODO: need to add the other noc discovery variants(?)
add_forza_test(forza_noc_discovery1 forza_noc_discovery 120 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precints=1;shape=1,1:1;program=ex2")
add_forza_test(forza_noc_discovery2 forza_noc_discovery 120 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=4;zones=1;precints=1;shape=1,1:1;program=ex2")
add_forza_test(forza_noc_discovery3 forza_noc_discovery 120 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=4;zones=2;precints=1;shape=1,1:1;program=ex2")

add_forza_test(forza_amoadd_c forza_amoadd_c 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precincts=1;shape=1,1:1;program=amoadd_c")
add_forza_test(forza_recv forza_recv 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precincts=1;shape=1,1:1")
add_forza_test(forza_send forza_send 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precincts=1;shape=1,1:1")
add_forza_test(forza_zen_setup forza_zen_setup 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=4;zones=2;precincts=1;shape=1,1:1")
add_forza_test(forza_zen_setup_cxx forza_zen_setup 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=4;zones=2;precincts=1;shape=1,1:1")
add_forza_test(forza_zqm_setup forza_zqm_setup 30 "all;memh;rv64;loader;forza" "hartsperzap=4;zaps=4;zones=2;precincts=1;shape=1,1:1")

add_forza_test(forza_python1 forza_python 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precincts=8;shape=2,2:4;program=ex2")
add_forza_test(forza_python2 forza_python 30 "all;memh;rv64;loader;forza" "hartsperzap=512;zaps=1;zones=1;precincts=8;shape=2,2:4;program=ex2")
add_forza_test(forza_python3 forza_python 60 "all;memh;rv64;loader;forza" "hartsperzap=512;zaps=2;zones=1;precincts=8;shape=2,2:4;program=ex2")
add_forza_test(forza_python4 forza_python 60 "all;memh;rv64;loader;forza" "hartsperzap=512;zaps=2;zones=2;precincts=8;shape=2,2:4;program=ex2")
add_forza_test(forza_python5 forza_python 120 "all;memh;rv64;loader;forza" "hartsperzap=512;zaps=2;zones=4;precincts=8;shape=2,2:4;program=ex2")

add_forza_test(forza_harts_per_zap forza_config_syscalls 30 "all;memh;rv64;loader;forza" "hartsperzap=128;zaps=1;zones=1;precincts=1;shape=1,1:1;program=ex2;progargs=128")
add_forza_test(forza_config_syscalls forza_config_syscalls 60 "all;memh;rv64;loader;forza" "hartsperzap=64;zaps=1;zones=1;precincts=1;shape=1,1:1;program=config_ecalls;progargs=64 1 8 3 0 0 0")

add_forza_test(forza_argc forza_argc 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precincts=1;shape=1,1:1;progargs=one")

add_forza_test(forza_barrier1 forza_barrier 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=1;zones=1;precincts=1;shape=1,1:1;program=forza_barrier_test_1hart")
add_forza_test(forza_barrier2 forza_barrier 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=4;zones=1;precincts=1;shape=1,1:1;program=forza_barrier_test_4zap")
add_forza_test(forza_barrier_multi forza_barrier 30 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=4;zones=1;precincts=1;shape=1,1:1;program=forza_barrier_test_multi_barrier")

add_forza_test(forza_zen_flow forza_zen_flow 60 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=2;zones=1;precincts=1;shape=1,1:1")

add_forza_test(forza_multisend forza_multisend 60 "all;memh;rv64;loader;forza" "hartsperzap=1;zaps=2;zones=1;precincts=1;shape=1,1:1")

#NOTE: security related tests are still using original rev test with custom script
#Security related test
add_rev_test(private_access_allowed_rza forza_security 30 "all;memh;rv64;loader;forza;" SCRIPT "run_private_valid.sh")
add_rev_test(private_access_denied_rza forza_security 30 "all;memh;rv64;loader;forza;security" SCRIPT "run_private_invalid.sh")
add_rev_test(shared_access_allowed forza_security 30 "all;memh;rv64;loader;forza;" SCRIPT "run_shared_valid.sh")
add_rev_test(shared_access_denied forza_security 30 "all;memh;rv64;loader;forza;security" SCRIPT "run_shared_invalid.sh")

