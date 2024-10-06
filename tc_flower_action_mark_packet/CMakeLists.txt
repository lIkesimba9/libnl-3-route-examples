cmake_minimum_required(VERSION 3.0)
project(tc_flower_action_mark_packet)

set(CMAKE_CXX_STANDARD 20)
set(NL_INCLUDE_DIRS /usr/include/libnl3)
add_executable(tc_flower_action_mark_packet main.cpp)



target_include_directories(${PROJECT_NAME} PUBLIC  ${NL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC  nl-3 nl-route-3)