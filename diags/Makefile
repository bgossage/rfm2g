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
# $Workfile: Makefile $
# $Revision: 40621 $
# $Modtime: 9/03/10 1:43p $
###########################################################################
#   Makefile for RFM2g utility
###########################################################################
#
#   Revision History:
#   Date         Who  Ver   Reason For Change
#   -----------  ---  ----  -----------------------------------------------
#
###########################################################################

MACH_TYPE:=$(shell uname -m)

#ifeq ($(MACH_TYPE), x86_64)
#API_LIBDIR	=/usr/lib64/rfm2g
#PTHREAD_LIBDIR	=/usr/lib64
#else
#API_LIBDIR	=/usr/lib/rfm2g
#PTHREAD_LIBDIR	=/usr/lib
#endif

API_LIBDIR	= ../include
PTHREAD_LIBDIR	= ../api
HFILES	=${API_LIBDIR}/rfm2g_api.h 
CFLAGS	=-Wall -I${API_LIBDIR} -DRFM2G_LINUX

OBS	=rfm2g_util.o

all:	rfm2g_util

rfm2g_util:	$(OBS)
	$(CC) $(LDFLAGS) -o rfm2g_util $(OBS) -L$(API_LIBDIR) -lrfm2g -L$(PTHREAD_LIBDIR) -lpthread  

rfm2g_util.o:$(HFILES) rfm2g_util.c

clean:
	rm -f *.o rfm2g_util core

install: all

uninstall: clean
