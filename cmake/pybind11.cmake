find_package (pybind11)
if (NOT pybind11_FOUND)
    set (ENABLE_PYBIND11 OFF)
endif()

if (ENABLE_PYBIND11)
    add_library (qrack_pybind11 SHARED
        src/pybind11_api.cpp
        )

    execute_process(COMMAND "echo $(python3-config --extension-suffix)" OUTPUT_VARIABLE PYBIND11_EXTENSION)
    set_target_properties(qrack_pybind11 PROPERTIES
        SUFFIX "${PYBIND11_EXTENSION}" )
    set_target_properties (qrack_pybind11 PROPERTIES
        VERSION ${PROJECT_VERSION}
        )

    target_link_libraries (qrack_pybind11 ${QRACK_LIBS})
    target_include_directories(qrack_pybind11 PRIVATE ${pybind11_INCLUDE_DIRS})
    target_compile_options (qrack_pybind11 PUBLIC ${QRACK_COMPILE_OPTS})
    target_compile_definitions(qrack_pybind11 PUBLIC -DDLL_EXPORTS)
endif (ENABLE_PYBIND11)
