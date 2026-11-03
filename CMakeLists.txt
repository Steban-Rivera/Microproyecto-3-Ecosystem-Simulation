cmake_minimum_required(VERSION 3.16)
project(Microproyecto3_Ecosystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find OpenMP
find_package(OpenMP REQUIRED)

# Common sources
set(COMMON_SOURCES
    src/animal.cpp
    src/animal.hpp
    src/input.cpp
    src/input.hpp
    src/statistics.cpp
    src/statistics.hpp
    src/ecosystem.cpp
    src/ecosystem.hpp
    src/sequential.cpp
    src/sequential.hpp
    src/parallel.cpp
    src/parallel.hpp
)

# Main Executable
add_executable(ecosystem_sim src/main.cpp ${COMMON_SOURCES})
target_include_directories(ecosystem_sim PRIVATE src)
target_link_libraries(ecosystem_sim PRIVATE OpenMP::OpenMP_CXX)

# Compiler warnings and optimizations
if(MSVC)
    target_compile_options(ecosystem_sim PRIVATE /W4 /O2 /permissive-)
else()
    target_compile_options(ecosystem_sim PRIVATE -Wall -Wextra -O3 -march=native)
endif()

# Automated Tests Executable
add_executable(ecosystem_tests tests/test_main.cpp ${COMMON_SOURCES})
target_include_directories(ecosystem_tests PRIVATE src)
target_link_libraries(ecosystem_tests PRIVATE OpenMP::OpenMP_CXX)

if(MSVC)
    target_compile_options(ecosystem_tests PRIVATE /W4 /O2 /permissive-)
else()
    target_compile_options(ecosystem_tests PRIVATE -Wall -Wextra -O3 -march=native)
endif()

enable_testing()
add_test(NAME AllEcosystemTests COMMAND ecosystem_tests)
