cmake_minimum_required (VERSION 3.10)
project(v8dbg)

# C++17 is required for some of the header files
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the preprocessor definitions for Unicode
add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)

# Configure the debug extension with the minimal sources needed
add_library(v8dbg SHARED "dbgext.cc" "dbgext.h" "dbgext.rc" "utilities.cc" "utilities.h")

# Add the implementation specific sources
target_sources(v8dbg PRIVATE "src/extension.cc" "src/extension.h" "src/object.cc" "src/object.h")
target_sources(v8dbg PRIVATE "src/v8.cc" "src/v8.h" "src/curisolate.cc" "src/curisolate.h" "src/list-chunks.cc" "src/list-chunks.h")

# Add the test binary
add_executable(v8dbg-test "test/main.cc" "test/common.h")

# DbgEng and DbgModel are needed for Debugger extensions. RuntimeObject for COM.
target_link_libraries(v8dbg DbgEng DbgModel RuntimeObject comsuppwd "f:/repos/ana/v8/out/debug_x64/v8_debug_helper.dll.lib")
target_link_libraries(v8dbg-test DbgEng DbgModel RuntimeObject)

target_include_directories(v8dbg PRIVATE "f:/repos/ana/v8/tools/debug_helper")