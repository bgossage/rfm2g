/*
===============================================================================
                            COPYRIGHT NOTICE

Copyright (c) 2006, 2008, GE Intelligent Platforms, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.  Redistributions in binary
form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided
with the distribution.  Neither the name of the GE Intelligent Platforms, Inc.
nor the names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------

$Workfile: rfm2g_driver.h $
$Revision: 40622 $
$Modtime: 9/03/10 1:16p $

-------------------------------------------------------------------------------
    Description: RFM2g Linux Header File
-------------------------------------------------------------------------------

==============================================================================*/

/*****************************************************************************/

/* Kernel Device Driver - This is defined in Makefile_PPC */
#ifdef CFG_RFM_KERNEL_DRIVER

#include <linux/config.h>
#include <net/rfm2g/rfm2g_kinc.h>

/* Installable Device Driver */
#else
#include "rfm2g_kinc.h"
#endif

/* Common System Headers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/zconf.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/devfs_fs_kernel.h>
#endif

/* Kernel Device Driver */
#ifdef CFG_RFM_KERNEL_DRIVER

#include <net/rfm2g/rfm2g_defs.h>
#include <net/rfm2g/rfm2g_struct.h>
#include <net/rfm2g/rfm2g_ioctl.h>
#include <net/rfm2g/rfm2g_regs.h>
#include <net/rfm2g/rfm2g_version.h>

/* Installable Device Driver */
#else

#include "rfm2g_defs.h"
#include "rfm2g_struct.h"
#include "rfm2g_ioctl.h"
#include "rfm2g_regs.h"
#include "rfm2g_version.h"

#endif

