pluginlibdir=$(libdir)/bitu
pluginlib_LTLIBRARIES = libcpuinfo.la
libcpuinfo_la_SOURCES = cpuinfo.c cpuinfo.h
libcpuinfo_la_CFLAGS = -I$(top_srcdir)/include
libcpuinfo_la_LIBADD =
libcpuinfo_la_LDFLAGS = $(top_builddir)/src/libbitu.la

noinst_PROGRAMS = test
test_SOURCES = test.c cpuinfo.c cpuinfo.h
test_CFLAGS = -I$(top_srcdir)/include
test_LDFLAGS = $(top_builddir)/src/libbitu.la
test_LDADD =