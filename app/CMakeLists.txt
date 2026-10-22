# Alternative to ImGuiApp.sln. Examples:
#   cmake -B build-x64 -A x64   && cmake --build build-x64 --config Release
#   cmake -B build-x86 -A Win32 && cmake --build build-x86 --config Release
cmake_minimum_required(VERSION 3.15)
project(ImGuiApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT so the exe runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(ImGuiApp WIN32
    src/main.cpp
    src/app.cpp
    src/connect_screen.cpp
    src/main_screen.cpp
    src/connection.cpp
    src/theme.cpp
    src/widgets.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_dx9.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
)
target_include_directories(ImGuiApp PRIVATE ${IMGUI_DIR})
target_link_libraries(ImGuiApp PRIVATE d3d9 dwmapi)

if(MSVC)
    target_compile_options(ImGuiApp PRIVATE /W3 /MP)
elseif(MINGW)
    target_link_libraries(ImGuiApp PRIVATE gdi32 -static)
endif()
