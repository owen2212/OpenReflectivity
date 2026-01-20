cmake_minimum_required(VERSION 3.16)
project(open_reflectivity LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(glad STATIC
    external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    external/glad/include
)

find_package(glfw3 3.3 REQUIRED)

find_package(OpenGL REQUIRED)

add_executable(app
    src/main.cpp
)

target_link_libraries(app PRIVATE
    glad
    glfw
    OpenGL::GL
)
