#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([bitu], [0.1], [lincoln@comum.org])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AM_INIT_AUTOMAKE(foreign)

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strdup])

# Checks for .pc packages
PKG_CHECK_MODULES([IKSEMEL], [iksemel])
AC_SUBST([IKSEMEL_CFLAGS])
AC_SUBST([IKSEMEL_LIBS])

PKG_CHECK_MODULES([TANINGIA], [taningia])
AC_SUBST([TANINGIA_CFLAGS])
AC_SUBST([TANINGIA_LIBS])

AC_CONFIG_FILES([
        Makefile
        include/Makefile
        include/bitu/Makefile
        src/Makefile
        plugins/Makefile
        plugins/processor/Makefile
        plugins/uptime/Makefile
])

AC_OUTPUT
