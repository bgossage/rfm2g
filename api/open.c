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
$Workfile: open.c $
$Revision: 40619 $
$Modtime: 9/03/10 1:35p $
-------------------------------------------------------------------------------
    Description: RFM2g API RFM2gOpen function.
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    31-OCT-2001  ML   1.0   Created.

===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
/*#include <asm/page.h>
*/
#include <unistd.h>
#include "rfm2g_api.h"

static RFM2GHANDLE min_handle = (RFM2GHANDLE)0xFFFFFFFF;
static RFM2GHANDLE max_handle = (RFM2GHANDLE)0;
#define RFM2G_STRUCT_ID (RFM2G_UINT32)(('R'<< 24) | ('F'<<16) | ('M'<<8) | ('2'))


/******************************************************************************
*
*  FUNCTION:   RFM2gOpen
*
*  PURPOSE:    Opens the RFM2g driver and returns an RFM2GHANDLE
*  PARAMETERS: (I)  *DevicePath: path to special device file, e.g. /dev/rfm2g0
*              (IO) *rh: pointer to an RFM2GHANDLE structure
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_BAD_PARAMETER_1 if DevicePath is NULL string
*              RFM2G_BAD_PARAMETER_2 if rh is NULL pointer
*              RFM2G_NOT_OPEN if open(2) call to driver fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gOpen( char *DevicePath, RFM2GHANDLE *rh)
{
    int fdesc;    /* File description returned from open(2) call  */
    int i;

    /* Check DevicePath */
    if( DevicePath == (char *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_1 );
    }

    /* Make sure the device file is present */
    if( access(DevicePath, F_OK) != 0 )
    {
        return( RFM2G_NO_RFM2G_BOARD );
    }

    /* Check the handle */
    if( rh == (RFM2GHANDLE *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Allocate an RFM2GHANDLE structure */
    *rh = (RFM2GHANDLE) calloc( 1, sizeof(rfm2ghandle_t) );
    if( *rh == (RFM2GHANDLE) NULL )
    {
        return( RFM2G_LOW_MEMORY );
    }

    /* Open the device driver */
    fdesc = open( DevicePath, O_RDWR );
    if( fdesc < 0 )
    {
        /* Deallocate the RFM2GHANDLE structure */
        free( (void *) *rh );
        *rh = NULL;
        return( RFM2G_NOT_OPEN );
    }

    /* Save a local copy of this RFM device's RFM2GCONFIG structure */
    if( ioctl( fdesc, IOCTL_RFM2G_GET_CONFIG, &((*rh)->Config) ) != 0 )
    {
		/* Deallocate the RFM2GHANDLE structure */
        free( (void *) *rh );
        *rh = NULL;
        return( RFM2G_OS_ERROR );
    }

    (*rh)->fd = fdesc;
	(*rh)->struct_id = RFM2G_STRUCT_ID;
	if (*rh < min_handle) {
		min_handle = *rh;
	}
	if (*rh > max_handle) {
		max_handle = *rh;
	}

	/* Init Handle Pointer in callbackInfo member */
    for( i=0; i < RFM2GEVENT_LAST; i++ )
	{
		(*rh)->callbackInfo[i].Handle = *rh;
		(*rh)->callbackInfo[i].Event = (RFM2GEVENTTYPE) i;
	}

    return( RFM2G_SUCCESS );

}   /* End of RFM2gOpen() */

STDRFM2GCALL
RFM2gCheckHandle(RFM2GHANDLE rh)
{
    if (rh == NULL) {
        return(RFM2G_NULL_DESCRIPTOR);
    }
	if ( (rh < min_handle) || (rh > max_handle) ) {
		return(RFM2G_BAD_PARAMETER_1);
	}
	if (rh->struct_id != RFM2G_STRUCT_ID) {
		return(RFM2G_BAD_PARAMETER_1);
	}
    return( RFM2G_SUCCESS );

}
