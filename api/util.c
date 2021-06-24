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
$Workfile: util.c $
$Revision: 40619 $
$Modtime: 9/03/10 1:35p $
-------------------------------------------------------------------------------
    Description: RFM2g API utility functions
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    31-OCT-2001  ML   1.0   Created.
    23 Jun 2008  EB   6.00  Added RFM2gGetSlidingWindow() and
                            RFM2gSetSlidingWindow().
    06 Aug 2008  EB   6.04  Changed  RFM2gSetSlidingWindow() to use 
                            IOCTL_RFM2G_SET_SLIDING_WINDOW_OFFSET, which
                            programs the necessary RFM registers and also
                            allows the driver to keep a copy of the current  
                            sliding window offset, needed when DMA is used with 
                            a sliding window.
===============================================================================
*/

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#define ALLOC_DATA
#include "rfm2g_api.h"

/* The first board revision that has support for Sliding Memory
   Windows in the firmware */
#define FIRST_BOARD_REV_WITH_SLIDING_WINDOWS ((RFM2G_UINT8)0x8C)


extern STDRFM2GCALL RFM2gCheckHandle(RFM2GHANDLE rh);

/******************************************************************************
*
*  FUNCTION:   RFM2gGetLed
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory board's
*              Fail LED.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Led: (O) The state of the Fail LED: 0=>OFF, ~0=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the Led is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetLed( RFM2GHANDLE rh, RFM2G_BOOL *Led )
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Verify Led */
    if( Led == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the current state of the LED */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_LED, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }
    if( temp == RFM2G_OFF )
    {
        *Led = RFM2G_OFF;
    }
    else
    {
        *Led = RFM2G_ON;
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetLed() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetLed
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory board's
*              Fail LED.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Led: (I) The state of the Fail LED: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetLed( RFM2GHANDLE rh, RFM2G_BOOL Led )
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Set the state of the LED */
    if( ioctl(rh->fd, IOCTL_RFM2G_SET_LED, Led) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetLed() */


/******************************************************************************
*
*  FUNCTION:   RFM2gGetDebugFlags
*
*  PURPOSE:    Get the driver's debug flags
*  PARAMETERS: rh: (I) Handle to currently opened RFM device
*              Flags: (O) Returns debug flags
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if Flags is NULL
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetDebugFlags( RFM2GHANDLE rh, RFM2G_UINT32 *Flags )
{
    RFM2G_UINT32 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify Flags */
    if( Flags == (RFM2G_UINT32 *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the debug flags */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_DEBUG_FLAGS, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    *Flags = temp;

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetDebugFlags() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetDebugFlags
*
*  PURPOSE:    Set the driver's debug flags
*  PARAMETERS: rh: (I) Handle to currently opened RFM device
*              Flags: (I) New debug flags
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetDebugFlags( RFM2GHANDLE rh, RFM2G_UINT32 Flags )
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}
    /* Set the debug flags */
    if( ioctl(rh->fd, IOCTL_RFM2G_SET_DEBUG_FLAGS, Flags) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetDebugFlags() */


/*****************************************************************************
*
*  FUNCTION:   RFM2gErrorMsg
*
*  PURPOSE:    Returns a pointer to a text string describing the ErrorCode
*  PARAMETERS: ErrorCode (I): Return code from an API function
*  RETURNS:    Pointer to a text string describing the ErrorCode
*  GLOBALS:    rfm2g_errstring
*
******************************************************************************/

char *
RFM2gErrorMsg( RFM2G_STATUS ErrorCode )
{
    /* Validate the ErrorCode, which should be zero or negative */
    if( (ErrorCode < 0)  ||  (ErrorCode > RFM2G_MAX_ERROR_CODE ))
    {
        ErrorCode = RFM2G_MAX_ERROR_CODE;
    }

    return( (char*) rfm2g_errlist[ ErrorCode ] );

}  /* end of RFM2gErrorMsg() */


/*****************************************************************************
*
*  FUNCTION:   RFM2gCheckRingCont
*
*  PURPOSE:    implements RFM board Ring Continuity  test.  Returns results.
*  PARAMETERS: rh: (I) Handle to currently opened RFM device
*  RETURNS:    Status
*  GLOBALS:    none
*
******************************************************************************/

STDRFM2GCALL
RFM2gCheckRingCont(RFM2GHANDLE rh)
{
    RFM2G_UINT32 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Check Ring Continuity */
    if( ioctl(rh->fd, IOCTL_RFM2G_CHECK_RING_CONT, &temp) != 0)
    {
        return( RFM2G_LINK_TEST_FAIL );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gCheckRingCont() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetDarkOnDark
*
*  PURPOSE:    Retrieves the current on/off state of the Reflective Memory
*              board's Dark on Dark feature
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the Dark on Dark feature: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the state is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetDarkOnDark(RFM2GHANDLE rh, RFM2G_BOOL *state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Verify state */
    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the current state of the Dark on Dark feature */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_DARK_ON_DARK, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }
    if( temp == RFM2G_OFF )
    {
        *state = RFM2G_OFF;
    }
    else
    {
        *state = RFM2G_ON;
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetDarkOnDark() */

/******************************************************************************
*
*  FUNCTION:   RFM2gSetDarkOnDark
*
*  PURPOSE:    Sets the on/off state of the Reflective Memory board's Dark
*              on Dark feature.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (I) The state of the Dark on Dark feature: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetDarkOnDark(RFM2GHANDLE rh, RFM2G_BOOL state)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Set the state of the Dark on Dark feature */
    if( ioctl(rh->fd, IOCTL_RFM2G_SET_DARK_ON_DARK, state) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetDarkOnDark() */

/******************************************************************************
*
*  FUNCTION:   RFM2gClearOwnData
*
*  PURPOSE:    Retrieves the current on/off state of the Own Data bit and
*              resets the state if set.  Calling this function will turn off
*              the OWN DATA LED if on.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the Own Data LED: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the state is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gClearOwnData(RFM2GHANDLE rh, RFM2G_BOOL *state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify state */
    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the current state of the Own Data LED */
    if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_OWN_DATA, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }
    if( temp == RFM2G_OFF )
    {
        *state = RFM2G_OFF;
    }
    else
    {
        *state = RFM2G_ON;
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gClearOwnData() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetTransmit
*
*  PURPOSE:    Retrieves the current on/off state of the reflective memory
*              board's transmitter.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the transmitter: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the state is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetTransmit(RFM2GHANDLE rh, RFM2G_BOOL *state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Verify state */
    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the current state of the transmitter */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_TRANSMIT_DISABLE, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }
    if( temp == RFM2G_OFF )
    {
        *state = RFM2G_ON;
    }
    else
    {
        *state = RFM2G_OFF;
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetTransmit() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetTransmit
*
*  PURPOSE:    Sets the on/off state of the reflective memory board's transmitter.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (I) The state of the transmitter: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetTransmit(RFM2GHANDLE rh, RFM2G_BOOL state)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

    if ( stat != RFM2G_SUCCESS ) {
        return (stat);
    }

    if (state == RFM2G_OFF)
    {
        state = RFM2G_ON;
    }
    else
    {
        state = RFM2G_OFF;
    }

    if( ioctl(rh->fd, IOCTL_RFM2G_SET_TRANSMIT_DISABLE, state) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetTransmit() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetLoopback
*
*  PURPOSE:    Retrieves the current on/off state of the reflective memory
*              board's loop back of the transmit signal to the receiver circuit
*              internally.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the transmit loopback: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the state is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetLoopback(RFM2GHANDLE rh, RFM2G_BOOL *state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Verify state */
    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the current state of the transmit loopback */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_LOOPBACK, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }
    if( temp == RFM2G_OFF )
    {
        *state = RFM2G_OFF;
    }
    else
    {
        *state = RFM2G_ON;
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetLoopback() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetLoopback
*
*  PURPOSE:    Sets the on/off state of the reflective memory board's transmitter.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (I) The state of the transmit loopback: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetLoopback(RFM2GHANDLE rh, RFM2G_BOOL state)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Set the state of the transmitter loopback */
    if( ioctl(rh->fd, IOCTL_RFM2G_SET_LOOPBACK, state) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetLoopback() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetParityEnable
*
*  PURPOSE:    Retrieves the current on/off state of the reflective memory
*              board's parity checking on all on-board memory accesses.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the parity enable: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the state is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetParityEnable(RFM2GHANDLE rh, RFM2G_BOOL *state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Verify state */
    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the current state of the parity enable */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_PARITY_ENABLE, &temp) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }
    if( temp == RFM2G_OFF )
    {
        *state = RFM2G_OFF;
    }
    else
    {
        *state = RFM2G_ON;
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetParityEnable() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetParityEnable
*
*  PURPOSE:    Sets the on/off state of the reflective memory board's parity
*              checking on all on-board memory accesses.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (I) The state of the parity enable: RFM2G_OFF=>OFF, RFM2G_ON=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetParityEnable(RFM2GHANDLE rh, RFM2G_BOOL state)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    if( ioctl(rh->fd, IOCTL_RFM2G_SET_PARITY_ENABLE, state) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetParityEnable() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetMemoryOffset
*
*  PURPOSE:    Gets the memory window of the reflective memory board.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              offset: (O) The current offset to the network address
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the offset is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetMemoryOffset(RFM2GHANDLE rh, RFM2G_MEM_OFFSETTYPE *offset)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

    if ( stat != RFM2G_SUCCESS ) {
        return (stat);
    }

    if( offset == (RFM2G_MEM_OFFSETTYPE  *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    if( ioctl(rh->fd, IOCTL_RFM2G_GET_MEMORY_OFFSET, offset) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetMemoryOffset() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetMemoryOffset
*
*  PURPOSE:    Sets the memory offset of the reflective memory board.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              offset: (I) The offset to the network address
*                Valid Offsets:
*                RFM2G_MEM_OFFSET0   1st 64 MB Memory offset
*                RFM2G_MEM_OFFSET1   2nd 64 MB Memory offset
*                RFM2G_MEM_OFFSET2   3rd 64 MB Memory offset
*                RFM2G_MEM_OFFSET3   4th 64 MB Memory offset
*
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the offset is invalid
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetMemoryOffset(RFM2GHANDLE rh, RFM2G_MEM_OFFSETTYPE offset)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

	switch(offset)
	{
		case RFM2G_MEM_OFFSET0:
		case RFM2G_MEM_OFFSET1:
		case RFM2G_MEM_OFFSET2:
		case RFM2G_MEM_OFFSET3:
			break;

		default:
            return( RFM2G_BAD_PARAMETER_2 );
			break;
	}

    if( ioctl(rh->fd, IOCTL_RFM2G_SET_MEMORY_OFFSET, offset) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetMemoryOffset() */


/******************************************************************************
*
*  FUNCTION:   RFM2gGetSlidingWindow
*
*  PURPOSE:    Gets the base reflective memory offset of the sliding memory
*                window.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              offset: (O) The offset of the current sliding memory 
*                window.
*              size: (O) The size in bytes of the current sliding memory
*                window.  May be set to NULL if the size is not requested.
*
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_NOT_SUPPORTED if the Sliding Memory Window feature
*                is not supported by the board's firmware.
*              RFM2G_OS_ERROR if ioctl(2) fails
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the offset is NULL
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL RFM2gGetSlidingWindow(RFM2GHANDLE rh, RFM2G_UINT32 *offset, 
    RFM2G_UINT32 *size)
{
    RFM2G_STATUS  status;
    RFM2G_UINT32  temp;
    RFM2G_UINT8   boardRev;
    RFM2GCONFIG   config;
    

    /* Validate the parameters */
    status = RFM2gCheckHandle(rh);
    if (status != RFM2G_SUCCESS)
    {
        return status;
    }
    if (offset == NULL)
    {
        return RFM2G_BAD_PARAMETER_2;
    }

    /* Get this board's firmware version */
    status = RFM2gGetConfig(rh, &config);
    if (status != RFM2G_SUCCESS)
    {
        return status;
    }
    boardRev = config.BoardRevision;

    /* Does this firmware support the Sliding Memory Window feature? */
    if (boardRev < FIRST_BOARD_REV_WITH_SLIDING_WINDOWS)
    {
        return RFM2G_NOT_SUPPORTED;
    }

    /* Read the base offset for the current Sliding Memory Window */
    status = RFM2gReadReg(rh, RFM2GCFGREGMEM, rfmor_las1ba, 4, &temp);
    if (status != RFM2G_SUCCESS)
    {
        return status;
    }

    /* Return the offset to the caller */
    *offset = temp & ~RFMOR_LAS1BA_ENABLE_LAS1;

    /* Return the window size to the caller, if requested */
    if (size != NULL)
    {
        /* Get the window size */
        status = RFM2gReadReg(rh, RFM2GCFGREGMEM, rfmor_las1rr, 4, (void *)size);
        if (status != RFM2G_SUCCESS)
        {
            return status;
        }

        switch (*size & RFMOR_LAS1RR_RANGE_MASK)
        {
        case RFMOR_LAS1RR_RANGE_2M:
            *size = 0x00200000;
            break;
        case RFMOR_LAS1RR_RANGE_16M:
            *size = 0x01000000;
            break;
        case RFMOR_LAS1RR_RANGE_64M:
            *size = 0x04000000;
            break;
        case RFMOR_LAS1RR_RANGE_128M:
            *size = 0x08000000;
            break;
        case RFMOR_LAS1RR_RANGE_256M:
            *size = 0x10000000;
            break;
        default:
            *size = 0;
            return RFM2G_DRIVER_ERROR;
        }
    }

    return RFM2G_SUCCESS;

}   /* End of RFM2gGetSlidingWindow() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetSlidingWindow
*
*  PURPOSE:    Sets the base reflective memory offset of the sliding memory
*                window.  The offset must be a multiple of the configured
*                memory size of the RFM2g board.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              offset: (I) The offset of the sliding memory window.
*
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_SUPPORTED if the Sliding Memory Window feature
*                is not supported by the board's firmware.
*              RFM2G_OS_ERROR if ioctl(2) fails
*              RFM2G_DRIVER_ERROR if the driver returns an invalid window size.
*              RFM2G_BAD_PARAMETER_2 if the offset is not a multiple of
*                the window size.
*              RFM2G_OUT_OF_RANGE if the offset and window size exceed the
*                board's memory size.
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL RFM2gSetSlidingWindow(RFM2GHANDLE rh, RFM2G_UINT32 offset)
{
    RFM2G_STATUS  status;
    RFM2G_UINT32  installedRam = 0;
    RFM2G_UINT32  windowSize;
    RFM2G_UINT8   boardRev;
    RFM2GCONFIG   config;


    /* Validate the handle */
    status = RFM2gCheckHandle(rh);
    if (status != RFM2G_SUCCESS)
    {
        return status;
    }

    /* Get this board's firmware version and size of the installed memory */
    status = RFM2gGetConfig(rh, &config);
    if (status != RFM2G_SUCCESS)
    {
        return status;
    }
    boardRev = config.BoardRevision;
    switch (config.Lcsr1 & RFM2G_LCSR_CFG)
    {
    case 0x00000000: /* 64 Mbytes */
        installedRam = 0x04000000;
        break;

    case 0x00100000: /* 128 Mbytes */ 
        installedRam = 0x08000000;
        break;

    case 0x00200000:  /* 256 Mbytes */
        installedRam = 0x10000000;
        break;

    case 0x00300000:  /* reserved */
        installedRam = 0x0;
        break;
    }

    /* Does this firmware support the Sliding Memory Window feature? */
    if (boardRev < FIRST_BOARD_REV_WITH_SLIDING_WINDOWS)
    {
        return RFM2G_NOT_SUPPORTED;
    }

    /* Get the windowSize */
    status = RFM2gReadReg(rh, RFM2GCFGREGMEM, rfmor_las1rr, 4, (void *)&windowSize);
    if (status != RFM2G_SUCCESS)
    {
        return status;
    }
    switch (windowSize & RFMOR_LAS1RR_RANGE_MASK)
    {
    case RFMOR_LAS1RR_RANGE_2M:
        windowSize = 0x00200000;
        break;
    case RFMOR_LAS1RR_RANGE_16M:
        windowSize = 0x01000000;
        break;
    case RFMOR_LAS1RR_RANGE_64M:
        windowSize = 0x04000000;
        break;
    case RFMOR_LAS1RR_RANGE_128M:
        windowSize = 0x08000000;
        break;
    case RFMOR_LAS1RR_RANGE_256M:
        windowSize = 0x10000000;
        break;
    default:
        return RFM2G_DRIVER_ERROR;
    }

    /* Is the offset a multiple of windowSize? */
    if ((offset % windowSize) != 0)
    {
        /* No */
        return RFM2G_BAD_PARAMETER_2;
    }

    /* Does the offset exceed the board's installed memory? */
    if (offset > installedRam)
    {
        return RFM2G_OUT_OF_RANGE;
    }

    /* Does the offset+windowSize exceed the board's installed memory? */
    if ((offset + windowSize) > installedRam)
    {
        return RFM2G_OUT_OF_RANGE;
    }

    /* Enable the Sliding Memory Window at the user's offset */
    if (ioctl(rh->fd, IOCTL_RFM2G_SET_SLIDING_WINDOW_OFFSET, offset) != 0)
    {
        return RFM2G_DRIVER_ERROR;
    }

    return RFM2G_SUCCESS;

}   /* End of RFM2gSetSlidingWindow() */

