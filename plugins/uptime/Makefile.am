pluginlibdir=$(libdir)/bitu
pluginlib_LTLIBRARIES = libuptime.la
libuptime_la_SOURCES = uptime.c uptime.h
libuptime_la_CFLAGS = -I$(top_srcdir)/include
libuptime_la_LDFLAGS = $(top_builddir)/src/libbitu.la -module -avoid-version

noinst_PROGRAMS = test
test_SOURCES = test.c uptime.c uptime.h
test_CFLAGS = -I$(top_srcdir)/include
test_LDFLAGS =
test_LDADD =
