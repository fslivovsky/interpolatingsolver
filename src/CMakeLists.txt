set (EXT_LIBRARIES ${ABC_LIBRARY} ${MINISAT_LIBRARY})

add_library(interpolating_minisat InterpolatingSolver.cc)

target_link_libraries (interpolating_minisat
  ${CLAUSEITPSEQ_LIBRARY}
  ${ABC_CPP_LIBRARY}
  ${AVY_DEBUG_LIBRARY}
  ${EXT_LIBRARIES}
  m dl pthread z
  ${RT_LIB})

find_package(pybind11 CONFIG)
message(STATUS "Found pybind11 v${pybind11_VERSION}: ${pybind11_INCLUDE_DIRS}")
if (pybind11_FOUND)
  message(STATUS "Building PyBind Module.")
  pybind11_add_module(itp SHARED InterpolatingSolverBindings.cc)
  target_link_libraries(itp PRIVATE interpolating_minisat)
endif()
