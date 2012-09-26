#
# This file implements the DEAL_II_INITIALIZE_VARIABLES macro, which is
# part of the deal.II library.
#
# Usage:
#       DEAL_II_INITIALIZE_CACHED_VARIABLES()
#
# This macro sets some cached variables to the values used for compiling
# the deal.II library.
#
# This macro has to be called before PROJECT()!
#

MACRO(DEAL_II_INITIALIZE_CACHED_VARIABLES)

  SET(CMAKE_BUILD_TYPE "Debug" CACHE STRING
    "Choose the type of build, options are: Debug, Release"
    )

  SET(CMAKE_CXX_FLAGS ${DEAL_II_CXX_FLAGS})
  SET(CMAKE_CXX_FLAGS_RELEASE ${DEAL_II_CXX_FLAGS_RELEASE})
  SET(CMAKE_CXX_FLAGS_DEBUG ${DEAL_II_CXX_FLAGS_DEBUG})

ENDMACRO()

