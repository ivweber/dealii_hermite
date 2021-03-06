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
# Configuration for the petsc library:
#


MACRO(FEATURE_PETSC_FIND_EXTERNAL var)

  FIND_PACKAGE(PETSC)

  IF(PETSC_FOUND)
    #
    # So, we have found a petsc library. Let's check whether we can use it.
    #
    SET(${var} TRUE)

    #
    # We support petsc from version 3.x.x onwards
    #
    IF(PETSC_VERSION_MAJOR LESS 3)
      MESSAGE(STATUS "Could not find a sufficient modern PETSc installation: "
        "Version >=3.0.0 required!"
        )
      SET(PETSC_ADDITIONAL_ERROR_STRING
        "Could not find a sufficient modern PETSc installation: "
        "Version >=3.0.0 required!\n"
        )
      SET(${var} FALSE)
    ENDIF()

    #
    # Petsc has to be configured with the same MPI configuration as
    # deal.II.
    #
    # petscconf.h should export PETSC_HAVE_MPIUNI 1 in case mpi support is
    # _NOT_ enabled.
    # So we check for this:
    #
    IF( (PETSC_WITH_MPIUNI AND DEAL_II_WITH_MPI)
         OR
         (NOT PETSC_WITH_MPIUNI AND NOT DEAL_II_WITH_MPI))
      MESSAGE(STATUS "Could not find a sufficient PETSc installation: "
        "PETSc has to be configured with the same MPI configuration as deal.II."
        )
      SET(PETSC_ADDITIONAL_ERROR_STRING
        ${PETSC_ADDITIONAL_ERROR_STRING}
        "Could not find a sufficient PETSc installation:\n"
        "PETSc has to be configured with the same MPI configuration as deal.II, but found:\n"
        "  DEAL_II_WITH_MPI = ${DEAL_II_WITH_MPI}\n"
        "  PETSC_WITH_MPI   = (NOT ${PETSC_WITH_MPIUNI})\n"
        )
      SET(${var} FALSE)
    ENDIF()


    #
    # Petsc has to be configured with the same number of bits for indices as
    # deal.II.
    #
    # petscconf.h should export PETSC_WITH_64BIT_INDICES 1 in case 64bits
    # indices support is enabled.
    # So we check for this:
    #
    IF( (NOT PETSC_WITH_64BIT_INDICES AND DEAL_II_WITH_64BIT_INDICES)
         OR
         (PETSC_WITH_64BIT_INDICES AND NOT DEAL_II_WITH_64BIT_INDICES))
      MESSAGE(STATUS "Could not find a sufficient PETSc installation: "
        "PETSc has to be configured to use the same number of bits for the "
        "global indices as deal.II."
        )
      SET(PETSC_ADDITIONAL_ERROR_STRING
        ${PETSC_ADDITIONAL_ERROR_STRING}
        "Could not find a sufficient PETSc installation:\n"
        "PETSc has to be configured to use the same number of bits for the "
        "global indices as deal.II, but found:\n"
        "  DEAL_II_WITH_64BIT_INDICES = ${DEAL_II_WITH_64BIT_INDICES}\n"
        "  PETSC_WITH_64BIT_INDICES = (${PETSC_WITH_64BIT_INDICES})\n"
        )
      SET(${var} FALSE)
    ENDIF()

    IF(NOT ${var})
      UNSET(PETSC_INCLUDE_DIR_ARCH CACHE)
      UNSET(PETSC_INCLUDE_DIR_COMMON CACHE)
      UNSET(PETSC_LIBRARY CACHE)
      SET(PETSC_DIR "" CACHE PATH
        "An optional hint to a PETSc directory"
        )
      SET(PETSC_ARCH "" CACHE STRING
        "An optional hint to a PETSc arch"
        )
    ENDIF()
  ENDIF()
ENDMACRO()


MACRO(FEATURE_PETSC_CONFIGURE_EXTERNAL)
  INCLUDE_DIRECTORIES(${PETSC_INCLUDE_DIRS})

  # The user has to know the location of the petsc headers as well:
  LIST(APPEND DEAL_II_USER_INCLUDE_DIRS ${PETSC_INCLUDE_DIRS})

  LIST(APPEND DEAL_II_EXTERNAL_LIBRARIES ${PETSC_LIBRARIES})

  #
  # Disable a bunch of warnings when compiling with petsc:
  #
  ENABLE_IF_SUPPORTED(CMAKE_CXX_FLAGS "-Wno-long-long")

  SET(DEAL_II_EXPAND_PETSC_VECTOR "PETScWrappers::Vector")
  SET(DEAL_II_EXPAND_PETSC_BLOCKVECTOR "PETScWrappers::BlockVector")
  SET(DEAL_II_EXPAND_PETSC_MPI_VECTOR "PETScWrappers::MPI::Vector")
  SET(DEAL_II_EXPAND_PETSC_MPI_BLOCKVECTOR "PETScWrappers::MPI::BlockVector")
ENDMACRO()


MACRO(FEATURE_PETSC_ERROR_MESSAGE)
  MESSAGE(FATAL_ERROR "\n"
    "Could not find the petsc library!\n"
    ${PETSC_ADDITIONAL_ERROR_STRING}
    "\nPlease ensure that the petsc library version 3.0.0 or newer is "
    "installed on your computer and is configured with the same mpi options "
    "as deal.II\n"
    "If the library is not at a default location, either provide some hints\n"
    "for the autodetection:\n"
    "PETSc installed with --prefix=<...> to a destination:\n"
    "    $ PETSC_DIR=\"...\" cmake <...>\n"
    "    $ cmake -DPETSC_DIR=\"...\" <...>\n"
    "PETSc compiled in source tree:\n"
    "    $ PETSC_DIR=\"...\"  PETSC_ARCH=\"...\" cmake <...>\n"
    "    $ cmake -DPETSC_DIR=\"...\" -DPETSC_ARCH=\"...\" <...>\n"
    "or set the relevant variables by hand in ccmake.\n\n"
    )
ENDMACRO()


CONFIGURE_FEATURE(PETSC)
