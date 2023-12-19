add_rev_test(SCRATCHPAD_BASIC scratchpad_basic 30 "all;memh;rv64;forza;scratchpad" SCRIPT "run_scratchpad_basic.sh")
add_rev_test(SCRATCHPAD_OVERFLOW scratchpad_overflow 30 "all;memh;rv64;forza;scratchpad" SCRIPT "run_scratchpad_overflow.sh")
add_rev_test(SCRATCHPAD_MULTI_ALLOC_OVERFLOW scratchpad_multi_alloc_overflow 30 "all;memh;rv64;forza;scratchpad" SCRIPT "run_scratchpad_multi_alloc_overflow.sh")
