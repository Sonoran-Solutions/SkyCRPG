add_library(SkyCRPG_UI SHARED src/plugin.cpp)
skycrpg_configure_plugin(SkyCRPG_UI)
target_link_libraries(SkyCRPG_UI PRIVATE CommonLibSSE::CommonLibSSE)
