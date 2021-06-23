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

$Workfile: rfm2g_defs_linux.h $
$Revision: 40623 $
$Modtime: 9/03/10 1:53p $

-------------------------------------------------------------------------------
    Description: RFM2g Linux OS Specific Header File
-------------------------------------------------------------------------------

==============================================================================*/

/*****************************************************************************/
/*

 The rfm2g_defs_linux.h header file should


INCLUDE FILES:

Include Dependencies

rfm2g_osspec.h         rfm2g_struct.h (Driver Side)
	|						|
	-------------------------
				|
				|
		rfm2g_defs_linux.h
				|
				|
			rfm2g_defs.h (Common)
				|
				|
	-------------------------
	|						|
rfm2g_types.h			rfm2g_errno.h (Common)

*/

#ifndef RFM2G_DEFS_LINUX_H
#define RFM2G_DEFS_LINUX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rfm2g_defs.h"

/*****************************************************************************/

/* Special offset values for mmap */
#define RFM2G_DMA_MMAP_OFFSET        1
#define RFM2G_BAR0_MMAP_OFFSET       2
#define RFM2G_BAR2_MMAP_OFFSET       4

/* Qflags definitions */
#define QFLAG_HALF_FULL             (1<<0)
#define QFLAG_OVERFLOWED            (1<<1)
#define QFLAG_DEQUEUED              (1<<2)

typedef struct rfm2gLinuxRegInfo
{
	RFM2GREGSETTYPE regset;
	RFM2G_UINT32 Offset;
	RFM2G_UINT32 Width;
	RFM2G_UINT32 Value;

} RFM2GLINUXREGINFO;

/*****************************************************************************/

/* RFM2GQINFO is used by RFM2gGetEventStats and RFM2gClearEventStats functions */

typedef struct rfm2gQInfo
{
   RFM2G_UINT8    Event;          /* Which queue to access            */
   RFM2G_BOOL     Overflowed;     /* Queue has overflowed if TRUE     */
   RFM2G_UINT8    QueuePeak;      /* Max num. of enqueued interrupts  */
   RFM2G_UINT8    reserved;       /* Alignment                        */
   RFM2G_UINT32   EventCount;     /* Number of events received        */
   RFM2G_UINT32   EventsQueued;   /* Number of events in queue        */
   RFM2G_UINT32   QueueSize;      /* How large is the queue           */
   RFM2G_UINT32   MaxQueueSize;   /* Max size of the queue            */
   RFM2G_UINT32   EventTimeout;   /* How long to wait for interrupt   */

}  RFM2GQINFO, *PRFM2GQINFO;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RFM2G_DEFS_LINUX_H */
