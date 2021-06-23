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
$Workfile: close.c $
$Revision: 40619 $
$Modtime: 9/03/10 1:35p $
-------------------------------------------------------------------------------
    Description: RFM2g API rfm2gClose function.
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    29-OCT-2001  ML   1.0   Created.

===============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
/*#include <asm/page.h>*/
#include "rfm2g_api.h"

extern STDRFM2GCALL RFM2gCheckHandle(RFM2GHANDLE rh);

/******************************************************************************
*
*  FUNCTION:   RFM2gClose
*
*  PURPOSE:    Closes the RFM2g driver and invalidates the RFM2GHANDLE
*  PARAMETERS: (I) *rh: initialized previously with a call to rfm2gOpen()
*  RETURNS:    RFM2G_SUCCESS if no error
*              RFM2G_BAD_PARAMETER_1 if rh is a NULL pointer
*              RFM2G_NOT_OPEN if the device is not currently open
*              RFM2G_OS_ERROR if close(2) does not succeed
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gClose( RFM2GHANDLE *rh )
{
RFM2G_STATUS stat;

    /* Check the handle */
    if( rh == (RFM2GHANDLE *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_1 );
    }

    stat = RFM2gCheckHandle(*rh);

    if ( stat != RFM2G_SUCCESS ) {
		return (stat);
    }

    if( (*rh)->fd == 0 )
    {
        return( RFM2G_NOT_OPEN );
    }

    /* Close the device */
    if( close( (*rh)->fd ) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

	/* Zero out the handle's data */
    memset(*rh, 0, sizeof(rfm2ghandle_t));

    /* Deallocate the RFM2GHANDLE structure */
    free( (void *)(*rh) );

    *rh = (RFM2GHANDLE) NULL;

    return( RFM2G_SUCCESS );

}   /* End of rfm2gClose() */
