project(InterpolatingSolver)
cmake_minimum_required(VERSION 3.8)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message (FATAL_ERROR
    "In-source builds are not allowed. Please clean your source tree and try again.")
endif()

# check for rt lib. Not needed on OSX.
find_library(RT_LIB NAMES rt)
if (NOT RT_LIB)
  set(RT_LIB "")
endif()
mark_as_advanced(RT_LIB)

# Add path for custom modules
set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
)

find_package(Abc REQUIRED)
if (ABC_FOUND)
  include_directories(${ABC_INCLUDE_DIR})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ABC_CXXFLAGS}")
  add_definitions (-DABC_NAMESPACE=${ABC_NAMESPACE})
else()
  set(ABC_LIBRARY "")
endif ()

find_package(Minisat REQUIRED)
if (MINISAT_FOUND)
  include_directories(${MINISAT_INCLUDE_DIR})
else()
  set (MINISAT_LIBRARY "")
endif()

find_package(Avy REQUIRED)
if (AVY_FOUND)
  include_directories(${AVY_SOURCE_DIR} ${AVY_INCLUDE_DIR})
endif ()

add_subdirectory(src)
