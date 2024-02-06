
# if(NOT CTEST_FORZA_TESTS)
#   set_tests_properties (NOC_DISCOVERY PROPERTIES DISABLED True)
#   message(NOTICE "   * Tests requiring FORZA are disabled (ie. -DCTEST_FORZA_REQUIRED_TESTS=OFF)
#     - If you'd like to still run these tests run: `ctest -L forza_required`")
#   message(VERBOSE "NOC_DISCOVERY - Disabled")
# endif()
add_rev_test(forza_noc_discovery forza_noc_discovery 120 "all;memh;rv64;loader;forza" SCRIPT "run_noc_discovery.sh")
add_rev_test(forza_amoadd_c forza_amoadd_c 30 "all;memh;rv64;loader;forza" SCRIPT "run_amoadd_c.sh")
add_rev_test(forza_recv forza_recv 30 "all;memh;rv64;loader;forza" SCRIPT "run_recv.sh")
add_rev_test(forza_send forza_send 30 "all;memh;rv64;loader;forza" SCRIPT "run_send.sh")
add_rev_test(forza_zen_setup forza_zen_setup 30 "all;memh;rv64;loader;forza" SCRIPT "run_zen_setup.sh")
add_rev_test(forza_zqm_setup forza_zqm_setup 30 "all;memh;rv64;loader;forza" SCRIPT "run_zqm_setup.sh")
add_rev_test(forza_python forza_python 100 "all;memh;rv64;loader;forza" SCRIPT "run_python.sh")
add_rev_test(forza_config_syscalls forza_config_syscalls 60 "all;memh;rv64;loader;forza" SCRIPT "run_test_config_syscalls.sh")
add_rev_test(forza_argc forza_argc 30 "all;memh;rv64;loader;forza" SCRIPT "run_forza_argc.sh")

#Security related test
add_rev_test(private_access_allowed_rza forza_security 30 "all;memh;rv64;loader;forza;" SCRIPT "run_private_valid.sh")
add_rev_test(private_access_denied_rza forza_security 30 "all;memh;rv64;loader;forza;security" SCRIPT "run_private_invalid.sh")
add_rev_test(shared_access_allowed forza_security 30 "all;memh;rv64;loader;forza;" SCRIPT "run_shared_valid.sh")
add_rev_test(shared_access_denied forza_security 30 "all;memh;rv64;loader;forza;security" SCRIPT "run_shared_invalid.sh")

