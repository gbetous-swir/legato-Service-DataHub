#*******************************************************************************
# Copyright (C) Sierra Wireless, Inc.
#*******************************************************************************

mkapp(dataHub.adef
  -i ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
  -i ${CMAKE_CURRENT_SOURCE_DIR}/interfaces/linux)

# This is a sample application
add_dependencies(samples_c dataHub)
