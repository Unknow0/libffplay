#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([libffplay], [1.0])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_CONFIG_SRCDIR([ffplay.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([ffmpeg], [libavdevice libavformat libavcodec libavutil libavfilter])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([
	Makefile
	libffplay.pc
	])

AC_OUTPUT
