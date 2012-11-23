pluginlibdir=$(libdir)/bitu
pluginlib_LTLIBRARIES = libhello.la
libhello_la_SOURCES = hello.c hello.h
libhello_la_CFLAGS = -I$(top_srcdir)/include
libhello_la_LIBADD =
libhello_la_LDFLAGS = $(top_builddir)/src/libbitu.la -module -avoid-version

noinst_PROGRAMS = test
test_SOURCES = test.c hello.c hello.h
test_CFLAGS = -I$(top_srcdir)/include
test_LDFLAGS = $(top_builddir)/src/libbitu.la
test_LDADD =
