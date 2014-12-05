
CC      = gcc
CFLAGS  = -Wall -O2 -ggdb
LDFLAGS =
LIBS    =
INSTALL = install
prefix  = /usr/local

# not recommended to mess with anything below this point

HDR       = uzixfs.h uzixdir.h byteorder.h
COMMONOBJ = uzixfs.o uzixdir.o byteorder.o

DISTNAME  = UXU-1.0

DFILES    = \
byteorder.c  uzixdir.c  uzixfscat.c   uzixfsls.c \
mkuzixfs.c   uzixfs.c   uzixfsinfo.c \
byteorder.h  uzixdir.h  uzixfs.h \
mkuzixfs.1  uzixfscat.1  uzixfsinfo.1  uzixfsls.1 \
Makefile COPYING ChangeLog README README.pt

LKFILES   = \
uzix.c uzix.h README Makefile

all: uzixfscat uzixfsinfo uzixfsls mkuzixfs

mkuzixfs: mkuzixfs.o $(COMMONOBJ)
	$(CC) $(LDFLAGS) $(LIBS) mkuzixfs.o $(COMMONOBJ) -o mkuzixfs

uzixfsls: uzixfsls.o $(COMMONOBJ)
	$(CC) $(LDFLAGS) $(LIBS) uzixfsls.o $(COMMONOBJ) -o uzixfsls

uzixfsinfo: uzixfsinfo.o $(COMMONOBJ)
	$(CC) $(LDFLAGS) $(LIBS) uzixfsinfo.o $(COMMONOBJ) -o uzixfsinfo

uzixfscat: uzixfscat.o $(COMMONOBJ)
	$(CC) $(LDFLAGS) $(LIBS) uzixfscat.o $(COMMONOBJ) -o uzixfscat

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f uzixfsinfo uzixfsls mkuzixfs uzixfscat *.o *~

cleandist:
	rm -f UXU-*.tar.gz

dist:
	rm -rf $(DISTNAME)
	mkdir $(DISTNAME)
	mkdir $(DISTNAME)/Linux
	cp -f $(DFILES) $(DISTNAME)
	cd Linux ; cp -f $(LKFILES) ../$(DISTNAME)/Linux
	rm -f $(DISTNAME).tar.gz
	tar zcf $(DISTNAME).tar.gz $(DISTNAME)
	rm -rf $(DISTNAME)

install: uzixfscat uzixfsinfo uzixfsls mkuzixfs
	$(INSTALL) -c -d -m 0755 $(prefix)/bin
	$(INSTALL) -c -d -m 0755 $(prefix)/man/man1
	$(INSTALL) -c -m 0755 uzixfsinfo $(prefix)/bin
	$(INSTALL) -c -m 0755 uzixfscat  $(prefix)/bin
	$(INSTALL) -c -m 0755 uzixfsls   $(prefix)/bin
	$(INSTALL) -c -m 0755 mkuzixfs   $(prefix)/bin
	$(INSTALL) -c -m 0644 uzixfsinfo.1 $(prefix)/man/man1
	$(INSTALL) -c -m 0644 uzixfscat.1  $(prefix)/man/man1
	$(INSTALL) -c -m 0644 uzixfsls.1   $(prefix)/man/man1
	$(INSTALL) -c -m 0644 mkuzixfs.1   $(prefix)/man/man1

# dependencies

byteorder.o:  byteorder.c $(HDR)
mkuzixfs.o:   mkuzixfs.c $(HDR)
uzixdir.o:    uzixdir.c $(HDR)
uzixfs.o:     uzixfs.c $(HDR)
uzixfscat.o:  uzixfscat.c $(HDR)
uzixfsinfo.o: uzixfsinfo.c $(HDR)
uzixfsls.o:   uzixfsls.c $(HDR)
