## ---------------------------------------------------------------------
## $Id$
##
## Copyright (C) 2012 - 2013 by the deal.II authors
##
## This file is part of the deal.II library.
##
## The deal.II library is free software; you can use it, redistribute
## it, and/or modify it under the terms of the GNU Lesser General
## Public License as published by the Free Software Foundation; either
## version 2.1 of the License, or (at your option) any later version.
## The full text of the license can be found in the file LICENSE at
## the top level of the deal.II distribution.
##
## ---------------------------------------------------------------------

#
# Set up deal.II specific definitions
#
# This file defines a long list of uncached variables, used throughout the
# configuration to determine paths, locations and names.
#
# Definitions marked with *) can be overriden by defining them to cache
# prior to the call of this file. This is done with the help of the
# SET_IF_EMPTY macro.
#
# General information about deal.II:
#
#     DEAL_II_PACKAGE_NAME            *)
#     DEAL_II_PACKAGE_VERSION         *)
#     DEAL_II_PACKAGE_VENDOR          *)
#     DEAL_II_PACKAGE_DESCRIPTION     *)
#     DEAL_II_VERSION_MAJOR
#     DEAL_II_VERSION_MINOR
#     DEAL_II_VERSION
#
# Information about paths, install locations and names:
#
#     DEAL_II_PROJECT_CONFIG_NAME     *)
#     DEAL_II_BASE_NAME               *)
#     DEAL_II_DEBUG_SUFFIX            *)
#     DEAL_II_RELEASE_SUFFIX          *)
#
#     DEAL_II_CMAKE_MACROS_RELDIR     *)
#     DEAL_II_DOCHTML_RELDIR          *)
#     DEAL_II_DOCREADME_RELDIR        *)
#     DEAL_II_EXAMPLES_RELDIR         *)
#     DEAL_II_EXECUTABLE_RELDIR       *)
#     DEAL_II_INCLUDE_RELDIR          *)
#     DEAL_II_LIBRARY_RELDIR          *)
#     DEAL_II_PROJECT_CONFIG_RELDIR   *)
#
#     DEAL_II_BUILD_TYPES
#     DEAL_II_WITH_BUNDLED_DIRECTORY
#     DEAL_II_WITH_DOC_DIRECTORY
#
# *)  Can be overwritten by the command line via -D<...>
#

########################################################################
#                                                                      #
#                  General information about deal.II:                  #
#                                                                      #
########################################################################

SET_IF_EMPTY(DEAL_II_PACKAGE_NAME "deal.II")

FILE(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" _version LIMIT_COUNT 1)
SET_IF_EMPTY(DEAL_II_PACKAGE_VERSION "${_version}")

SET_IF_EMPTY(DEAL_II_PACKAGE_VENDOR
  "The deal.II Authors <http://www.dealii.org/>"
  )

SET_IF_EMPTY(DEAL_II_PACKAGE_DESCRIPTION
  "Library for solving partial differential equations with the finite element method"
  )

STRING(REGEX REPLACE
  "^([0-9]+)\\..*" "\\1" DEAL_II_VERSION_MAJOR "${DEAL_II_PACKAGE_VERSION}"
  )
STRING(REGEX REPLACE
  "^[0-9]+\\.([0-9]+).*" "\\1" DEAL_II_VERSION_MINOR "${DEAL_II_PACKAGE_VERSION}"
  )
SET(DEAL_II_VERSION ${DEAL_II_VERSION_MAJOR}.${DEAL_II_VERSION_MINOR})


########################################################################
#                                                                      #
#         Information about paths, install locations and names:        #
#                                                                      #
########################################################################

SET(DEAL_II_PROJECT_CONFIG_NAME "${DEAL_II_PACKAGE_NAME}")

STRING(REPLACE "." "_" _base_name "${DEAL_II_PACKAGE_NAME}")
SET_IF_EMPTY(DEAL_II_BASE_NAME "${_base_name}")
SET_IF_EMPTY(DEAL_II_DEBUG_SUFFIX ".g")
SET_IF_EMPTY(DEAL_II_RELEASE_SUFFIX "")

IF(DEAL_II_COMPONENT_COMPAT_FILES)
  #
  # The good, old directory structure:
  #
  SET_IF_EMPTY(DEAL_II_CMAKE_MACROS_RELDIR "cmake/macros")
  SET_IF_EMPTY(DEAL_II_COMMON_RELDIR "common")
  SET_IF_EMPTY(DEAL_II_DOCHTML_RELDIR "doc")
  SET_IF_EMPTY(DEAL_II_DOCREADME_RELDIR "")
  SET_IF_EMPTY(DEAL_II_EXAMPLES_RELDIR "examples")
  SET_IF_EMPTY(DEAL_II_EXECUTABLE_RELDIR "bin")
  IF( "${CMAKE_INSTALL_PREFIX}" STREQUAL "${CMAKE_BINARY_DIR}" AND
      (NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}") )
    #
    # Ensure that in case of an out of source build BINARY_DIR/include !=
    # INSTALL_PREFIX/include us always true. Otherwise stale headers might
    # get included resulting in a failing build.
    #
    SET_IF_EMPTY(DEAL_II_INCLUDE_RELDIR "include/install")
  ELSE()
    SET_IF_EMPTY(DEAL_II_INCLUDE_RELDIR "include")
  ENDIF()
  SET_IF_EMPTY(DEAL_II_LIBRARY_RELDIR "lib")
  SET_IF_EMPTY(DEAL_II_PROJECT_CONFIG_RELDIR "${DEAL_II_LIBRARY_RELDIR}/cmake/${DEAL_II_PROJECT_CONFIG_NAME}")
ELSE()
  #
  # IF DEAL_II_COMPONENT_COMPAT_FILES=off, we assume that we have to
  # obey the FSHS...
  #
  SET_IF_EMPTY(DEAL_II_CMAKE_MACROS_RELDIR "share/${DEAL_II_PACKAGE_NAME}/cmake/Macros")
  SET_IF_EMPTY(DEAL_II_COMMON_RELDIR "share/${DEAL_II_PACKAGE_NAME}/common")
  SET_IF_EMPTY(DEAL_II_DOCREADME_RELDIR "share/doc/${DEAL_II_PACKAGE_NAME}")
  SET_IF_EMPTY(DEAL_II_DOCHTML_RELDIR "${DEAL_II_DOCREADME_RELDIR}/html")
  SET_IF_EMPTY(DEAL_II_EXAMPLES_RELDIR "share/doc/${DEAL_II_PACKAGE_NAME}/examples")
  SET_IF_EMPTY(DEAL_II_EXECUTABLE_RELDIR "bin")
  SET_IF_EMPTY(DEAL_II_INCLUDE_RELDIR "include")
  SET_IF_EMPTY(DEAL_II_LIBRARY_RELDIR "lib${LIB_SUFFIX}")
  SET_IF_EMPTY(DEAL_II_PROJECT_CONFIG_RELDIR "${DEAL_II_LIBRARY_RELDIR}/cmake/${DEAL_II_PROJECT_CONFIG_NAME}")
ENDIF()


IF(CMAKE_BUILD_TYPE MATCHES "Debug")
  LIST(APPEND DEAL_II_BUILD_TYPES "DEBUG")
ENDIF()

IF(CMAKE_BUILD_TYPE MATCHES "Release")
  LIST(APPEND DEAL_II_BUILD_TYPES "RELEASE")
ENDIF()


########################################################################
#                                                                      #
#   Cleanup and setup that has to happen after the call to PROJECT():  #
#                                                                      #
########################################################################

#
# Cleanup some files used for storing the names of all object targets that
# will be bundled to the deal.II library.
# (Right now, i.e. cmake 2.8.8, this is the only reliable way to get
# information into a global scope...)
#
FOREACH(_build ${DEAL_II_BUILD_TYPES})
  STRING(TOLOWER "${_build}" _build_lowercase)
  FILE(REMOVE
    ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/deal_ii_objects_${_build_lowercase}
    )
ENDFOREACH()

IF(DEAL_II_PREFER_STATIC_LIBS)
  #
  # Invert the search order for libraries when DEAL_II_PREFER_STATIC_LIBS
  # is set. This will prefer static archives instead of shared libraries:
  #
  # TODO: Does this work on a Windows or CYGWIN target?
  LIST(REVERSE CMAKE_FIND_LIBRARY_SUFFIXES)
ENDIF()

#
# Cross compilation stuff:
#
IF(CMAKE_CROSSCOMPILING)
  #
  # Disable platform introspection when cross compiling
  #
  SET(DEAL_II_ALLOW_PLATFORM_INTROSPECTION OFF CACHE BOOL "" FORCE)

  #
  # Import native expand_instantiations for use in cross compilation:
  #
  SET(DEAL_II_NATIVE "DEAL_II_NATIVE-NOTFOUND" CACHE FILEPATH
    "A pointer to a native deal.Ii build directory"
    )
  INCLUDE(${DEAL_II_NATIVE}/cmake/scripts/importExecutables.cmake)
ENDIF()
