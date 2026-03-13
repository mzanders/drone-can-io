############################################################################
# src/modules/rc_input/example/CMakeLists.txt
############################################################################

if(CONFIG_PARAM_CMD)
nuttx_add_application(
  NAME param
  STACKSIZE ${CONFIG_DEFAULT_TASK_STACKSIZE}
  MODULE ${CONFIG_PARAM_CMD}
  SRCS param_cmd.c
  DEPENDS param_store
)
endif()
