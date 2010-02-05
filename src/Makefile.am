lib_LTLIBRARIES = libbitu.la
libbitu_la_SOURCES = util.c loader.c server.c hashtable.c hashtable.h hashtable-utils.c hashtable-utils.h
libbitu_la_CFLAGS = $(IKSEMEL_CFLAGS) -I$(top_srcdir)/include
libbitu_la_LIBADD = $(IKSEMEL_LIBS) -ldl

bin_PROGRAMS = bitu bitush

bitu_SOURCES = main.c
bitu_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
bitu_LDADD = $(TANINGIA_LIBS) ./libbitu.la

bitush_SOURCES = shell.c
bitush_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
bitush_LDADD = $(TANINGIA_LIBS) ./libbitu.la -lreadline

noinst_PROGRAMS = test-plugin test-server

test_plugin_SOURCES = test-plugin.c
test_plugin_CFLAGS = -I$(top_srcdir)/include
test_plugin_LDADD = ./libbitu.la -ldl

test_server_SOURCES = test-server.c
test_server_CFLAGS = -I$(top_srcdir)/include
test_server_LDADD = ./libbitu.la -ldl
