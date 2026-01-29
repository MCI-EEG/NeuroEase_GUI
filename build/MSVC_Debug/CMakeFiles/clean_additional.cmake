# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\NeuroEase_GUI_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\NeuroEase_GUI_autogen.dir\\ParseCache.txt"
  "NeuroEase_GUI_autogen"
  )
endif()
