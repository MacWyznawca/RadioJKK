# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# idf_build_set_property(COMPILE_OPTIONS "-Wno-error=maybe-uninitialized;" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=stringop-truncation;" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=format-overflow=;" APPEND)


# idf_build_set_property(MINIMAL_BUILD ON)
project(RadioJKK)