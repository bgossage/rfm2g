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
$Workfile: config.c $
$Revision: 40619 $
$Modtime: 9/03/10 1:35p $
-------------------------------------------------------------------------------
    Description: RFM2g API configuration functions
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    31-OCT-2001  ML   1.0   Created.
    18 Jun 2008  EB   6.00  Changed 'Offset' parameter of RFM2gUserMemory to
                            RFM2G_UINT64.  This is to allow a 64-bit physical
                            address of a DMA buffer above the 4GB range to be
                            mapped.  This change is in support of PAE (Physical
                            Address Extension)-enabled Linux.
    09 Jul 2008  EB   06.02 Including sys/user.h to pick up the definition of
                            PAGE_SIZE in the 64-bit Linux distribution.
    30 Sep 2008  EB   07.00 Corrected bug in RFM2gUnMapUserMemory() where a
                            local variable was initialized using a dereference
                            from a pointer that wasn't yet checked against NULL.
                            Added a test to api/config.c inside Rfm2gUserMemory() 
                            to detect offsets that are not page-aligned.
    14 Oct 2008  EB   07.01 Corrected casting in RFM2gUnMapUserMemory() to store
                            the UserMemoryPtr into the RFM2G_UINT32 usrMemPtr.
                            Before this change, 64-bit Linux compilers would warn
                            about trying to fit a 64-bit pointer into a 32-bit 
                            integer.
    17 Oct 2008  EB   07.02 Corrected casting done in the last edit to make it
                            compile without warnings in both 32-bit and 64-bit
                            distributions.

===============================================================================
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
#include <asm/page.h>
#endif
#include <sys/user.h>
#include "rfm2g_version.h"
#include "rfm2g_api.h"

extern STDRFM2GCALL RFM2gCheckHandle(RFM2GHANDLE rh);

#define PAGE_SHIFT	12
#ifdef __ASSEMBLY__
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#else
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#endif
#define PAGE_MASK	(~(PAGE_SIZE-1))

/******************************************************************************
*
*  FUNCTION:   RFM2gUserMemory
*
*  PURPOSE:    (1) Maps RFM memory to the user space, allowing direct access to
*              RFM memory via pointer dereferencing.
*              (2) Maps DMA buffer memory preallocated with "mem=xxxM" to the
*              user space, allowing direct access to the buffer via pointer
*              dereferencing.  This functionality is activated when Offset
*              is ORed with RFM2G_DMA_MMAP_OFFSET.
*              (3) Maps the RFM's PLX registers to user space, allowing
*              direct access via pointer dereferencing.  This functionality is
*              activated when Offset is set to RFM2G_BAR0_MMAP_OFFSET.
*              (4) Maps the RFM registers to user space, 
*              allowing direct access via pointer dereferencing.  This 
*              functionality is activated when Offset is set to 
*              RFM2G_BAR2_MMAP_OFFSET.
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              UserMemoryPtr (IO): Where to put the pointer to mapped RFM space
*              Offset (I): Base byte offset of memory to map, aligned to 
*                PAGE_SIZE.  May be ORed with RFM2G_DMA_MMAP_OFFSET,
*                RFM2G_BAR0_MMAP_OFFSET, or RFM2G_BAR2_MMAP_OFFSET.
*              Pages (I): The number of PAGE_SIZE pages to map. For Intel
*                platforms, PAGE_SIZE is 4096 bytes.
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the UserMemoryPtr pointer is NULL
*              RFM2G_BAD_PARAMETER_3 if the Offset is not page aligned
*              RFM2G_BAD_PARAMETER_4 if 0 pages are requested
*              RFM2G_OUT_OF_RANGE if the requested size exceeds the Reflective
*                Memory
*              RFM2G_MEM_NOT_MAPPED if the mmap(2) fails
*              RFM2G_OS_ERROR if ioctl() fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gUserMemory( RFM2GHANDLE rh, volatile void **UserMemoryPtr, RFM2G_UINT64 Offset,
    RFM2G_UINT32 Pages )
{
    void *MappedRfm2g;
    RFM2G_STATUS stat;
    RFM2G_UINT32 Offset32 = 0;

    stat = RFM2gCheckHandle(rh);

    if ( stat != RFM2G_SUCCESS ) {
		return (stat);
    }

    /* Check UserMemoryPtr */
    if( UserMemoryPtr == (volatile void **) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Check Pages */
    if( Pages == 0 )
    {
        return( RFM2G_BAD_PARAMETER_4 );
    }

    /* Range check */
    if (!(Offset & RFM2G_DMA_MMAP_OFFSET)) /* is this NOT for a DMA buffer? */
    {
        Offset32 = Offset & 0xFFFFFFFF;

        /* No, continue with regular checks */
        if( (Pages * PAGE_SIZE) > rh->Config.MemorySize )
        {
            return( RFM2G_OUT_OF_RANGE );
        }
        if( Offset32 + (Pages * PAGE_SIZE) > rh->Config.MemorySize )
        {
            return( RFM2G_OUT_OF_RANGE );
        }
    }

    /* Make sure an Offset value doesn't accidently have an RFM2G_BARx_MMAP_OFFSET flag set */
    if (Offset & RFM2G_BAR0_MMAP_OFFSET)
    {
        if (Offset != RFM2G_BAR0_MMAP_OFFSET)
        {
            return RFM2G_BAD_PARAMETER_3;
        }
    }
    if (Offset & RFM2G_BAR2_MMAP_OFFSET)
    {
        if (Offset != RFM2G_BAR2_MMAP_OFFSET)
        {
            return RFM2G_BAD_PARAMETER_3;
        }
    }

    /* Is this for a DMA buffer, BAR0, or BAR2? */
    if (Offset & (RFM2G_DMA_MMAP_OFFSET | RFM2G_BAR0_MMAP_OFFSET | RFM2G_BAR2_MMAP_OFFSET))
    {
        /* Store the 64-bit Offset containing the flag in the driver */
        if( ioctl(rh->fd, IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET, &Offset) != 0 )
        {
            return( RFM2G_OS_ERROR );
        }

        /* The driver's mmap() will not use the Offset32 in this case but
           we have to put something here that is at least page aligned. */
        Offset32 = PAGE_SIZE;
    }

    /* Map the memory */
    MappedRfm2g = mmap( 0, Pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
        MAP_SHARED, rh->fd, Offset32 );

    if( MappedRfm2g == (void *) -1 )
    {
        return( RFM2G_MEM_NOT_MAPPED );
    }

    *UserMemoryPtr = MappedRfm2g;

    return( RFM2G_SUCCESS );

}   /* End of RFM2gUserMemory() */


/******************************************************************************
*
*  FUNCTION:   RFM2gUserMemoryBytes
*
*  PURPOSE:    (1) Maps RFM memory to the user space, allowing direct access to
*              RFM memory via pointer dereferencing.
*              (2) Maps DMA buffer memory preallocated with "mem=xxxM" to the
*              user space, allowing direct access to the buffer via pointer
*              dereferencing.  This functionality is activated when Offset
*              is ORed with RFM2G_DMA_MMAP_OFFSET.
*              (3) Maps the RFM's PLX registers to user space, allowing
*              direct access via pointer dereferencing.  This functionality is
*              activated when Offset is set to RFM2G_BAR0_MMAP_OFFSET.
*              (4) Maps the RFM registers to user space, 
*              allowing direct access via pointer dereferencing.  This 
*              functionality is activated when Offset is set to 
*              RFM2G_BAR2_MMAP_OFFSET.
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              UserMemoryPtr (IO): Where to put the pointer to mapped RFM space
*              Offset (I): Base byte offset of memory to map, aligned to 
*                PAGE_SIZE.  May be ORed with RFM2G_DMA_MMAP_OFFSET,
*                RFM2G_BAR0_MMAP_OFFSET, or RFM2G_BAR2_MMAP_OFFSET.
*              Bytes (I): The number of bytes to map.
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the UserMemoryPtr pointer is NULL
*              RFM2G_BAD_PARAMETER_3 if 0 pages are requested
*              RFM2G_OUT_OF_RANGE if the requested size exceeds the Reflective
*                Memory
*              RFM2G_MEM_NOT_MAPPED if the mmap(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gUserMemoryBytes( RFM2GHANDLE rh, volatile void **UserMemoryPtr, RFM2G_UINT64 Offset,
    RFM2G_UINT32 Bytes )
{
	if (Bytes == 0)
	{
        return( RFM2G_BAD_PARAMETER_4 );
	}

    /* Round up to the nearest page boundary, if necessary */
    return (RFM2gUserMemory( rh, UserMemoryPtr, Offset,
                             ((Bytes / PAGE_SIZE) + ((Bytes % PAGE_SIZE > 0) ? 1 : 0))));
}


/******************************************************************************
*
*  FUNCTION:   RFM2gUnMapUserMemory
*
*  PURPOSE:    Unmaps RFM memory to the user space
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              UserMemoryPtr (IO): Pointer to mapped RFM space
*              Pages (I): The number of PAGE_SIZE pages originally mapped to
*                UserMemoryPtr.  For Intel platforms, PAGE_SIZE is 4096 bytes.
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the UserMemoryPtr pointer is NULL
*              RFM2G_OS_ERROR if the munmap(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gUnMapUserMemory( RFM2GHANDLE rh, volatile void **UserMemoryPtr, RFM2G_UINT32 Pages )
{
    RFM2G_STATUS stat;
    RFM2G_UINT32 usrMemPtr;


    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) 
    {
		return (stat);
	}

    /* Check UserMemoryPtr */
    if( UserMemoryPtr == (volatile void **) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }
    if( *UserMemoryPtr == (volatile void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    if (Pages == 0)
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

#ifdef __x86_64__
    usrMemPtr = (RFM2G_UINT32) ((RFM2G_UINT64)*UserMemoryPtr);
#else
    usrMemPtr = (RFM2G_UINT32) (*UserMemoryPtr);
#endif

    /* Give the driver a chance to clear the DMA buffer 64-bit offset, 
       if this UserMemoryPtr happens to be pointing at a DMA buffer */
    if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_DMA_MMAP_OFFSET, usrMemPtr) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    /* Unmap the mmap'ed RFM memory */
    if( munmap( (void *)*UserMemoryPtr, Pages * PAGE_SIZE ) != 0 )
    {
        return( RFM2G_OS_ERROR ); /* munmap() failed */
    }

    *UserMemoryPtr  = (void *) NULL;

    return( RFM2G_SUCCESS );

}   /* End of RFM2GUnMapUserMemory() */

/******************************************************************************
*
*  FUNCTION:   RFM2gUnMapUserMemoryBytes
*
*  PURPOSE:    Unmaps RFM memory to the user space
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              UserMemoryPtr (IO): Pointer to mapped RFM space
*              Bytes (I): The number of bytes originally mapped to
*                UserMemoryPtr.
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the UserMemoryPtr pointer is NULL
*              RFM2G_OS_ERROR if the munmap(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gUnMapUserMemoryBytes( RFM2GHANDLE rh, volatile void **UserMemoryPtr,
    RFM2G_UINT32 Bytes )
{
	if (Bytes == 0)
	{
        return( RFM2G_BAD_PARAMETER_3 );
	}
    
    /* Round up to the nearest page boundary, if necessary */
    return (RFM2gUnMapUserMemory( rh, UserMemoryPtr,
		    ((Bytes / PAGE_SIZE) + ((Bytes % PAGE_SIZE > 0) ? 1 : 0))));
}

/******************************************************************************
*
*  FUNCTION:   RFM2GGetConfig
*
*  PURPOSE:    Gets a copy of the RFM2GCONFIG structure from the driver
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              Configuration (O): Pointer to RFM2GCONFIG structure
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to RFM2GCONFIG
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetConfig(RFM2GHANDLE rh, RFM2GCONFIG *Config)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Check the RFM2GCONFIG pointer */
    if( Config == (RFM2GCONFIG *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Save a local copy of this RFM device's RFM2GCONFIG structure */
    if( ioctl( rh->fd, IOCTL_RFM2G_GET_CONFIG, Config ) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetConfiguration */


/******************************************************************************
*
*  FUNCTION:   RFM2gNodeID
*
*  PURPOSE:    Gets a copy of the board's Node ID from the driver
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              NodeIdPtr (O): Pointer to RFM2G_NODE
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to RFM2G_NODE
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gNodeID(RFM2GHANDLE rh, RFM2G_NODE *NodeIdPtr)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Check the RFM2G_NODE pointer */
    if( NodeIdPtr == (RFM2G_NODE *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the NodeId from the config structure. */
    *NodeIdPtr = rh->Config.NodeId;

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gBoardID
*
*  PURPOSE:    Gets a copy of the board's type ID from the driver
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              NodeIdPtr (O): Pointer to RFM2G_UINT8
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to RFM2G_UINT8
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gBoardID(RFM2GHANDLE rh, RFM2G_UINT8 *BoardIdPtr)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Check the RFM2G_NODE pointer */
    if( BoardIdPtr == (RFM2G_UINT8 *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the BoardId from the config structure. */
    *BoardIdPtr = rh->Config.BoardId;

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gSize
*
*  PURPOSE:    Gets the Boards total meomory size in bytes
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              SizePtr (O): Pointer to RFM2G_UINT32
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to RFM2G_UINT32
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSize(RFM2GHANDLE rh, RFM2G_UINT32 *SizePtr)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Check the RFM2G_NODE pointer */
    if( SizePtr == (RFM2G_UINT32 *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the board size from the config structure. */
    *SizePtr = rh->Config.MemorySize;

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gFirst
*
*  PURPOSE:    Gets the Offset to the first useable address in memory
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              FirstPtr (O): Pointer to RFM2G_UINT32
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to RFM2G_UINT32
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gFirst(RFM2GHANDLE rh, RFM2G_UINT32 *FirstPtr)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Check the RFM2G_NODE pointer */
    if( FirstPtr == (RFM2G_UINT32 *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the first valid board memory offset. */
    *FirstPtr = 0;

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gDeviceName
*
*  PURPOSE:    Gets the device name for this instance of the driver
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              NamePtr (O): Pointer to char
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to char
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gDeviceName(RFM2GHANDLE rh, char *NamePtr)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Check the char pointer */
    if( NamePtr == (char *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the Device Name. */
    strcpy(NamePtr, &(rh->Config.Device[0]));

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gDllVersion
*
*  PURPOSE:    Gets the version of the api library
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              VersionPtr (O): Pointer to char
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to char
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gDllVersion(RFM2GHANDLE rh, char *VersionPtr)
{
    /* Check the handle */
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Check the char pointer */
    if( VersionPtr == (char *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the Library Version. */
    strcpy(VersionPtr, RFM2G_PRODUCT_VERSION);

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gDriverVersion
*
*  PURPOSE:    Gets the version of the driver
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              VersionPtr (O): Pointer to char
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_OS_ERROR if ioctl() fails
*              RFM2G_BAD_PARAMETER_1 for NULL Handle
*              RFM2G_BAD_PARAMETER_2 for NULL pointer to char
*              RFM2G_NOT_OPEN if driver is not open
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gDriverVersion(RFM2GHANDLE rh, char *VersionPtr)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Check the char pointer */
    if( VersionPtr == (char *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Return the Library Version. */
    strcpy(VersionPtr, RFM2G_PRODUCT_VERSION);

    return( RFM2G_SUCCESS );

}

/******************************************************************************
*
*  FUNCTION:   RFM2gGetDMAByteSwap
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory board's
*              DMA Byte Swap.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the DMA Byte Swap: 0=>OFF, ~0=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the Led is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetDMAByteSwap(RFM2GHANDLE rh, RFM2G_BOOL * state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    if( ioctl(rh->fd, IOCTL_RFM2G_GET_DMA_BYTESWAP, &temp) != 0 )
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

}   /* End of RFM2gGetDMAByteSwap() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetDMAByteSwap
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory board's
*              DMA Byte Swap.
*  PARAMETERS: rh: (I) Handle to open RRM2g device
*              state: (I) The state of the DMA Byte Swap: 0=>OFF, ~0=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetDMAByteSwap(RFM2GHANDLE rh, RFM2G_BOOL state)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    if( ioctl(rh->fd, IOCTL_RFM2G_SET_DMA_BYTE_SWAP, state) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetDMAByteSwap() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetPIOByteSwap
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory board's
*              DMA Byte Swap.
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              state: (O) The state of the PIO Byte Swap: 0=>OFF, ~0=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the Led is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetPIOByteSwap(RFM2GHANDLE rh, RFM2G_BOOL * state)
{
    RFM2G_UINT8 temp;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    if( state == (RFM2G_BOOL *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    if( ioctl(rh->fd, IOCTL_RFM2G_GET_PIO_BYTESWAP, &temp) != 0 )
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

}   /* End of RFM2gGetPIOByteSwap() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSetPIOByteSwap
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory board's
*              DMA Byte Swap.
*  PARAMETERS: rh: (I) Handle to open RRM2g device
*              state: (I) The state of the PIO Byte Swap: 0=>OFF, ~0=>ON
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSetPIOByteSwap(RFM2GHANDLE rh, RFM2G_BOOL state)
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    if( ioctl(rh->fd, IOCTL_RFM2G_SET_PIO_BYTE_SWAP, state) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSetPIOByteSwap() */

/******************************************************************************
*
*  FUNCTION:   RFM2gWriteReg
*
*  PURPOSE:    Write a value to a location in one of the Reflective Memory board's
*              memory windows.
*  PARAMETERS: rh: (I) Handle to open RRM2g device
*              regset: (I) The memory window to write to
*              Offset: (I) The offset into the memory window to write to
*              Width: (I) The size of the write
*              Value: (I) The value to write
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL
RFM2gWriteReg(RFM2GHANDLE rh, RFM2GREGSETTYPE regset, RFM2G_UINT32 Offset,
			  RFM2G_UINT32 Width, RFM2G_UINT32 Value)
{
    RFM2G_STATUS stat;
	RFM2GLINUXREGINFO rfm2gLinuxReg;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    switch( Width )
    {
        case RFM2G_BYTE:
        case RFM2G_WORD:
        case RFM2G_LONG:
			break;

        case RFM2G_LONGLONG: /* Not supported */
		default:
			return ( RFM2G_BAD_PARAMETER_4 );
	}

	switch( regset )
	{
		case RFM2GCFGREGMEM:
			if (Offset > (512 - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GCTRLREGMEM:
			/* The space reserved for this group of registers is 64 bytes. p. 73 of
			   VMIPCI-5565 Ultrahigh-Speed Fiber-Optic Reflective Memory with Interrupts
               PRODUCT MANUAL */
			if (Offset > (RFM2G_REGSIZE - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GMEM:
			if (Offset > (rh->Config.MemorySize - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		/* The following are not supported by this driver */
		case RFM2GCFGREGIO:
		case RFM2GRESERVED0REG:
		case RFM2GRESERVED1REG:
		default:
			return ( RFM2G_BAD_PARAMETER_2 );
	}

	rfm2gLinuxReg.regset = regset;
	rfm2gLinuxReg.Offset = Offset;
	rfm2gLinuxReg.Width  = Width;
	rfm2gLinuxReg.Value  = Value;

    if( ioctl(rh->fd, IOCTL_RFM2G_WRITE_REG, &rfm2gLinuxReg) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gRegWrite() */

/******************************************************************************
*
*  FUNCTION:   RFM2gReadReg
*
*  PURPOSE:    Read a value from a location in one of the Reflective Memory board's
*              memory windows.
*  PARAMETERS: rh: (I) Handle to open RRM2g device
*              regset: (I) The memory window to read from
*              Offset: (I) The offset into the memory window to read from
*              Width: (I) The size of the read
*              Value: (I) Pointer to the value read
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL RFM2gReadReg(RFM2GHANDLE rh, RFM2GREGSETTYPE regset,
                          RFM2G_UINT32 Offset, RFM2G_UINT32 Width,
                          void *Value)
{
    RFM2G_STATUS stat;
	RFM2GLINUXREGINFO rfm2gLinuxReg;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    switch( Width )
    {
        case RFM2G_BYTE:
        case RFM2G_WORD:
        case RFM2G_LONG:
			break;

        case RFM2G_LONGLONG: /* Not supported */
		default:
			return ( RFM2G_BAD_PARAMETER_4 );
	}

	switch( regset )
	{
		case RFM2GCFGREGMEM:
			if (Offset > (512 - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GCTRLREGMEM:
			/* The space reserved for this group of registers is 64 bytes. p. 73 of
			   VMIPCI-5565 Ultrahigh-Speed Fiber-Optic Reflective Memory with Interrupts
               PRODUCT MANUAL */
			if (Offset > (RFM2G_REGSIZE - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GMEM:
			if (Offset > (rh->Config.MemorySize - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		/* The following are not supported by this driver */
		case RFM2GCFGREGIO:
		case RFM2GRESERVED0REG:
		case RFM2GRESERVED1REG:
		default:
			return ( RFM2G_BAD_PARAMETER_2 );
	}

    if( Value == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_5 );
    }

	rfm2gLinuxReg.regset = regset;
	rfm2gLinuxReg.Offset = Offset;
	rfm2gLinuxReg.Width  = Width;

    if( ioctl(rh->fd, IOCTL_RFM2G_READ_REG, &rfm2gLinuxReg) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    switch( Width )
    {
        case RFM2G_BYTE:
			*((RFM2G_UINT8*) Value) = (RFM2G_UINT8) rfm2gLinuxReg.Value;
			break;
        case RFM2G_WORD:
			*((RFM2G_UINT16*) Value) = (RFM2G_UINT16) rfm2gLinuxReg.Value;
			break;
        case RFM2G_LONG:
			*((RFM2G_UINT32*) Value) = (RFM2G_UINT32) rfm2gLinuxReg.Value;
			break;

        case RFM2G_LONGLONG: /* Not supported */
		default:
			return ( RFM2G_BAD_PARAMETER_3 );
	}

    return( RFM2G_SUCCESS );

}   /* End of RFM2gReadReg() */

/******************************************************************************
*
*  FUNCTION:   RFM2gSetReg
*
*  PURPOSE:    Write a value to a location in one of the Reflective Memory board's
*              memory windows.
*  PARAMETERS: rh: (I) Handle to open RRM2g device
*              regset: (I) The memory window to write to
*              Offset: (I) The offset into the memory window to write to
*              Width: (I) The size of the write
*              Value: (I) The value to write
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL RFM2gSetReg(RFM2GHANDLE rh, RFM2GREGSETTYPE regset,
    RFM2G_UINT32 Offset, RFM2G_UINT32 Width, RFM2G_UINT32 Mask)
{
    RFM2G_STATUS stat;
	RFM2GLINUXREGINFO rfm2gLinuxReg;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    switch( Width )
    {
        case RFM2G_BYTE:
        case RFM2G_WORD:
        case RFM2G_LONG:
			break;

        case RFM2G_LONGLONG: /* Not supported */
		default:
			return ( RFM2G_BAD_PARAMETER_4 );
	}

	switch( regset )
	{
		case RFM2GCFGREGMEM:
			if (Offset > (512 - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GCTRLREGMEM:
			/* The space reserved for this group of registers is 64 bytes. p. 73 of
			   VMIPCI-5565 Ultrahigh-Speed Fiber-Optic Reflective Memory with Interrupts
               PRODUCT MANUAL */
			if (Offset > (RFM2G_REGSIZE - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GMEM:
			if (Offset > (rh->Config.MemorySize - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		/* The following are not supported by this driver */
		case RFM2GCFGREGIO:
		case RFM2GRESERVED0REG:
		case RFM2GRESERVED1REG:
		default:
			return ( RFM2G_BAD_PARAMETER_2 );
	}

	rfm2gLinuxReg.regset = regset;
	rfm2gLinuxReg.Offset = Offset;
	rfm2gLinuxReg.Width  = Width;
	rfm2gLinuxReg.Value  = Mask;

    if( ioctl(rh->fd, IOCTL_RFM2G_SET_REG, &rfm2gLinuxReg) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );
}

/******************************************************************************
*
*  FUNCTION:   RFM2gClearReg
*
*  PURPOSE:    Write a value to a location in one of the Reflective Memory board's
*              memory windows.
*  PARAMETERS: rh: (I) Handle to open RRM2g device
*              regset: (I) The memory window to write to
*              Offset: (I) The offset into the memory window to write to
*              Width: (I) The size of the write
*              Value: (I) The value to write
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_OS_ERROR if ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL RFM2gClearReg(RFM2GHANDLE rh, RFM2GREGSETTYPE regset,
    RFM2G_UINT32 Offset, RFM2G_UINT32 Width, RFM2G_UINT32 Mask)
{
    RFM2G_STATUS stat;
	RFM2GLINUXREGINFO rfm2gLinuxReg;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    switch( Width )
    {
        case RFM2G_BYTE:
        case RFM2G_WORD:
        case RFM2G_LONG:
			break;

        case RFM2G_LONGLONG: /* Not supported */
		default:
			return ( RFM2G_BAD_PARAMETER_4 );
	}

	switch( regset )
	{
		case RFM2GCFGREGMEM:
			if (Offset > (512 - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GCTRLREGMEM:
			/* The space reserved for this group of registers is 64 bytes. p. 73 of
			   VMIPCI-5565 Ultrahigh-Speed Fiber-Optic Reflective Memory with Interrupts
               PRODUCT MANUAL */
			if (Offset > (RFM2G_REGSIZE - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		case RFM2GMEM:
			if (Offset > (rh->Config.MemorySize - Width))
			{
				return ( RFM2G_BAD_PARAMETER_3 );
			}
			break;

		/* The following are not supported by this driver */
		case RFM2GCFGREGIO:
		case RFM2GRESERVED0REG:
		case RFM2GRESERVED1REG:
		default:
			return ( RFM2G_BAD_PARAMETER_2 );
	}

	rfm2gLinuxReg.regset = regset;
	rfm2gLinuxReg.Offset = Offset;
	rfm2gLinuxReg.Width  = Width;
	rfm2gLinuxReg.Value  = Mask;

    if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_REG, &rfm2gLinuxReg) != 0 )
    {
        return( RFM2G_OS_ERROR );
    }

    return( RFM2G_SUCCESS );
}

/******************************************************************************
*
*  FUNCTION:   RFM2gMapDeviceMemory
*
*  PURPOSE:    Maps register memory to the user space, allowing direct access to
*              RFM registers via pointer dereferencing
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              regset: (I) The memory window to map
*              DeviceMemoryPtr (IO): Where to put the pointer to mapped space
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the UserMemoryPtr pointer is NULL
*              RFM2G_BAD_PARAMETER_3 if 0 pages are requested
*              RFM2G_OUT_OF_RANGE if the requested size exceeds the Reflective
*                Memory
*              RFM2G_MEM_NOT_MAPPED if the memory mapping fails
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL RFM2gMapDeviceMemory(RFM2GHANDLE rh, RFM2GREGSETTYPE regset, volatile void **DeviceMemoryPtr)
{
    RFM2G_STATUS stat;
	RFM2G_UINT64 Offset;
	RFM2G_UINT32 Pages;

    stat = RFM2gCheckHandle(rh);

    if ( stat != RFM2G_SUCCESS ) {
		return (stat);
    }

	switch( regset )
	{
		case RFM2GCFGREGMEM:
			Offset = RFM2G_BAR0_MMAP_OFFSET;
			Pages = 1;
			break;

		case RFM2GCTRLREGMEM:
			Offset = RFM2G_BAR2_MMAP_OFFSET;
			Pages = 1;
			break;

		case RFM2GMEM:
			Offset = 0;
			Pages = rh->Config.MemorySize / PAGE_SIZE;
			break;

		/* The following are not supported by this driver */
		case RFM2GCFGREGIO:
		case RFM2GRESERVED0REG:
		case RFM2GRESERVED1REG:
		default:
			return ( RFM2G_BAD_PARAMETER_2 );
	}

    if( DeviceMemoryPtr == (volatile void **) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    /* Map the memory */
    stat = RFM2gUserMemory(rh, DeviceMemoryPtr, Offset, Pages);
    if (stat != RFM2G_SUCCESS)
    {
        return stat;
    }

    /* *DeviceMemoryPtr is page aligned, the PCI address may not be page aligned, adjust
       as necessary */
    if (RFM2GCFGREGMEM == regset)
    {
        *DeviceMemoryPtr = (void*) (((unsigned long ) *DeviceMemoryPtr) | (rh->Config.PciConfig.rfm2gOrBase & 0xfff));
    }
    else if (RFM2GCTRLREGMEM == regset)
    {
        *DeviceMemoryPtr = (void*) (((unsigned long) *DeviceMemoryPtr) | (rh->Config.PciConfig.rfm2gCsBase & 0xfff));
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gMapDeviceMemory() */

/******************************************************************************
*
*  FUNCTION:   RFM2gUnMapDeviceMemory
*
*  PURPOSE:    Unmaps register memory to the user space
*  PARAMETERS: rh (I): Handle to open RFM2g device
*              regset: (I) The memory window to map
*              DeviceMemoryPtr (IO): Pointer to mapped space
*  RETURNS:    RFM2G_SUCCESS if no errors
*              RFM2G_NOT_OPEN if the device has not yet been opened
*              RFM2G_BAD_PARAMETER_1 if the handle is NULL
*              RFM2G_BAD_PARAMETER_2 if the UserMemoryPtr pointer is NULL
*              RFM2G_OS_ERROR if the munmap(2) fails
*  GLOBALS:    None
*
******************************************************************************/
STDRFM2GCALL RFM2gUnMapDeviceMemory(RFM2GHANDLE rh, RFM2GREGSETTYPE regset, volatile void **DeviceMemoryPtr)
{
    RFM2G_STATUS stat;
	RFM2G_UINT32 Pages;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

	switch( regset )
	{
		case RFM2GCFGREGMEM:
			Pages = 1;
			break;

		case RFM2GCTRLREGMEM:
			Pages = 1;
			break;

		case RFM2GMEM:
			Pages = rh->Config.MemorySize / PAGE_SIZE;
			break;

		/* The following are not supported by this driver */
		case RFM2GCFGREGIO:
		case RFM2GRESERVED0REG:
		case RFM2GRESERVED1REG:
		default:
			return ( RFM2G_BAD_PARAMETER_2 );
	}

    /* Check UserMemoryPtr */
    if( DeviceMemoryPtr == (volatile void **) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }
    if( *DeviceMemoryPtr == (void *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    /* RFM2gMapDeviceMemory may have added an offset within a page, remove
       the offset */
    *DeviceMemoryPtr = (void*) (((unsigned long) *DeviceMemoryPtr) & 0xFFFFF000);

    /* Unmap the mmap'ed RFM memory */
    if( munmap( (void *)*DeviceMemoryPtr, Pages * PAGE_SIZE ) != 0 )
    {
        return( RFM2G_OS_ERROR ); /* munmap() failed */
    }

    *DeviceMemoryPtr  = (void *) NULL;

    return( RFM2G_SUCCESS );
}




