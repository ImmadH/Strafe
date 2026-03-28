cmake_minimum_required(VERSION 3.20)

project(Strafe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

find_package(Vulkan REQUIRED)
add_subdirectory(vendor/glfw)

add_executable(Strafe
    src/main.cpp
    src/app.cpp
    src/instance.cpp
)

target_link_libraries(Strafe PRIVATE
    Vulkan::Vulkan
    glfw
)

target_include_directories(Strafe PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(Strafe PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)