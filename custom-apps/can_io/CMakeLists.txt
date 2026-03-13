############################################################################
# src/modules/rc_input/example/CMakeLists.txt
############################################################################

if(CONFIG_CAN_IO_APP)
nuttx_add_application(
  NAME can_io
  STACKSIZE ${CONFIG_DEFAULT_TASK_STACKSIZE}
  MODULE ${CONFIG_CAN_IO_APP}
  SRCS can_io_main.c can_io_param.c
  DEPENDS rc_input
  DEPENDS param_store
)
endif()
