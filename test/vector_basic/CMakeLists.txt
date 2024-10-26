if (SSTPIM_ENABLE_TESTING)

    add_test(NAME "PIM-REV"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND run.bash 0)
    set_tests_properties("PIM-REV"
        PROPERTIES
        TIMEOUT 15
        LABELS "all"
    )

    add_test(NAME "PIM-APPX"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND run.bash 1)
    set_tests_properties("PIM-APPX"
        PROPERTIES
        TIMEOUT 15
        LABELS "all"
    )
endif()

# EOF
