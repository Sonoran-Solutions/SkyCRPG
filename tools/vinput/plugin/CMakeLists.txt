# Standalone test-only target. Not part of the root CMake project on purpose: it needs
# neither CommonLib nor vcpkg, only the upstream vinput static library built by
# upstream build.bat (see ../README.md). Never add an install/package rule.
cmake_minimum_required(VERSION 3.25)

project(SkyCRPG_VInput VERSION 0.0.1 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "SkyCRPG_VInput targets the Windows x64 SKSE ABI; build with MSVC x64.")
endif()

set(SKYCRPG_VINPUT_DIST "" CACHE PATH "Upstream dist/ directory holding vinput_x64.lib")
set(SKYCRPG_VINPUT_SRC "" CACHE PATH "Upstream src/ directory holding vinput.h")
set(SKYCRPG_VINPUT_UPSTREAM "unknown" CACHE STRING "Upstream vinput commit recorded in the load log")
set(SKYCRPG_SOURCE_COMMIT "unknown" CACHE STRING "Source commit recorded in the load log")

if(NOT EXISTS "${SKYCRPG_VINPUT_DIST}/vinput_x64.lib")
	message(FATAL_ERROR "SKYCRPG_VINPUT_DIST must hold vinput_x64.lib (run upstream build.bat :arch x64 first)")
endif()
if(NOT EXISTS "${SKYCRPG_VINPUT_SRC}/vinput.h")
	message(FATAL_ERROR "SKYCRPG_VINPUT_SRC must hold vinput.h")
endif()

get_filename_component(SKYCRPG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT, matching upstream's /MT objects

add_library(SkyCRPG_VInput SHARED src/plugin.cpp)
target_include_directories(SkyCRPG_VInput PRIVATE "${SKYCRPG_ROOT}/shared/include" "${SKYCRPG_VINPUT_SRC}")
target_compile_definitions(SkyCRPG_VInput PRIVATE
	UNICODE _UNICODE
	VINPUT_COMPILE_IN
	SKYCRPG_SOURCE_COMMIT="${SKYCRPG_SOURCE_COMMIT}"
	SKYCRPG_VINPUT_UPSTREAM="${SKYCRPG_VINPUT_UPSTREAM}")
target_compile_options(SkyCRPG_VInput PRIVATE /W4 /WX /permissive- /Zi /Brepro)
target_link_options(SkyCRPG_VInput PRIVATE /DEBUG /OPT:REF /OPT:ICF /Brepro "/PDBALTPATH:%_PDB%")
# The libraries upstream's vinput.h documents for VINPUT_COMPILE_IN hosts, plus the skeleton's.
target_link_libraries(SkyCRPG_VInput PRIVATE
	"${SKYCRPG_VINPUT_DIST}/vinput_x64.lib"
	shell32 ole32 user32 gdi32 advapi32 windowscodecs d3d11 dxgi dxguid dinput8)
