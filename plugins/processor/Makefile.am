lib_LTLIBRARIES = libprocessor.la
libprocessor_la_SOURCES = processor.c processor.h
libprocessor_la_CFLAGS = -I$(top_srcdir)/include
libprocessor_la_LIBADD =
libprocessor_la_LDFLAGS = $(top_builddir)/src/libslclient.la

noinst_PROGRAMS = test
test_SOURCES = test.c processor.c processor.h
test_CFLAGS = -I$(top_srcdir)/include
test_LDFLAGS = $(top_builddir)/src/libslclient.la
test_LDADD =