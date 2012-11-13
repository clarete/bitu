lib_LTLIBRARIES = libbitu.la
libbitu_la_SOURCES = util.c loader.c server.c hashtable.c hashtable.h	\
	hashtable-utils.c hashtable-utils.h conf.c transport.c		\
	transport-xmpp.c transport-irc.c

libbitu_la_CFLAGS = $(TANINGIA_CFLAGS) $(LIBIRCCLIENT_CFLAGS)	\
	$(IKSEMEL_CFLAGS) $(PTHREAD_CFLAGS) -I$(top_srcdir)/include
libbitu_la_LIBADD = $(TANINGIA_LIBS) $(LIBIRCCLIENT_LIBS)	\
	$(IKSEMEL_LIBS) $(PTHREAD_LIBS) -ldl

bin_PROGRAMS = bitu bituctl

bitu_SOURCES = main.c
bitu_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
bitu_LDADD = $(TANINGIA_LIBS) $(IKSEMEL_LIBS) ./libbitu.la

bituctl_SOURCES = shell.c
bituctl_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
bituctl_LDADD = $(TANINGIA_LIBS) ./libbitu.la -lreadline

noinst_PROGRAMS = test-plugin test-server test-util test-conf

test_plugin_SOURCES = test-plugin.c
test_plugin_CFLAGS =  $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
test_plugin_LDADD = $(TANINGIA_LIBS) ./libbitu.la -ldl

test_server_SOURCES = test-server.c
test_server_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
test_server_LDADD = ./libbitu.la $(TANINGIA_LIBS) $(IKSEMEL_LIBS) -ldl

test_util_SOURCES = test-util.c
test_util_CFLAGS = -I$(top_srcdir)/include
test_util_LDADD = ./libbitu.la

test_conf_SOURCES = test-conf.c
test_conf_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
test_conf_LDADD = ./libbitu.la $(TANINGIA_LIBS)
