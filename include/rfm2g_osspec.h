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

$Workfile: rfm2g_osspec.h $
$Revision: 40623 $
$Modtime: 9/03/10 1:53p $

-------------------------------------------------------------------------------
    Description: RFM2g Linux OS Specific Header File
-------------------------------------------------------------------------------

==============================================================================*/

/*****************************************************************************/
/*

 The rfm2g_osspec.h header file should

 1) define STDRFM2GCALL which is used for functions that return RFM2G_STATUS
 2) define RFM2GHANDLE
 3) prototypes for driver specific functions
 4) include other header files as necessary.
 5) include common API definitions header file rfm2g_defs.h
 6) Include header file that defines RFM2G types, rfm2g_types.h

INCLUDE FILES:

User Application Include Dependencies

	rfm2g_util.c (Common)
		|
	rfm2g_api.h (Common)
		|
	rfm2g_osspec.h (Driver Specific)
		|
	rfm2g_defs.h (Common)
		|
rfm2g_errno.h (Common)  rfm2g_types.h (Driver Specific)
*/

#ifndef RFM2G_OSSPEC_H
#define RFM2G_OSSPEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <semaphore.h>

#include "rfm2g_defs_linux.h"
#include "rfm2g_ioctl.h"

/*****************************************************************************/

/* Define a MACRO for the standard calling pattern */
#define STDRFM2GCALL _LibDeclSpec RFM2G_STATUS _CallDeclSpec

/* RFM2GEVENT_FIFO_AF has been deprecrated and replaced with RFM2GEVENT_RXFIFO_AFULL */
#define RFM2GEVENT_FIFO_AF RFM2GEVENT_RXFIFO_AFULL

typedef struct rfm2ghandle_s *RFM2GHANDLE;

typedef struct rfm2gCallbackInfo
{
	int CallbackStat;               /* Results of registering Callback func */
    void (*Callback)(void *, RFM2GEVENTINFO*); /* Callback function pointer */
    pthread_t thread;               /* IDs for threads used in async notify */
	RFM2GHANDLE Handle;
	RFM2GEVENTTYPE Event;
	sem_t callbackdispachSemId;
} rfm2gCallbackInfo_t, *RFM2GCALLBACKINFO;
/*****************************************************************************/

/* Define the device handle */

typedef struct rfm2ghandle_s
{
    RFM2G_INT32  fd;          /* File descriptor returned by open(2) call */
    RFM2GCONFIG  Config;      /* Information about opened RFM device      */
	rfm2gCallbackInfo_t callbackInfo[RFM2GEVENT_LAST];
    RFM2G_UINT32 struct_id;
} rfm2ghandle_t;

/* Linux Driver Specific Functions */
STDRFM2GCALL RFM2gGetEventStats(RFM2GHANDLE rh, RFM2GQINFO *Qinfo);
STDRFM2GCALL RFM2gFlushEventQueue(RFM2GHANDLE rh, RFM2GEVENTTYPE Event);
STDRFM2GCALL RFM2gClearEventStats(RFM2GHANDLE rh, RFM2GEVENTTYPE EventType );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RFM2G_OSSPEC_H */
