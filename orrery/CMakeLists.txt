cmake_minimum_required (VERSION 3.2)
project(liborrery VERSION 0.1.0.0 LANGUAGES CXX)

add_library(orrery
  src/mailbox.cpp
)

target_include_directories(orrery PUBLIC include)
target_link_libraries(orrery PUBLIC orrery_msgs)
find_package(CMFC REQUIRED)
add_subdirectory(cmf)
