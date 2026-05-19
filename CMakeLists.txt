cmake_minimum_required(VERSION 3.22)

project(supre_robot_power
  VERSION 0.1.0
  LANGUAGES C CXX)

include(FetchContent)

option(POWER_SERIAL_BUILD_PYTHON "Build pybind11 Python bindings" ON)
option(POWER_SERIAL_BUILD_TESTS "Build tests" ON)
option(POWER_SERIAL_BUILD_EXAMPLES "Build examples" ON)
option(POWER_SERIAL_FETCH_WJWWOOD_SERIAL "Fetch and build wjwwood/serial" ON)

if(NOT POWER_SERIAL_FETCH_WJWWOOD_SERIAL)
  message(FATAL_ERROR "Only POWER_SERIAL_FETCH_WJWWOOD_SERIAL=ON is currently supported")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(wjwwood_serial
  URL https://github.com/wjwwood/serial/archive/refs/heads/main.tar.gz)
FetchContent_GetProperties(wjwwood_serial)
if(NOT wjwwood_serial_POPULATED)
  FetchContent_Populate(wjwwood_serial)
endif()

add_library(wjwwood_serial STATIC
  ${wjwwood_serial_SOURCE_DIR}/src/serial.cc
  ${wjwwood_serial_SOURCE_DIR}/src/impl/unix.cc
  ${wjwwood_serial_SOURCE_DIR}/src/impl/list_ports/list_ports_linux.cc)
set_target_properties(wjwwood_serial PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(wjwwood_serial PUBLIC ${wjwwood_serial_SOURCE_DIR}/include)
target_link_libraries(wjwwood_serial PUBLIC rt pthread)
target_compile_options(wjwwood_serial PRIVATE -Wno-deprecated)

add_library(power_serial
  src/power_serial.c
  src/power_serial_client.cpp)
set_target_properties(power_serial PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(power_serial
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_definitions(power_serial PRIVATE _DEFAULT_SOURCE)
target_compile_options(power_serial PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(power_serial PRIVATE wjwwood_serial)

if(POWER_SERIAL_BUILD_EXAMPLES)
  add_executable(power_serial_cli examples/power_serial_cli.c)
  target_link_libraries(power_serial_cli PRIVATE power_serial)
  set_target_properties(power_serial_cli PROPERTIES LINKER_LANGUAGE CXX)
endif()

if(POWER_SERIAL_BUILD_PYTHON)
  find_package(pybind11 CONFIG REQUIRED)
  pybind11_add_module(power_serial_py bindings/power_serial_py.cpp)
  set_target_properties(power_serial_py PROPERTIES OUTPUT_NAME power_serial)
  target_link_libraries(power_serial_py PRIVATE power_serial)
  target_compile_options(power_serial_py PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(POWER_SERIAL_BUILD_TESTS)
  enable_testing()
  add_executable(test_power_serial tests/test_power_serial.c)
  target_link_libraries(test_power_serial PRIVATE power_serial)
  set_target_properties(test_power_serial PROPERTIES LINKER_LANGUAGE CXX)
  add_test(NAME power_serial_core COMMAND test_power_serial)
  if(POWER_SERIAL_BUILD_PYTHON)
    add_test(NAME power_serial_python
      COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_power_serial_py.py)
    set_tests_properties(power_serial_python PROPERTIES
      ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}")
  endif()
endif()

install(TARGETS power_serial
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
