cmake_minimum_required(VERSION 3.20)
project(Strafe VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(Vulkan REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

include_directories(
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)

link_directories(${GLFW_LIBRARY_DIRS})

add_executable(Strafe
  
    src/main.cpp
    src/app.cpp
    src/glfw.cpp
)

# Link libraries
target_link_libraries(Strafe
    ${Vulkan_LIBRARIES}
    ${GLFW_LIBRARIES}
)




