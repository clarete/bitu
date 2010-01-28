lib_LTLIBRARIES = libslclient.la
libslclient_la_SOURCES = util.c
libslclient_la_CFLAGS = $(IKSEMEL_CFLAGS) -I$(top_srcdir)/include
libslclient_la_LIBADD = $(IKSEMEL_LIBS)

bin_PROGRAMS = slclient
slclient_SOURCES = main.c
slclient_CFLAGS = $(TANINGIA_CFLAGS) -I$(top_srcdir)/include
slclient_LDADD = $(TANINGIA_LIBS)
