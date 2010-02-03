lib_LTLIBRARIES = libbitu.la
libbitu_la_SOURCES = util.c loader.c hashtable.c hashtable.h hashtable-utils.c hashtable-utils.h
libbitu_la_CFLAGS = $(IKSEMEL_CFLAGS) -I$(top_srcdir)/include
libbitu_la_LIBADD = $(IKSEMEL_LIBS) -ldl

bin_PROGRAMS = bitu

bitu_SOURCES = main.c
bitu_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
bitu_LDADD = $(TANINGIA_LIBS) ./libbitu.la

noinst_PROGRAMS = test-plugin

test_plugin_SOURCES = test-plugin.c
test_plugin_CFLAGS = -I$(top_srcdir)/include
test_plugin_LDADD = ./libbitu.la -ldl
