###########################################################################
#                           COPYRIGHT NOTICE
#
#   Copyright (C) 2001, 2004, 2008 GE Intelligent Platforms, Inc.
#   International Copyright Secured.  All Rights Reserved.
#
###########################################################################
#   @(#)Makefile 01.0i4 GE Intelligent Platforms, Inc.
###########################################################################
#   Top level makefile for the RFM2g Reflective Memory device driver
###########################################################################
#
#   Revision History:
#   Date         Who  Ver   Reason For Change
#   -----------  ---  ----  -----------------------------------------------
#   01-NOV-2001  ML   1.0   Created.
#   24-SEP-2003  EB   01.02 Support added for Red Hat(tm) 8.0 and 9.0.
#
###########################################################################
#
# The SUBDIRS macro lists the individual components which are to be
# manipulated.
#
SUBDIRS	=driver api diags
#
# All targets share a common set of rules
#
all install clean uninstall:
	@for x in ${SUBDIRS}; do                    \
		( cd $$x; make ${MAKEFLAGS} $@ )    \
	done
#
