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
# Try to find the UMFPACK library
#
# This module exports
#
#   UMFPACK_LIBRARIES
#   UMFPACK_INCLUDE_DIRS
#   UMFPACK_LINKER_FLAGS
#

INCLUDE(FindPackageHandleStandardArgs)

FOREACH(_comp SUITESPARSE SUITESPARSE_CONFIG UMFPACK AMD CHOLMOD COLAMD)
  SET_IF_EMPTY(${_comp}_DIR "$ENV{${_comp}_DIR}")
ENDFOREACH()


#
# UMFPACK depends on BLAS and LAPACK, so search for them:
# TODO: There might be an external dependency for metis, ignore this for
# now.
#
FIND_PACKAGE(LAPACK)
FIND_PACKAGE(METIS)

#
# Two macros to make life easier:
#

MACRO(FIND_UMFPACK_PATH _comp _file)
  STRING(TOLOWER ${_comp} _comp_lowercase)
  STRING(TOUPPER ${_comp} _comp_uppercase)
  FIND_PATH(${_comp}_INCLUDE_DIR ${_file}
    HINTS
      ${${_comp_uppercase}_DIR}
      ${SUITESPARSE_DIR}/${_comp}
      ${UMFPACK_DIR}/../${_comp}
      ${UMFPACK_DIR}/${_comp}
      ${UMFPACK_DIR}
    PATH_SUFFIXES
      ${_comp_lowercase} include/${_comp_lowercase} include Include ${_comp}/Include suitesparse
    )
ENDMACRO()

MACRO(FIND_UMFPACK_LIBRARY _comp _name)
  STRING(TOUPPER ${_comp} _comp_uppercase)
  FIND_LIBRARY(${_comp}_LIBRARY
    NAMES ${_name}
    HINTS
      ${${_comp_uppercase}_DIR}
      ${SUITESPARSE_DIR}/${_comp}
      ${UMFPACK_DIR}/../${_comp}
      ${UMFPACK_DIR}/${_comp}
      ${UMFPACK_DIR}
    PATH_SUFFIXES
    lib${LIB_SUFFIX} lib64 lib Lib ${_comp}/Lib
    )
  IF(NOT "${ARGN}" STREQUAL "REQUIRED")
    IF(${_comp}_LIBRARY MATCHES "-NOTFOUND")
      SET(${_comp}_LIBRARY "")
      UNSET(${_comp}_LIBRARY CACHE)
    ENDIF()
  ENDIF()
ENDMACRO()


#
# Search for include directories:
#
FIND_UMFPACK_PATH(UMFPACK umfpack.h)
FIND_UMFPACK_PATH(AMD amd.h)

#
# Well, recent versions of UMFPACK include SuiteSparse_config.h, if so,
# ensure that we'll find these headers as well.
#
IF(NOT UMFPACK_INCLUDE_DIR MATCHES "-NOTFOUND")
  FILE(STRINGS "${UMFPACK_INCLUDE_DIR}/umfpack.h" UMFPACK_SUITESPARSE_STRING
    REGEX "#include \"SuiteSparse_config.h\"")
  IF(NOT "${UMFPACK_SUITESPARSE_STRING}" STREQUAL "")
    FIND_UMFPACK_PATH(SuiteSparse_config SuiteSparse_config.h)
    LIST(APPEND _required SuiteSparse_config_INCLUDE_DIR)
  ENDIF()
ENDIF()

#
# Link against everything we can find to avoid underlinkage:
#
FIND_UMFPACK_LIBRARY(UMFPACK umfpack REQUIRED)
FIND_UMFPACK_LIBRARY(AMD amd REQUIRED)
FIND_UMFPACK_LIBRARY(CHOLMOD cholmod)
FIND_UMFPACK_LIBRARY(COLAMD colamd)
FIND_UMFPACK_LIBRARY(CCOLAMD ccolamd)
FIND_UMFPACK_LIBRARY(CAMD camd)
FIND_UMFPACK_LIBRARY(SuiteSparse_config suitesparseconfig)

SET(_output ${UMFPACK_LIBRARY} ${CHOLMOD_LIBRARY} ${CCOLAMD_LIBRARY} ${COLAMD_LIBRARY} ${CAMD_LIBRARY} ${AMD_LIBRARY} ${SuiteSparse_config_LIBRARY})
FIND_PACKAGE_HANDLE_STANDARD_ARGS(UMFPACK DEFAULT_MSG
  _output # Cosmetic: Gives nice output
  UMFPACK_LIBRARY
  AMD_LIBRARY
  UMFPACK_INCLUDE_DIR
  AMD_INCLUDE_DIR
  ${_required}
  LAPACK_FOUND
  )

MARK_AS_ADVANCED(
  AMD_INCLUDE_DIR
  AMD_LIBRARY
  atlas_LIBRARY
  blas_LIBRARY
  CAMD_LIBRARY
  CHOLMOD_LIBRARY
  CCOLAMD_LIBRARY
  COLAMD_LIBRARY
  SuiteSparse_config_INCLUDE_DIR
  SuiteSparse_config_LIBRARY
  UMFPACK_INCLUDE_DIR
  UMFPACK_LIBRARY
  )


IF(UMFPACK_FOUND)
  SET(UMFPACK_LIBRARIES
    ${UMFPACK_LIBRARY}
    ${CHOLMOD_LIBRARY}
    ${CCOLAMD_LIBRARY}
    ${COLAMD_LIBRARY}
    ${CAMD_LIBRARY}
    ${AMD_LIBRARY}
    ${SuiteSparse_config_LIBRARY}
    )

  #
  # For good measure:
  #
  IF(METIS_FOUND)
    LIST(APPEND UMFPACK_LIBRARIES ${METIS_LIBRARIES})
  ENDIF()

  LIST(APPEND UMFPACK_LIBRARIES ${LAPACK_LIBRARIES})

  #
  # Add rt to the link interface as well (for whatever reason,
  # libsuitesparse.so depends on clock_gettime but the shared
  # lib does not record its dependence on librt.so as evidenced
  # by ldd :-( ):
  #
  SWITCH_LIBRARY_PREFERENCE()
  FIND_LIBRARY(rt_LIBRARY NAMES rt)
  SWITCH_LIBRARY_PREFERENCE()
  MARK_AS_ADVANCED(rt_LIBRARY)
  IF(NOT rt_LIBRARY MATCHES "-NOTFOUND")
    LIST(APPEND UMFPACK_LIBRARIES ${rt_LIBRARY})
  ENDIF()

  SET(UMFPACK_INCLUDE_DIRS
    ${UMFPACK_INCLUDE_DIR}
    ${AMD_INCLUDE_DIR}
    ${SuiteSparse_config_INCLUDE_DIR}
    )
  SET(UMFPACK_LINKER_FLAGS
    ${LAPACK_LINKER_FLAGS}
    )

  MARK_AS_ADVANCED(UMFPACK_DIR SUITESPARSE_DIR)
ELSE()
  SET(UMFPACK_DIR "" CACHE PATH
    "An optional hint to an UMFPACK directory"
    )
  SET(SUITESPARSE_DIR "" CACHE PATH
    "An optional hint to a SUITESPARSE directory"
    )
ENDIF()
