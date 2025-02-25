cmake_minimum_required(VERSION 3.28)

# Set the project name
project(parallelBnBForPCmax)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 20)
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




set(BASEFILES_Refactored
    src/_refactoredBnB/Solver/solver_base.hpp
)
set(SOURCES_Refactored
    src/experiments/readData/readData.cpp
    src/_refactoredBnB/executeSolver.cpp
)
#todo profiler
set(CUSTOMSOURCES_Refactored
    src/experiments/readData/readData.cpp
    src/experiments/Profiler.cpp
)

# define dependencies and compile flags
add_library(RefactoredBnBDependencies INTERFACE)
target_link_libraries(RefactoredBnBDependencies INTERFACE TBB::tbb)
target_include_directories(RefactoredBnBDependencies INTERFACE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/growt
    ${PROJECT_SOURCE_DIR}/external/task-based-workstealing
)
# required by growt
target_compile_options(RefactoredBnBDependencies INTERFACE -mcx16)


# Add the main executable
add_executable(RefactoredBnB ${BASEFILES_Refactored} ${SOURCES_Refactored})
target_link_libraries(RefactoredBnB PRIVATE RefactoredBnBDependencies)

# Add another project with different sources
add_executable(RefactoredBnB_custom ${BASEFILES_Refactored} ${CUSTOMSOURCES_Refactored})
target_link_libraries(RefactoredBnB_custom PRIVATE BnBdependencies tcmalloc_and_profiler profiler)

# Add the main executable
add_executable(full_refactored src/experiments/readData/readData.cpp
    src/_refactoredBnB/main.cpp)
target_link_libraries(full_refactored PRIVATE RefactoredBnBDependencies)

add_dependencies(full_refactored RefactoredBnB)
