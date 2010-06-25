#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([bitu], [0.3], [lincoln@comum.org])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE(foreign)
LT_INIT

AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h fcntl.h sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_C_INLINE

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strdup atexit memset select socket strerror])

# Checks for .pc packages
PKG_CHECK_MODULES([IKSEMEL], [iksemel])
AC_SUBST([IKSEMEL_CFLAGS])
AC_SUBST([IKSEMEL_LIBS])

PKG_CHECK_MODULES([TANINGIA], [taningia])
AC_SUBST([TANINGIA_CFLAGS])
AC_SUBST([TANINGIA_LIBS])

AC_CONFIG_FILES([
        libbitu.pc
        Makefile
        include/Makefile
        include/bitu/Makefile
        src/Makefile
        plugins/Makefile
        plugins/cpuinfo/Makefile
        plugins/uptime/Makefile
])

AC_OUTPUT
