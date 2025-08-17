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
add_library(unofficial::UUID::uuid ALIAS PkgConfig::uuid)

add_subdirectory(mxl)


find_package(httplib CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)

add_executable(
    fabrics-perf
    "src/main.cpp"
    "src/Config.cpp"
    "src/Test.cpp"
    "src/Rate.cpp"
    "src/FlowSetup.cpp"
    "src/Executor.cpp"
    "src/Reflector.cpp"
    "src/Runner.cpp"
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
)

target_include_directories(
    fabrics-perf
    PRIVATE
    "mxl/lib/src"
)
