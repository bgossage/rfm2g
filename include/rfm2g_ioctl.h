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
$Workfile: rfm2g_ioctl.h $
$Revision: 40623 $
$Modtime: 9/03/10 1:53p $
-------------------------------------------------------------------------------
    Description: RFM2G IOCTL command definitions
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    12-OCT-2001  ML   1.0   Created.
    18 Jun 2008  EB   6.00  Added IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET and
                            IOCTL_RFM2G_CLEAR_DMA_MMAP_OFFSET.  This is  
                            to store the 64-bit physical address of a DMA buffer
                            or let the driver know that the BAR0 or BAR2 
                            register space is to be mapped.
                            This change is in support of PAE (Physical
                            Address Extension)-enabled Linux.
    06 Aug 2008  EB   6.04  Added IOCTL_RFM2G_SET_SLIDING_WINDOW_OFFSET that
                            programs the necessary RFM registers and also
                            allows the driver to keep a copy of the current  
                            sliding window offset, needed when DMA is used with 
                            a sliding window.

===============================================================================
*/


#ifndef __RFM2G_IOCTL_H
#define __RFM2G_IOCTL_H
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif
#include "rfm2g_types.h"
#include "rfm2g_regs.h"


/*****************************************************************************/

/* The RFM2G magic number */
#define RFM2G_MAGIC 0xeb


/*****************************************************************************/

/* Data Transfer */

typedef struct rfm2gAtomic  /* Used for peeking and poking */
{
    RFM2G_UINT64    data;       /* Data read from or written to RFM          */
    RFM2G_UINT32    offset;     /* Offset into Reflective Memory             */
    RFM2G_UINT8     width;      /* 1=8 bits, 2=16 bits, 4=32 bits, 8=64 bits */
    RFM2G_UINT8     spare[3];   /* For alignment                             */

} RFM2GATOMIC;

typedef struct rfm2gTransfer  /* Used for reading and writing */
{
    RFM2G_UINT32    Offset;     /* Offset into Reflective Memory             */
    RFM2G_UINT32    Length;     /* Number of bytes to transfer               */
    void *          Buffer;     /* Address to transfer to/from               */
} RFM2GTRANSFER;

#define IOCTL_RFM2G_ATOMIC_PEEK       _IOWR(RFM2G_MAGIC, 10, struct rfm2gAtomic)
#define IOCTL_RFM2G_ATOMIC_POKE       _IOW(RFM2G_MAGIC,  11, struct rfm2gAtomic)
#define IOCTL_RFM2G_READ              _IOW(RFM2G_MAGIC,  73, RFM2G_ADDR)
#define IOCTL_RFM2G_WRITE             _IOW(RFM2G_MAGIC,  74, RFM2G_ADDR)

/*****************************************************************************/

#define IOCTL_RFM2G_ENABLE_EVENT      _IOW(RFM2G_MAGIC,  20, RFM2GEVENTINFO)
#define IOCTL_RFM2G_DISABLE_EVENT     _IOW(RFM2G_MAGIC,  21, RFM2GEVENTINFO)
#define IOCTL_RFM2G_SEND_EVENT        _IOW(RFM2G_MAGIC,  22, RFM2GEVENTINFO)
#define IOCTL_RFM2G_WAIT_FOR_EVENT    _IOWR(RFM2G_MAGIC, 23, RFM2GEVENTINFO)
#define IOCTL_RFM2G_CANCEL_EVENT      _IOW(RFM2G_MAGIC,  24, RFM2GEVENTINFO)
#define IOCTL_RFM2G_GET_EVENT_STATS   _IOWR(RFM2G_MAGIC, 25, RFM2GQINFO)
#define IOCTL_RFM2G_CLEAR_EVENT_STATS _IOW(RFM2G_MAGIC,  26, RFM2G_UINT8)
#define IOCTL_RFM2G_FLUSH_QUEUE       _IOW(RFM2G_MAGIC,  29, RFM2G_UINT8)
#define IOCTL_RFM2G_CLEAR_EVENT       _IOW(RFM2G_MAGIC,  71, RFM2G_UINT16)
#define IOCTL_RFM2G_CLEAR_EVENT_COUNT _IOW(RFM2G_MAGIC,  72, RFM2G_UINT8)

/*****************************************************************************/

/* Configuration */

#define IOCTL_RFM2G_GET_CONFIG               _IOWR(RFM2G_MAGIC, 30, struct RFM2GCONFIG_)
#define IOCTL_RFM2G_GET_DMA_THRESHOLD        _IOR(RFM2G_MAGIC, 33, RFM2G_UINT32)
#define IOCTL_RFM2G_SET_DMA_THRESHOLD        _IOW(RFM2G_MAGIC, 34, RFM2G_UINT32)
#define IOCTL_RFM2G_GET_DMA_BYTESWAP         _IOR(RFM2G_MAGIC, 67, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_DMA_BYTE_SWAP        _IOW(RFM2G_MAGIC, 68, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_PIO_BYTESWAP         _IOR(RFM2G_MAGIC, 69, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_PIO_BYTE_SWAP        _IOW(RFM2G_MAGIC, 70, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET  _IOW(RFM2G_MAGIC, 77, RFM2G_UINT64)
#define IOCTL_RFM2G_CLEAR_DMA_MMAP_OFFSET    _IOW(RFM2G_MAGIC, 78, RFM2G_UINT32)

/*****************************************************************************/

/* Debugging */

#define IOCTL_RFM2G_GET_DEBUG_FLAGS    _IOR(RFM2G_MAGIC, 40, RFM2G_UINT32)
#define IOCTL_RFM2G_SET_DEBUG_FLAGS    _IOW(RFM2G_MAGIC, 41, RFM2G_UINT32)


/*****************************************************************************/

/* Utility */

#define IOCTL_RFM2G_SET_LED               _IOW(RFM2G_MAGIC, 50, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_LED               _IOR(RFM2G_MAGIC, 51, RFM2G_UINT8)
#define IOCTL_RFM2G_CHECK_RING_CONT       _IOR(RFM2G_MAGIC, 52, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_DARK_ON_DARK      _IOR(RFM2G_MAGIC, 53, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_DARK_ON_DARK      _IOW(RFM2G_MAGIC, 54, RFM2G_UINT8)
#define IOCTL_RFM2G_CLEAR_OWN_DATA        _IOR(RFM2G_MAGIC, 55, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_TRANSMIT_DISABLE  _IOR(RFM2G_MAGIC, 56, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_TRANSMIT_DISABLE  _IOW(RFM2G_MAGIC, 57, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_LOOPBACK	      _IOR(RFM2G_MAGIC, 58, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_LOOPBACK	      _IOW(RFM2G_MAGIC, 59, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_PARITY_ENABLE     _IOR(RFM2G_MAGIC, 60, RFM2G_UINT8)
#define IOCTL_RFM2G_SET_PARITY_ENABLE     _IOW(RFM2G_MAGIC, 61, RFM2G_UINT8)
#define IOCTL_RFM2G_GET_MEMORY_OFFSET     _IOR(RFM2G_MAGIC, 62, RFM2G_MEM_OFFSETTYPE)
#define IOCTL_RFM2G_SET_MEMORY_OFFSET     _IOW(RFM2G_MAGIC, 63, RFM2G_MEM_OFFSETTYPE)
#define IOCTL_RFM2G_SET_PCI_COMPATIBILITY _IOW(RFM2G_MAGIC, 64, RFM2G_UINT8)
#define IOCTL_RFM2G_READ_REG              _IOR(RFM2G_MAGIC, 65, RFM2G_UINT32)
#define IOCTL_RFM2G_WRITE_REG             _IOW(RFM2G_MAGIC, 66, RFM2G_UINT32)

#define IOCTL_RFM2G_CLEAR_REG             _IOW(RFM2G_MAGIC, 75, RFM2G_UINT32)
#define IOCTL_RFM2G_SET_REG               _IOW(RFM2G_MAGIC, 76, RFM2G_UINT32)
#define IOCTL_RFM2G_SET_SLIDING_WINDOW_OFFSET  _IOW(RFM2G_MAGIC, 79, RFM2G_UINT32)

/*****************************************************************************/


#endif  /* __RFM2G_IOCTL_H */
