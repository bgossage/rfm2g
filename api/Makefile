################################################################################
#                            COPYRIGHT NOTICE
#
# Copyright (c) 2001, 2004, 2008, GE Intelligent Platforms, Inc.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.  Redistributions in binary
# form must reproduce the above copyright notice, this list of conditions and
# the following disclaimer in the documentation and/or other materials provided
# with the distribution.  Neither the name of the GE Intelligent Platforms, Inc.
# nor the names of its contributors may be used to endorse or promote products
# derived from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 
################################################################################
#   @(#)Makefile Version: 01.04
################################################################################
#   Makefile for the RFM2g API Library
################################################################################
#
#   Revision History:
#   Date         Who  Ver   Reason For Change
#   -----------  ---  ----  -----------------------------------------------
#   31-OCT-2001  ML   01.00 Created.
#   30-SEP-2002  ML   01.01 Added support for RH 7.3
#   24-SEP-2003  EB   01.02 Added support for RH 8.0 and 9.0.  Choose an
#                           INCLUDEDIR appropriate to the kernel version.
#   08-DEC-2004  ML   01.04 Cleanup inc paths since no kernel includes
#                           are used anymore.
#
################################################################################

LIB	=librfm2g.a
MACH_TYPE:=$(shell uname -m)

ifeq ($(MACH_TYPE), x86_64)
RFM2G_LIBDIR=/usr/lib64/rfm2g
else
RFM2G_LIBDIR=/usr/lib/rfm2g
endif

LIBOBS	=open.o close.o transfer.o util.o config.o event.o rfm2g_errno.o

HFILES  =../include/rfm2g_api.h       \
         ../include/rfm2g_defs.h      \
         ../include/rfm2g_errno.h     \
         ../include/rfm2g_ioctl.h     \
         ../include/rfm2g_regs.h      \
         ../include/rfm2g_struct.h    \
         ../include/rfm2g_types.h     \
         ../include/rfm2g_version.h   \
         ../include/rfm2g_osspec.h    \
         ../include/rfm2g_defs_linux.h

CFLAGS = -Wall -I../include 

${LIB}: ${LIBOBS}
	ar -cruvs ${LIB} $?

all:	${LIB}

open.o:	    $(HFILES) open.c
close.o:    $(HFILES) close.c
transfer.o: $(HFILES) transfer.c
util.o:     $(HFILES) util.c
config.o:   $(HFILES) config.c
event.o:    $(HFILES) event.c
rfm2g_errno.o: $(HFILES) rfm2g_errno.c

install: all
	install -d ${RFM2G_LIBDIR}
	install -m0644 -p ${LIB} ${RFM2G_LIBDIR}
	@for x in ${HFILES}; do                             \
		( install -m0444 -p $${x} ${RFM2G_LIBDIR} )   \
	done

# Clean up intermediate files
clean:
	rm -f *.o *.a core

uninstall: clean
	rm -f ${RFM2G_LIBDIR}/${LIB}
	rm -f ${RFM2G_LIBDIR}/*.h
	rmdir ${RFM2G_LIBDIR}
