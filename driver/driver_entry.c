/*
===============================================================================
                            COPYRIGHT NOTICE

Copyright (c) 2006-2014, GE Intelligent Platforms, Inc.
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
$Workfile: driver_entry.c $
$Revision: 41412 $
$Modtime: 9/03/10 1:16p $
-------------------------------------------------------------------------------
    Description: Standard file operations entry points for the RFM2G driver
        (Linux)
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    15-OCT-2001  ML   1.0   Created.
    12-SEP-2003  EB   01.02 Added ifdef around remap_page_range() to
                            conditionally use the special Red Hat 9 version
                            of the function.  The fixupflags.sh script is
                            used to define the macro tested with the ifdef.
    08-DEC-2004  ML   01.04 Moved spinlock acquire after mapping BARs
    18 Jun 2008  EB   06.00 Added support of PAE (Physical Address
                            Extension)-enabled Linux.
    09 Jul 2008  EB   06.02 Added conditional casting in IsReadyForDma() to
                            convert the buf parameter to a 32-bit integer
                            in either the 32-bit or 64-bit Linux.
    06 Aug 2008  EB   06.04 Added IOCTL_RFM2G_SET_SLIDING_WINDOW_OFFSET that
                            programs the necessary RFM registers and also
                            allows the driver to keep a copy of the current
                            sliding window offset, needed when DMA is used with
                            a sliding window.  Corrected range checking
                            to make it consider sliding windows in both
                            PIO and DMA cases.
    09 Oct 2008  EB   07.00 In rfm2g_open(), map the smaller of total memory
                            or the window size.  Before this change, if a
                            small sliding window was set (e.g. 2MB) and the OS
                            maps the board high up in PCI memory space
                            (e.g. close to the 4GB boundary), then attempting
                            to map the total memory (e.g. 128MB) would fail
                            because the PCI base address + total memory size
                            was greater than 4GB.


===============================================================================
*/
#include "rfm2g_driver.h"

#define RFM2G_MEMORY_SPACE  1
#define RFM2G_IO_SPACE      2

/*** Local prototypes ********************************************************/

#ifdef __cplusplus
extern "C" {
#endif

static RFM2G_UINT8 rfm2g_peek( RFM2GCONFIGLINUX *cfg, RFM2G_ADDR rfm2gAddr,
                               RFM2G_UINT64 *pData, RFM2G_UINT8 width );
static RFM2G_UINT8 rfm2g_poke( RFM2GCONFIGLINUX *cfg, RFM2G_ADDR rfm2gAddr,
                               RFM2G_UINT64 *pData, RFM2G_UINT8 width );
static int set_lcsr_bit(RFM2GCONFIGLINUX *cfg, unsigned int cmd,
                        unsigned long arg, unsigned int bit, char* pDescription );
static int get_lcsr_bit(RFM2GCONFIGLINUX *cfg, unsigned int cmd, unsigned long arg,
                        unsigned int bit, char* pDescription );

static int RFM2gDMA(RFM2GCONFIGLINUX *cfg, RFM2G_UINT32 dmaDesc, RFM2G_UINT64 pciAddress,
                    RFM2G_UINT32 rfmOffset, RFM2G_UINT32 count, RFM2G_UINT32 addrAdjust);

static int IsReadyForDma(RFM2G_UINT8 unit, char *buf, size_t count, RFM2GDMAINFO *DmaInfo,
                         RFM2G_UINT32 *addrAdjust);

#ifdef __cplusplus
}
#endif

/******************************************************************************
*
*  FUNCTION:   rfm2g_open
*
*  PURPOSE:    Entry point for open(2) call.  Maps RFM memory into kernel space.
*  PARAMETERS: *inode: (I) Passed in by OS, used for minor number determination
*              *filp:  (I) Unused, passed in by OS
*  RETURNS:    0: Success
*              -ENOMEM:  Unable to map memory
*  GLOBALS:    rfm2gDeviceInfo, devname
*
******************************************************************************/

int
rfm2g_open( struct inode *inode, struct file *filp )
{
    static char *me = "rfm2g_open()";
    RFM2G_UINT8  unit; /*Minor device number, used as index in rfm2gDeviceInfo*/
    RFM2G_ADDR BaseAddr = 0;
    RFM2G_ADDR MappedAddr = 0;
    unsigned long flags;
    RFM2GCONFIGLINUX *cfg;
    RFM2GDMAINFO *DmaInfo;
    RFM2G_UINT32  mapSize;  /* How much RFM board memory to map */


    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( inode->i_rdev );
    cfg = &(rfm2gDeviceInfo[unit].Config );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                   unit, me);

    /* initialize the DMA management structure for this open call */
    if ((DmaInfo = kmalloc(sizeof(RFM2GDMAINFO), GFP_KERNEL)) == NULL)
    {
        WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: Allocating kernel memory "
                "failed. pid:%d\n", devname, unit, me, current->pid );
        }
        return( -EFAULT);
    }
    memset( (void *)DmaInfo, 0, sizeof(RFM2GDMAINFO));
    DmaInfo->Threshold = RFM2G_DMA_THRESHOLD_MAX;

    filp->private_data = (void *)DmaInfo;

    /* Map the RFM board's memory and register spaces */
    if( rfm2gDeviceInfo[unit].Instance == 0 )
    {
        WHENDEBUG(RFM2G_DBOPEN)
        {
            printk(KERN_ERR"%s%d: %s Enabling interrupts, mapping mem & "
                "I/O space. pid:%d\n", devname, unit, me, current->pid );
        }

        BaseAddr = cfg->PCI.rfm2gBase;
        if( BaseAddr != 0 )
        {
            /* Map the smaller of the two choices: total board memory or memory window */
            mapSize = cfg->MemorySize;
            if (mapSize > cfg->PCI.rfm2gWindowSize)
            {
                mapSize = cfg->PCI.rfm2gWindowSize;
                WHENDEBUG(RFM2G_DBOPEN)
                {
                    printk(KERN_ERR"%s%d: %s Mapping the smaller Window Size %d bytes.  pid:%d\n",
                        devname, unit, me, cfg->PCI.rfm2gWindowSize, current->pid );
                }
            }

            MappedAddr = (RFM2G_ADDR)ioremap_nocache( BaseAddr, mapSize);
            if( MappedAddr == 0 )
            {
                /* Error */
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: Unable to map RFM "
                       "memory space. pid:%d\n", devname, unit, me, current->pid );
                }
                return( -ENOMEM );
            }

            cfg->pBaseAddress = (RFM2G_UINT8 *) MappedAddr;
        }
    }

    /* Acquire lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s About to acquire spinlock. pid:%d\n",
               devname, unit, me, current->pid);
    }
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

    /* Count how many times we've been opened */
    rfm2gDeviceInfo[unit].Instance++;
    rfm2gDeviceInfo[unit].Flags |= RFM2G_OPEN;

    WHENDEBUG(RFM2G_DBOPEN)
    {
        printk(KERN_ERR"%s%d: %s Instance %d  Flags = 0x%08X. pid:%d\n",
            devname, unit, me, rfm2gDeviceInfo[unit].Instance,
            rfm2gDeviceInfo[unit].Flags, current->pid );
    }

    /* Release lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
    }
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s. pid:%d\n", devname,
                                  unit, me, current->pid);

    return(0);

}   /* End of rfm2g_open() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_release
*
*  PURPOSE:    Entry point for close(2) call.  Unmaps RFM memory.
*  PARAMETERS: *inode: (I) Passed in by OS, used for minor number determination
*              *filp:  Unused, passed in by OS
*  RETURNS:    void (kernel 2.0.x), or 0 (kernel 2.2.x and higher)
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/

int
rfm2g_release( struct inode *inode, struct file *filp )
{
    static char *me = "rfm2g_release()";
    RFM2GCONFIGLINUX *cfg;
    RFM2G_UINT8  unit;  /* Minor device number, used as index in rfm2gDeviceInfo */
    unsigned long flags;
    RFM2GEVENTINFOLINUX event;
    int  i;

    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( inode->i_rdev );
    cfg = &(rfm2gDeviceInfo[unit].Config );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s. pid:%d\n", devname,
                                  unit, me, current->pid);

    /* free dma management structure */
    if (filp->private_data != NULL)
    {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }

    /* Acquire lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s About to acquire spinlock. pid:%d\n",
               devname, unit, me, current->pid);
    }
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

    /* Count how many times we've been closed */
    rfm2gDeviceInfo[unit].Instance--;

    if( rfm2gDeviceInfo[unit].Instance == 0 )
    {
        WHENDEBUG(RFM2G_DBCLOSE)
        {
            printk(KERN_ERR"%s%d: %s Disabling interrupts, unmapping mem & "
                "I/O space. pid:%d\n", devname, unit, me, current->pid );
        }

        /* Disable interrupts */
        for(i=0; i<LINUX_RFM2G_NUM_OF_EVENTS; i++)
        {
            event.Unit = unit;
            event.Event = i;

            /* Make sure the interrupt is supported on this board */
            switch( event.Event )
            {

                case RFM2GEVENT_DMADONE:    /* Fall thru */
                    /* These interrupts are disabled during DMA operations */
                    continue;
                    break;

                case RFM2GEVENT_RESET:                 /* Fall thru */
                case RFM2GEVENT_INTR1:                 /* Fall thru */
                case RFM2GEVENT_INTR2:                 /* Fall thru */
                case RFM2GEVENT_INTR3:                 /* Fall thru */
                case RFM2GEVENT_INTR4:                 /* Fall thru */
                    /* These interrupt events are valid for all boards */
                    break;

                case RFM2GEVENT_BAD_DATA:              /* Fall thru */
                case RFM2GEVENT_RXFIFO_FULL:           /* Fall thru */
                case RFM2GEVENT_ROGUE_PKT:             /* Fall thru */
                case RFM2GEVENT_RXFIFO_AFULL:          /* Fall thru */
                case RFM2GEVENT_SYNC_LOSS:             /* Fall thru */
                case RFM2GEVENT_MEM_WRITE_INHIBITED:   /* Fall thru */
                case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:  /* Fall thru */
                    /* These interrupt events shall stay enabled. */
                    continue;
                    break;

                default:
                    /* Release lock */
                    WHENDEBUG(RFM2G_DBMUTEX)
                    {
                        printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
                    }
                    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR | RFM2G_DBOPEN)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: Interrupt %d is "
                            "invalid. pid:%d\n", devname, unit, me, event.Event, current->pid );
                    }
                    return( -ENOMEM );
                    break;
            }

            /* Disable the interrupt */
            DisableInterrupt(&event, RFM2G_TRUE);
			CancelInterrupt(&event, RFM2G_TRUE);
        }

    	/* Release lock */
    	WHENDEBUG(RFM2G_DBMUTEX)
    	{
        	printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
    	}
        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

        /* Unmap the RFM board's memory space */
        if( cfg->pBaseAddress != (RFM2G_UINT8 *) NULL )
        {
            iounmap( (void *) cfg->pBaseAddress );
            cfg->pBaseAddress = (RFM2G_UINT8 *) NULL;
        }
    }
    else
    {
        /* Release lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        }
        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
    }


    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s. pid:%d\n", devname,
                                  unit, me, current->pid);

    return(0);

}   /* End of rfm2g_release() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_llseek
*
*  PURPOSE:    Entry point for lseek(2) call.  Adjusts file pointer to an offset
*              into RFM memory.
*  PARAMETERS: *filp: (IO) Passed in by OS, contains file pointer to be updated.
*                  It is also used for minor number
*                  determination.
*              offset: (I) Offset into RFM memory
*              whence: (I) Determines relative basis of offset
*  RETURNS:    0: Success
*              -EINVAL: Indicates invalid offset or whence values.
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/


loff_t
rfm2g_llseek( struct file *filp, loff_t offset, int whence )
{
    static char *me = "rfm2g_llseek()";
    RFM2GCONFIGLINUX *cfg;
    RFM2G_UINT8  unit;  /* Minor device number, used as index in rfm2gDeviceInfo */


    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( filp->f_dentry->d_inode->i_rdev );
    cfg = &(rfm2gDeviceInfo[unit].Config );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                   unit, me);

    /* Validate the requested offset */
    if( offset > cfg->MemorySize )
    {

        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: offset %d too big for %d "
                "byte board\n", devname, unit, me, (RFM2G_INT32)offset,
                (RFM2G_INT32)cfg->MemorySize );
        }

        return( -EINVAL );
    }

    /* Now move the file offset */
    switch( whence )
    {
        case 0: /*SEEK_SET*/
            filp->f_pos = offset;
            break;

        case 1: /*SEEK_CUR*/  /* Not supported */
        case 2: /*SEEK_END*/  /* Not supported */
        default:
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s:  invalid whence = %d\n",
                    devname, unit, me, whence );
            }

            return( -EINVAL );
        }
    }

    WHENDEBUG(RFM2G_DBREAD | RFM2G_DBWRITE)
    {
        printk(KERN_ERR"%s%d: %s Seeking to Reflective Memory offset 0x%X\n",
            devname, unit, me, (int)offset );
    }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s\n", devname,
                                   unit, me);

    return( 0 );

}   /* End of rfm2g_llseek() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_read
*
*  PURPOSE:    Entry point for read(2) call.
*  PARAMETERS:  *filp: (I) Passed in by OS.  Used
*                  for minor number determination.
*              *buf: (O) User space buffer that will receive data from RFM memory
*              count: (I) The number of bytes to read
*              *offset: (I) Passed in by OS, this is
*                  the offset into RFM memory.
*  RETURNS:    The number of bytes successfully read
*              -EFAULT Error copying data to user
*                  space.
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/
ssize_t
rfm2g_read( struct file *filp, char *buf, size_t count, loff_t *offset )
{
    static char *me = "rfm2g_read()";
    RFM2GCONFIGLINUX *cfg;
    RFM2G_UINT8  unit;  /* Minor device number, used as index in rfm2gDeviceInfo */
    void *rfm2gPtr;
    RFM2G_UINT32 l_offset;
    RFM2GDMAINFO *DmaInfo;
    RFM2G_UINT32 addrAdjust = 0;


    l_offset = (RFM2G_UINT32) *offset;

    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( filp->f_dentry->d_inode->i_rdev );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                    unit, me);

    /* Get a pointer to the specified offset */
    DmaInfo = (RFM2GDMAINFO *)(filp->private_data);
    cfg = &(rfm2gDeviceInfo[unit].Config );
    rfm2gPtr = (void *) ( cfg->pBaseAddress + l_offset );

    WHENDEBUG(RFM2G_DBREAD)
    {
        printk(KERN_ERR"%s%d: %s Reading %zu bytes from offset 0x%X to "
            "address 0x%p\n", devname, unit, me, count, l_offset, (char *)buf );
    }

    /* See if DMA needs to be used */
    if (IsReadyForDma(unit, buf, count, DmaInfo, &addrAdjust))
    {
		return (RFM2gDMA(cfg, RFMOR_DMAPTR_TO_PCI, DmaInfo->BuffAddr, l_offset, count, addrAdjust));
    }
    else /* Not using DMA */
    {
        /* Make sure the offset is within the window size.  We don't want the
        offset to go beyond the end of any sliding window. */
        if ((l_offset + count) > cfg->PCI.rfm2gWindowSize)
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: (RFM offset + count) exceeds WindowSize\n",
                    devname, unit, me );
            }

            return( -EFAULT );
        }

        /* Send the data to the user */
        if( copy_to_user(buf, rfm2gPtr, count) > 0 )
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                    devname, unit, me );
            }

            return( -EFAULT );
        }
        mb();
    }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s\n", devname,
                                    unit, me);

    return( count );

}   /* End of rfm2g_read() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_write
*
*  PURPOSE:    Entry point for write(2) call.
*  PARAMETERS: *filp: (I) Passed in by OS.
*                  used for minor number determination.
*              *buf: (I) User space buffer containing data to be written to RFM
*                  memory
*              count: (I) The number of bytes to written
*              *offset: (I) Passed in by OS, this is
*                  the offset into RFM memory.
*  RETURNS:    The number of bytes successfully written
*              -EFAULT Error copying data from user
*                  space.
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/
ssize_t
rfm2g_write( struct file *filp, const char *buf, size_t count, loff_t *offset )
{
    static char *me = "rfm2g_write()";
    RFM2GCONFIGLINUX *cfg;
    RFM2G_UINT8  unit; /* Minor device number, used as index in rfm2gDeviceInfo */
    void *rfm2gPtr;
    RFM2G_UINT32 l_offset;
    RFM2GDMAINFO *DmaInfo;
    RFM2G_UINT32 addrAdjust = 0;
    char *tempBuf = (char *)buf;


    l_offset = (RFM2G_UINT32) *offset;

    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( filp->f_dentry->d_inode->i_rdev );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                    unit, me);

    /* Get a pointer to the specified offset */
    DmaInfo = (RFM2GDMAINFO*)(filp->private_data);
    cfg = &(rfm2gDeviceInfo[unit].Config );
    rfm2gPtr = (void *) ( cfg->pBaseAddress + l_offset );

    WHENDEBUG(RFM2G_DBWRITE)
    {
        printk(KERN_ERR"%s%d: %s Writing %zu bytes to offset 0x%X from "
            "address 0x%p\n", devname, unit, me, count, l_offset, buf );
    }

    /* See if DMA needs to be used */
    if (IsReadyForDma(unit, tempBuf, count, DmaInfo, &addrAdjust))
    {
		return (RFM2gDMA(cfg, 0x0, DmaInfo->BuffAddr, l_offset, count, addrAdjust));
    }
    else /* Not using DMA */
    {
        /* Make sure the offset is within the window size.  We don't want the
        offset to go beyond the end of any sliding window. */
        if ((l_offset + count) > cfg->PCI.rfm2gWindowSize)
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: (RFM offset + count) exceeds WindowSize\n",
                    devname, unit, me );
            }

            return( -EFAULT );
        }

        /* Get the data from the user */
        if( copy_from_user(rfm2gPtr, (void *)buf, count) != 0 )
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                    devname, unit, me );
            }

            return( -EFAULT );
        }
        mb();
    }
    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s\n", devname, unit, me);

    return( count );

}   /* End of rfm2g_write() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_peek
*
*  PURPOSE:    Read one byte, word, or longword from an offset in Reflective
*              Memory.
*  PARAMETERS: rfm2gAddr: (I) RFM address to read data
*              pData:   (O) Where to put the data
*              width:   (I) Byte, word, or long width of data
*  RETURNS:    0: Success
*              -EINVAL: Invalid data width
*  GLOBALS:    devname
*
******************************************************************************/

RFM2G_UINT8
rfm2g_peek( RFM2GCONFIGLINUX *cfg, RFM2G_ADDR rfm2gAddr, RFM2G_UINT64 *pData, RFM2G_UINT8 width )
{
    static char *me = "rfm2g_peek()";

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s?: Entering %s\n", devname, me);

    /* Do the read */
    switch( width )
    {
        case RFM2G_BYTE:
			if (cfg->pioByteSwap == RFM2G_FALSE)
			{
				*pData = (RFM2G_UINT64) readb( (char *)(rfm2gAddr) );
			}
			else
			{
				/*
				 * Read data.  PLX Byte Swap is turn on, compensate for 4 byte swap.
				 * The address is exclusive ored with 3 to produce byte invariant
				 * addressing.
				 */
				*pData = (RFM2G_UINT64) readb( (char *)(rfm2gAddr ^ 3) );
			}
            break;

        case RFM2G_WORD:
			if (cfg->pioByteSwap == RFM2G_FALSE)
			{
				/*
				 * Read Data.  readw only Byte Swaps
				 * if CPU is big endian.  Data on PCI Bus is little endian.
				 * PLX will not byte swap data.  Data is little endian
				 * on RFM2g local bus.
				 */

	            *pData = (RFM2G_UINT64) readw( (char *)(rfm2gAddr) );

				/*
				 * RFM  PLX Byte Swap  PCI Bus  CPU     readw                 Value
				 * Little   OFF        Little   Little  No Swap performed     Little
				 * Little   OFF        Little   Big     Swaps after read      Big
				 */
			}
			else
			{
				if ((rfm2gAddr % sizeof(RFM2G_UINT16)) != 0)
					{
					return (-EINVAL);
					}

				/*
				 * Read Data.  readw only Byte Swaps
				 * if CPU is Big Endian, converting the data to little
				 * endian format.  Data on PCI Bus is little endian.
				 * PLX Byte swap will convert data from little endian on
				 * RFM2g local bus to big endian on PCI bus.  The address is
				 * exclusive ored with 2 to produce byte invariant addressing.
				 */

	            *pData = (RFM2G_UINT64) readw( (char *)(rfm2gAddr ^ 2) );

				/*
				 * RFM  PLX Byte Swap  PCI Bus  CPU     BSPX_PCI_MEM_IN_WORD  Value
				 * Little   ON         Big      Little  No Swap performed     Big
				 * Little   ON         Big      Big     Swaps after read      Little
				 */

				/*
				 * PLX byte swap will convert data from little endian on
				 * RFM2g local bus to big endian on PCI bus. Because
				 * readw performs a byte swap after read
				 * for big endian systems and not for little endian systems,
				 * we must do a unconditional byte swap if PLX byte swap is
				 * turned on.
				 */
				*pData = (((*pData)<<8)  | ( (*pData) >> 8));
			}
			/*
			 * RFM  PLX Byte Swap PCI Bus  CPU    readw                LONGSWAP Value
			 * Little    OFF      Little   Little No Swap performed    NO       Little
			 * Little    OFF      Little   Big    Swaps after read     NO       Big
			 * Little    ON       Big      Little No Swap performed    YES      Little
			 * Little    ON       Big      Big    Swaps after read     YES      Big
			 */
            break;

        case RFM2G_LONG:

			/* If PIO byte swap is enabled, make sure the offset is aligned. */
			if ( (cfg->pioByteSwap == RFM2G_TRUE) && ((rfm2gAddr % sizeof(RFM2G_UINT32)) != 0))
			{
				return (-EINVAL);
			}

			/*
			 * Read Data.  readl only Byte Swaps
			 * if CPU is Big Endian.  Data on PCI Bus is little endian
			 * if PLX byte swap is off, otherwise it is big endian.
			 * Data is little endian on RFM2g local bus.
			 */

            *pData = (RFM2G_UINT64) readl( (char *)(rfm2gAddr) );

			/*
			 * RFM     PLX Byte Swap  PCI Bus  CPU     BSPX_PCI_MEM_IN_LONG  Value
			 * Little      OFF        Little   Little  No Swap performed     Little
			 * Little      OFF        Little   Big     Swaps after read      Big
			 * Little      ON         Big      Little  No Swap performed     Big
			 * Little      ON         Big      Big     Swaps after read      Little
			 */

			if (cfg->pioByteSwap != RFM2G_FALSE)
			{
				/*
				 * PLX byte swap will convert data from little endian on
				 * RFM2g local bus to big endian on PCI bus. Because
				 * BSPX_PCI_MEM_IN_LONG performs a byte swap after read
				 * for big endian systems and not for little endian systems,
				 * we must do a unconditional byte swap if PLX byte swap is
				 * turned on.
				 */
				*pData = ( (( *pData & 0x000000ff) << 24) |
						   (( *pData & 0x0000ff00) << 8)  |
						   (( *pData & 0x00ff0000) >> 8)  |
						   (( *pData & 0xff000000) >> 24)
						 );
			}

			/*
			 * RFM  PLX Byte Swap PCI Bus  CPU    BSPX_PCI_MEM_IN_WORD LONGSWAP Value
			 * Little    OFF      Little   Little No Swap performed    NO       Little
			 * Little    OFF      Little   Big    Swaps after read     NO       Big
			 * Little    ON       Big      Little No Swap performed    YES      Little
			 * Little    ON       Big      Big    Swaps after read     YES      Big
			 */
            break;

        case RFM2G_LONGLONG: /* Not supported */

#ifdef RFM2G_LONGLONG_NOT_SUPPORTED

            RFM2G_UINT32 Value0;
            RFM2G_UINT32 Value1;
            RFM2G_UINT32* pValue = (RFM2G_UINT32*) pData;

			/* If PIO byte swap is enabled, make sure the offset is aligned. */
			if ( (cfg->pioByteSwap == RFM2G_TRUE) && ((rfm2gAddr % sizeof(RFM2G_UINT64)) != 0))
			{
				return (-EINVAL);
			}

            /* Read Data. */
            Value0 = readl( (char *)(rfm2gAddr);
            Value1 = readl( (char *)(rfm2gAddr + 4);

            if (cfg->pioByteSwap != RFM2G_FALSE)
            {
				/*
				 * PLX byte swap will convert data from little endian on
				 * RFM2g local bus to big endian on PCI bus. Because
				 * BSPX_PCI_MEM_IN_LONG performs a byte swap after read
				 * for big endian systems and not for little endian systems,
				 * we must do a unconditional byte swap if PLX byte swap is
				 * turned on.
				 */
				Value0 = ( (( Value0 & 0x000000ff) << 24) |
						   (( Value0 & 0x0000ff00) << 8)  |
						   (( Value0 & 0x00ff0000) >> 8)  |
						   (( Value0 & 0xff000000) >> 24)
						 );
				Value1 = ( (( Value1 & 0x000000ff) << 24) |
						   (( Value1 & 0x0000ff00) << 8)  |
						   (( Value1 & 0x00ff0000) >> 8)  |
						   (( Value1 & 0xff000000) >> 24)
						 );
            }
            pValue[0] = Value0;
            pValue[1] = Value1;

            break;
#endif

        default:
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s?: Exiting %s: Invalid data width %d\n",
                    devname, me, width );
            }

            return( -EINVAL );
        }
    }

    WHENDEBUG(RFM2G_DBPEEK)
    {
        printk(KERN_ERR"%s?: %s Width = %d  VirtRfmAddr = 0x%lX  Data = 0x%X\n",
            devname, me, width, rfm2gAddr, (RFM2G_UINT32) *pData );
    }
    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s?: Exiting %s\n", devname, me);

    return( 0 );

}   /* End of rfm2g_peek() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_poke
*
*  PURPOSE:    Write one byte, word, or longword to an offset in Reflective
*              Memory.
*  PARAMETERS: rfmAddr: (I) RFM address to write data
*              pData:   (I) Where to get the data
*              width:   (I) Byte, word, long, or longlong width of data
*  RETURNS:    0: Success
*              -EINVAL: Invalid data width
*  GLOBALS:    devname
*
******************************************************************************/

RFM2G_UINT8
rfm2g_poke( RFM2GCONFIGLINUX *cfg, RFM2G_ADDR rfm2gAddr, RFM2G_UINT64 *pData, RFM2G_UINT8 width )
{
    static char *me = "rfm2g_poke()";

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s?: Entering %s\n", devname, me);

    /* Do the write */
    switch( width )
    {
        case RFM2G_BYTE:

		    if (cfg->pioByteSwap == RFM2G_FALSE)
	        {
	            writeb( (RFM2G_UINT8) *pData,  (char *)(rfm2gAddr) );
			}
			else
			{
				/*
				 * Write data.  PLX Byte Swap is turn on, compensate for 4 byte swap.
				 * The address is exclusive or with 3 to produce byte invariant
				 * addressing.
				 */
	            writeb( (RFM2G_UINT8) *pData,  (char *)(rfm2gAddr ^ 3) );
			}
            break;

        case RFM2G_WORD:

			if (cfg->pioByteSwap == RFM2G_FALSE)
			{
				/*
				 * CPU   PLX Byte Swap  Value   BSPX_PCI_MEM_OUT_LONG PCI Bus  RFM
				 * Little    OFF        Little  No Swap performed     Little   Little
				 * Big       OFF        Big     Swaps prior to write  Little   Little
				 */

				/*
				 * Write Data.  BSPX_PCI_MEM_OUT_WORD only Byte Swaps
				 * if CPU is Big Endian.  Data on PCI Bus is Little Endian.
				 * PLX will not byte swap data.  Data is Little Endian
				 * on RFM2g local bus.
				 */

				writew( (RFM2G_UINT16) *pData, (char *)(rfm2gAddr) );
			}
			else
			{
				if ((rfm2gAddr % sizeof(RFM2G_UINT16)) != 0)
				{
					return (-EINVAL);
				}

				/*
				 * If CPU is little endian, swap Value to big endian.
				 * if CPU is big endian, data is byte swapped to
				 * little endian.
				 */

				/*
				 * CPU   PLX Byte Swap  Value   BSPX_PCI_MEM_OUT_LONG PCI Bus  RFM
				 * Little    ON         Big     No Swap performed     Big      Little
				 * Big       ON         Little  Swaps prior to write  Big      Little
				 */

				/*
				 * Write Data.  writew only Byte Swaps
				 * if CPU is Big Endian, converting the data to big
				 * endian format.  Data on PCI Bus is Big Endian.
				 * PLX Byte swap will convert data to Little Endian on
				 * RFM2g local bus.
				 */

				/* Byte swap contents of pData */

				writew( (( (RFM2G_UINT16) *pData) << 8)  | (( (RFM2G_UINT16) *pData) >> 8),
					    (char *) (rfm2gAddr ^ 2)
					  );

			}
            break;

        case RFM2G_LONG:
			/* If PIO byte swap is enabled, make sure the offset is aligned. */
			if ( (cfg->pioByteSwap == RFM2G_TRUE) && ((rfm2gAddr % sizeof(RFM2G_UINT32)) != 0))
			{
				return (-EINVAL);
			}

			/*
			 * CPU     PLX Byte Swap  Value    BSPX_PCI_MEM_OUT_LONG   PCI Bus  RFM
			 * Little      OFF        Little   No Swap performed       Little   Little
			 * Big         OFF        Big      Swaps prior to write    Little   Little
			 * Little      ON         Big      No Swap performed       Big      Little
			 * Big         ON         Little   Swaps prior to write    Big      Little
			 */
			if (cfg->pioByteSwap == RFM2G_FALSE)
			{
				/*
				 * Write Data.  BSPX_PCI_MEM_OUT_LONG only byte swaps
				 * if CPU is Big Endian.  Data is little endian on PCI bus
				 * if PLX byte swap is off, otherwise data is big endian on
				 * PCI bus.  Data is little endian on RFM2g local bus.
				 */
	            writel( (RFM2G_UINT32) *pData, (char *)(rfm2gAddr) );
			}
			else
			{
				/*
				 * PLX Byte swap will convert data to little endian on
				 * RFM2g local bus from big endian on PCI bus. If CPU is
				 * little endian, we need to byte swap to make the value
				 * a big endian value.  If CPU is big endian, we need to
				 * byte swap to make the value a little endian value because
				 * BSPX_PCI_MEM_OUT_LONG is going to swap the value prior
				 * to write.
				 */
				writel( ( ((( (RFM2G_UINT32) *pData ) & 0x000000ff) << 24) |
	                                  ((( (RFM2G_UINT32) *pData ) & 0x0000ff00) << 8)  |
	                                  ((( (RFM2G_UINT32) *pData ) & 0x00ff0000) >> 8)  |
	                                  ((( (RFM2G_UINT32) *pData ) & 0xff000000) >> 24)
	                                ),
	                                (char *)(rfm2gAddr) );
			}
            break;

        case RFM2G_LONGLONG: /* Not supported */
        default:
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s: Exiting %s?: Invalid data width %d\n",
                    devname, me, width );
            }

            return( -EINVAL );
        }
    }

    WHENDEBUG(RFM2G_DBPOKE)
    {
        printk(KERN_ERR"%s?: %s Width = %d  VirtRfmAddr = 0x%lx  Data = 0x%X\n",
            devname, me, width, rfm2gAddr, (RFM2G_UINT32) *pData );
    }
    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s: Exiting %s\n", devname, me);

    return( 0 );

}   /* End of rfm2g_poke() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_mmap
*
*  PURPOSE:    Entry point for mmap(2) call.  Maps RFM memory into user
*              address space.
*  PARAMETERS: *filp: (I) Passed in by OS.  used
*                  for minor number determination.
*              *vma: (I) Passed in by OS, used to get virtual and physical addresses
*                  and size, which are passed into remap_page_range()
*  RETURNS:    0: Success
*              -EAGAIN: Memory mapping failed
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/

int
rfm2g_mmap( struct file *filp, struct vm_area_struct *vma )
{
    static char *me         = "rfm2g_mmap()";
    unsigned long virtaddr  = 0;
    unsigned long physaddr  = 0;
    unsigned long offset    = 0;
    unsigned long size      = 0;
    RFM2GCONFIGLINUX *cfg   = NULL;
    RFM2G_UINT8  unit;  /* Minor device number, used as index in rfm2gDeviceInfo */
    int dma                 = RFM2G_FALSE;
    unsigned int pageNumber = 0;
    RFM2GDMAINFO *DmaInfo   = (RFM2GDMAINFO *)(filp->private_data);


    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( filp->f_dentry->d_inode->i_rdev );
    cfg = &(rfm2gDeviceInfo[unit].Config );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                    unit, me);

    virtaddr = vma->vm_start;
    size     = vma->vm_end - vma->vm_start;
    offset   = vma->vm_pgoff << PAGE_SHIFT; /* convert pages to bytes */

    WHENDEBUG(RFM2G_DBMMAP) printk(KERN_ERR"%s: %s offset = 0x%08X\n", devname, me, (unsigned int)offset);
    WHENDEBUG(RFM2G_DBTRACE | RFM2G_DBMMAP) printk(KERN_ERR"%s: %s vma->vm_start=0x%X vma->vm_end=0x%X vma->vm_pgoff=0x%X. pid:%d\n",
				devname, me, (RFM2G_UINT32) vma->vm_start, (RFM2G_UINT32) vma->vm_end,
				(RFM2G_UINT32) vma->vm_pgoff, current->pid);

    if (DmaInfo->InUseAction == RFM2G_BAR0_MMAP_OFFSET)
	{
        DmaInfo->InUseAction = 0;

		/* Give the user the BAR0 Register space - Memory Mapped */
        physaddr = cfg->PCI.rfm2gOrBase;

		WHENDEBUG(RFM2G_DBMMAP)
		{
			printk(KERN_ERR"%s: %s virtaddr=0x%X physaddr=0x%X size=0x%X offset=0x%X RFM2G_BAR0_MMAP_OFFSET. pid:%d\n",
				devname, me, (RFM2G_UINT32)virtaddr, (RFM2G_UINT32)physaddr,
				(RFM2G_UINT32)size, (RFM2G_UINT32) offset, current->pid );
		}

        physaddr &= 0xFFFFF000;
	}
	else if (DmaInfo->InUseAction == RFM2G_BAR2_MMAP_OFFSET)
	{
        DmaInfo->InUseAction = 0;

		/* Give the user the BAR2 Register space */
        physaddr = cfg->PCI.rfm2gCsBase;

		WHENDEBUG(RFM2G_DBMMAP)
		{
			printk(KERN_ERR"%s: %s virtaddr=0x%X physaddr=0x%X size=0x%X offset=0x%X RFM2G_BAR2_MMAP_OFFSET. pid:%d\n",
				devname, me, (RFM2G_UINT32)virtaddr, (RFM2G_UINT32)physaddr,
				(RFM2G_UINT32)size, (RFM2G_UINT32) offset, current->pid );
		}

        physaddr &= 0xFFFFF000;
	}
    else if (DmaInfo->InUseAction == RFM2G_DMA_MMAP_OFFSET)
    {
        DmaInfo->InUseAction = 0;

        /* Map the DMA buffer that was allocated at boot time using 'mem=xxxM' */
        dma = RFM2G_TRUE;

		if ((DmaInfo->BuffAddr == 0) || (size == 0))
		{
			WHENDEBUG(RFM2G_DBERROR)
			{
	 			printk(KERN_ERR"%s%d: Exiting %s: DMA buffer address (0x%llX) and/or size (%d) "
					"is zero. pid:%d\n", devname, unit, me,
					DmaInfo->BuffAddr, (unsigned int) size, current->pid );
			}

            /* Give the semaphore, which was taken inside rfm2g_ioctl(IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET) */
            up(rfm2gDeviceInfo[unit].mmapSem);

			return( -EAGAIN );
		}

        DmaInfo->BuffSize = size;
        DmaInfo->VirtAddr = virtaddr;

		WHENDEBUG(RFM2G_DBMMAP)
		{
			printk(KERN_ERR"%s: %s virtaddr=0x%X physaddr=0x%llX size=0x%X offset=0x%X RFM2G_DMA_MMAP_OFFSET. pid:%d\n",
				devname, me, (RFM2G_UINT32)virtaddr, DmaInfo->BuffAddr,
				(RFM2G_UINT32)size, (RFM2G_UINT32) offset, current->pid );
		}

    }
    else if ((offset + size) <= cfg->PCI.rfm2gWindowSize)
    {
        /* Map reflective memory */
        physaddr = cfg->PCI.rfm2gBase + offset;

        /* Check for PAGE_SIZE alignment */
        if( offset & (PAGE_SIZE-1) )
        {
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: Offset (0x%X) is not PAGE_SIZE "
                    "aligned. pid:%d\n", devname, unit, me, (unsigned int) offset, current->pid );
            }

            return( -EAGAIN );
        }

        WHENDEBUG(RFM2G_DBMMAP)
        {
            printk(KERN_ERR"%s: %s virtaddr=0x%X physaddr=0x%X size=0x%X. pid:%d\n",
                devname, me, (RFM2G_UINT32)virtaddr, (RFM2G_UINT32)physaddr,
                (RFM2G_UINT32)size, current->pid );
        }
    }
    else
	{
		/* invalid offset */
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: Offset (0x%X) and size (%d) "
                "are out of range. pid:%d\n", devname, unit, me,
                (unsigned int) offset, (unsigned int) size, current->pid );
        }

        return( -EAGAIN );
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#else
#ifdef CFG_RFM_KERNEL_DRIVER
    {
        unsigned long prot;

        /* make mapped memory non-cachable */
        vma->vm_flags |= VM_RESERVED | VM_LOCKED | VM_IO | VM_SHM;
        prot = pgprot_val(vma->vm_page_prot);
        prot |= _PAGE_NO_CACHE | _PAGE_GUARDED;
        vma->vm_page_prot = __pgprot(prot);
    }
#else
    vma->vm_flags |= VM_RESERVED;
#endif
#endif

    /* In kernel 2,6,11 The remap_page_range() function has been replaced with
       remap_pfn_range().  The third arg is the page number now. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)

    if (dma == RFM2G_TRUE)
    {
        pageNumber = (unsigned int)(DmaInfo->BuffAddr/PAGE_SIZE);
    }
    else
    {
        pageNumber = physaddr/PAGE_SIZE;
    }

    if (remap_pfn_range(vma, virtaddr, pageNumber, size, vma->vm_page_prot))
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: remap_pfn_range() failed. pid:%d\n",
                devname, unit, me, current->pid );
        }

        if (dma == RFM2G_TRUE)
        {
            DmaInfo->BuffAddr = 0;
            DmaInfo->VirtAddr = 0;

            /* Give the semaphore, which was taken inside rfm2g_ioctl(IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET) */
            up(rfm2gDeviceInfo[unit].mmapSem);
        }

        return( -EAGAIN );
    }

#else
    /* The remap_page_range() function has a new parameter starting with
       the 2.5.3 kernel, and also with Red Hat 9.0 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,3) || defined REDHAT9KERNEL

    if (dma == RFM2G_TRUE)
    {
        physaddr = (unsigned int)(DmaInfo->BuffAddr & 0xFFFFFFFF);
    }

    if( remap_page_range( vma, virtaddr, physaddr, size, vma->vm_page_prot ) )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: remap_page_range() failed\n",
                devname, unit, me );
        }

        if (dma == RFM2G_TRUE)
        {
            DmaInfo->BuffAddr = 0;
            DmaInfo->VirtAddr = 0;

            /* Give the semaphore, which was taken inside rfm2g_ioctl(IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET) */
            up(rfm2gDeviceInfo[unit].mmapSem);
        }
        return( -EAGAIN );
    }

#else

    if (dma == RFM2G_TRUE)
    {
        physaddr = (unsigned int)(DmaInfo->BuffAddr & 0xFFFFFFFF);
    }

    if( remap_page_range( virtaddr, physaddr, size, vma->vm_page_prot ) )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: remap_page_range() failed\n",
                devname, unit, me );
        }

        if (dma == RFM2G_TRUE)
        {
            DmaInfo->BuffAddr = 0;
            DmaInfo->VirtAddr = 0;

            /* Give the semaphore, which was taken inside rfm2g_ioctl(IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET) */
            up(rfm2gDeviceInfo[unit].mmapSem);
        }

        return( -EAGAIN );
    }
#endif
#endif

    if (dma == RFM2G_TRUE)
    {
        /* Give the semaphore, which was taken inside rfm2g_ioctl(IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET) */
        up(rfm2gDeviceInfo[unit].mmapSem);
    }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s. pid:%d\n", devname,
                                    unit, me, current->pid);

    return(0);

}   /* End of rfm2g_mmap() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_ioctl
*
*  PURPOSE:    Entry point for ioctl(2) call.
*  PARAMETERS: *inode: (I) Passed in by OS, used for minor number determination
*              *filp: Passed in by OS, unused.
*              cmd: (I) The ioctl command to be processed
*              arg: (IO) Argument to cmd, or return value from command
*  RETURNS:    0: Success
*              -EINVAL: Invalid parameter or command
*              -EFAULT: Failure during command execution
*              -ETIMEDOUT: Timed out waiting on an interrupt
*              -ENOSYS: Interrupt event not supported by board
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
int
rfm2g_ioctl( struct inode *inode, struct file *filp, unsigned int cmd,
    unsigned long arg )
#else
long
rfm2g_ioctl(struct file *filp, unsigned int cmd, unsigned long arg )
#endif
{
    static char *me = "rfm2g_ioctl()";
    RFM2GCONFIGLINUX *cfg;
    RFM2G_UINT8  unit; /* Minor device number,used as index in rfm2gDeviceInfo*/
    unsigned long flags=0;
	int ret_status = 0;
    RFM2G_UINT32 stat;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,36)
    	struct inode *inode = filp->f_dentry->d_inode;
#endif

	/* Valid Call? */
	if (_IOC_TYPE(cmd) != RFM2G_MAGIC)
	{
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s: Exiting %s: invalid ioctl magic num = %d expected %d\n",
                devname, me, _IOC_TYPE(cmd), RFM2G_MAGIC);
        }
		return -ENOTTY;
	}

    /* Extract the minor number so we know which RFM device to access */
    unit = MINOR( inode->i_rdev );
    cfg = &(rfm2gDeviceInfo[unit].Config );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                   unit, me);

    /* Process the ioctl command */
    switch( cmd )
    {
        case IOCTL_RFM2G_GET_CONFIG:
        {
			RFM2GCONFIG Config;
            char *myself = "GET_CONFIG";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

			/* Copy the common stuff over */
			Config.NodeId              = cfg->NodeId;
			Config.BoardId             = cfg->BoardId;
			Config.Unit                = cfg->Unit;
			Config.PlxRevision         = cfg->PlxRevision;
			Config.MemorySize          = cfg->MemorySize;
			strcpy(Config.Device, cfg->Device);
			strcpy(Config.Name, RFM2G_PRODUCT_STRING);
			strcpy(Config.DriverVersion, RFM2G_PRODUCT_VERSION);
			Config.BoardRevision       = cfg->RevisionId;
			Config.RevisionId          = cfg->PCI.revision;
            Config.BuildId             = cfg->BuildId;
            memcpy(&Config.PciConfig, &cfg->PCI, sizeof(RFM2GPCICONFIG));

			/* Read LCSR1 */
			Config.Lcsr1 = (RFM2G_UINT32) readl( (char *)(( (RFM2G_ADDR) cfg->pCsRegisters + rfm2g_lcsr )));

            /* Copy the data back to the user */
            if( copy_to_user( (void *)arg, (void *)&Config,
                sizeof(RFM2GCONFIG) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }
        }
        break;

        case IOCTL_RFM2G_SET_LED:
        {
			ret_status = set_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_LED, "SET_LED");
        }
        break;

        case IOCTL_RFM2G_GET_LED:
        {
			ret_status = get_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_LED, "GET_LED");
        }
        break;

        case IOCTL_RFM2G_ATOMIC_PEEK:
        {
            char *myself = "ATOMIC_PEEK";
            RFM2GATOMIC  Data;     /* Info necessary to do the access         */
    	    RFM2G_ADDR rfm2gAddr;  /* RFM address to read data              */
            RFM2G_ADDR base;     /* Base address of RFM I/O or memory space */
            RFM2G_UINT32 maxsize;  /* Max size of RFM I/O or memory space     */

            WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBPEEK)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            if( copy_from_user( (void *)&Data, (void *)arg,
                sizeof(RFM2GATOMIC) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Validate the data width */
            switch( Data.width )
            {
                case RFM2G_BYTE: break;
                case RFM2G_WORD: break;
                case RFM2G_LONG: break;
                case RFM2G_LONGLONG: /* Not supported, fall thru to default */
                default:
                {
                    WHENDEBUG(RFM2G_DBERROR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: Invalid data width %d\n",
                            devname, unit, me, Data.width );
                    }

                    return( -EINVAL );
                }
            }

            base    =  (RFM2G_ADDR)cfg->pBaseAddress;
            maxsize = cfg->PCI.rfm2gWindowSize - Data.width;

            /* Point to the desired offset in Reflective Memory */
            rfm2gAddr = base + (RFM2G_ADDR)Data.offset;

            /* Make sure we are not out of range */
            if( rfm2gAddr > (base + maxsize) )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: offset 0x%X is out of range\n",
                        devname, unit, me, Data.offset );
                }

                return( -EINVAL );
            }

            if( rfm2g_peek( cfg, rfm2gAddr, &(Data.data), Data.width ) != 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: rfm2g_peek() failed\n",
                        devname, unit, me );
                }

                return( -EINVAL );
            }

            /* Copy the data back to the user */
            if( copy_to_user( (void *)arg, (void *)&Data,
                sizeof(RFM2GATOMIC) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }
        }
        break;

        case IOCTL_RFM2G_ATOMIC_POKE:
        {
            char *myself = "ATOMIC_POKE";
            RFM2GATOMIC  Data;     /* Info necessary to do the access         */
            RFM2G_ADDR rfm2gAddr;  /* RFM address to write data               */
            RFM2G_ADDR base;     /* Base address of RFM I/O or memory space */
            RFM2G_UINT32 maxsize;  /* Max size of RFM I/O or memory space     */

            WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBPOKE)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            if( copy_from_user( (void *)&Data, (void *)arg,
                sizeof(RFM2GATOMIC) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Validate the data width */
            switch( Data.width )
            {
                case RFM2G_BYTE: break;
                case RFM2G_WORD: break;
                case RFM2G_LONG: break;
                case RFM2G_LONGLONG: /* Not supported, fall thru to default */
                default:
                {
                    WHENDEBUG(RFM2G_DBERROR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: Invalid data width %d\n",
                            devname, unit, me, Data.width );
                    }

                    return( -EINVAL );
                }
            }

	        base    =  (RFM2G_ADDR)cfg->pBaseAddress;
            maxsize = cfg->PCI.rfm2gWindowSize - Data.width;

            /* Point to the desired offset in Reflective Memory */
            rfm2gAddr = base + Data.offset;

            /* Make sure we are not out of range */
            if( rfm2gAddr > (base + maxsize) )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: offset 0x%X is out of range\n",
                        devname, unit, me, Data.offset );
                }

                return( -EINVAL );
            }

            if( rfm2g_poke( cfg, rfm2gAddr, (void *) &(Data.data), Data.width ) != 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: rfm2g_poke() failed\n",
                        devname, unit, me );
                }

                return( -EINVAL );
            }
        }
        break;

        case IOCTL_RFM2G_CLEAR_DMA_MMAP_OFFSET:
        {
            char *myself = "CLEAR_DMA_MMAP_OFFSET";
            RFM2GDMAINFO *DmaInfo = (RFM2GDMAINFO*)(filp->private_data);
            RFM2G_UINT32 virtAddr = (RFM2G_UINT32) arg;

            /* Is this a match for the virtual pointer previously returned by mmap()? */
            if (DmaInfo->VirtAddr == virtAddr)
            {
                DmaInfo->VirtAddr = 0;
                DmaInfo->BuffAddr = 0;
                WHENDEBUG(RFM2G_DBMMAP)
                {
                    printk(KERN_ERR"%s%d: %s(%s): DMA buffer Virtual Address 0x%0x8 unregistered. pid:%d\n",
                        devname, unit, me, myself, virtAddr, current->pid );
                }
            }
            else
            {
                WHENDEBUG(RFM2G_DBMMAP)
                {
                    printk(KERN_ERR"%s%d: %s(%s): DMA buffer Virtual Address 0x%0x8 NOT unregistered. pid:%d\n",
                        devname, unit, me, myself, virtAddr, current->pid );
                }
            }
        }
        break;

        case IOCTL_RFM2G_SET_SPECIAL_MMAP_OFFSET:
        {
            char *myself = "SET_SPECIAL_MMAP_OFFSET";
            RFM2GDMAINFO *DmaInfo = (RFM2GDMAINFO*)(filp->private_data);
            RFM2G_UINT64 specialOffset = 0;

            if( copy_from_user( (void *)&specialOffset, (void *)arg,
                sizeof(RFM2G_UINT64) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s(%s): copy_from_user() failed. pid:%d\n",
                        devname, unit, me, myself, current->pid );
                }

                return( -EFAULT );
            }

            /* Which special mmap offset is this? */
            if (specialOffset & RFM2G_DMA_MMAP_OFFSET)
            {
                /* Take the semaphore.  This semaphore will normally be given back
                inside rfm2g_mmap() */
                stat = down_interruptible(rfm2gDeviceInfo[unit].mmapSem);
                if (stat != 0)
                {
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBMMAP)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s:(%s) down interrupted "
                            "stat %d. pid:%d\n", devname, unit, me, myself, stat, current->pid);
                    }
                    return(-EFAULT);
                }

                DmaInfo->InUseAction = RFM2G_DMA_MMAP_OFFSET;
                DmaInfo->BuffAddr = specialOffset & ~((RFM2G_UINT64)RFM2G_DMA_MMAP_OFFSET);
                WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBMMAP)
                {
                    printk(KERN_ERR"%s%d: %s(%s) Next rfm2g_mmap() will use DMA buffer physaddr 0x%llX. pid:%d\n",
                        devname, unit, me, myself, DmaInfo->BuffAddr, current->pid);
                }

                /* Make sure the address is page aligned */
                if (DmaInfo->BuffAddr & (PAGE_SIZE-1))
                {
                    WHENDEBUG(RFM2G_DBERROR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s(%s): DMA Buffer Physical Address 0x%llX\n",
                            devname, unit, me, myself, DmaInfo->BuffAddr );
                        printk(KERN_ERR"%s%d:     is not page aligned.. pid:%d\n",
                            devname, unit, current->pid);
                    }

                    /* Give the semaphore */
                    up(rfm2gDeviceInfo[unit].mmapSem);

                    return( -EFAULT );
                }
            }
            else if (specialOffset & RFM2G_BAR0_MMAP_OFFSET)
            {
                DmaInfo->InUseAction = RFM2G_BAR0_MMAP_OFFSET;
                WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBMMAP)
                {
                    printk(KERN_ERR"%s%d: %s(%s) Next rfm2g_mmap() will use RFM2G_BAR0_MMAP_OFFSET. pid:%d\n",
                        devname, unit, me, myself, current->pid);
                }
            }
            else if (specialOffset & RFM2G_BAR2_MMAP_OFFSET)
            {
                DmaInfo->InUseAction = RFM2G_BAR2_MMAP_OFFSET;
                WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBMMAP)
                {
                    printk(KERN_ERR"%s%d: %s(%s) Next rfm2g_mmap() will use RFM2G_BAR2_MMAP_OFFSET. pid:%d\n",
                        devname, unit, me, myself, current->pid);
                }
            }
            else
            {
                DmaInfo->InUseAction = 0;
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s(%s): Unknown special mmap offset flag.. pid:%d\n",
                        devname, unit, me, myself, current->pid );
                }

                return( -EFAULT );
            }

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s(%s) DMA Buffer Physical Address = 0x%llX. pid:%d\n",
                    devname, unit, me, myself, DmaInfo->BuffAddr, current->pid );
            }
        }
        break;

        case IOCTL_RFM2G_SET_DEBUG_FLAGS:
        {
            char *myself = "SET_DEBUG_FLAGS";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s, debugflags = 0x%08X\n",
                    devname, unit, me, myself, (RFM2G_UINT32) arg );
            }

            rfm2gDebugFlags = (RFM2G_UINT32) arg;
        }
        break;

        case IOCTL_RFM2G_GET_DEBUG_FLAGS:
        {
            char *myself = "GET_DEBUG_FLAGS";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s, debugflags = 0x%08X\n",
                    devname, unit, me, myself, rfm2gDebugFlags);
            }


            /* Copy the debug flags back to the user */
            if( copy_to_user( (void *)arg, (void *)&rfm2gDebugFlags,
                sizeof(RFM2G_UINT32) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }
        }
        break;

        case IOCTL_RFM2G_SEND_EVENT:
        {
            char *myself = "SEND_EVENT";
            RFM2GEVENTINFO event;   /* Info needed to send an interrupt  */
            RFM2G_ADDR rfm2gAddr;   /* Base address of RFM memory space  */

            RFM2G_UINT8 cmd = 0;    /* compiled command byte */

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            if( copy_from_user( (void *)&event, (void *)arg,
                sizeof(RFM2GEVENTINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Don't allow "self-interrupt", They won't happen */
            if( event.NodeId == cfg->NodeId )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s: Exiting %s: self-interrupt attempted\n",
                        devname, me );
                }

                return( -EINVAL );
            }

            rfm2gAddr = (RFM2G_ADDR) cfg->pCsRegisters;
            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

            /* Write the extended interrupt data */
			writel( *( (RFM2G_UINT32 *) &event.ExtendedInfo ), (char *)((rfm2gAddr + rfm2g_ntd)) );

            /* Write the target node id */
			writeb( (RFM2G_UINT8) event.NodeId,  (char *)((rfm2gAddr + rfm2g_ntn)) );

            /* Build the interrupt command */
            if( event.NodeId == RFM2G_NODE_ALL )
            {
                cmd = RFM2G_NIC_GLOBAL;
            }

			switch(event.Event)
			{
				case RFM2GEVENT_RESET:
						cmd |= RFM2G_NIC_RSTREQ;
						break;

				case RFM2GEVENT_INTR1:
						cmd |= RFM2G_NIC_INT1;
						break;

				case RFM2GEVENT_INTR2:
						cmd |= RFM2G_NIC_INT2;
						break;

				case RFM2GEVENT_INTR3:
						cmd |= RFM2G_NIC_INT3;
						break;

				case RFM2GEVENT_INTR4:
						cmd |= RFM2G_NIC_INITINT;
						break;

				default:
						/* Invalid event type */
						return( -EINVAL );
			}

            /* Write the interrupt command */
			writeb( *( (RFM2G_UINT8 *) &cmd ),  (char *)((rfm2gAddr + rfm2g_nic)) );

            mb();

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
        }
        break;


        case IOCTL_RFM2G_ENABLE_EVENT:
        {
            char *myself = "ENABLE_EVENT";
            RFM2GEVENTINFO event;
			RFM2GEVENTINFOLINUX eventLinux;

            WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* See which event the user wants to enable */
            if( copy_from_user( (void *)&event, (void *)arg,
                sizeof(RFM2GEVENTINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Make sure the interrupt is supported on this board */
            switch( event.Event )
            {

                case RFM2GEVENT_RESET:                 /* Fall thru */
                case RFM2GEVENT_INTR1:                 /* Fall thru */
                case RFM2GEVENT_INTR2:                 /* Fall thru */
                case RFM2GEVENT_INTR3:                 /* Fall thru */
                case RFM2GEVENT_INTR4:                 /* Fall thru */
                case RFM2GEVENT_BAD_DATA:              /* Fall thru */
                case RFM2GEVENT_RXFIFO_FULL:           /* Fall thru */
                case RFM2GEVENT_ROGUE_PKT:             /* Fall thru */
                case RFM2GEVENT_RXFIFO_AFULL:          /* Fall thru */
                case RFM2GEVENT_SYNC_LOSS:             /* Fall thru */
                case RFM2GEVENT_MEM_WRITE_INHIBITED:   /* Fall thru */
                case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:  /* Fall thru */
                    /* These interrupt events are valid for all boards */
                    break;

                default:
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: Interrupt %d is "
                            "invalid\n", devname, unit, me, event.Event );
                    }

                    return( -EINVAL );
                    break;
            }

            /* Enable the interrupt */
			eventLinux.Unit = unit;
			eventLinux.ExtendedInfo = event.ExtendedInfo;
			eventLinux.NodeId = event.NodeId;
			eventLinux.Timeout = event.Timeout;
			eventLinux.Event = event.Event;

            switch( EnableInterrupt(&eventLinux, RFM2G_FALSE) )
            {
                case 0:
                    /* The interrupt was successfully enabled */
                    break;

                case -1:
                    /* This interrupt has already been enabled */
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: EnableInterrupt() "
                            "failed\n", devname, unit, me );
                    }
                    return( -EACCES );
                    break;

                default:
                    /* Some other error occured */
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: EnableInterrupt() "
                            "failed\n", devname, unit, me );
                    }
                    return( -EFAULT );
                    break;
            }
        }
        break;

        case IOCTL_RFM2G_DISABLE_EVENT:
        {
			char *myself = "DISABLE_EVENT";
			RFM2GEVENTINFO event;
			RFM2GEVENTINFOLINUX eventLinux;

            WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* See which event the user wants to disable */
            if( copy_from_user( (void *)&event, (void *)arg,
                sizeof(RFM2GEVENTINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Make sure the interrupt is supported on this board */
            switch( event.Event )
            {
                case RFM2GEVENT_RESET:                 /* Fall thru */
                case RFM2GEVENT_INTR1:                 /* Fall thru */
                case RFM2GEVENT_INTR2:                 /* Fall thru */
                case RFM2GEVENT_INTR3:                 /* Fall thru */
                case RFM2GEVENT_INTR4:                 /* Fall thru */
                case RFM2GEVENT_BAD_DATA:              /* Fall thru */
                case RFM2GEVENT_RXFIFO_FULL:           /* Fall thru */
                case RFM2GEVENT_ROGUE_PKT:             /* Fall thru */
                case RFM2GEVENT_RXFIFO_AFULL:          /* Fall thru */
                case RFM2GEVENT_SYNC_LOSS:             /* Fall thru */
                case RFM2GEVENT_MEM_WRITE_INHIBITED:   /* Fall thru */
                case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:  /* Fall thru */
                    /* These interrupt events are valid for all boards */
                    break;

                default:
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: Interrupt %d is "
                            "invalid\n", devname, unit, me, event.Event );
                    }

                    return( -EINVAL );
                    break;
            }

            /* Disable the interrupt */
			eventLinux.Unit = unit;
			eventLinux.ExtendedInfo = event.ExtendedInfo;
			eventLinux.NodeId = event.NodeId;
			eventLinux.Timeout = event.Timeout;
			eventLinux.Event = event.Event;

            switch( DisableInterrupt(&eventLinux, RFM2G_FALSE) )
            {
                case 0:
                    /* The interrupt was successfully disabled */
                    break;

                case -1:
                    /* This interrupt was not already enabled */
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: DisableInterrupt() "
                            "failed\n", devname, unit, me );
                    }
                    return( -EACCES );
                    break;

                case -2:  /* Fall thru */
                default:
                    /* Some other error occured */
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: DisableInterrupt() "
                            "failed\n", devname, unit, me );
                    }
                    return( -EFAULT );
                    break;
            }
        }
        break;

        case IOCTL_RFM2G_GET_EVENT_STATS:
        {
            char *myself = "GET_EVENT_STATS";
            rfm2gEventQueueHeader_t *qhead;
            RFM2GQINFO  qinfo;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* Read the RFM2GQINFO structure because we need the event number */
            if( copy_from_user( (void *)&qinfo, (void *)arg,
                sizeof(RFM2GQINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Gather the stats */
            qhead = &(rfm2gDeviceInfo[unit].EventQueue[qinfo.Event].req_header);
            qinfo.Overflowed   = qhead->reqh_flags & QUEUE_OVERFLOW;
            qinfo.EventCount   = qhead->reqh_counter;
            qinfo.EventsQueued = qhead->reqh_size;
            qinfo.QueueSize    = qhead->reqh_maxSize;
            qinfo.MaxQueueSize      = qhead->reqh_maxSize;
            qinfo.EventTimeout = qhead->reqh_timeout;
            qinfo.QueuePeak    = qhead->reqh_qpeak;

            /* Copy the data back to the user */
            if( copy_to_user( (void *)arg, (void *)&qinfo,
                sizeof(RFM2GQINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s: Exiting %s: copy_to_user() failed\n",
                        devname, me );
                }

                return( -EFAULT );
            }
        }
        break;

        case IOCTL_RFM2G_WAIT_FOR_EVENT:
        {
            int iStatus = 0;
            char *myself = "WAIT_FOR_EVENT";
            RFM2GEVENTINFO event;   /* Storage for received interrupt info */
            RFM2GEVENTINFOLINUX eventLinux;
            RFM2G_INT32 status = 0;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            if( copy_from_user( (void *)&event, (void *)arg,
                sizeof(RFM2GEVENTINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk( KERN_ERR"%s%d: %s About to acquire spinlock\n",
                    devname, unit, me);
            }

            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

            eventLinux.Unit = unit;
            eventLinux.ExtendedInfo = event.ExtendedInfo;
            eventLinux.NodeId = event.NodeId;
            eventLinux.Timeout = event.Timeout;
            eventLinux.Event = event.Event;

            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

            status = WaitForInterrupt( &eventLinux, flags );

            switch( status )
            {
            case -ETIMEDOUT:

                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: WaitForInterrupt() "
                        "Timed out waiting\n", devname, unit, me );
                }

                /* Timed out waiting for interrupt */
                iStatus = -ETIMEDOUT;
                break;

            case -EALREADY:

                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: WaitForInterrupt() "
                        "Notification already requested\n", devname, unit, me );
                }

                /* Notification was already requested for this event */
                iStatus  = -EALREADY;
                break;

            case -ENOSYS:
                /* This interrupt was not already enabled */
                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: WaitForInterrupt() "
                        "failed\n", devname, unit, me );
                }
                iStatus = -ENOSYS;
                break;

            case -EIDRM:

                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: WaitForInterrupt() "
                        "canceled\n", devname, unit, me );
                }

                iStatus = -EIDRM;
                break;

            case -EINTR:
                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: WaitForInterrupt() "
                        "Signal\n", devname, unit, me );
                }
                iStatus = -EINTR;
                break;
            }

            /* Leave if any errors returned. */
            if (iStatus != 0)
                return(iStatus);

            /*****************************/
            /* We received the interrupt */
            /*****************************/

            /* Copy info about the received interrupt back to the user */

            event.ExtendedInfo = eventLinux.ExtendedInfo;
            event.NodeId = eventLinux.NodeId;
            event.Timeout = eventLinux.Timeout;
            event.Event = eventLinux.Event;

            if( copy_to_user( (void *)arg, (void *)&event,
                sizeof(RFM2GEVENTINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }
        }
        break;

        case IOCTL_RFM2G_CANCEL_EVENT:
        {
			char *myself = "CANCEL_EVENT";
			RFM2GEVENTINFO event;
			RFM2GEVENTINFOLINUX eventLinux;
            int CancelIntStatus;

            WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* See which event the user wants to cancel */
            if( copy_from_user( (void *)&event, (void *)arg,
                sizeof(RFM2GEVENTINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            /* Wake up any sleeping process */
			eventLinux.Unit = unit;
			eventLinux.ExtendedInfo = event.ExtendedInfo;
			eventLinux.NodeId = event.NodeId;
			eventLinux.Timeout = event.Timeout;
			eventLinux.Event = event.Event;

            CancelIntStatus = CancelInterrupt( &eventLinux, RFM2G_FALSE );

            switch( CancelIntStatus )
            {
                case 0:  /* No errors */
                    break;

                case -1: /* Interrupt was not already enabled */
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: CancelInterrupt() "
                            "failed\n", devname, unit, me );
                    }

                    return( -ENOSYS );
                    break;

                default: /* Fall thru */
                case -2: /* CancelInterrupt() failed for some misc reason */
                    WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                    {
                        printk(KERN_ERR"%s%d: Exiting %s: CancelInterrupt() "
                            "failed\n", devname, unit, me );
                    }

                    return( -EFAULT );
                    break;
            }
        }
        break;

        case IOCTL_RFM2G_CLEAR_EVENT_COUNT:
        {
            char *myself = "CLEAR_EVENT_COUNT";
            rfm2gEventQueueHeader_t *qhead;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s, unit = %d, queue = %d\n",
                    devname, unit, me, myself, unit, (int)arg );
            }

            /* Validate the interrupt type number */
            if( (arg >= RFM2GEVENT_LAST) )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: Invalid interrupt queue %d\n",
                        devname, unit, me, (int)arg );
                }

                return( -EINVAL );
            }

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

            /* Clear the event counter for this unit */
            qhead = &( rfm2gDeviceInfo[unit].EventQueue[arg].req_header );
            qhead->reqh_counter = 0;
            qhead->reqh_qpeak = qhead->reqh_size;
            mb();

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
        }
        break;


        case IOCTL_RFM2G_CLEAR_EVENT_STATS:
        {
            char *myself = "CLEAR_EVENT_STATS";
            rfm2gEventQueueHeader_t *qhead;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s, unit = %d, queue = %d\n",
                    devname, unit, me, myself, unit, (int)arg );
            }

            /* Validate the interrupt type number */
            if( (arg >= RFM2GEVENT_LAST) )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: Invalid interrupt queue %d\n",
                        devname, unit, me, (int)arg );
                }

                return( -EINVAL );
            }

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

            /* Clear the event counter for this unit */
            qhead = &( rfm2gDeviceInfo[unit].EventQueue[arg].req_header );
            qhead->reqh_counter = 0;
            qhead->reqh_qpeak = qhead->reqh_size;
            mb();

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
        }
        break;

        case IOCTL_RFM2G_CHECK_RING_CONT:
        {
            char *myself = "CHECK_RING_CONT";
            RFM2G_ADDR    csRegs;      /* Base address of RFM cs register space */
            RFM2G_ADDR    temp;       /* Value read/written to registers        */
            RFM2G_BOOL      found = RFM2G_FALSE;
            int i;

            csRegs = (RFM2G_ADDR)cfg->pCsRegisters;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

            /* Clear the own data bit */
            temp = readl( (char *)(csRegs + rfm2g_lcsr) );
            temp &= ~RFM2G_LCSR_OWNDATA;
            writel(temp, (char *)(csRegs + rfm2g_lcsr) );

            /* Now use one of the "reserved" ints to send a packet around the ring */
            /* Write the target node id */
            writeb( (RFM2G_UINT8)(cfg->NodeId), (char *)(csRegs + rfm2g_ntn) );

            /* Write the interrupt command */
            writeb( (RFM2G_UINT8)RFM2G_NIC_RSVD, (char *)(csRegs + rfm2g_nic) );

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

            /* Now we wait until the OWNDATA bit is set or we timeout (try for 2 second) */
            for (i=200; i>=0; i-=4)
            {
                /* See if OWNDATA got set */
                temp = readl( (char *)(csRegs + rfm2g_lcsr) );
                if (temp & RFM2G_LCSR_OWNDATA)
                {
                    /* Got it, get out of here */
                    found = RFM2G_TRUE;
                    break;
                }

                /* Delay for 10000 us  */
                udelay(10000);
            }

            /* did our OWNDATA bit get set? */
            if (!found)
                return( -EFAULT );
        }
        break;


        case IOCTL_RFM2G_GET_DMA_THRESHOLD:
        {
            char *myself = "GET_DMA_THRESHOLD";
            RFM2GDMAINFO *DmaInfo = (RFM2GDMAINFO*)(filp->private_data);
            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s, Threshold = %u\n",
                    devname, unit, me, myself, DmaInfo->Threshold );
            }

            /* see if DMA is supported for this board */
            if (!(cfg->Capabilities & RFM2G_SUPPORTS_DMA))
            {
                return( -ENOSYS);
            }

            /* Copy the data back to the user */
            if( copy_to_user( (void *)arg, (void *)&DmaInfo->Threshold,
                sizeof(RFM2G_UINT32) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }
        }
            break;


        case IOCTL_RFM2G_SET_DMA_THRESHOLD:
        {
            char *myself = "SET_DMA_THRESHOLD";
            RFM2GDMAINFO *DmaInfo = (RFM2GDMAINFO *)(filp->private_data);

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname,
                       unit, me, myself);
            }

            /* see if DMA is supported for this board */
            if (!(cfg->Capabilities & RFM2G_SUPPORTS_DMA))
            {
                return( -ENOSYS);
            }

            /* Copy the data back to the user */
            if( copy_from_user( (void *)&DmaInfo->Threshold, (void *)arg,
                sizeof(RFM2G_UINT32) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s Threshold = %d\n", devname,
                       unit, me, myself, DmaInfo->Threshold);
            }

        }
            break;

        case IOCTL_RFM2G_SET_DARK_ON_DARK:
        {
			ret_status = set_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_DOD, "SET_DARK_ON_DARK");
        }
        break;

        case IOCTL_RFM2G_GET_DARK_ON_DARK:
        {
			ret_status = get_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_DOD, "GET_DARK_ON_DARK");
        }
        break;

        case IOCTL_RFM2G_SET_TRANSMIT_DISABLE:
        {
			ret_status = set_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_XMTDIS, "SET_TRANSMIT_DISABLE");
        }
        break;

        case IOCTL_RFM2G_GET_TRANSMIT_DISABLE:
        {
			ret_status = get_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_XMTDIS, "GET_TRANSMIT_DISABLE");
        }
        break;

        case IOCTL_RFM2G_SET_LOOPBACK:
        {
			ret_status = set_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_LOOP, "SET_LOOPBACK");
        }
        break;

        case IOCTL_RFM2G_GET_LOOPBACK:
        {
			ret_status = get_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_LOOP, "GET_LOOPBACK");
        }
        break;

        case IOCTL_RFM2G_SET_PARITY_ENABLE:
        {
			ret_status = set_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_PTY, "SET_PARITY_ENABLE");
        }
        break;

        case IOCTL_RFM2G_GET_PARITY_ENABLE:
        {
			ret_status = get_lcsr_bit(cfg, cmd, arg, RFM2G_LCSR_PTY, "GET_PARITY_ENABLE");
        }
        break;

        case IOCTL_RFM2G_SET_MEMORY_OFFSET:
        {
            char *myself = "SET_MEMORY_WINDOW";

			WHENDEBUG(RFM2G_DBIOCTL)
			{
				printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
			}


			/*  Offset 1 and Offset 0 When the host PCI system writes to the on-board
				memory and initiates a packet over the network, Offset 1 and Offset 0 will
				apply an offset to the network address as it is sent or received over the
				network.
				Offset 1 Offset 0 Offset Applied
				0 0 0
				0 1 $4000000
				1 0 $8000000
				1 1 $C000000
			 */
			switch(arg)
			{
				case RFM2G_MEM_OFFSET0:
					/* 0 0 0 */
					ret_status = set_lcsr_bit(cfg, cmd, RFM2G_FALSE, RFM2G_LCSR_OFF0, "SET_MEMORY_WINDOW");
					if (ret_status == 0)
					{
						ret_status = set_lcsr_bit(cfg, cmd, RFM2G_FALSE, RFM2G_LCSR_OFF1, "SET_MEMORY_WINDOW");
					}
					break;

				case RFM2G_MEM_OFFSET1:
					/* 0 1 $4000000 */
					ret_status = set_lcsr_bit(cfg, cmd, RFM2G_TRUE, RFM2G_LCSR_OFF0, "SET_MEMORY_WINDOW");
					if (ret_status == 0)
					{
						ret_status = set_lcsr_bit(cfg, cmd, RFM2G_FALSE, RFM2G_LCSR_OFF1, "SET_MEMORY_WINDOW");
					}
					break;

				case RFM2G_MEM_OFFSET2:
					/* 1 0 $8000000 */
					ret_status = set_lcsr_bit(cfg, cmd, RFM2G_FALSE, RFM2G_LCSR_OFF0, "SET_MEMORY_WINDOW");
					if (ret_status == 0)
					{
						ret_status = set_lcsr_bit(cfg, cmd, RFM2G_TRUE, RFM2G_LCSR_OFF1, "SET_MEMORY_WINDOW");
					}
					break;

				case RFM2G_MEM_OFFSET3:
					/* 1 1 $C000000 */
					ret_status = set_lcsr_bit(cfg, cmd, RFM2G_TRUE, RFM2G_LCSR_OFF0, "SET_MEMORY_WINDOW");
					if (ret_status == 0)
					{
						ret_status = set_lcsr_bit(cfg, cmd, RFM2G_TRUE, RFM2G_LCSR_OFF1, "SET_MEMORY_WINDOW");
					}
					break;

				default:
                    return( -EINVAL );
			}
        }
        break;

        case IOCTL_RFM2G_GET_MEMORY_OFFSET:
        {
			RFM2G_MEM_OFFSETTYPE offset;
		    RFM2G_UINT32  temp;      /* Value read from LCSR              */
            char *myself = "GET_MEMORY_OFFSET";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			/* Get current contents of LCSR */
			temp = readl( (char *)((RFM2G_ADDR) ( cfg->pCsRegisters + rfm2g_lcsr )));

			switch (temp & (RFM2G_LCSR_OFF0 | RFM2G_LCSR_OFF1))
			{
				case 0:
					offset = RFM2G_MEM_OFFSET0;
					break;

				case RFM2G_LCSR_OFF0:
					offset = RFM2G_MEM_OFFSET1;
					break;

				case RFM2G_LCSR_OFF1:
					offset = RFM2G_MEM_OFFSET2;
					break;

				case (RFM2G_LCSR_OFF0 | RFM2G_LCSR_OFF1):
					offset = RFM2G_MEM_OFFSET3;
					break;

				default:
					return( -EFAULT );
			}

			/* Copy the data back to the user */
			if( copy_to_user( (void *)arg, (void *)&offset,
				sizeof(RFM2G_MEM_OFFSETTYPE) ) > 0 )
			{
				WHENDEBUG(RFM2G_DBERROR)
				{
					printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
						devname, cfg->Unit, me );
				}

				return( -EFAULT );
			}

        }
        break;

        case IOCTL_RFM2G_SET_SLIDING_WINDOW_OFFSET:
        {
            char *myself = "SET_SLIDING_WINDOW_OFFSET";
            RFM2G_UINT32 temp;
            RFM2G_ADDR orbase = (RFM2G_ADDR)cfg->pOrRegisters; /* Base address
                                                       of RFM's or space    */

            /* Store the current sliding window offset */
            cfg->SlidingWindowOffset = (RFM2G_UINT32) arg;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s, Sliding Window offset = 0x%08X\n",
                    devname, unit, me, myself, (RFM2G_UINT32) arg );
            }

            /* Set the current sliding window offset */
            writel((cfg->SlidingWindowOffset | RFMOR_LAS1BA_ENABLE_LAS1),
                (char *)(orbase + rfmor_las1ba));

            /* Read the value back to to flush the previous write */
            temp = readl((char *)(orbase + rfmor_las1ba));
        }
        break;

        case IOCTL_RFM2G_CLEAR_OWN_DATA:
        {
            char *myself = "CLEAR_OWN_DATA";
            RFM2G_ADDR    csRegs;      /* Base address of RFM cs register space */
            RFM2G_UINT32    temp;       /* Value read/written to registers        */
            RFM2G_UINT8     found = RFM2G_FALSE;

            csRegs = (RFM2G_ADDR)cfg->pCsRegisters;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* See if OWNDATA set */
            temp = readl( (char *)(csRegs + rfm2g_lcsr) );
            if (temp & RFM2G_LCSR_OWNDATA)
            {
                found = RFM2G_TRUE;
            }

			/* Copy the data back to the user */
			if( copy_to_user( (void *)arg, (void *)&found,
				sizeof(RFM2G_UINT8) ) > 0 )
			{
				WHENDEBUG(RFM2G_DBERROR)
				{
					printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
						devname, unit, me );
				}
				return( -EFAULT );
			}

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

            /* Clear the own data bit */
            temp = readl( (char *)(csRegs + rfm2g_lcsr) );
            temp &= ~RFM2G_LCSR_OWNDATA;
            writel(temp, (char *)(csRegs + rfm2g_lcsr) );

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
        }
        break;

        case IOCTL_RFM2G_SET_PCI_COMPATIBILITY:
        {
		    RFM2G_UINT8  *pRfm2g = NULL;      /* Temp pointer to RFM control registers     */
            char *myself = "SET_PCI_COMPATIBILITY";
			unsigned int tmp;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			/* Temporarily map the PLX Runtime registers */
			pRfm2g = (RFM2G_UINT8 *) ioremap_nocache( cfg->PCI.rfm2gOrBase,
													  cfg->PCI.rfm2gOrWindowSize );
			if( pRfm2g == (RFM2G_UINT8 *) NULL )
			{
				WHENDEBUG(RFM2G_DBERROR)
				{
					printk( KERN_ERR"%s:%s ioremap_nocache() failed.\n",
						me, devname );
				}

				return( -ENOMEM );
			}

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

			tmp = readl(pRfm2g + rfmor_DmaArb);
			tmp &= 0xFEFFFFFF;
			writel(tmp, pRfm2g + rfmor_DmaArb);

			/* Enable bits 31:28 of LBRD0 register. (Direct Slave Retry Delay Clocks 8*F) */
			tmp = readl(pRfm2g + rfmor_lbrd0);
			tmp &= 0x0FFFFFFF;
			tmp |= 0xF0000000;
			writel(tmp, pRfm2g + rfmor_lbrd0);

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

			/* Unmap the temporary pointer to the RFM control registers */
			if( pRfm2g != (RFM2G_UINT8 *) NULL )
			{
				iounmap( (void *) pRfm2g );
			}

			ret_status = 0;
        }
        break;

        case IOCTL_RFM2G_WRITE:
        {
			RFM2GTRANSFER rfm2gTransfer;
		    RFM2GDMAINFO *DmaInfo;
            RFM2G_UINT32  addrAdjust = 0;
            int           status;
            char         *myself = "WRITE";

			ret_status = 0;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			/* Get a pointer to the specified offset */
			DmaInfo = (RFM2GDMAINFO *)(filp->private_data);

            if( copy_from_user( (void *)&rfm2gTransfer, (void *)arg,
                sizeof(RFM2GTRANSFER) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed \n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            if (rfm2gTransfer.Length != 0)
            {
                /* See if DMA needs to be used */
                if (IsReadyForDma(unit, (char *)rfm2gTransfer.Buffer, rfm2gTransfer.Length, DmaInfo, &addrAdjust))
                {
                    status = RFM2gDMA(cfg, 0x0, DmaInfo->BuffAddr, rfm2gTransfer.Offset,
                        rfm2gTransfer.Length, addrAdjust);
                    if (status < 0)
                    {
                        return -EFAULT;
                    }
                }
                else /* Not using DMA */
                {
                    void *rfm2gPtr;
                    rfm2gPtr = (void *) ( (RFM2G_ADDR)cfg->pBaseAddress + rfm2gTransfer.Offset );

                    /* Make sure the offset is within the window size.  We don't want the
                    offset to go beyond the end of any sliding window. */
                    if ((rfm2gTransfer.Offset + rfm2gTransfer.Length) > cfg->PCI.rfm2gWindowSize)
                    {
                        WHENDEBUG(RFM2G_DBERROR)
                        {
                            printk(KERN_ERR"%s%d: Exiting %s: (RFM offset + count) exceeds WindowSize\n",
                                devname, unit, me );
                        }

                        return( -EFAULT );
                    }

                    /* Get the data from the user */

                    if( copy_from_user(rfm2gPtr, (void *) rfm2gTransfer.Buffer, rfm2gTransfer.Length) != 0 )
                    {
                        WHENDEBUG(RFM2G_DBERROR)
                        {
                            printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed 1\n",
                                devname, unit, me );
                        }

                        return( -EFAULT );
                    }
                    mb();
                }
            }
		}
		break;

        case IOCTL_RFM2G_WRITE_REG:
        {
			RFM2GLINUXREGINFO rfm2gLinuxRegInfo;
			void* pAddr = NULL;
            char *myself = "WRITE_REG";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			ret_status = 0;

            if( copy_from_user( (void *)&rfm2gLinuxRegInfo, (void *)arg,
                sizeof(RFM2GLINUXREGINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
				case RFM2G_WORD:
				case RFM2G_LONG:
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
					return ( -EINVAL );
			}

			switch(rfm2gLinuxRegInfo.regset)
			{
				case RFM2GCFGREGIO:
					return ( -EINVAL );
					break;

				case RFM2GCFGREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pOrRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GCTRLREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pCsRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pBaseAddress + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GRESERVED0REG:
					return ( -EINVAL );
					break;

				case RFM2GRESERVED1REG:
					return ( -EINVAL );
					break;

				default:
					return ( -EINVAL );
			}

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
		            writeb( (RFM2G_UINT8) rfm2gLinuxRegInfo.Value,  (char *)(pAddr) );
					break;
				case RFM2G_WORD:
		            writew( (RFM2G_UINT16) rfm2gLinuxRegInfo.Value,  (char *)(pAddr) );
					break;
				case RFM2G_LONG:
		            writel( (RFM2G_UINT32) rfm2gLinuxRegInfo.Value,  (char *)(pAddr) );
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
					return ( -EINVAL );
			}
			mb();
        }
        break;

        case IOCTL_RFM2G_SET_REG:
        {
			RFM2GLINUXREGINFO rfm2gLinuxRegInfo;
			void* pAddr = NULL;
            char *myself = "SET_REG";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			ret_status = 0;

            if( copy_from_user( (void *)&rfm2gLinuxRegInfo, (void *)arg,
                sizeof(RFM2GLINUXREGINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
				case RFM2G_WORD:
				case RFM2G_LONG:
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
					return ( -EINVAL );
			}

			switch(rfm2gLinuxRegInfo.regset)
			{
				case RFM2GCFGREGIO:
					return ( -EINVAL );
					break;

				case RFM2GCFGREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pOrRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GCTRLREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pCsRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pBaseAddress + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GRESERVED0REG:
					return ( -EINVAL );
					break;

				case RFM2GRESERVED1REG:
					return ( -EINVAL );
					break;

				default:
					return ( -EINVAL );
			}

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk( KERN_ERR"%s%d: %s About to acquire spinlock\n",
                        devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
					{
						RFM2G_UINT8 temp;
						temp =  readb( (char *)(pAddr) );
						temp |= (RFM2G_UINT8) rfm2gLinuxRegInfo.Value;
						writeb( temp,  (char *)(pAddr) );
					}
					break;

				case RFM2G_WORD:
					{
						RFM2G_UINT16 temp;
						temp =  readw( (char *)(pAddr) );
						temp |= (RFM2G_UINT16) rfm2gLinuxRegInfo.Value;
						writew( temp,  (char *)(pAddr) );
					}
					break;
				case RFM2G_LONG:
					{
						RFM2G_UINT32 temp;
						temp =  readl( (char *)(pAddr) );
						temp |= (RFM2G_UINT32) rfm2gLinuxRegInfo.Value;
						writel( temp,  (char *)(pAddr) );
					}
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
                    /* This should have been caught above, but in case we get here,
                       release the lock */

                    /* Release lock */
                    WHENDEBUG(RFM2G_DBMUTEX)
                    {
                        printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
                    }
                    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
					return ( -EINVAL );
			}
			mb();

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
        }
        break;

        case IOCTL_RFM2G_CLEAR_REG:
        {
			RFM2GLINUXREGINFO rfm2gLinuxRegInfo;
			void* pAddr = NULL;
            char *myself = "CLEAR_REG";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			ret_status = 0;

            if( copy_from_user( (void *)&rfm2gLinuxRegInfo, (void *)arg,
                sizeof(RFM2GLINUXREGINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
				case RFM2G_WORD:
				case RFM2G_LONG:
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
					return ( -EINVAL );
			}

			switch(rfm2gLinuxRegInfo.regset)
			{
				case RFM2GCFGREGIO:
					return ( -EINVAL );
					break;

				case RFM2GCFGREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pOrRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GCTRLREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pCsRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pBaseAddress + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GRESERVED0REG:
					return ( -EINVAL );
					break;

				case RFM2GRESERVED1REG:
					return ( -EINVAL );
					break;

				default:
					return ( -EINVAL );
			}

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk( KERN_ERR"%s%d: %s About to acquire spinlock\n",
                        devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
					{
						RFM2G_UINT8 temp;
						temp =  readb( (char *)(pAddr) );
						temp &= ~((RFM2G_UINT8) rfm2gLinuxRegInfo.Value);
						writeb( temp,  (char *)(pAddr) );
					}
					break;

				case RFM2G_WORD:
					{
						RFM2G_UINT16 temp;
						temp =  readw( (char *)(pAddr) );
						temp &= ~((RFM2G_UINT16) rfm2gLinuxRegInfo.Value);
						writew( temp,  (char *)(pAddr) );
					}
					break;
				case RFM2G_LONG:
					{
						RFM2G_UINT32 temp;
						temp =  readl( (char *)(pAddr) );
						temp &= ~((RFM2G_UINT32) rfm2gLinuxRegInfo.Value);
						writel( temp,  (char *)(pAddr) );
					}
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:

                    /* This should have been caught above, but in case we get here,
                       release the lock */

                    /* Release lock */
                    WHENDEBUG(RFM2G_DBMUTEX)
                    {
                        printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
                    }
                    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
					return ( -EINVAL );
			}
			mb();

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
        }
        break;

        case IOCTL_RFM2G_READ:
        {
			RFM2GTRANSFER rfm2gTransfer;
		    RFM2GDMAINFO *DmaInfo;
            RFM2G_UINT32  addrAdjust = 0;
            int           status;
            char         *myself = "READ";

			ret_status = 0;

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			/* Get a pointer to the specified offset */
			DmaInfo = (RFM2GDMAINFO *)(filp->private_data);

            if( copy_from_user( (void *)&rfm2gTransfer, (void *)arg,
                sizeof(RFM2GTRANSFER) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed \n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s Length= 0x%X DmaInfo->Threshold = 0x%X DmaInfo->BuffAddr = 0x%llX\n",
                    devname, unit, me, myself, rfm2gTransfer.Length, DmaInfo->Threshold, DmaInfo->BuffAddr);
            }

            if (rfm2gTransfer.Length != 0)
            {
                /* See if DMA needs to be used */
                if (IsReadyForDma(unit, (char *)rfm2gTransfer.Buffer, rfm2gTransfer.Length, DmaInfo, &addrAdjust))
                {
                    status = RFM2gDMA(cfg, RFMOR_DMAPTR_TO_PCI, DmaInfo->BuffAddr, rfm2gTransfer.Offset,
                        rfm2gTransfer.Length, addrAdjust);
                    if (status < 0)
                    {
                        return -EFAULT;
                    }
                }
                else /* Not using DMA */
                {
                    void *rfm2gPtr;
                    rfm2gPtr = (void *) ( (RFM2G_ADDR)cfg->pBaseAddress + rfm2gTransfer.Offset );

                    /* Make sure the offset is within the window size.  We don't want the
                    offset to go beyond the end of any sliding window. */
                    if ((rfm2gTransfer.Offset + rfm2gTransfer.Length) > cfg->PCI.rfm2gWindowSize)
                    {
                        WHENDEBUG(RFM2G_DBERROR)
                        {
                            printk(KERN_ERR"%s%d: Exiting %s: (RFM offset + count) exceeds WindowSize\n",
                                devname, unit, me );
                        }

                        return( -EFAULT );
                    }

                    /* Send the data to the user */
                    if( copy_to_user(rfm2gTransfer.Buffer, rfm2gPtr, rfm2gTransfer.Length) > 0 )
                    {
                        WHENDEBUG(RFM2G_DBERROR)
                        {
                            printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                                devname, unit, me );
                        }

                        return( -EFAULT );
                    }
                    mb();
                }
            }
		}
		break;

        case IOCTL_RFM2G_READ_REG:
        {
			RFM2GLINUXREGINFO rfm2gLinuxRegInfo;
			void* pAddr = NULL;
            char *myself = "READ_REG";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			ret_status = 0;

            if( copy_from_user( (void *)&rfm2gLinuxRegInfo, (void *)arg,
                sizeof(RFM2GLINUXREGINFO) ) > 0 )
            {
                WHENDEBUG(RFM2G_DBERROR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: copy_from_user() failed\n",
                        devname, unit, me );
                }

                return( -EFAULT );
            }

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
				case RFM2G_WORD:
				case RFM2G_LONG:
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
					return ( -EINVAL );
			}

			switch(rfm2gLinuxRegInfo.regset)
			{
				case RFM2GCFGREGIO:
					return ( -EINVAL );
					break;

				case RFM2GCFGREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pOrRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GCTRLREGMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pCsRegisters + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GMEM:
					pAddr = (void*) ( (RFM2G_ADDR) cfg->pBaseAddress + rfm2gLinuxRegInfo.Offset);
					break;

				case RFM2GRESERVED0REG:
					return ( -EINVAL );
					break;

				case RFM2GRESERVED1REG:
					return ( -EINVAL );
					break;

				default:
					return ( -EINVAL );
			}

			switch( rfm2gLinuxRegInfo.Width )
			{
				case RFM2G_BYTE:
		            rfm2gLinuxRegInfo.Value = (RFM2G_ADDR) readb( (char *)(pAddr) );
					break;
				case RFM2G_WORD:
		            rfm2gLinuxRegInfo.Value = (RFM2G_ADDR) readw( (char *)(pAddr) );
					break;
				case RFM2G_LONG:
		            rfm2gLinuxRegInfo.Value = (RFM2G_ADDR) readl( (char *)(pAddr) );
					break;

				case RFM2G_LONGLONG: /* Not supported */
				default:
					return ( -EINVAL );
			}

			/* Copy the data back to the user */
			if( copy_to_user( (void *)arg, (void *)&rfm2gLinuxRegInfo,
				sizeof(RFM2GLINUXREGINFO) ) > 0 )
			{
				WHENDEBUG(RFM2G_DBERROR)
				{
					printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
						devname, cfg->Unit, me );
				}

				return( -EFAULT );
			}
        }
        break;

        case IOCTL_RFM2G_FLUSH_QUEUE:
				/* Linux specific, obsolete
			       Clear event will flush the queue and clear the H/W */
        case IOCTL_RFM2G_CLEAR_EVENT:
        {
			char *myself = "CLEAR_EVENT";
		    RFM2G_UINT32  saved_lisr;
            rfm2gEventQueueHeader_t *qhead;

            WHENDEBUG(RFM2G_DBIOCTL | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, unit, me, myself);
            }

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
                       devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

			saved_lisr = readl( (char *)(cfg->pCsRegisters + rfm2g_lisr) );
			saved_lisr |= RFM2G_LISR_UNUSED; /* Enable Global Interrupt */

			switch (arg)
			{
                case RFM2GEVENT_RESET:
	                saved_lisr &= ~RFM2G_LISR_RSTREQ;
					break;

                case RFM2GEVENT_INTR1:
					writeb( 0x00, cfg->pCsRegisters + rfm2g_sid1);
					break;

                case RFM2GEVENT_INTR2:
					writeb( 0x00, cfg->pCsRegisters + rfm2g_sid2);
					break;

                case RFM2GEVENT_INTR3:
					writeb( 0x00, cfg->pCsRegisters + rfm2g_sid3);
					break;

                case RFM2GEVENT_INTR4:
					writeb( 0x00, cfg->pCsRegisters + rfm2g_initn);
					break;

                case RFM2GEVENT_BAD_DATA:
	                saved_lisr &= ~RFM2G_LISR_BADDATA;
					break;

                case RFM2GEVENT_RXFIFO_FULL:
	                saved_lisr &= ~RFM2G_LISR_RXFULL;
					break;

                case RFM2GEVENT_ROGUE_PKT:
	                saved_lisr &= ~RFM2G_LISR_ROGUE_PKT;
					break;

                case RFM2GEVENT_RXFIFO_AFULL:
	                saved_lisr &= ~RFM2G_LISR_RXAFULL;
					break;

                case RFM2GEVENT_SYNC_LOSS:
	                saved_lisr &= ~RFM2G_LISR_SNCLOST;
					break;

                case RFM2GEVENT_MEM_WRITE_INHIBITED:
	                saved_lisr &= ~RFM2G_LISR_WRINIB;
					break;

                case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:
	                saved_lisr &= ~RFM2G_LISR_PTYERR;
					break;

				default:

					/* Release lock */
					WHENDEBUG(RFM2G_DBMUTEX)
					{
						printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
					}
					spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

					WHENDEBUG(RFM2G_DBERROR)
					{
						printk(KERN_ERR"%s%d: Exiting %s: Invalid interrupt queue %d\n",
							devname, unit, me, (int)arg );
					}

					return( -EINVAL );
			}

            writel( saved_lisr, (char *)(cfg->pCsRegisters + rfm2g_lisr) );

            /* Flush this event's queue */
            qhead = &( rfm2gDeviceInfo[unit].EventQueue[arg].req_header );
            qhead->reqh_size  = 0;
            qhead->reqh_flags &= ~QUEUE_OVERFLOW;
            qhead->reqh_qpeak = 0;
            qhead->reqh_head  = 0;
            qhead->reqh_tail  = 0;
			mb();

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

			ret_status = 0;
        }
        break;

        case IOCTL_RFM2G_GET_DMA_BYTESWAP:
        {
		    RFM2G_UINT8  state = (RFM2G_UINT8) cfg->dmaByteSwap;
            char *myself = "GET_DMA_BYTESWAP";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			/* Copy the data back to the user */
			if( copy_to_user( (void *)arg, (void *)&state,
				sizeof(RFM2G_UINT8) ) > 0 )
			{
				WHENDEBUG(RFM2G_DBERROR)
				{
					printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
						devname, cfg->Unit, me );
				}

				return( -EFAULT );
			}

			ret_status = 0;
        }
        break;

        case IOCTL_RFM2G_SET_DMA_BYTE_SWAP:
        {
	        RFM2G_ADDR orbase;        /* Base address of RFM's or space    */
	        RFM2G_ADDR temp_reg32;
            char *myself = "SET_DMA_BYTE_SWAP";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

	        orbase  = (RFM2G_ADDR) cfg->pOrRegisters;

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk( KERN_ERR"%s%d: %s About to acquire spinlock\n",
                        devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

			/*
			 * Setup PLX chip to Byte Swap, This swap only works for long word (4 byte)
			 * transfers
			 */

            temp_reg32 = readl( (char *)(orbase + rfmor_endian_desc) );

			/*
			 * Turn on bit 7 in Big/Little Endian Descriptor: BIGEND, Offset $0C PLX chip
			 *
			 * Bit 7
			 *
			 * DMA Channel 0 Big Endian Mode (Address Invariance). Writing a one (1)
			 * specifies use of Big Endian data ordering for DMA Channel 0 accesses to the
			 * Local Address Space. Writing a zero (0) specifies Little Endian ordering.
			 */
			if (arg != RFM2G_FALSE)
				{
				temp_reg32 |= 0x80;
				cfg->dmaByteSwap = RFM2G_TRUE;
				}
			else
				{
				temp_reg32 &= ~0x80;
				cfg->dmaByteSwap = RFM2G_FALSE;
				}

            writel( temp_reg32, (char *)(orbase + rfmor_endian_desc) );

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

			ret_status = 0;
        }
        break;

        case IOCTL_RFM2G_GET_PIO_BYTESWAP:
        {
		    RFM2G_UINT8  state = (RFM2G_UINT8) cfg->pioByteSwap;
            char *myself = "GET_PIO_BYTESWAP";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

			/* Copy the data back to the user */
			if( copy_to_user( (void *)arg, (void *)&state,
				sizeof(RFM2G_UINT8) ) > 0 )
			{
				WHENDEBUG(RFM2G_DBERROR)
				{
					printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
						devname, cfg->Unit, me );
				}

				return( -EFAULT );
			}

			ret_status = 0;
        }
        break;

        case IOCTL_RFM2G_SET_PIO_BYTE_SWAP:
        {
	        RFM2G_ADDR orbase;        /* Base address of RFM's or space    */
	        RFM2G_UINT32 temp_reg32;
            char *myself = "SET_PIO_BYTE_SWAP";

            WHENDEBUG(RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: %s cmd = %s\n",
                    devname, unit, me, myself);
            }

	        orbase  = (RFM2G_ADDR) cfg->pOrRegisters;

            /* Acquire lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk( KERN_ERR"%s%d: %s About to acquire spinlock\n",
                        devname, unit, me);
            }
            spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

			/*
			 * Setup PLX chip to Byte Swap, This swap only works for long word (4 byte)
			 * transfers
			 */

            temp_reg32 = readl( (char *)(orbase + rfmor_endian_desc) );

			/*
			 * Turn on bit 5 in Big/Little Endian Descriptor: BIGEND, Offset $0C PLX chip
			 *
			 * Bit 5
			 *
			 * Direct Slave Address Space 1 Big Endian Mode (Address Invariance).
			 * Writing a one (1) specifies use of Big Endian data ordering for Direct
			 * Slave accesses to Local Address Space 1. Writing a zero (0) specifies
			 * Little Endian ordering.
			 */
			if (arg != RFM2G_FALSE)
				{
				temp_reg32 |= 0x20;
				cfg->pioByteSwap = RFM2G_TRUE;
				}
			else
				{
				temp_reg32 &= ~0x20;
				cfg->pioByteSwap = RFM2G_FALSE;
				}

            writel( temp_reg32, (char *)(orbase + rfmor_endian_desc) );

            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

			ret_status = 0;
        }
        break;

        default:
        {
            WHENDEBUG(RFM2G_DBERROR | RFM2G_DBIOCTL)
            {
                printk(KERN_ERR"%s%d: Exiting %s:Invalid ioctl command 0x%x 0x%zx\n",
                    devname, unit, me, cmd, IOCTL_RFM2G_GET_CONFIG );
            }

            return( -EINVAL );

        }
    }
    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s\n", devname,
                                    unit, me);

    return( ret_status );

}   /* End of rfm2g_ioctl() */

static int
get_lcsr_bit(RFM2GCONFIGLINUX *cfg, unsigned int cmd, unsigned long arg, unsigned int bit, char* pDescription )
{
    static char *me = "rfm2g_ioctl()";
    RFM2G_UINT8  state;      /* Current state of the bit          */
    RFM2G_ADDR rfm2gAddr;  /* Base address of RFM memory space  */
    RFM2G_UINT32  temp;      /* Value read from LCSR              */

    WHENDEBUG(RFM2G_DBIOCTL)
    {
        printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, cfg->Unit, me, pDescription);
    }

    /* Point at the RFM board's LCSR Register */
    rfm2gAddr = (RFM2G_ADDR) ( cfg->pCsRegisters + rfm2g_lcsr );

    /* Get current contents of LCSR */

	temp = readl( (char *)(rfm2gAddr));

    if( (temp & bit) == 0 )
    {
        state = 0;  /* bit is off */
    }
    else
    {
        state = 1;  /* bit is on */
    }

    /* Copy the data back to the user */
    if( copy_to_user( (void *)arg, (void *)&state,
        sizeof(RFM2G_UINT8) ) > 0 )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: copy_to_user() failed\n",
                devname, cfg->Unit, me );
        }

        return( -EFAULT );
    }
	return (0);
}

static int
set_lcsr_bit(RFM2GCONFIGLINUX *cfg, unsigned int cmd, unsigned long arg, unsigned int bit, char* pDescription )
{
    static char *me = "rfm2g_ioctl()";
    RFM2G_ADDR rfm2gAddr;   /* Base address of RFM memory space  */
    RFM2G_UINT32  temp;       /* Value read/written to LCSR        */
    unsigned long flags;

    WHENDEBUG(RFM2G_DBIOCTL)
    {
        printk(KERN_ERR"%s%d: %s cmd = %s\n", devname, cfg->Unit, me, pDescription);
    }

    /* Point at the RFM board's LCSR Register */
    rfm2gAddr = (RFM2G_ADDR) ( cfg->pCsRegisters + rfm2g_lcsr );

    /* Acquire lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
               devname, cfg->Unit, me);
    }
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

    /* Get current contents of CSR2 */
	temp = readl( (char *)(rfm2gAddr));

    if( arg == RFM2G_FALSE )
    {
        temp &= ~(bit);
    }
    else
    {
        temp |= bit;
    }

	writel( *( (RFM2G_UINT32 *) &temp ), (char *)(rfm2gAddr) );

    /* Release lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, cfg->Unit, me);
    }
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
    return(0);
}

static int
RFM2gDMA(RFM2GCONFIGLINUX *cfg, RFM2G_UINT32 dmaDesc, RFM2G_UINT64 pciAddress,
         RFM2G_UINT32 rfmOffset, RFM2G_UINT32 count, RFM2G_UINT32 addrAdjust)
{
    static char           *me = "RFM2gDMA()";
    RFM2G_ADDR             orbase;        /* Base address of RFM's or space */
    RFM2GEVENTINFOLINUX    event;
    RFM2GEVENTQUEUE_HEADER qheader;
    RFM2G_UINT32           temp_reg32;
    unsigned long          flags;
    RFM2G_UINT32           ChunkSize;
    RFM2G_UINT32           AmountSent;
	RFM2G_INT32            stat = 0;
    RFM2G_UINT8            unit;  /* Minor device number, used as index in rfm2gDeviceInfo */
    RFM2G_UINT32           pciAddress_Lower, pciAddress_Upper;


    /* Extract the minor number so we know which RFM device to access */
    unit = cfg->Unit;

    WHENDEBUG(RFM2G_DBREAD)
    {
        printk(KERN_ERR"%s%d: %s(using DMA). pid:%d\n", devname, unit, me, current->pid);
    }

    /* Add the base offset of the current sliding window to the rfmOffset.
       This will make the DMA start at the beginning of the sliding window. */
    rfmOffset += cfg->SlidingWindowOffset;

    /* Make sure this DMA doesn't go off the end of the rfm board */
    if ((rfmOffset + count) > cfg->MemorySize)
    {
		WHENDEBUG(RFM2G_DBERROR | RFM2G_DBDMA)
		{
			printk(KERN_ERR"%s%d: Exiting %s: Offset + SlidingWindowOffset + count > MemorySize\n",
				devname, unit, me);
		}

        return -EINVAL;
    }

    /* initialize event structure for call to WaitForInterrupt() */
    event.ExtendedInfo = 0;
    event.NodeId = 0;
    event.Event = RFM2GEVENT_DMADONE;
    event.Timeout = 8000; //INFINITE_TIMEOUT;
    event.Unit = unit;
    event.Qflags = 0;

    orbase  = (RFM2G_ADDR)cfg->pOrRegisters;
    /* Get a pointer to this interrupt queue header */
    qheader = &rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_DMADONE].req_header;

    /* See if we have to deal with the PLX errata */
    if ( (cfg->PlxRevision < 0xAD) && (count > 0x100) )
    {
        ChunkSize = 0x100;
    }
    else
    {
        ChunkSize = 0x7fff80;
    }
	stat = down_interruptible(rfm2gDeviceInfo[unit].DMASem);
	if (stat != 0)
	{
		WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
		{
            printk(KERN_ERR"%s%d: Exiting %s: down interrupted, "
				"stat %d, pid:%d\n", devname, unit, me, stat, current->pid);
		}
		return(-2);
	}
    for ( AmountSent = 0; (AmountSent < count) && (stat==0); AmountSent += ChunkSize)
    {
        /* Check for short remainder */
        if ( ChunkSize > (count - AmountSent) )
        {
            ChunkSize = count - AmountSent;
        }

        /* Acquire lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk( KERN_ERR"%s%d: %s About to acquire spinlock. pid:%d\n",
                    devname, unit, me, current->pid);
        }
        spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

        /* Stop now if this interrupt is already enabled */
        if( qheader->reqh_flags & EVENT_ENABLED )
        {
            /* Release lock */
            WHENDEBUG(RFM2G_DBMUTEX)
            {
                printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
            }
            spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

            WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: Interrupt already enabled, "
                    "event %d\n", devname, unit, me, event.Event);
            }
			up(rfm2gDeviceInfo[unit].DMASem);
            return(-2);
        }

        /* Set PCI Address */
        pciAddress_Lower = (RFM2G_UINT32)((pciAddress + addrAdjust + AmountSent) & 0xFFFFFFFF);
        pciAddress_Upper = (RFM2G_UINT32)((pciAddress + addrAdjust + AmountSent) >> 32);
        writel( pciAddress_Lower, (char *)(orbase + rfmor_DmaPciAddr0) );
        writel( pciAddress_Upper, (char *)(orbase + rfmor_DmaDac0) );

        /* PCI registers */
        /* Setup DMA Mode register */
        writel( (RFMOR_DMAMODE_32BIT_BUS |
                 RFMOR_DMAMODE_TA_READY  |
                 RFMOR_DMAMODE_BTERM |
                 RFMOR_DMAMODE_LOCAL_BURST |
                 RFMOR_DMAMODE_DONE_INT |
                 RFMOR_DMAMODE_INT_TO_PCI), (char *)(orbase + rfmor_DmaMode0) );

        /* Set Transfer Byte Count */
        writel( ChunkSize, (char *)(orbase + rfmor_DmaCount0) );

        /* Set initiator address */
        writel( (rfmOffset + AmountSent), (char *)(orbase + rfmor_DmaLocAddr0) );

        /* Enable from RFM to PC or PC to RFM based on value in dmaDesc */
        writel( dmaDesc, (char *)(orbase + rfmor_DmaPtr0) );

        /* Enable DMA Done Interrupt */

        /* Now enable the interrupt */
        temp_reg32 = readl( (char *)(orbase + rfmor_intcsr) );
        temp_reg32 |= RFMOR_INTCSR_ENABLEDMA0INT;
        temp_reg32 |= RFMOR_INTCSR_ENABLEINT;
        writel( temp_reg32, (char *)(orbase + rfmor_intcsr) );

        /* flush PCI write fifo */
        readl( (char*)(orbase + rfmor_intcsr));

        qheader->reqh_flags |= EVENT_ENABLED;
        (cfg->IntrCount)++;

		wmb();

        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: %s Enabled interrupt event %d. pid:%d\n",
                devname, unit, me, event.Event, current->pid );
        }

	  spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

        /* Do DMA and Wait here until interrupt occurs */
        stat = DoDMAWithWaitForInt( &event, flags );

        /* Acquire lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk( KERN_ERR"%s%d: %s About to acquire spinlock. pid:%d\n",
                    devname, unit, me, current->pid);
        }
        spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

        qheader->reqh_flags = 0;
		(cfg->IntrCount)--;

        /* Release lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        }
        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
		if(stat !=0 )
		{
			WHENDEBUG(RFM2G_DBMUTEX)
			{
                printk(KERN_ERR"%s%d: %s Aborted. pid:%d\n", devname, unit, me, current->pid);
			}
			up(rfm2gDeviceInfo[unit].DMASem);
			return( -EINTR);
		}
    }
	up(rfm2gDeviceInfo[unit].DMASem);
	return (count);
}


static int
IsReadyForDma(RFM2G_UINT8 unit, char *buf, size_t count, RFM2GDMAINFO *DmaInfo, RFM2G_UINT32 *addrAdjust)
{
    static char *me = "IsReadyForDma()";
#ifdef __x86_64__
    RFM2G_UINT32 virtaddr = (RFM2G_UINT32)((RFM2G_UINT64)buf);
#else
    RFM2G_UINT32 virtaddr = (RFM2G_UINT32)buf;
#endif

    WHENDEBUG(RFM2G_DBDMA)
    {
        printk(KERN_ERR"%s%d: %s virtaddr 0x%08X, count %d. pid:%d\n", devname, unit, me, virtaddr, (int) count, current->pid);
    }

    /* Don't DMA if there hasn't been a DMA buffer preallocated with mem=xxxM */
    if (DmaInfo->BuffAddr == 0)
    {
        WHENDEBUG(RFM2G_DBDMA)
        {
            printk(KERN_ERR"%s%d: %s Don't DMA because there is no preallocated DMA buffer.. pid:%d\n", devname, unit, me, current->pid);
        }
        return 0;
    }

    /* Don't DMA if the data size is below the DMA threshold */
    if (count < DmaInfo->Threshold)
    {
        WHENDEBUG(RFM2G_DBDMA)
        {
            printk(KERN_ERR"%s%d: %s Don't DMA because the data size is below the DMA threshold.. pid:%d\n", devname, unit, me, current->pid);
        }
        return 0;
    }

    /* Make sure the virtaddr is not outside of the mmap'ed region used for DMA */
    *addrAdjust = virtaddr - DmaInfo->VirtAddr;
    if (*addrAdjust > DmaInfo->BuffSize)
    {
        *addrAdjust = 0;
        WHENDEBUG(RFM2G_DBDMA)
        {
            printk(KERN_ERR"%s%d: %s Don't DMA because the virtual buffer address is outside of the mmap'ed region used for DMA.. pid:%d\n", devname, unit, me, current->pid);
        }
        return 0;
    }

    /* Make sure the amount of data doesn't exceed the mmap'ed region used for DMA */
    if (count > (DmaInfo->BuffSize - *addrAdjust))
    {
        *addrAdjust = 0;
        WHENDEBUG(RFM2G_DBDMA)
        {
            printk(KERN_ERR"%s%d: %s Don't DMA because the amount of data exceeds the mmap'ed region used for DMA. pid:%d\n", devname, unit, me, current->pid);
        }
        return 0;
    }

    WHENDEBUG(RFM2G_DBDMA)
    {
        printk(KERN_ERR"%s%d: %s Okay to DMA.. pid:%d\n", devname, unit, me, current->pid);
    }

    /* Perform DMA */
    return 1;
}


