cmake_minimum_required(VERSION 3.15)
project(quant_hub VERSION 1.0.0 LANGUAGES CXX)

option(WITH_CUDA "Build with CUDA support" OFF)
option(WITH_OPENCL "Build with OpenCL support" ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)

if(WITH_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 14)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    find_package(CUDA REQUIRED)
endif()

if(WITH_OPENCL)
    find_package(OpenCL REQUIRED)
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${GTEST_INCLUDE_DIRS}
)

if(WITH_CUDA)
    include_directories(${CUDA_INCLUDE_DIRS})
endif()

if(WITH_OPENCL)
    include_directories(${OpenCL_INCLUDE_DIRS})
endif()

# Enable testing
enable_testing()

# Add subdirectories
add_subdirectory(src)

# Add test directory
add_subdirectory(tests)
