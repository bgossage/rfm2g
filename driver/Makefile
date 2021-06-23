################################################################################
#                            COPYRIGHT NOTICE
#
# Copyright (c) 2004-2014, GE Intelligent Platforms, Inc.
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
#   @(#)Makefile 01.04 GE Intelligent Platforms, Inc.
################################################################################
#   Makefile for the RFM2g device driver (Linux)
################################################################################
#
#   Revision History:
#   Date         Who  Ver   Reason For Change
#   -----------  ---  ----  -----------------------------------------------
#   11-OCT-2001  ML   01.00 Created.
#   30-SEP-2002  ML   01.01 Added support for RH 7.3
#   24-SEP-2003  EB   01.02 Call the fixupflags.sh script to enable a
#                           successful build on Red Hat 7.2, 8.0, or 9.0.
#                           Choose an INCLUDEDIR appropriate to the kernel
#                           version.
#   08-DEC-2004  ML   01.04 Modified to support 2.6 as well as call old
#                           original Makefile for 2.4 kernel
#
###########################################################################

PROJNAME:=rfm2g
DEVFILE:=/dev/rfm2g

ifneq ($(KERNELRELEASE),)

obj-m	   += rfm2g.o
rfm2g-objs := driver_init.o proc_page.o driver_entry.o device_probe.o \
              device_init.o interrupts.o
EXTRA_CFLAGS += -I$(src)/../include

else

PATCHLEVEL:=$(shell uname -r | cut -d. -f2)
RFM2G_BOOT:=/etc/rc.d/init.d
RFM2G_BOOT3:=/etc/rc.d/rc3.d/S50rfm2g
RFM2G_BOOT5:=/etc/rc.d/rc5.d/S50rfm2g
ifneq ("$(wildcard /etc/lsb-release)", "")
OS:=$(shell lsb_release -si)
else
OS:=Fedora
endif

EXTRA_CFLAGS += -I$(src)/../include
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

ifeq ($(PATCHLEVEL), 4)
INSTRULES:=
UNINSTRULES:=
else
INSTRULES:=all
UNINSTRULES:=clean
endif

# Install paths
moduledir:= /lib/modules/$(shell uname -r)/

all:
        ifeq ($(PATCHLEVEL), 4)
		$(MAKE) -f Makefile2.4 $@
        else
		$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
        endif

clean:
        ifeq ($(PATCHLEVEL), 4)
		$(MAKE) -f Makefile2.4 $@
        else
		$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) $@
        endif

install: $(INSTRULES)
        ifeq ($(PATCHLEVEL), 4)
		$(MAKE) -f Makefile2.4 $@
        else
#		all
		$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install
        ifeq ($(OS),Ubuntu)
		install -m0755 -p ../rfm2g_init_ubuntu /etc/init.d/rfm2g_init
		update-rc.d rfm2g_init defaults
		service rfm2g_init start
        else
		install -m0755 -p ../rfm2g_init ${RFM2G_BOOT}/rfm2g_init
		install -m0755 -p ../rfm2g_load ${RFM2G_BOOT}/rfm2g_load
		install -m0755 -p ../rfm2g_unload ${RFM2G_BOOT}/rfm2g_unload
		cp ../rfm2g_init.service /lib/systemd/system/
		systemctl enable rfm2g_init.service
		sh ../rfm2g_load
        endif
        endif

uninstall: $(UNINSTRULES)
        ifeq ($(PATCHLEVEL), 4)
		$(MAKE) -f Makefile2.4 $@
        else
#		clean
        ifeq ($(OS),Ubuntu)
		service rfm2g_init stop
		update-rc.d -f rfm2g_init remove
		rm -f ${RFM2G_BOOT}/rfm2g_init
        else
		sh ../rfm2g_unload
		rm -f $(moduledir)/extra/$(PROJNAME).*
		systemctl disable rfm2g_init.service
		rm -f /lib/systemd/system/rfm2g_init.service
		rm -f ${RFM2G_BOOT}/rfm2g_init
		rm -f ${RFM2G_BOOT}/rfm2g_load
		rm -f ${RFM2G_BOOT}/rfm2g_unload
        endif
        endif

default:
        ifeq ($(PATCHLEVEL), 4)
		$(MAKE) -f Makefile2.4 $@
        else
		$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
        endif


endif
