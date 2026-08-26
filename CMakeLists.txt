cmake_minimum_required(VERSION 3.24)
project(sourcesight-linux VERSION 0.1.0 LANGUAGES CXX)

if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "This CMake target is for Linux. Use the Visual Studio solution on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)
find_package(glfw3 3.4 REQUIRED)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER APP_SOURCES EXCLUDE REGEX "/external/")
list(REMOVE_ITEM APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/renderer/window/Window.cpp")

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/external/imgui")
list(APPEND APP_SOURCES
    "${IMGUI_DIR}/imgui.cpp" "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp" "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "src/external/AsyncLogger/src/LogCapture.cpp"
    "src/external/AsyncLogger/src/Logger.cpp"
    "src/external/AsyncLogger/src/LogIntermediate.cpp"
    "src/external/AsyncLogger/src/LogMessage.cpp"
    "src/external/AsyncLogger/src/LogStream.cpp")

add_executable(sourcesight ${APP_SOURCES})
target_compile_definitions(sourcesight PRIVATE _GNU_SOURCE)
target_compile_options(sourcesight PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(sourcesight PRIVATE src src/external src/external/imgui
    src/external/json/include
    src/external/AsyncLogger/src src/external/AsyncLogger/include/AsyncLogger)
target_precompile_headers(sourcesight PRIVATE src/common.hpp)
target_link_libraries(sourcesight PRIVATE CURL::libcurl OpenGL::GL glfw X11::X11 pthread)
install(TARGETS sourcesight RUNTIME DESTINATION bin)
