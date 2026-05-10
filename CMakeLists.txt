cmake_minimum_required(VERSION 3.16)
project(EmbeddedDesignPatterns)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

# Creational
add_subdirectory(01_factory)
add_subdirectory(02_object)
add_subdirectory(03_opaque)
add_subdirectory(04_singleton)

# Structural
add_subdirectory(05_callback)
add_subdirectory(06_inheritance)
add_subdirectory(07_virtual_api)

# Other / Concurrency
add_subdirectory(08_bridge)
add_subdirectory(09_scheduler)
add_subdirectory(10_spinlock)
add_subdirectory(11_mutex)
add_subdirectory(12_state_machine)

# ------------------------------------------------------------------
# Convenience target: run every demo in order
# Plain ASCII only in echo strings -- portable across all /bin/sh
# ------------------------------------------------------------------
add_custom_target(run_all
    COMMAND ${CMAKE_COMMAND} -E echo "=================================================="
    COMMAND ${CMAKE_COMMAND} -E echo "  EMBEDDED DESIGN PATTERNS IN C -- STUDY GUIDE"
    COMMAND ${CMAKE_COMMAND} -E echo "=================================================="

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [01] FACTORY METHOD --------------------------"
    COMMAND $<TARGET_FILE:01_factory>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [02] OBJECT PATTERN --------------------------"
    COMMAND $<TARGET_FILE:02_object>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [03] OPAQUE PATTERN --------------------------"
    COMMAND $<TARGET_FILE:03_opaque>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [04] SINGLETON --------------------------------"
    COMMAND $<TARGET_FILE:04_singleton>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [05] CALLBACK ---------------------------------"
    COMMAND $<TARGET_FILE:05_callback>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [06] INHERITANCE / POLYMORPHISM --------------"
    COMMAND $<TARGET_FILE:06_inheritance>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [07] VIRTUAL API ADAPTER -------------------"
    COMMAND $<TARGET_FILE:07_virtual_api>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [08] BRIDGE -----------------------------------"
    COMMAND $<TARGET_FILE:08_bridge>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [09] CONCURRENCY SCHEDULER -------------------"
    COMMAND $<TARGET_FILE:09_scheduler>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [10] SPINLOCK ---------------------------------"
    COMMAND $<TARGET_FILE:10_spinlock>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [11] MUTEX ------------------------------------"
    COMMAND $<TARGET_FILE:11_mutex>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "--- [12] BEHAVIORAL STATE MACHINE ----------------"
    COMMAND $<TARGET_FILE:12_state_machine>

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=================================================="
    COMMAND ${CMAKE_COMMAND} -E echo "  All patterns complete."
    COMMAND ${CMAKE_COMMAND} -E echo "=================================================="

    DEPENDS
        01_factory 02_object 03_opaque 04_singleton
        05_callback 06_inheritance 07_virtual_api
        08_bridge 09_scheduler 10_spinlock 11_mutex 12_state_machine
    COMMENT "Running all embedded design pattern demos"
)
