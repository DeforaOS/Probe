TARGETS	= $(OBJDIR)../data/Probe.h $(OBJDIR)Probe$(EXEEXT) $(OBJDIR)DaMon$(EXEEXT)
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
EXEEXT	=
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

$(OBJDIR)Probe$(EXEEXT): $(Probe_OBJS)
	$(CC) -o $(OBJDIR)Probe$(EXEEXT) $(Probe_OBJS) $(Probe_LDFLAGS)

DaMon_OBJS = $(OBJDIR)damon.o
DaMon_CFLAGS = $(CPPFLAGSF) $(CPPFLAGS) $(CFLAGSF) $(CFLAGS)
DaMon_LDFLAGS = $(LDFLAGSF) $(LDFLAGS)

$(OBJDIR)DaMon$(EXEEXT): $(DaMon_OBJS)
	$(CC) -o $(OBJDIR)DaMon$(EXEEXT) $(DaMon_OBJS) $(DaMon_LDFLAGS)

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
	$(INSTALL) -m 0755 $(OBJDIR)Probe$(EXEEXT) $(DESTDIR)$(BINDIR)/Probe$(EXEEXT)
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(OBJDIR)DaMon$(EXEEXT) $(DESTDIR)$(BINDIR)/DaMon$(EXEEXT)

uninstall:
	$(RM) -- $(DESTDIR)$(BINDIR)/Probe$(EXEEXT)
	$(RM) -- $(DESTDIR)$(BINDIR)/DaMon$(EXEEXT)

.PHONY: all clean distclean install uninstall
