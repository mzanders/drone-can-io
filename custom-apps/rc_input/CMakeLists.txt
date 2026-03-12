############################################################################
# src/modules/rc_input/example/CMakeLists.txt
############################################################################

if(CONFIG_RC_INPUT_EXAMPLE)
nuttx_add_application(
  NAME
  rcinput
  STACKSIZE
  ${CONFIG_DEFAULT_TASK_STACKSIZE}
  MODULE
  ${CONFIG_MODULES_RC_INPUT_EXAMPLE}
  SRCS
  rcinput_main.c
  DEPENDS
  rc_input
)
endif()
