ALL=px

CFLAGS=-g -O2 -Wall -Wno-switch -Wextra -Wwrite-strings
LDFLAGS=-lprocps

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

all: $(ALL)

clean: FRC
	rm -f $(ALL)

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	install -m0755 $(ALL) $(DESTDIR)$(BINDIR)
	install -m0644 $(ALL:=.1) $(DESTDIR)$(MANDIR)/man1

README: px.1
	mandoc -Tutf8 $< | col -bx >$@

FRC:
