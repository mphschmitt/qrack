find_package (pybind11)
if (NOT pybind11_FOUND)
    set (ENABLE_PYBIND11 OFF)
endif()

if (ENABLE_PYBIND11)
    add_library (pureqrack SHARED
        src/pureqrack.cpp
        )

    execute_process(COMMAND "echo $(python3-config --extension-suffix)" OUTPUT_VARIABLE PYBIND11_EXTENSION)
    set_target_properties(pureqrack PROPERTIES
        SUFFIX "${PYBIND11_EXTENSION}" )
    set_target_properties (pureqrack PROPERTIES
        VERSION ${PROJECT_VERSION}
        )

    target_link_libraries (pureqrack ${QRACK_LIBS})
    target_include_directories(pureqrack PRIVATE ${pybind11_INCLUDE_DIRS})
    target_compile_options (pureqrack PUBLIC ${QRACK_COMPILE_OPTS})
    target_compile_definitions(pureqrack PUBLIC -DDLL_EXPORTS)
endif (ENABLE_PYBIND11)
