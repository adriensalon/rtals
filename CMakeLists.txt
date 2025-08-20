
cmake_minimum_required(VERSION 3.25.0)
project(rtals VERSION 3.7.1 DESCRIPTION "rtals")

option(RTALS_BUILD_DEMO "Build the demo executable" ON)

# rtals library
set(rtals_source
    source/rtals.cpp
)
add_library(rtals STATIC ${rtals_source})
set_target_properties(rtals PROPERTIES CXX_STANDARD 17)
target_include_directories(rtals PUBLIC include)

# demo executable
if(RTALS_BUILD_DEMO)
    add_executable(rtals_demo demo/main.cpp)
    set_target_properties(rtals_demo PROPERTIES CXX_STANDARD 17)
    target_link_libraries(rtals_demo PUBLIC rtals)
endif()
