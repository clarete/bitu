#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([bitu], [0.3], [http://projects.comum.org/bitu])
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

# Checks for libraries.
AC_CHECK_LIB([readline], [readline])
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([ircclient], [irc_connect], [
        AC_SUBST([LIBIRCCLIENT_CFLAGS])
        AC_SUBST([LIBIRCCLIENT_LIBS], [-lircclient])
])

# Checking for pthread
ACX_PTHREAD([
        AC_SUBST([PTHREAD_CFLAGS])
        AC_SUBST([PTHREAD_LIBS])
        CC="$PTHREAD_CC"
],[
        AC_MSG_ERROR([Compiling without threads support])
])

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

PKG_CHECK_MODULES([TANINGIA], [taningia >= 0.2.2])
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
