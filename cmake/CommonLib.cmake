# Selected in ADR-001; original source remains external and unmodified.
include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "Do not build CommonLib's unrelated suite" FORCE)
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "Pinned AE runtime only" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Pinned AE runtime" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "VR is outside the support baseline" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "Policy adapters use no trampoline branch patches" FORCE)
set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "Bounded source build" FORCE)
FetchContent_Declare(CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibSSE-NG.git
    GIT_TAG 7a60f4de794095d7b0f8928d1b930a52e9a7da83
    GIT_SUBMODULES extern/openvr
    GIT_SUBMODULES_RECURSE FALSE)
FetchContent_MakeAvailable(CommonLibSSE)
