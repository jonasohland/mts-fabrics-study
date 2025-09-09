cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

# Allow version in project definition
cmake_policy(SET CMP0048 NEW)
# Only interpret if arguments as variables or keywords when unquoted.
cmake_policy(SET CMP0054 NEW)
# Allow visibility definitions
cmake_policy(SET CMP0063 NEW)
# Interpret target_sources paths as relative to the current source dir
cmake_policy(SET CMP0076 NEW)
# Set the timestamps of all extracted contents to the time of the extraction
cmake_policy(SET CMP0135 NEW)

find_package(PkgConfig)

pkg_check_modules(uuid REQUIRED IMPORTED_TARGET uuid)
pkg_check_modules(csv2 REQUIRED IMPORTED_TARGET csv2)
pkg_check_modules(ucx REQUIRED IMPORTED_TARGET ucx)
add_library(unofficial::UUID::uuid ALIAS PkgConfig::uuid)

add_subdirectory(mxl)

find_package(httplib CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(CUDAToolkit REQUIRED)

add_executable(
    fabrics-perf
    "src/main.cpp"
    "src/Config.cpp"
    "src/Test.cpp"
    "src/Rate.cpp"
    "src/FlowSetup.cpp"
    "src/Output.cpp"
    "src/Executor.cpp"
    "src/Reflector.cpp"
    "src/Runner.cpp"
    "src/Perf.cpp"
    "src/ScopedGPUMaxClocks.cpp"
    "src/tests/ucx/Context.cpp"
    "src/tests/ucx/Worker.cpp"
)

set_target_properties(
    fabrics-perf
    PROPERTIES
    CXX_STANDARD 20
)

target_link_libraries(
    fabrics-perf
    PRIVATE
    mxl-static
    mxl-fabrics-static
    httplib::httplib
    CLI11::CLI11
    stduuid
    PkgConfig::csv2
    CUDA::cudart
    CUDA::nvml
    PkgConfig::ucx
)

target_include_directories(
    fabrics-perf
    PRIVATE
    "mxl/lib/src"
)

target_include_directories(
    fabrics-perf
    PRIVATE
    "mxl/lib/fabrics/ofi/src/internal"
)
