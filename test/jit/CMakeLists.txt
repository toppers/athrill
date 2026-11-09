# Unit tests of the experimental JIT (src/main/jit), without a target:
#   cmake -S test/jit -B build-jit-test -DATHRILL_SLJIT_DIR=<sljit checkout>
#   cmake --build build-jit-test && ctest --test-dir build-jit-test --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(athrill_jit_test C)

get_filename_component(ATHRILL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(ATHRILL_SLJIT_DIR "${ATHRILL_ROOT}/../sljit" CACHE PATH "sljit checkout")
get_filename_component(ATHRILL_SLJIT_DIR "${ATHRILL_SLJIT_DIR}" ABSOLUTE)
if(NOT EXISTS "${ATHRILL_SLJIT_DIR}/sljit_src/sljitLir.c")
    message(FATAL_ERROR "sljit not found at ${ATHRILL_SLJIT_DIR} (set ATHRILL_SLJIT_DIR)")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
add_executable(athrill_jit_test
    jit_test.c
    "${ATHRILL_ROOT}/src/main/jit/athrill_jit.c"
    "${ATHRILL_SLJIT_DIR}/sljit_src/sljitLir.c")
target_include_directories(athrill_jit_test PRIVATE
    "${ATHRILL_ROOT}/src/inc"
    "${ATHRILL_ROOT}/src/main/jit"
    "${ATHRILL_SLJIT_DIR}/sljit_src")
# SLJIT_DEBUG 1: sljit checks every operation's arguments (catches misuse in tests)
target_compile_definitions(athrill_jit_test PRIVATE
    SLJIT_CONFIG_AUTO=1 SLJIT_DEBUG=1 SLJIT_VERBOSE=0 ATHRILL_ADDRESS_WIDTH=32)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(athrill_jit_test PRIVATE -O2 -g -Wall -Wextra -Wno-unused-parameter)
endif()

enable_testing()
add_test(NAME athrill-jit COMMAND athrill_jit_test)
