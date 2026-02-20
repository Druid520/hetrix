cmake_minimum_required(VERSION 3.14)
project(hetrix VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall -Wextra -DNDEBUG")

add_executable(hetrix src/main.cpp)
find_package(Threads REQUIRED)
target_link_libraries(hetrix PRIVATE Threads::Threads stdc++fs)
target_include_directories(hetrix PRIVATE src)

install(TARGETS hetrix DESTINATION bin)
