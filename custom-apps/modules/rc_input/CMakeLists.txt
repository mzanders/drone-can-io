############################################################################
# src/modules/rc_input/CMakeLists.txt
############################################################################

if(CONFIG_RC_INPUT)

set(SRCS rc_input.c)

if(CONFIG_RC_INPUT_BACKEND_SBUS)
  list(APPEND SRCS sbus_backend.c)
endif()

if(CONFIG_RC_INPUT_BACKEND_CRSF)
  list(APPEND SRCS crsf_backend.c)
endif()

if(CONFIG_RC_INPUT_BACKEND_DRONECAN)
  list(APPEND SRCS dronecan_backend.c)
endif()

nuttx_add_library(rc_input STATIC)
target_sources(rc_input PRIVATE ${SRCS})
target_include_directories(rc_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

endif()