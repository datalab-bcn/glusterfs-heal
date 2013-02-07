
# Copyright (c) 2012-2013 DataLab, S.L. <http://www.datalab.es>
#
# This file is part of the features/heal translator for GlusterFS.
#
# The features/heal translator for GlusterFS is free software: you can
# redistribute it and/or modify it under the terms of the GNU General
# Public License as published by the Free Software Foundation, either
# version 3 of the License, or (at your option) any later version.
#
# The features/heal translator for GlusterFS is distributed in the hope
# that it will be useful, but WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the features/heal translator for GlusterFS. If not, see
# <http://www.gnu.org/licenses/>.

SRCDIR = ../../gluster

XLATOR = heal.so
TYPE = features

OBJS := heal.o
OBJS += heal-type-dict.o

#------------------------------------------------------------------------------

PREFIX := $(shell sed -n "s/^prefix *= *\(.*\)/\1/p" $(SRCDIR)/Makefile)
GF_HOST_OS := $(shell sed -n "s/^GF_HOST_OS *= *\(.*\)/\1/p" $(SRCDIR)/Makefile)
VERSION := $(shell sed -n "s/^VERSION *= *\(.*\)/\1/p" $(SRCDIR)/Makefile)

LIBDIR = $(PREFIX)/lib

V ?= 0

VCC = $(VCC_$(V))
VCC_0 = @echo "  CC " $@;

VLD = $(VLD_$(V))
VLD_0 = @echo "  LD " $@;

VCP = $(VCP_$(V))
VCP_0 = @echo "  CP " $^;

VMV = @

VRM = $(VRM_$(V))
VRM_0 = @echo "  CLEAN";

VLN = $(VLN_$(V))
VLN_0 = @echo "  LN " $^;

VMKDIR = @

CC = gcc
LD = gcc
MV = mv
CP = cp
RM = rm
LN = ln
MKDIR = mkdir

INCS := -I$(SRCDIR)
INCS += -I$(SRCDIR)/libglusterfs/src
INCS += -I$(SRCDIR)/contrib/uuid

LIBS := -lglusterfs

DEFS := -DHAVE_CONFIG_H
DEFS += -D_FILE_OFFSET_BITS=64
DEFS += -D_GNU_SOURCE
DEFS += -D$(GF_HOST_OS)

CFLAGS = -fPIC -Wall -O0 -g
LDFLAGS = -fPIC -g -shared -nostartfiles -L$(LIBDIR)

DEPDIR = .deps
TGTDIR = $(LIBDIR)/glusterfs/$(VERSION)/xlator/$(TYPE)

all:	$(XLATOR)

-include $(DEPDIR)/*.Po

$(XLATOR):	$(OBJS)
		$(VLD)$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

install:	$(XLATOR)
		$(VCP)$(CP) $^ $(TGTDIR)/$^.0.0.0
		$(VLN)$(LN) -sf $^.0.0.0 $(TGTDIR)/$^.0
		$(VLN)$(LN) -sf $^.0.0.0 $(TGTDIR)/$^

clean:
		$(VRM)$(RM) -f *.o $(XLATOR)

.c.o:
		$(VMKDIR)$(MKDIR) -p $(DEPDIR)
		$(VCC)$(CC) $(CFLAGS) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c $(DEFS) $(INCS) -o $@ $<
		$(VMV)$(MV) -f $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Po
