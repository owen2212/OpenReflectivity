cmake_minimum_required(VERSION 3.16)
project(open_reflectivity LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_library(glad STATIC
    external/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    external/glad/include
)

find_package(glfw3 3.3 REQUIRED)

find_package(OpenGL REQUIRED)

find_package(BZip2 REQUIRED)

add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
)

target_link_libraries(imgui PUBLIC
    glfw
)

add_executable(app
    src/main.cpp
    src/app/geo.cpp
    src/app/imgui_setup.cpp
    src/app/input.cpp
    src/app/map_data.cpp
    src/app/products.cpp
    src/app/radar_render_data.cpp
    src/app/renderers.cpp
    src/app/screenshot.cpp
    src/app/sidebar.cpp
    src/app/view_state.cpp
    src/rsl/rsl_wrapper.cpp
    src/gl/buffer.cpp
    src/gl/texture.cpp
    src/gl/vertex_array.cpp
    src/gl/shader.cpp
)

target_include_directories(app PRIVATE
    src
    external/stb
)

file(GLOB RSL_SOURCES CONFIGURE_DEPENDS
    external/rsl/*.c
)

add_library(rsl STATIC
    ${RSL_SOURCES}
)

target_include_directories(rsl PUBLIC
    external/rsl
)

target_compile_definitions(rsl PRIVATE
    WSR88D_SITE_INFO_FILE=\"${CMAKE_SOURCE_DIR}/external/rsl/wsr88d_locations.dat\"
)

target_link_libraries(rsl PUBLIC
    m
    BZip2::BZip2
)

target_compile_options(rsl PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/w>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
)

target_link_libraries(app PRIVATE
    glad
    imgui
    rsl
    glfw
    OpenGL::GL
)
