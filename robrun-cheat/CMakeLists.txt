cmake_minimum_required(VERSION 3.20)
project(RobRunCheat CXX)
set(CMAKE_CXX_STANDARD 20)

# ── ImGui — clone into vendor/imgui ──────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SRCS
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(RobRunCheat WIN32
    src/main.cpp
    src/overlay.cpp
    ${IMGUI_SRCS}
)

target_include_directories(RobRunCheat PRIVATE
    include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# static runtime — single self-contained EXE, no MSVC redist needed
set_property(TARGET RobRunCheat PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(RobRunCheat PRIVATE
    d3d11 dxgi user32 Shell32
)

target_compile_definitions(RobRunCheat PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    # READ_MODE=1  # uncomment once kernel driver is set up
)

# strip debug info from Release
target_compile_options(RobRunCheat PRIVATE
    $<$<CONFIG:Release>:/O2 /GL /GS->
)
target_link_options(RobRunCheat PRIVATE
    $<$<CONFIG:Release>:/LTCG /DEBUG:NONE>
)
