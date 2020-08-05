cmake_minimum_required(VERSION 3.1.3)
project(lock-free-parallel-hashing CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O3 -ggdb -Wall -Wextra -march=native -mtune=native)
#add_compile_options(-O0 -ggdb -Wall -Wextra -march=native -mtune=native)
link_libraries("-lpthread")

string ( REPLACE "/" "\/" remove_path_regex ${CMAKE_CURRENT_SOURCE_DIR} + "/")

set(GEN_SYS_HEADER_FILE ";${CMAKE_CURRENT_SOURCE_DIR}/src/gen_system_header.cc")
string ( REPLACE "/" "\/" REMOVE_GEN_SOURCE ${GEN_SYS_HEADER_FILE})

file(GLOB _EXE_SOURCES src/*.cc)
STRING( REGEX REPLACE "${REMOVE_GEN_SOURCE}" "" EXE_SOURCES ${_EXE_SOURCES})

set(SOURCES
  "lib/util/arg.cc")

include_directories(lib)

add_executable(gen_sys_header ${GEN_SYS_HEADER_FILE} ${SOURCES})
target_link_libraries(gen_sys_header)
add_custom_target(run_gen_sys_header
  COMMAND gen_sys_header
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating PRECOMPUTED_SYS_INFO.h"
  SOURCES ${GEN_SYS_HEADER_FILE}
  )

foreach(EXE_SRC_CODE ${EXE_SOURCES})
  STRING( REPLACE "src/" "" _exe1 ${EXE_SRC_CODE})
  STRING( REPLACE ".cc" "" _exe2 ${_exe1})
  string( REGEX REPLACE ${remove_path_regex} "" EXE ${_exe2} )
  add_executable(${EXE} ${EXE_SRC_CODE} ${SOURCES})
  target_link_libraries(${EXE})
  add_dependencies(${EXE} run_gen_sys_header)
endforeach(EXE_SRC_CODE ${EXE_SOURCES})



 include_directories(tests)
 
 file(GLOB TEST_SOURCES tests/*.cc)
 
#"^\/home\/noah\/programs\/parallel_hashtable\/tests/"
 
foreach(test_source ${TEST_SOURCES})
  string( REPLACE ".cc" "" _test_exe1 ${test_source} )
  string( REGEX REPLACE ${remove_path_regex} "" _test_exe2 ${_test_exe1} )
  string( REPLACE "/" "-" test_exe ${_test_exe2})
  add_executable( ${test_exe} ${test_source} )
  target_link_libraries(${test_exe})
  add_dependencies(${test_exe} run_gen_sys_header)
endforeach(test_source ${TEST_SOURCES})


