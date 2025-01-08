cmake_minimum_required(VERSION 3.28)

# Set the project name
project(parallelBnBForPCmax)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(TBB REQUIRED)
    
set(BASEFILES
    src/customBnB/threadLocal/threadLocal.cpp
    src/customBnB/customBnB_base.hpp
)
set(SOURCES
    src/experiments/readData/readData.cpp
    src/experiments/lawrinenko_test2.cpp
)
set(CUSTOMSOURCES
    src/experiments/readData/readData.cpp
    src/experiments/Profiler.cpp
)

# define dependencies and compile flags
add_library(BnBdependencies INTERFACE)
target_link_libraries(BnBdependencies INTERFACE TBB::tbb)
target_include_directories(BnBdependencies INTERFACE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/growt
)
# required by growt
target_compile_options(BnBdependencies INTERFACE -mcx16)


# Add the main executable
add_executable(parallelBnBForPCmax ${BASEFILES} ${SOURCES})
target_link_libraries(parallelBnBForPCmax PRIVATE BnBdependencies)

# Add another project with different sources
add_executable(parallelBnBForPCmax_custom ${BASEFILES} ${CUSTOMSOURCES})
target_link_libraries(parallelBnBForPCmax_custom PRIVATE BnBdependencies tcmalloc_and_profiler profiler)
