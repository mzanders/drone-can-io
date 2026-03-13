############################################################################
# src/modules/rc_input/CMakeLists.txt
############################################################################

if(CONFIG_PARAM_STORE)

set(SRCS param_store.c param.c)

nuttx_add_library(param_store STATIC)
target_sources(param_store PRIVATE ${SRCS})
target_include_directories(param_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

endif()