cmake_minimum_required(VERSION 3.16)
project(YComp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ycomp src/main.cpp)
install(TARGETS ycomp RUNTIME DESTINATION bin)

enable_testing()
add_test(
    NAME ycomp_integration
    COMMAND ${CMAKE_COMMAND}
        "-DYCOMP=$<TARGET_FILE:ycomp>"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/integration"
        "-DCMAKE_COMMAND=${CMAKE_COMMAND}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.cmake
)
