
# if(NOT CTEST_FORZA_TESTS)
#   set_tests_properties (NOC_DISCOVERY PROPERTIES DISABLED True)
#   message(NOTICE "   * Tests requiring FORZA are disabled (ie. -DCTEST_FORZA_REQUIRED_TESTS=OFF)
#     - If you'd like to still run these tests run: `ctest -L forza_required`")
#   message(VERBOSE "NOC_DISCOVERY - Disabled")
# endif()
add_rev_test(forza_noc_discovery forza_noc_discovery 30 "all;memh;rv64;loader;forza" SCRIPT "run_noc_discovery.sh")
add_rev_test(forza_amoadd_c forza_amoadd_c 30 "all;memh;rv64;loader;forza" SCRIPT "run_amoadd_c.sh")
