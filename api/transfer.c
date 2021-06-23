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
$Workfile: transfer.c $
$Revision: 40619 $
$Modtime: 9/03/10 1:35p $
-------------------------------------------------------------------------------
    Description: RFM2g API data transfer functions
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    31-OCT-2001  ML   1.0   Created.

===============================================================================
*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "rfm2g_api.h"

extern STDRFM2GCALL RFM2gCheckHandle(RFM2GHANDLE rh);

/******************************************************************************
*
*  FUNCTION:   RFM2gPeek8
*
*  PURPOSE:    Read a single byte from an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (O) Value read from Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_3 if the Value pointer is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPeek8( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT8 *Value )
{
    RFM2GATOMIC data;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify Value */
    if( Value == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    data.width = RFM2G_BYTE;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    data.offset = Offset;

    /* Do the peek */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_PEEK, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    /* Store the value */
    *Value  = (RFM2G_UINT8) data.data;


    return( RFM2G_SUCCESS );

}   /* End of RFM2gPeek8() */


/******************************************************************************
*
*  FUNCTION:   RFM2gPeek16
*
*  PURPOSE:    Read a single word from an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (O) Value read from Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_3 if the Value pointer is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned word or longword accesses
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPeek16( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT16 *Value )
{
    RFM2GATOMIC data;
    RFM2G_UINT8 alignment;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify Value */
    if( Value == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    alignment = 1;
    data.width = RFM2G_WORD;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify the Offset is properly aligned */
    if( Offset & alignment )
    {
        return( RFM2G_UNALIGNED_OFFSET );
    }
    data.offset = Offset;

    /* Do the peek */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_PEEK, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    /* Store the data */
    *Value = (RFM2G_UINT16) data.data;


    return( RFM2G_SUCCESS );

}   /* End of RFM2gPeek16() */


/******************************************************************************
*
*  FUNCTION:   RFM2gPeek32
*
*  PURPOSE:    Read a single long word from an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (O) Value read from Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_3 if the Value pointer is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned word or longword accesses
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPeek32( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT32 *Value )
{
    RFM2GATOMIC data;
    RFM2G_UINT8 alignment;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify Value */
    if( Value == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    alignment = 3;
    data.width = RFM2G_LONG;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify the Offset is properly aligned */
    if( Offset & alignment )
    {
        return( RFM2G_UNALIGNED_OFFSET );
    }
    data.offset = Offset;

    /* Do the peek */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_PEEK, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    /* Store the data */
    *Value = (RFM2G_UINT32) data.data;

    return( RFM2G_SUCCESS );

}   /* End of RFM2gPeek32() */

/******************************************************************************
*
*  FUNCTION:   RFM2gPeek64
*
*  PURPOSE:    Read a single long word from an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (O) Value read from Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_3 if the Value pointer is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned word or longword accesses
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPeek64( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT64 *Value )
{
    RFM2GATOMIC data;
    RFM2G_UINT8 alignment;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify Value */
    if( Value == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    alignment = 7;
    data.width = RFM2G_LONGLONG;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify the Offset is properly aligned */
    if( Offset & alignment )
    {
        return( RFM2G_UNALIGNED_OFFSET );
    }
    data.offset = Offset;

    /* This function is not supported at this time, return RFM2G_NOT_IMPLEMENTED */
    return( RFM2G_NOT_IMPLEMENTED );

    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_PEEK, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    /* Store the data */
    *Value = data.data;

    return( RFM2G_SUCCESS );

}   /* End of RFM2gPeek64() */


/******************************************************************************
*
*  FUNCTION:   RFM2gPoke8
*
*  PURPOSE:    Write a single byte to an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (I) Value written to Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPoke8( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT8 Value )
{
    RFM2GATOMIC data;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    data.width = RFM2G_BYTE;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    data.offset = Offset;

    /* Store the data */
    data.data = (RFM2G_UINT64) Value;

    /* Do the poke */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_POKE, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gPoke8() */


/******************************************************************************
*
*  FUNCTION:   RFM2gPoke16
*
*  PURPOSE:    Write a single word to an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (I) Value written to Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned word or longword accesses
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPoke16( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT16 Value )
{
    RFM2GATOMIC data;
    RFM2G_UINT8 alignment;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    alignment = 1;
    data.width = RFM2G_WORD;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify the Offset is properly aligned */
    if( Offset & alignment )
    {
        return( RFM2G_UNALIGNED_OFFSET );
    }
    data.offset = Offset;

    /* Store the data */
    data.data = (RFM2G_UINT64) Value;

    /* Do the poke */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_POKE, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gPoke16() */


/******************************************************************************
*
*  FUNCTION:   RFM2gPoke32
*
*  PURPOSE:    Write a single longword to an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (I) Value written to Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned word or longword accesses
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPoke32( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT32 Value )
{
    RFM2GATOMIC data;
    RFM2G_UINT8 alignment;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    alignment = 3;
    data.width = RFM2G_LONG;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify the Offset is properly aligned */
    if( Offset & alignment )
    {
        return( RFM2G_UNALIGNED_OFFSET );
    }
    data.offset = Offset;

    /* Store the data */
    data.data = (RFM2G_UINT64) Value;

    /* Do the poke */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_POKE, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gPoke32() */

/******************************************************************************
*
*  FUNCTION:   RFM2gPoke64
*
*  PURPOSE:    Write a single longword to an offset in Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Value: (I) Value written to Offset
*              Width: (I) Indicates byte, word, or longword access
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned word or longword accesses
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gPoke64( RFM2GHANDLE rh, RFM2G_UINT32 Offset, RFM2G_UINT64 Value )
{
    RFM2GATOMIC data;
    RFM2G_UINT8 alignment;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    alignment = 7;
    data.width = RFM2G_LONGLONG;

    /* Verify the Offset is in range */
    if( Offset > rh->Config.MemorySize - data.width )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify the Offset is properly aligned */
    if( Offset & alignment )
    {
        return( RFM2G_UNALIGNED_OFFSET );
    }
    data.offset = Offset;

    /* Store the data */
    data.data = Value;

    /* This function is not supported at this time, return RFM2G_NOT_IMPLEMENTED */
    return( RFM2G_NOT_IMPLEMENTED );

    /* Do the poke */
    if( ioctl(rh->fd, IOCTL_RFM2G_ATOMIC_POKE, &data) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gPoke64() */

/******************************************************************************
*
*  FUNCTION:   RFM2gRead
*
*  PURPOSE:    Read one or more longwords starting at an offset in
*              Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory from which to read
*              Buffer: (O) Data read from Reflective Memory
*              Length: (I) Number of bytes to read
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_3 if the Buffer pointer is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned offset
*              RFM2G_UNALIGNED_ADDRESS for unaligned buffer address
*              RFM2G_LSEEK_ERROR if lseek fails
*              RFM2G_READ_ERROR if read does not complete successfully
*              RFM2G_EVENT_IN_USE if the DMA engine was needed but was busy
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gRead( RFM2GHANDLE rh, RFM2G_UINT32 Offset, void *Buffer,
           RFM2G_UINT32 Length )
{
	RFM2GTRANSFER rfm2gTransfer;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify the Offset is in range */
    if( Offset + Length > rh->Config.MemorySize )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify Buffer */
    if( Buffer == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

	rfm2gTransfer.Length = Length;
	rfm2gTransfer.Offset = Offset;
	rfm2gTransfer.Buffer = Buffer;

    if( ioctl(rh->fd, IOCTL_RFM2G_READ, (RFM2G_ADDR) &rfm2gTransfer) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gRead() */


/******************************************************************************
*
*  FUNCTION:   RFM2gWrite
*
*  PURPOSE:    Write one or more longwords starting at an offset in
*              Reflective Memory
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Offset: (I) Offset in Reflective Memory to begin the write
*              Buffer: (I) Data read from Reflective Memory
*              Length: (I) Number of bytes to write
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_3 if the Buffer pointer is NULL
*              RFM2G_OUT_OF_RANGE for offsets too large for the amount of memory
*              RFM2G_UNALIGNED_OFFSET for unaligned offset
*              RFM2G_UNALIGNED_ADDRESS for unaligned buffer address
*              RFM2G_LSEEK_ERROR if lseek fails
*              RFM2G_WRITE_ERROR if write does not complete successfully
*              RFM2G_EVENT_IN_USE if the DMA engine was needed but was busy
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gWrite( RFM2GHANDLE rh, RFM2G_UINT32 Offset, void *Buffer,
            RFM2G_UINT32 Length )
{
	RFM2GTRANSFER rfm2gTransfer;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify the Offset is in range */
    if( Offset + Length > rh->Config.MemorySize )
    {
        return( RFM2G_OUT_OF_RANGE );
    }

    /* Verify Buffer */
    if( Buffer == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

	rfm2gTransfer.Length = Length;
	rfm2gTransfer.Offset = Offset;
	rfm2gTransfer.Buffer = Buffer;

    if( ioctl(rh->fd, IOCTL_RFM2G_WRITE, (RFM2G_ADDR) &rfm2gTransfer) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gWrite() */


/******************************************************************************
*
*  FUNCTION:   RFM2gGetDMAThreshold
*
*  PURPOSE:    Get the read/write size that will initiate a DMA transfer.
*              If a read/write transfers more than this many bytes DMA will
*              be used.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              Threshold: (O) Number of bytes after which DMA will be used.
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if Threshold is NULL
*              RFM2G_NOT_SUPPORTED if the device does not support DMA
*              RFM2G_OS_ERROR ioctl call failed
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetDMAThreshold(RFM2GHANDLE rh, RFM2G_UINT32 *Threshold)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Check the Threshold pointer */
    if( Threshold == (RFM2G_UINT32 *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Get the threshold */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_DMA_THRESHOLD, Threshold) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );
}   /* End of RFM2gGetDMAThreshold() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetDMAThreshold
*
*  PURPOSE:    Set the read/write size that will initiate a DMA transfer.
*              If a read/write transfers more than this many bytes DMA will
*              be used.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              NewThreshold: (O) Number of bytes after which DMA will be used.
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_NOT_SUPPORTED if the device does not support DMA
*              RFM2G_OS_ERROR ioctl call failed
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetDMAThreshold(RFM2GHANDLE rh, RFM2G_UINT32 NewThreshold)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Set the threshold */
    if( ioctl(rh->fd, IOCTL_RFM2G_SET_DMA_THRESHOLD, &NewThreshold) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );
}   /* End of RFM2gSetDMAThreshold() */

