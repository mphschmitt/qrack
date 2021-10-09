find_package (pybind11)
if (NOT pybind11_FOUND)
    set (ENABLE_PYBIND11 OFF)
endif()

if (ENABLE_PYBIND11)
    pybind11_add_module(pureqrack SHARED
        src/pureqrack.cpp
        )

    target_link_libraries (pureqrack PRIVATE ${QRACK_LIBS})
    target_compile_options (pureqrack PUBLIC ${QRACK_COMPILE_OPTS})
    target_compile_definitions(pureqrack PUBLIC -DDLL_EXPORTS)
endif (ENABLE_PYBIND11)
