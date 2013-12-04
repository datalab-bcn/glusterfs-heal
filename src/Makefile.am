
MAKEFLAGS = $(AM_MAKEFLAGS)

xlator_LTLIBRARIES := heal.la
xlatordir = $(gflibdir)/xlator/features

heal_la_LDFLAGS := -module -avoid-version

heal_la_SOURCES := heal.c
heal_la_SOURCES += heal-type-dict.c

heal_la_LIBADD = $(gfdir)/libglusterfs/src/libglusterfs.la $(gfsys)/src/libgfsys.la
