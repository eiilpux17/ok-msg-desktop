cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif(POLICY CMP0091)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif(POLICY CMP0135)


set(FETCHCONTENT_QUIET OFF)
include(FetchContent)
include(ExternalProject)

project(OkMSGProject)

# 设置默认构建类型为Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

get_filename_component(third_party_loc "3rdparty" REALPATH)
get_filename_component(submodules_loc "src" REALPATH)
get_filename_component(cmake_helpers_loc "cmake" REALPATH)

include(cmake/init_target.cmake)
include(cmake/generate_target.cmake)
include(cmake/generate_appdata_changelog.cmake)
include(cmake/dependencies.cmake)
include(cmake/platform.cmake)
include(cmake/build.cmake)
include(cmake/develop.cmake)
include(cmake/test.cmake)
include(cmake/enable.cmake)

include_directories(.)

add_subdirectory(src)

if(ENABLE_PLUGINS)
  include(${PROJECT_SOURCE_DIR}/plugins/plugins.cmake)
endif()

