lib_LTLIBRARIES = libslclient.la
libslclient_la_SOURCES = util.c loader.c hashtable.c hashtable.h hashtable-utils.c hashtable-utils.h
libslclient_la_CFLAGS = $(IKSEMEL_CFLAGS) -I$(top_srcdir)/include
libslclient_la_LIBADD = $(IKSEMEL_LIBS) -ldl

bin_PROGRAMS = slclient

slclient_SOURCES = main.c
slclient_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
slclient_LDADD = $(TANINGIA_LIBS) ./libslclient.la

noinst_PROGRAMS = test-plugin

test_plugin_SOURCES = test-plugin.c
test_plugin_CFLAGS = -I$(top_srcdir)/include
test_plugin_LDADD = ./libslclient.la -ldl
