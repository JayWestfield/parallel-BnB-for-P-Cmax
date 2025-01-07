cmake_minimum_required(VERSION 3.30)

# Set the project name
project(parallelBnBForPCmax)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(TBB REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_BINARY_DIR})

include_directories(${PROJECT_SOURCE_DIR}/external/growt)

# Add the growt submodule
add_subdirectory(${PROJECT_SOURCE_DIR}/external/growt)
set(BASEFILES 
    src/customBnB/threadLocal/threadLocal.cpp
    src/customBnB/customBnB_base.hpp
)# Add your source files
set(SOURCES
    src/experiments/readData/readData.cpp
    src/experiments/lawrinenko_test2.cpp
    # src/customBnB/globalValues.cpp
    # Add other source files here
)
set(CUSTOMSOURCES
    src/experiments/readData/readData.cpp
    src/customBnB/customBnB_base.hpp
)


# Add the main executable
add_executable(parallelBnBForPCmax ${BASEFILES} ${SOURCES})
target_link_libraries(parallelBnBForPCmax PRIVATE TBB::tbb)
target_include_directories(parallelBnBForPCmax PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/external/growt
)

# Add another project with different compiler flags
add_executable(parallelBnBForPCmax_custom ${BASEFILES} ${CUSTOMSOURCES})
target_link_libraries(parallelBnBForPCmax_custom PRIVATE TBB::tbb)
target_include_directories(parallelBnBForPCmax_custom PRIVATE
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/external/growt
)

# Add more projects as needed with different flags
# add_executable(parallelBnBForPCmax_debug ${SOURCES})
# target_link_libraries(parallelBnBForPCmax_debug PRIVATE TBB::tbb)
# target_include_directories(parallelBnBForPCmax_debug PRIVATE
#                           ${PROJECT_BINARY_DIR}
#                           ${PROJECT_SOURCE_DIR}/external/growt
#                           )
# set_sanitizer_flags(parallelBnBForPCmax_debug)
