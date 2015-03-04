TARGETS	= $(OBJDIR)../data/Probe.h $(OBJDIR)Probe $(OBJDIR)DaMon
OBJDIR	=
PREFIX	= /usr/local
DESTDIR	=
BINDIR	= $(PREFIX)/bin
SBINDIR	= $(PREFIX)/sbin
CC	= cc
CPPFLAGSF=
CPPFLAGS=
CFLAGSF	= -W `pkg-config --cflags libApp`
CFLAGS	= -Wall -g -O2 -pedantic
LDFLAGSF= `pkg-config --libs libApp`
LDFLAGS	= 
RM	= rm -f
LN	= ln -f
MKDIR	= mkdir -m 0755 -p
INSTALL	= install


all: $(TARGETS)

$(OBJDIR)../data/Probe.h: ../data/Probe.interface
	./appbroker.sh -P "$(PREFIX)" -- "$(OBJDIR)../data/Probe.h"

Probe_OBJS = $(OBJDIR)probe.o
Probe_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
Probe_LDFLAGS = $(LDFLAGSF) $(LDFLAGS) -Wl,--export-dynamic

$(OBJDIR)Probe: $(Probe_OBJS)
	$(CC) -o $(OBJDIR)Probe $(Probe_OBJS) $(Probe_LDFLAGS)

DaMon_OBJS = $(OBJDIR)damon.o
DaMon_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
DaMon_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)DaMon: $(DaMon_OBJS)
	$(CC) -o $(OBJDIR)DaMon $(DaMon_OBJS) $(DaMon_LDFLAGS)

$(OBJDIR)probe.o: probe.c ../data/Probe.h ../config.h
	$(CC) $(Probe_CFLAGS) -o $(OBJDIR)probe.o -c probe.c

$(OBJDIR)damon.o: damon.c ../config.h
	$(CC) $(DaMon_CFLAGS) -o $(OBJDIR)damon.o -c damon.c

clean:
	$(RM) -- $(../data/Probe.h_OBJS) $(Probe_OBJS) $(DaMon_OBJS)
	./appbroker.sh -c -P "$(PREFIX)" -- "../data/Probe.h"

distclean: clean
	$(RM) -- $(TARGETS)

install: $(TARGETS)
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)Probe $(DESTDIR)$(BINDIR)/Probe
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)DaMon $(DESTDIR)$(BINDIR)/DaMon

uninstall:
	$(RM) -- $(DESTDIR)$(BINDIR)/Probe
	$(RM) -- $(DESTDIR)$(BINDIR)/DaMon

.PHONY: all clean distclean install uninstall
