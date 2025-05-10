cmake_minimum_required(VERSION 3.10)
project(SuperglideTrainer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output as a Windows GUI app (no console)
set(CMAKE_WIN32_EXECUTABLE TRUE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Source files (now in src/)
set(SOURCES
    src/main.cpp
    src/InputHandler.cpp
    src/TrainerLogic.cpp
    src/imgui.cpp
    src/imgui_draw.cpp
    src/imgui_widgets.cpp
    src/imgui_tables.cpp
    src/imgui_demo.cpp
    src/imgui_impl_win32.cpp
    src/imgui_impl_dx11.cpp
    src/imgui_internal.h
    src/imgui_impl_win32.h
    src/imgui_impl_dx11.h
)

add_executable(SuperglideTrainer WIN32 ${SOURCES})

# Include directories (add both src and root for robustness)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link DirectX 11, DWM, and Win32 libraries
if(MSVC)
    target_link_libraries(SuperglideTrainer d3d11 dxgi d3dcompiler dwmapi user32 gdi32 shell32)
    target_compile_options(SuperglideTrainer PRIVATE /W4 /DUNICODE /D_UNICODE)
else()
    target_link_libraries(SuperglideTrainer d3d11 dxgi d3dcompiler dwmapi user32 gdi32 shell32)
endif() 