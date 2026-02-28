cmake_minimum_required(VERSION 3.27)
project(xilinx_bootgen_loader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Linker Optimization ────────────────────────────────────────────────
if(APPLE)
    add_link_options(-Wl,-ld_new)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options($<$<CONFIG:Release>:-O3>)
endif()

include(FetchContent)

# ── idax dependency ─────────────────────────────────────────────────────
FetchContent_Declare(
    idax
    GIT_REPOSITORY https://github.com/19h/idax.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(idax)

# ── Parser core library (Decoupled from IDA SDK for tests) ──────────────
add_library(xilinx_parser STATIC src/parser.cpp)
target_include_directories(xilinx_parser PUBLIC src)

# ── Loader target ───────────────────────────────────────────────────────
if(COMMAND ida_add_loader)
    ida_add_loader(zynqmp_boot_image_loader
        SOURCES src/loader.cpp src/bridge.cpp
        LIBRARIES idax::idax xilinx_parser
    )
else()
    message(WARNING "ida_add_loader command not found")
endif()

# ── Tests ───────────────────────────────────────────────────────────────
enable_testing()

add_executable(xilinx_loader_tests tests/test_main.cpp)
target_link_libraries(xilinx_loader_tests PRIVATE xilinx_parser)
add_test(NAME xilinx_loader_tests COMMAND xilinx_loader_tests)
