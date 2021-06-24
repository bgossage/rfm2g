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
$Workfile: device_init.c $
$Revision: 40643 $
$Modtime: 9/03/10 1:16p $
-------------------------------------------------------------------------------
    Description: Initialize the PCI Reflective Memory board and related data
        structures
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    18-OCT-2001  ML   1.0   Created.
    08-DEC-2004  ML   01.04 Linux 2.6 support Remove pcibios_ functios with
                            newer pci_ equivalent functions.
    20 Jun 2008  EB   6.00  Added mmapSem semaphore.
                            This change is in support of PAE (Physical Address
                            Extension)-enabled Linux.
    06 Aug 2008  EB   6.04  Init the new SlidingWindowOffset member of the
                            RFM2GCONFIGLINUX structure.

===============================================================================
*/

#include "rfm2g_driver.h"

/* The first board revision that has support for Sliding Memory
   Windows in the firmware */
#define FIRST_BOARD_REV_WITH_SLIDING_WINDOWS ((RFM2G_UINT8)0x8C)


/*** Local prototypes ********************************************************/

#ifdef __cplusplus
extern "C" {
#endif

RFM2G_INT8 GetPciSpaceSize( struct pci_dev *dev,
        RFM2G_UINT8 offset, spinlock_t* spinlock, RFM2G_UINT32 *size );

#ifdef __cplusplus
}
#endif


/******************************************************************************
*
*  FUNCTION:   rfm2gInitDevice
*
*  PURPOSE:    Collect and store all pertinent info about an RFM board.
*  PARAMETERS: pDeviceInfo: (IO) Pointer to an element of the rfmDeviceInfo
*                  array that will soon contain data about an RFM board.
*              *dev: (O) Points to structure containing PCI info about
*                  RFM board
               inst: (I) The instance of this board 0, 1, 2...
*  RETURNS:    0: Success
*              -ENOMOM: Unable to remap RFM memory into kernel space
*              Negative number: Error
*  GLOBALS:    devname
*
******************************************************************************/

RFM2G_INT32
rfm2gInitDevice( RFM2GDEVICEINFO *pDeviceInfo, struct pci_dev *dev, RFM2G_UINT8 inst )
{
    static char        *me = "rfm2gInitDevice()";
    char                cInst[4];
    RFM2G_INT8          result;       /* Return values of pci access functions */
    RFM2G_UINT16        device;       /* Temp for device value                 */
    RFM2G_UINT8         interrupt;    /* Temp for interrupt value              */
    RFM2G_UINT32        devfn;        /* Temp for devfn value                  */
    RFM2G_UINT32        addr;         /* Temp for addr value                   */
    unsigned long       IntFlags = 0; /* Used to save status of interrupts     */
    RFM2G_UINT32        size;         /* Temp for size value                   */
    RFM2G_UINT8         bus;          /* Temp for interrupt value              */
    RFM2G_UINT8         boardId;      /* Temp for RFM board ID                 */
    RFM2G_UINT32        csr;          /* Temp for an RFM board LCSR* register  */
    RFM2GCONFIGLINUX   *cfg;          /* Local pointer to config structure     */
	int                 stat;
	RFM2G_UINT8         devicerev;
    RFM2G_UINT32        eeprom_cntrl;
    RFM2G_UINT32        flush32;
    RFM2G_ADDR        BaseAddr = 0;
    RFM2G_ADDR        MappedAddr = 0;
    unsigned long       flags;
    RFM2GEVENTINFOLINUX event;
    int                 i;
    WHENDEBUG(RFM2G_DBTRACE)printk(KERN_ERR"%s?: Entering %s\n", devname, me);

    if( pDeviceInfo == (RFM2GDEVICEINFO *) NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s?:%s First parameter is NULL pointer.\n",
                devname, me );
        }

        return( -1 );
    }

    if( dev == (struct pci_dev *) NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s Second parameter is NULL pointer.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( -1 );
    }
    stat = pci_enable_device(dev);
	if (stat < 0 )
    {
		return stat;
	}
    cfg = &pDeviceInfo->Config;
    cfg->Size = sizeof(RFM2GCONFIGLINUX);
    cfg->SpinLock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
    if( cfg->SpinLock == NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s Unable to allocate SpinLock.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( -1 );
    }
    spin_lock_init((spinlock_t*)cfg->SpinLock);

    pDeviceInfo->DMASem = kmalloc(sizeof(struct semaphore),GFP_KERNEL);
    if( pDeviceInfo->DMASem == NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s Unable to allocate DMASem.\n",
                devname, pDeviceInfo->unit, me );
	    kfree(cfg->SpinLock);
        }

        return( -1 );
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
    init_MUTEX(pDeviceInfo->DMASem);
#else
   sema_init(pDeviceInfo->DMASem, 1);
#endif

    pDeviceInfo->mmapSem = kmalloc(sizeof(struct semaphore),GFP_KERNEL);
    if( pDeviceInfo->mmapSem == NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s Unable to allocate mmapSem.\n",
                devname, pDeviceInfo->unit, me );
	    kfree(cfg->SpinLock);
        }

        return( -1 );
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
    init_MUTEX(pDeviceInfo->mmapSem);
#else
    sema_init(pDeviceInfo->mmapSem, 1);
#endif

    /* Get the PCI bus, function, & devfn numbers, and other easy stuff */
    cfg->PCI.bus = bus = dev->bus->number;
    cfg->PCI.devfn = devfn = dev->devfn;
    cfg->PCI.function = PCI_FUNC( dev->devfn );
    cfg->RegisterSize = 0;
    cfg->MaxNodeId = RFM2G_NODE_MAX;
    cfg->IntrCount = 0;

    cfg->Unit = inst;
	cfg->dmaByteSwap = RFM2G_FALSE;
	cfg->pioByteSwap = RFM2G_FALSE;

    /* save the device name for this board */
    sprintf(cInst, "%d", inst);
    strcpy(cfg->Device, "/dev/");
    strcat(cfg->Device, devname);
/* ~~rem 05-16-2007 Added condition to fix DEVFS name */
#ifdef CONFIG_DEVFS_FS
    strcat(cfg->Device, "/");
#endif
    strcat(cfg->Device, cInst);

printk( "%s%d:%s Device = %s\n", devname, pDeviceInfo->unit, me, cfg->Device );

    /* Get the device ID, e.g. 0x5565 */
    result = pci_read_config_word( dev, PCI_DEVICE_ID, &device );

    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s PCI Device read error.\n", devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.type = device;

    /* Get the device revision */
    result = pci_read_config_byte( dev, PCI_REVISION_ID, &devicerev );

    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s PCI Device read error.\n", devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
	cfg->PCI.revision = devicerev;

	/* Get the interrupt number */
    interrupt = dev->irq;

    cfg->PCI.interruptNumber = interrupt;
    WHENDEBUG(RFM2G_DBTRACE)
    {
    	printk( KERN_ERR "%s%d:%s PCI irq # %d.\n", devname, pDeviceInfo->unit, me, cfg->PCI.interruptNumber );
    }

//printk( "%s%d:%s RFM2G_DBTRACE=%d, rfm2gDebugFlags=0x%x (%d)\n",
//    devname, pDeviceInfo->unit, me, RFM2G_DBTRACE, rfm2gDebugFlags, rfm2gDebugFlags );

    /* Get the PCI physical address for the local, runtime and DMA Registers
        on the RFM board (rfmor) */
    result = pci_read_config_dword( dev, PCI_BASE_ADDRESS_0, &addr );

    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s%d:%s PCI Config Mem Address read error.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.rfm2gOrBase = addr & PCI_BASE_ADDRESS_MEM_MASK;

    /* Get the size of the rfmor register area */
    result = GetPciSpaceSize( dev, PCI_BASE_ADDRESS_0,
                (spinlock_t*)(cfg->SpinLock), &size );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s%d:%s Unable to get size of PCI memory space.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.rfm2gOrWindowSize = size;
	cfg->PCI.rfm2gOrRegSize = size;

    /* Get the PCI physical address for the RFM Control and Status registers
        on the RFM board */
    result = pci_read_config_dword( dev, PCI_BASE_ADDRESS_2, &addr );

    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s%d:%s PCI Config Mem Address read error.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.rfm2gCsBase = addr & PCI_BASE_ADDRESS_MEM_MASK;

    /* Get the size of the rfmor register area */
    result = GetPciSpaceSize( dev, PCI_BASE_ADDRESS_2,
                (spinlock_t*)(cfg->SpinLock), &size );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s%d:%s Unable to get size of PCI memory space.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.rfm2gCsWindowSize = size;
    cfg->PCI.rfm2gCsRegSize = size;

    /* Get the PCI physical address for the memory on the RFM board */
    result = pci_read_config_dword( dev, PCI_BASE_ADDRESS_3, &addr );

    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s%d:%s PCI Config Mem Address read error.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.rfm2gBase = addr & PCI_BASE_ADDRESS_MEM_MASK;

    /* Get the size of the rfmor register area */
    result = GetPciSpaceSize( dev, PCI_BASE_ADDRESS_3,
                (spinlock_t*)(cfg->SpinLock), &size );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s%d:%s Unable to get size of PCI memory space.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( result );
    }
    cfg->PCI.rfm2gWindowSize = size;

    BaseAddr = cfg->PCI.rfm2gCsBase;
    if( BaseAddr != 0 )
    {
        MappedAddr = (RFM2G_ADDR ) ioremap_nocache( BaseAddr,
            cfg->PCI.rfm2gCsWindowSize );
        if( MappedAddr == 0 )
        {
            /* Error */
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: Unable to map RFM "
                   "CS register space\n", devname, pDeviceInfo->unit, me );
            }
            return( -ENOMEM );
        }
        cfg->pCsRegisters = (RFM2G_UINT8 *) MappedAddr;
    }

    BaseAddr = cfg->PCI.rfm2gOrBase;
    if( BaseAddr != 0 )
    {
        MappedAddr = (RFM2G_ADDR) ioremap_nocache( BaseAddr,
            cfg->PCI.rfm2gOrWindowSize );
        if( MappedAddr == 0 )
        {
            /* Error */
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: Unable to map RFM "
                   "OR register space\n", devname, pDeviceInfo->unit, me );
            }
            return( -ENOMEM );
        }
        cfg->pOrRegisters = (RFM2G_UINT8 *) MappedAddr;
    }

    /* Read this RFM's board ID */
    cfg->RevisionId = readb( cfg->pCsRegisters + rfm2g_brv );

    /* Read this RFM's build ID */
    cfg->BuildId = readw( cfg->pCsRegisters + rfm2g_build_id );

    /* Read this RFM's board ID */
    cfg->BoardId = boardId = readb( cfg->pCsRegisters + rfm2g_bid );

    /* Read this RFM's node ID */
    cfg->NodeId = readb( cfg->pCsRegisters + rfm2g_nid );

    /* Find out how much memory is on this RFM board */
    switch( cfg->BoardId )
    {
        case RFM2G_BID_5565:
            csr = readl( cfg->pCsRegisters + rfm2g_lcsr );
            csr &= RFM2G_LCSR_CFG;
            break;

        default:
            WHENDEBUG(RFM2G_DBERROR)
            {
                printk( KERN_ERR"%s%d:%s Unknown board ID \"0x%02X\".\n",
                    devname, pDeviceInfo->unit, me, cfg->BoardId );
            }
            return( -1 );
            break;
    }
    switch( csr )
    {
        case 0:
            cfg->MemorySize = 64*1024*1024;
            break;

       case (1<<20):
            cfg->MemorySize = 128*1024*1024;
            break;

       case (2<<20):
            cfg->MemorySize = 256*1024*1024;
            break;
        default:
            cfg->MemorySize = 0;

            WHENDEBUG(RFM2G_DBERROR)
            {
                printk( KERN_ERR"%s%d:%s Invalid Memory Size code\"0x%X\".\n",
                    devname, pDeviceInfo->unit, me, (csr>>20) );
            }
            return( -1 );
            break;
    }

    cfg->UserMemorySize = cfg->MemorySize - cfg->RegisterSize;
    /* Determine this board's Capabilities */
    switch( cfg->BoardId )
    {
        case RFM2G_BID_5565:
            cfg->Capabilities = RFM2G_SUPPORTS_DMA                |
                                RFM2G_HAS_EXTENDED_ADDRESSING;
            break;
    }

    /* Set the default sliding window offset if this feature is supported */
    if (cfg->RevisionId >= FIRST_BOARD_REV_WITH_SLIDING_WINDOWS)
    {
        /* Read the current sliding window offset from the register */
        cfg->SlidingWindowOffset = readl((char *)(cfg->pOrRegisters + rfmor_las1ba));

        /* Remove bits that aren't part of the offset */
        cfg->SlidingWindowOffset &= ~RFMOR_LAS1BA_ENABLE_LAS1;
    }
    else
    {
        cfg->SlidingWindowOffset = 0;
    }

    /* Lock register accesses */
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), IntFlags );

    /* disable all interrupts */
    writel( 0x00000000, cfg->pCsRegisters + rfm2g_lier );

    /* Clear pending events */
    writeb( 0x00, cfg->pCsRegisters + rfm2g_sid1);
    writeb( 0x00, cfg->pCsRegisters + rfm2g_sid2);
    writeb( 0x00, cfg->pCsRegisters + rfm2g_sid3);
    writeb( 0x00, cfg->pCsRegisters + rfm2g_initn);
    /*
     * Read the local interrupt status register, this is
     * necessary on the PIORC because the LISR does not clear
     * bits that have not been read already set.
     */
    flush32 = readl( (char *)(cfg->pCsRegisters + rfm2g_lisr) );
    /* Now enable the global interrupt enable in rfm2g_lisr */
    /* Also clears any pending interrupt flags */
    writel( RFM2G_LISR_UNUSED, cfg->pCsRegisters + rfm2g_lisr );

    /* Turn off the Fail LED */
    csr = readl(cfg->pCsRegisters + rfm2g_lcsr );
    csr &= ~(RFM2G_LCSR_LED);     /* Turn LED off */
    writel( csr, cfg->pCsRegisters + rfm2g_lcsr );

    /* Unlock register accesses. */
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), IntFlags );

    /* Save off the PLX revision value */
    cfg->PlxRevision = readb( cfg->pOrRegisters + rfmor_revision);

    /* Enable Memory Read Multiple command for DMA */
    eeprom_cntrl = readl( (char *)(cfg->pOrRegisters + rfmor_cntrl) );
    eeprom_cntrl = (eeprom_cntrl & ~RFM2GOR_CNTRL_READCMD_MASK) | RFM2GOR_CNTRL_MRMUL_CMD;
    writel( eeprom_cntrl, (cfg->pOrRegisters + rfmor_cntrl) );
    flush32 = readl( (char *)(cfg->pOrRegisters + rfmor_cntrl) ); /* flush cache */

    WHENDEBUG(RFM2G_DBTRACE)
    {
        printk(KERN_ERR"%s%d: %s Enabling interrupts, mapping mem & "
            "I/O space\n", devname, pDeviceInfo->unit, me );
    }


    /* Acquire lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
               devname, pDeviceInfo->unit, me);
    }
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

    /* Enable interrupts */
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
    if(request_irq( cfg->PCI.interruptNumber, rfm2g_interrupt,SA_SHIRQ,
           	"PCI RFM2g", (void *)(pDeviceInfo) ) )
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
    if(request_irq( cfg->PCI.interruptNumber, rfm2g_interrupt,IRQF_SHARED,
           	"PCI RFM2g", (void *)(pDeviceInfo) ) )
#else
    if(request_irq( cfg->PCI.interruptNumber, (irq_handler_t)rfm2g_interrupt,IRQF_SHARED,
           	"PCI RFM2g", (void *)(pDeviceInfo) ) )
#endif
	{
        printk(KERN_ERR"%s%d: %s Failed: Installing handler for PCI interrupt %d\n",
                	devname, pDeviceInfo->unit, me, cfg->PCI.interruptNumber );
            return( -EIO );

	}
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );
    WHENDEBUG(RFM2G_DBINTR)
    {
        printk(KERN_ERR"%s%d: %s Installed handler for PCI interrupt %d\n",
                	devname, pDeviceInfo->unit, me, cfg->PCI.interruptNumber );
    }

    /* Now enable the global interrupt enable in rfm2g_lisr */
    writel( RFM2G_LISR_UNUSED, (cfg->pCsRegisters + rfm2g_lisr) );

    for(i=0; i<LINUX_RFM2G_NUM_OF_EVENTS; i++)
    {
        event.Unit = pDeviceInfo->unit;
        event.Event = i;

        /* Make sure the interrupt is supported on this board */
        switch( event.Event )
        {
            case RFM2GEVENT_DMADONE:    /* Fall thru */
                /* These interrupts are enabled during DMA operation */
                continue;
                break;

            case RFM2GEVENT_RESET:                 /* Fall thru */
            case RFM2GEVENT_INTR1:                 /* Fall thru */
            case RFM2GEVENT_INTR2:                 /* Fall thru */
            case RFM2GEVENT_INTR3:                 /* Fall thru */
            case RFM2GEVENT_INTR4:                 /* Fall thru */
				continue; /* Leave these events disabled */
				break;

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
                /* Release lock */
                WHENDEBUG(RFM2G_DBMUTEX)
                {
                    printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, pDeviceInfo->unit, me);
                }
                spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR | RFM2G_DBOPEN)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: Event %d is "
                        "invalid\n", devname, pDeviceInfo->unit, me, event.Event );
                }
                return( -ENOMEM );
                break;
        }


        /* Enable the interrupt */
        switch( EnableInterrupt(&event, RFM2G_TRUE) )
        {
            case 0:
                /* The interrupt was successfully enabled */
                break;

            case -1:
                /* This interrupt has already been enabled */
                /* Release lock */
                WHENDEBUG(RFM2G_DBMUTEX)
                {
                    printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, pDeviceInfo->unit, me);
                }
                spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: EnableInterrupt() "
                        "failed\n", devname, pDeviceInfo->unit, me );
                }
                return( -EACCES );
                break;

            default:
                /* Some other error occured */
                /* Release lock */
                WHENDEBUG(RFM2G_DBMUTEX)
                {
                    printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, pDeviceInfo->unit, me);
                }
                spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

                WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
                {
                    printk(KERN_ERR"%s%d: Exiting %s: EnableInterrupt() "
                        "failed\n", devname, pDeviceInfo->unit, me );
                }
                return( -EFAULT );
                break;
        }
    }

    /* Release lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, pDeviceInfo->unit, me);
    }
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

    WHENDEBUG(RFM2G_DBTRACE)printk(KERN_ERR"%s%d: Exiting %s\n", devname, pDeviceInfo->unit, me);
    return(0);

}   /* End of rfm2gInitDevice() */

/******************************************************************************
*
*  FUNCTION:   rfm2gSilenceDevice
*
*  PURPOSE:    Silence the device, we are done.
*  PARAMETERS: pDeviceInfo: (IO) Pointer to an element of the rfmDeviceInfo
*                  array that will soon contain data about an RFM board.
*              *dev: (O) Points to structure containing PCI info about
*                  RFM board
               inst: (I) The instance of this board 0, 1, 2...
*  RETURNS:    0: Success
*              -ENOMOM: Unable to remap RFM memory into kernel space
*              Negative number: Error
*  GLOBALS:    devname
*
******************************************************************************/

RFM2G_INT32
rfm2gSilenceDevice( RFM2GDEVICEINFO *pDeviceInfo, struct pci_dev *dev, RFM2G_UINT8 inst )
{
    static char        *me = "rfm2gInitDevice()";
    RFM2GCONFIGLINUX   *cfg;          /* Local pointer to config structure     */
    unsigned long       flags;
    RFM2G_UINT32        temp;

    WHENDEBUG(RFM2G_DBTRACE)printk(KERN_ERR"%s?: Entering %s\n", devname, me);

    if( pDeviceInfo == (RFM2GDEVICEINFO *) NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s?:%s First parameter is NULL pointer.\n",
                devname, me );
        }

        return( -1 );
    }

    if( dev == (struct pci_dev *) NULL )
    {
        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR "%s%d:%s Second parameter is NULL pointer.\n",
                devname, pDeviceInfo->unit, me );
        }

        return( -1 );
    }
    cfg = &pDeviceInfo->Config;
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );
    WHENDEBUG(RFM2G_DBINTR)
    {
        printk(KERN_ERR"%s%d: %s Installed handler for PCI interrupt %d\n",
                	devname, pDeviceInfo->unit, me, cfg->PCI.interruptNumber );
    }

    /* disable all interrupts from FPGA */
    writel( 0x00000000, cfg->pCsRegisters + rfm2g_lier );

    /* Prevent device from generating interrupts */
    temp = readl( (char *)(cfg->pOrRegisters + rfmor_intcsr) );
    temp &= ~(RFMOR_INTCSR_ENABLEINT | RFMOR_INTCSR_ENABLELOCINT | RFMOR_INTCSR_ENABLEDMA0INT);
    writel( temp, (char *)(cfg->pOrRegisters + rfmor_intcsr) );

    /* Now disable the global interrupt enable in rfm2g_lisr */
    writel( RFM2G_LISR_DISABLE, (cfg->pCsRegisters + rfm2g_lisr) );
	WHENDEBUG(RFM2G_DBCLOSE)
	{
        printk(KERN_ERR"%s%d: %s Instance %d  Flags = 0x%08X\n",
        	devname, pDeviceInfo->unit, me, pDeviceInfo->Instance,
        	pDeviceInfo->Flags );
    }

    /* Release lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, pDeviceInfo->unit, me);
    }
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
    free_irq( cfg->PCI.interruptNumber, (void *)(pDeviceInfo) );
    WHENDEBUG(RFM2G_DBINTR)
    {
        printk(KERN_ERR"%s%d: %s Removed handler for PCI interrupt %d\n",
                	devname, pDeviceInfo->unit, me, cfg->PCI.interruptNumber );
    }

    if( cfg->pCsRegisters != (RFM2G_UINT8 *) NULL )
    {
        iounmap( (void *) cfg->pCsRegisters );
        cfg->pCsRegisters = (RFM2G_UINT8 *) NULL;
    }

    if( cfg->pOrRegisters != (RFM2G_UINT8 *) NULL )
    {
        iounmap( (void *) cfg->pOrRegisters );
        cfg->pOrRegisters = (RFM2G_UINT8 *) NULL;
    }

    return(0);

}   /* End of rfm2gInitDevice() */

/******************************************************************************
*
*  FUNCTION:   GetPciSpaceSize
*
*  PURPOSE:    Reads PCI configuration registers to determine the size of the
*              PCI I/O space or memory space window.
*  PARAMETERS: dev: pci_dev pointer
	       OLD replaced by *dev bus:    (I) PCI bus number
*              OLD replaced by *dev devfn:  (I) combined device and function numbers
*              offset: (I) which Base Address Register to access
*              spinlock: (I) Spinlock to grab before messing with board regs.
*              *size:  (O) size of indicated PCI address window
*  RETURNS:    PCIBIOS_SUCCESSFUL upon success, or negative value if error
*  GLOBALS:    devname
*
******************************************************************************/

RFM2G_INT8
GetPciSpaceSize( struct pci_dev *dev, RFM2G_UINT8 offset,
    spinlock_t *spinlock, RFM2G_UINT32 *size )
{
    static char  *me = "GetPciSpaceSize()";
    RFM2G_UINT32  addr = 0;             /* Temp to hold address data */
    RFM2G_UINT32  tsize = 0;            /* Temp to hold size data */
    unsigned long IntFlags = 0;         /* Stores states of interrupts */
    char          result;               /* Results of the pcibios*() calls */
    RFM2G_BOOL    iospace = RFM2G_FALSE; /* Set RFM2G_TRUE if addr is an I/O address */


    WHENDEBUG(RFM2G_DBTRACE)printk(KERN_ERR"%s: Entering %s\n", devname, me);

    /* Validate size parameter */
    if( size == (RFM2G_UINT32 *) NULL )
    {
        return( PCIBIOS_BAD_REGISTER_NUMBER );
    }

    *size = 0;

    /* Disable interrupts and lock registers while we do this */
    spin_lock_irqsave( spinlock, IntFlags );

    /* Read the base address register */
    result = pci_read_config_dword(dev, offset, &addr );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        /* Re-enable interrupts and free register lock */
        spin_unlock_irqrestore( spinlock, IntFlags );

        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s:%s Error on 1st read of Base Address Register.\n",
                devname, me );
        }

        return( result );
    }

    /* Determine if this is I/O space or memory space */
    if( addr & PCI_BASE_ADDRESS_SPACE )
    {
        iospace = RFM2G_TRUE;
    }
    else
    {
        iospace = RFM2G_FALSE;
    }

    /* Write all ones to the base address register */
    result = pci_write_config_dword( dev, offset, ~0 );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        /* Re-enable interrupts and free register lock */
        spin_unlock_irqrestore( spinlock, IntFlags );

        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s:%s Error on 1st write to Base Address Register.\n",
                devname, me );
        }

        return( result );
    }

    /* Now read the base address register again. */
    result = pci_read_config_dword( dev, offset, &tsize );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        /* Re-enable interrupts and free register lock */
        spin_unlock_irqrestore( spinlock, IntFlags );

        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s:%s Error on 2nd read of Base Address Register.\n",
                devname, me );
        }

        return( result );
    }

    /* Restore the original contents of the base address register */
    result = pci_write_config_dword( dev, offset, addr );
    if( result != PCIBIOS_SUCCESSFUL )
    {
        /* Re-enable interrupts and free register lock */
        spin_unlock_irqrestore( spinlock, IntFlags );

        WHENDEBUG(RFM2G_DBERROR)
        {
            printk( KERN_ERR"%s:%s Error on 2nd write to Base Address Register.\n",
                devname, me );
        }

        return( result );
    }

    /* Re-enable interrupts and free register lock */
    spin_unlock_irqrestore( spinlock, IntFlags );

    /* Finally, we can calculate the size of the memory window */
    if( iospace == RFM2G_TRUE )
    {
        tsize &= PCI_BASE_ADDRESS_IO_MASK;
    }
    else
    {
        tsize &= PCI_BASE_ADDRESS_MEM_MASK;
    }
    *size = ~tsize + 1;

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s: Exiting %s\n", devname, me);

    return( PCIBIOS_SUCCESSFUL );

}   /* End of GetPciSpaceSize() */
