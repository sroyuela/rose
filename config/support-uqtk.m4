##### http://www.rosecompiler.org
#
# SYNOPSIS
#
#   ROSE_SUPPORT_UQTK([])
#
# DESCRIPTION
#
#   Determine if support is requested for the SANDIA Uncertainty Quantification Toolkit.
#
#
#
# LAST MODIFICATION
#
#   2012-03-28
#
# COPYLEFT
#
#   Copyright (c) 2011 Justin Too <too1@llnl.gov>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([ROSE_SUPPORT_UQTK],
[
  ROSE_ARG_WITH(
    [uqtk],
    [for Uncertainty Quantification Toolkit (UQTK)],
    [use the Uncertainty Quantification Toolkit (UQTK)],
    []
  )
  if test "x$CONFIG_HAS_ROSE_WITH_UQTK" != "xno"; then
    UQTK_INSTALL_PATH="$ROSE_WITH_UQTK"
    UQTK_INCLUDE_PATH="$ROSE_WITH_UQTK/include"
    UQTK_LIBRARY_PATH="$ROSE_WITH_UQTK/lib"
  else
    UQTK_INSTALL_PATH=
    UQTK_INCLUDE_PATH=
    UQTK_LIBRARY_PATH=
  fi

  ROSE_ARG_WITH(
    [uqtk-include],
    [if the Uncertainty Quantification Toolkit (UQTK) include directory was specified],
    [use this Uncertainty Quantification Toolkit (UQTK) include directory],
    []
  )
  if test "x$CONFIG_HAS_ROSE_WITH_UQTK_INCLUDE" != "xno"; then
      UQTK_INCLUDE_PATH="$ROSE_WITH_UQTK_INCLUDE"
  fi

  ROSE_ARG_WITH(
    [uqtk-lib],
    [if the Uncertainty Quantification Toolkit (UQTK) library directory was specified],
    [use this Uncertainty Quantification Toolkit (UQTK) library directory],
    []
  )
  if test "x$CONFIG_HAS_ROSE_WITH_UQTK_LIB" != "xno"; then
      UQTK_LIBRARY_PATH="$ROSE_WITH_UQTK_LIB" 
  fi

  if test "x$UQTK_INCLUDE_PATH" != "x"; then
      AC_CHECK_FILE(
          [${UQTK_INCLUDE_PATH}/PCSet.h],
          [],
          [ROSE_MSG_ERROR([PCSet.h is missing, can't compile with UQTK])])
  fi

  AM_CONDITIONAL(ROSE_WITH_UQTK, [test "x$UQTK_INCLUDE_PATH" != "x" && test "x$UQTK_LIBRARY_PATH" != "x"])
  AM_CONDITIONAL(ROSE_WITH_UQTK_INCLUDE, [test "x$UQTK_INCLUDE_PATH" != "x"])

  AC_SUBST(UQTK_INSTALL_PATH)
  AC_SUBST(UQTK_INCLUDE_PATH)
  AC_SUBST(UQTK_LIBRARY_PATH)

# End macro ROSE_SUPPORT_UQTK.
])

