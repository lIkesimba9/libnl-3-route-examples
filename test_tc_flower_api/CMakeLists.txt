cmake_minimum_required(VERSION 3.0)
project(test_tc_flower_api)

set(CMAKE_CXX_STANDARD 17)
set(NL_INCLUDE_DIRS /usr/include/libnl3)
add_executable(test_tc_flower_api main.cpp)


target_include_directories(${PROJECT_NAME} PUBLIC  ${NL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC  nl-3 nl-route-3)

