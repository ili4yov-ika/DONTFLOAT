# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\DONTFLOAT_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\DONTFLOAT_autogen.dir\\ParseCache.txt"
  "DONTFLOAT_autogen"
  )
endif()
