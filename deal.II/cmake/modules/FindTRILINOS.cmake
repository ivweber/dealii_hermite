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
# Try to find the Trilinos library
#
# This module exports:
#
#   TRILINOS_DIR (cached)
#   TRILINOS_INCLUDE_DIRS
#   TRILINOS_LIBRARIES
#   TRILINOS_VERSION_MAJOR
#   TRILINOS_VERSION_MINOR
#   TRILINOS_VERSION_SUBMINOR
#   TRILINOS_WITH_MPI
#

INCLUDE(FindPackageHandleStandardArgs)

SET_IF_EMPTY(TRILINOS_DIR "$ENV{TRILINOS_DIR}")

#
# Include the trilinos package configuration:
#
FIND_PACKAGE(TRILINOS_CONFIG
  CONFIG QUIET
  NAMES Trilinos TRILINOS
  HINTS
    ${TRILINOS_DIR}/lib/cmake/Trilinos
    ${TRILINOS_DIR}
  PATH_SUFFIXES
    lib64/cmake/Trilinos
    lib/cmake/Trilinos
    lib${LIB_SUFFIX}/cmake/Trilinos
    include/trilinos
    include/Trilinos
  NO_SYSTEM_ENVIRONMENT_PATH
  )

SET(TRILINOS_INCLUDE_DIRS ${Trilinos_INCLUDE_DIRS})

#
# We'd like to have the full library names but the Trilinos package only
# exports a list with short names...
# So we check again for every lib and store the full path:
#
FOREACH(_library ${Trilinos_LIBRARIES})
  FIND_LIBRARY(TRILINOS_LIBRARY_${_library}
    NAMES ${_library}
    HINTS ${Trilinos_LIBRARY_DIRS}
    NO_DEFAULT_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_CMAKE_PATH
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    )

  LIST(APPEND TRILINOS_LIBRARIES ${TRILINOS_LIBRARY_${_library}})

  #
  # Remove the variables from the cache, so that updating TRILINOS_DIR will
  # find the new libraries..
  #
  UNSET(TRILINOS_LIBRARY_${_library} CACHE)
ENDFOREACH()

#
# Add the link interface:
#
LIST(APPEND TRILINOS_LIBRARIES
  ${Trilinos_TPL_LIBRARIES}
  ${MPI_CXX_LIBRARIES} # for good measure
  )

FIND_PACKAGE_HANDLE_STANDARD_ARGS(TRILINOS DEFAULT_MSG
  TRILINOS_LIBRARIES # cosmetic: Gives nice output
  TRILINOS_CONFIG_FOUND
  )

MARK_AS_ADVANCED(TRILINOS_CONFIG_DIR)


IF(TRILINOS_FOUND)
  #
  # Extract the major and minor version numbers:
  #
  STRING(REGEX REPLACE
    "^([0-9]+).*$" "\\1"
    TRILINOS_VERSION_MAJOR "${Trilinos_VERSION}")

  STRING(REGEX REPLACE
    "^[0-9]+\\.([0-9]+).*$" "\\1"
    TRILINOS_VERSION_MINOR "${Trilinos_VERSION}")

  STRING(REGEX REPLACE
    "^[0-9]+\\.[0-9]+\\.([0-9]+).*$" "\\1"
    TRILINOS_VERSION_SUBMINOR "${Trilinos_VERSION}")

  #
  # Determine whether Trilinos was configured with MPI and 64bit indices:
  #
  FIND_FILE(EPETRA_CONFIG_H Epetra_config.h
    HINTS ${TRILINOS_INCLUDE_DIRS}
    NO_DEFAULT_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    NO_CMAKE_PATH
    NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH
    )
  FILE(STRINGS "${EPETRA_CONFIG_H}" EPETRA_MPI_STRING
    REGEX "#define HAVE_MPI")
  IF("${EPETRA_MPI_STRING}" STREQUAL "")
    SET(TRILINOS_WITH_MPI FALSE)
  ELSE()
    SET(TRILINOS_WITH_MPI TRUE)
  ENDIF()
  FILE(STRINGS "${EPETRA_CONFIG_H}" EPETRA_32BIT_STRING
    REGEX "#define EPETRA_NO_32BIT_GLOBAL_INDICES")
  IF("${EPETRA_64BIT_STRING}" STREQUAL "")
    SET(TRILINOS_WITH_NO_32BITS_INDICES TRUE)
  ELSE()
    SET(TRILINOS_WITH_NO_32BITS_INDICES FALSE)
  ENDIF()
  FILE(STRINGS "${EPETRA_CONFIG_H}" EPETRA_64BIT_STRING
    REGEX "#define EPETRA_NO_64BIT_GLOBAL_INDICES")
  IF("${EPETRA_64BIT_STRING}" STREQUAL "")
    SET(TRILINOS_WITH_NO_64BITS_INDICES TRUE)
  ELSE()
    SET(TRILINOS_WITH_NO_64BITS_INDICES FALSE)
  ENDIF()

  UNSET(EPETRA_CONFIG_H CACHE)

  MARK_AS_ADVANCED(TRILINOS_DIR)
ELSE()
  SET(TRILINOS_DIR "" CACHE PATH
    "An optional hint to a Trilinos installation"
    )
ENDIF()

