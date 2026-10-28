# Internal disposable-profile instrumentation. Never add an install/package rule.
add_library(SkyCRPG_TestProbe SHARED src/plugin.cpp)
skycrpg_configure_plugin(SkyCRPG_TestProbe)
target_link_libraries(SkyCRPG_TestProbe PRIVATE CommonLibSSE::CommonLibSSE bcrypt)
