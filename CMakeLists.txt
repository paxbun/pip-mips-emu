# Copyright (c) 2021 Chanjung Kim (paxbun). All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.13)
project(pip-mips-emu VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Library definitions
add_library(pip-mips-emu STATIC
    ${PROJECT_SOURCE_DIR}/Source/Common.cc
    ${PROJECT_SOURCE_DIR}/Source/Emulator.cc
    ${PROJECT_SOURCE_DIR}/Source/File.cc
    ${PROJECT_SOURCE_DIR}/Source/Memory.cc
    ${PROJECT_SOURCE_DIR}/Source/NamedEntryMap.cc
)
target_include_directories(pip-mips-emu PUBLIC ${PROJECT_SOURCE_DIR}/Public)

# Executable definitions
add_executable(runfile ${PROJECT_SOURCE_DIR}/Source/Main.cc)
target_link_libraries(runfile pip-mips-emu)

# Unit tests
option(ENABLE_PIP_MIPS_EMU_TESTS "Enable unit tests" OFF)
if (ENABLE_PIP_MIPS_EMU_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    function(add_pip_mips_emu_test TEST_NAME)
        set(FILE_NAME ${TEST_NAME})
        set(EXE_NAME ${TEST_NAME})
        
        string(REGEX REPLACE "([^A-Z\-])([A-Z][A-Z]+)([A-Z][a-z])" "\\1-\\2-\\3" EXE_NAME "${EXE_NAME}")
        string(REGEX REPLACE "([A-Z]+)$" "-\\1" EXE_NAME "${EXE_NAME}")
        string(REGEX REPLACE "([^-])([A-Z][a-z]+)" "\\1-\\2" EXE_NAME "${EXE_NAME}")
        string(TOLOWER "${EXE_NAME}" EXE_NAME)
        
        add_executable(${EXE_NAME} "Tests/${FILE_NAME}.cc")
        target_link_libraries(${EXE_NAME} GTest::gtest_main pip-mips-emu)
        add_test(NAME ${TEST_NAME} COMMAND ${EXE_NAME})
        
        unset(FILE_NAME)
        unset(EXE_NAME)
        unset(TEST_NAME)
    endfunction()

    add_pip_mips_emu_test(FileTest)
    add_pip_mips_emu_test(MemoryTest)
    add_pip_mips_emu_test(NamedEntryMapTest)
endif()