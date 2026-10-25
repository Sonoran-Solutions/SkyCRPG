cmake_minimum_required(VERSION 3.25)

project(SkyCRPG VERSION 0.0.1 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "SkyCRPG plugins target the Windows x64 SKSE ABI; build with MSVC x64 (see docs/BUILD_AND_TEST.md).")
endif()

set(SKYCRPG_SOURCE_COMMIT "unknown" CACHE STRING "Source commit recorded in plugin load logs")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no VC++ redistributable needed in the game prefix

function(skycrpg_configure_plugin target)
	target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/shared/include")
	target_compile_definitions(${target} PRIVATE
		UNICODE _UNICODE
		SKYCRPG_VERSION="${PROJECT_VERSION}"
		SKYCRPG_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		SKYCRPG_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		SKYCRPG_VERSION_PATCH=${PROJECT_VERSION_PATCH}
		SKYCRPG_SOURCE_COMMIT="${SKYCRPG_SOURCE_COMMIT}")
	target_compile_options(${target} PRIVATE /W4 /WX /permissive- /Zi /Brepro)
	# /Brepro and a name-only PDB path keep the DLL byte-identical across build directories.
	target_link_options(${target} PRIVATE /DEBUG /OPT:REF /OPT:ICF /Brepro "/PDBALTPATH:%_PDB%")
	target_link_libraries(${target} PRIVATE shell32 ole32)
endfunction()

add_subdirectory(modules/camera)
add_subdirectory(modules/ui)
