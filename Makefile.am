EXTRA_DIST=README.md

lib_LTLIBRARIES=libcontainer.la
libcontainer_la_SOURCES=ffplay.c bus.c

pkginclude_HEADERS=libffplay.h

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libffplay.pc

distclean-local:
	-rm -rf Makefile.in aclocal.m4 autom4te.cache/ config.guess config.sub depcomp install-sh ltmain.sh missing
