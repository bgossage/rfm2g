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
$Workfile: interrupts.c $
$Revision: 41411 $
$Modtime: 9/03/10 1:16p $
-------------------------------------------------------------------------------
    Description: Support for handling received interrupts
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    18-OCT-2001  ML   1.0   Created.
    16-DEC-2005  BZ   debug Added some additional debug in rfm2g_interrupt()

===============================================================================
*/
#include "rfm2g_driver.h"

/*** Local prototypes ********************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void nonDmaInterrupts( RFM2G_UINT8 unit, RFM2G_UINT8 int_type );

#ifdef __cplusplus
}
#endif


/******************************************************************************
*
*  FUNCTION:   InitEventQueues
*
*  PURPOSE:    Initialize event queues to be empty and max sized.
*  PARAMETERS: unit: (I) Which rfm2gDeviceInfo event queue to initialize
*              size: (I) Maximum possible queue size
*  RETURNS:    void
*  GLOBALS:    rfm2gDeviceInfo, rfm2g_device_count, devname
*
******************************************************************************/

void
InitEventQueues( RFM2G_UINT8 unit, RFM2G_UINT8 size )
{
    static char *me = "InitEventQueues()";
    rfm2gEventQueue_t *queue;
    int i;


    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                    unit, me);

    WHENDEBUG(RFM2G_DBINTR)
    {
        printk(KERN_ERR"%s%d: %s Initializing event queues to %d elements\n",
            devname, unit, me, size );
    }

    /* Initialize each queue */
    for( i=0; i<LINUX_RFM2G_NUM_OF_EVENTS; i++ )
    {
        queue = &( rfm2gDeviceInfo[unit].EventQueue[i] );
        queue->req_header.reqh_head     = 0;
        queue->req_header.reqh_tail     = 0;
        queue->req_header.reqh_size     = 0;
        queue->req_header.reqh_counter  = 0;
        queue->req_header.reqh_maxSize  = 0;
        queue->req_header.reqh_qpeak    = 0;
        queue->req_header.reqh_flags    = 0;
        init_waitqueue_head(&(queue->req_header.reqh_wait));
    }

   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_RESET].req_header.reqh_maxSize=
        1;

    /* The four network interrupts are queued */
    rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_INTR1].req_header.reqh_maxSize =
        size;
    rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_INTR2].req_header.reqh_maxSize =
        size;
    rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_INTR3].req_header.reqh_maxSize =
        size;
    rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_INTR4].req_header.reqh_maxSize =
        size;

    /* The BAD_DATA and FIFO_AF interrupts are queued to a depth of 1 */
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_BAD_DATA].req_header.reqh_maxSize=
        1;
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_RXFIFO_FULL].req_header.reqh_maxSize=
        1;
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_ROGUE_PKT].req_header.reqh_maxSize=
        1;
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_RXFIFO_AFULL].req_header.reqh_maxSize=
        1;
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_SYNC_LOSS].req_header.reqh_maxSize=
        1;
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_MEM_WRITE_INHIBITED].req_header.reqh_maxSize=
        1;
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_LOCAL_MEM_PARITY_ERR].req_header.reqh_maxSize=
        1;

       /* The DMA Done interrupt is queued to a depth of 1 */
   rfm2gDeviceInfo[unit].EventQueue[RFM2GEVENT_DMADONE].req_header.reqh_maxSize=
        1;

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s\n", devname,
                                    unit, me);

}   /* End of InitEventQueues() */


/******************************************************************************
*
*  FUNCTION:   rfm2g_interrupt
*
*  PURPOSE:    Main handler for interrupts generated by the RFM board
*  PARAMETERS: irq: (I) Interrupt number, unused.
*              dev_id: (I) A pointer to the base address of rfmDeviceInfo[]
*              regs: (I) Processors context, unused.
*  RETURNS:    None
*  GLOBALS:    rfm2geviceInfo, rfm2g_device_count
*
******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
void
rfm2g_interrupt( int irq, void *dev_id, struct pt_regs *regs )
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
int
rfm2g_interrupt( int irq, void *dev_id, struct pt_regs *regs )
#else
int
rfm2g_interrupt( int irq, void *dev_id )

#endif
{
    static char *me = "rfm2g_interrupt()";
    RFM2GDEVICEINFO   *devinfo;            /* This RFM's device info        */
    RFM2GCONFIGLINUX  *cfg;                /* This RFM's config struct      */
    rfm2gEventQueue_t *queue;              /* This RFM's interrupt queue    */
    rfm2gEventQueueHeader_t *qheader;
    register RFM2G_UINT32 saved_intcsr;    /* Local copy of INTCSR contents */
    RFM2G_UINT8 * csbase;             /* Base address of RFM's memory space */
    RFM2G_UINT8 * orbase;               /* Base address of RFM's I/O space  */
    unsigned long flags = 0;
    RFM2G_UINT32  saved_lisr, saved_lier;  /* Local copy of ISR contents    */
    RFM2G_INT32  unit;                     /* Which RFM board to access     */
    int handled=0;
    devinfo = (RFM2GDEVICEINFO*)dev_id;
    cfg     = &(devinfo->Config);
    csbase = (RFM2G_UINT8 *)cfg->pCsRegisters;
    orbase  = (RFM2G_UINT8 *)cfg->pOrRegisters;
    unit = cfg->Unit;
    WHENDEBUG(RFM2G_DBTRACE | RFM2G_DBINTR | RFM2G_DBINTR_NOT)
    {
        printk(KERN_CRIT"%s%d: Entering %s\n", devname, unit, me);
    }

    /* Check board to see if an interrupt was asserted */

    /* Acquire lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
       printk(KERN_ERR"%s%d: %s About to acquire spinlock\n",
              devname, unit, me);
    }
    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );
    do
    {
        /* Make sure we are open */
        if ((orbase == (RFM2G_UINT8 *)0) || (csbase == (RFM2G_UINT8 *)0))
        {
            WHENDEBUG(RFM2G_DBINTR)
            {
                printk( KERN_CRIT"%s%d: %s not open, skipping int.\n", devname,
                       unit, me );
            }
            /* This board not open yet */
            break;
        }
        /* Read the INTCSR and save the contents */
        saved_intcsr = readl( (char *)(orbase + rfmor_intcsr) );

        /* Added following for additional debug per Matt. BZ */
        /*###################################################*/
        WHENDEBUG(RFM2G_DBINTR)
        {
            printk( KERN_CRIT"%s%d: %s saved_intcsr is 0x%x.\n", devname,
                    unit, me, saved_intcsr );
        }
        /*###################################################*/

        /* Does saved INTCSR indicate mailbox -OR- DMA interrupts?  If so,
           this interrupt came from our RFM board. */
        if( saved_intcsr & (RFMOR_INTCSR_LOCINT | RFMOR_INTCSR_DMA0INT) )
        {
            handled = 1;
            WHENDEBUG(RFM2G_DBINTR)
            {
                printk( KERN_CRIT"%s%d: %s interrupted.\n", devname,
                        unit, me );
            }
        }
        else
        {
            WHENDEBUG(RFM2G_DBINTR_NOT)
            {
                printk( KERN_CRIT"%s%d: %s did NOT interrupt.\n",
                        devname, unit, me );
            }
            break;
        }
        /****************************************************************/
        /* At this point we know we had an interrupt from our RFM board */
        /****************************************************************/
        /* Was this a DMA done interrupts? */
        if( (saved_intcsr & RFMOR_INTCSR_DMA0INT) &&
            (cfg->Capabilities & RFM2G_SUPPORTS_DMA) )
        {
            /* Count the DMA interrupt */
            queue = &(devinfo->EventQueue[RFM2GEVENT_DMADONE]);
            qheader = &queue->req_header;
            queue->req_header.reqh_counter++;

            /* increment queue size */
            (qheader->reqh_size)++;

            queue->req_header.reqh_flags &= ~(EVENT_TIMEDOUT);
            mb();

             /* Was user waiting for an interrupt? */
            if( queue->req_header.reqh_flags & EVENT_AWAIT )
            {
                /* Wake up process waiting for DMA to complete */
                wake_up_interruptible( &(queue->req_header.reqh_wait) );
            }

            /*Disable DMA Done Interrupt */
            saved_intcsr &= ~RFMOR_INTCSR_ENABLEDMA0INT;
            writel( saved_intcsr, (char *)(orbase + rfmor_intcsr) );

            /* Clear DMA Done interrupt status */
            writeb ( RFMOR_DMACSR_CLEAR_INT, (char *)(orbase + rfmor_Dma0Csr) );

            WHENDEBUG(RFM2G_DBINTR)
            {
                printk( KERN_CRIT"%s%d: %s DMA complete.\n",
                        devname, unit, me );
            }
        }
        /* Did we get a mailbox interrupt? */
        if( saved_intcsr & RFMOR_INTCSR_LOCINT )
        {
            /* Read and save the IRS register */
            saved_lisr = readl( (char *)(csbase + rfm2g_lisr) );

			/* And the LISR with the LIER so we know what is enabled that needs servicing */
            saved_lier = readl( (char *)(csbase + rfm2g_lier) );
			saved_lisr &= saved_lier;

            saved_lisr |= RFM2G_LISR_UNUSED; /* Enable Global Interrupt */

            WHENDEBUG(RFM2G_DBINTR)
            {
                printk( KERN_CRIT"%s%d: %s Mailbox interrupt, LISR = 0x%08X.\n",
                        devname, unit, me, saved_lisr );
            }

            /* Which mailbox interrupt(s) occurred? */
            if( saved_lisr & RFM2G_LISR_RSTREQ )
            {
                nonDmaInterrupts(unit, RFM2GEVENT_RESET);

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_RSTREQ;
            }

            if( saved_lisr & RFM2G_LISR_INT1 )
            {
                nonDmaInterrupts(unit, RFM2GEVENT_INTR1);
            }

            if( saved_lisr & RFM2G_LISR_INT2 )
            {
                nonDmaInterrupts(unit, RFM2GEVENT_INTR2);
            }

            if( saved_lisr & RFM2G_LISR_INT3 )
            {
                nonDmaInterrupts(unit, RFM2GEVENT_INTR3);
            }

            if( saved_lisr & RFM2G_LISR_INITINT )
            {
                nonDmaInterrupts(unit, RFM2GEVENT_INTR4);
            }

            if( saved_lisr & RFM2G_LISR_BADDATA )
            {
                RFM2G_UINT32 disable_value;
                nonDmaInterrupts(unit, RFM2GEVENT_BAD_DATA);

                /* Disable this until read to prevent flooding the interrupt handler */
                disable_value = readl( (char *)(csbase + rfm2g_lier) );
                writel( (disable_value & ~RFM2G_LIER_BAD_DATA) , (char *)(csbase + rfm2g_lier) );

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_BADDATA;
            }

            if( saved_lisr & RFM2G_LISR_RXFULL)
            {
                nonDmaInterrupts(unit, RFM2GEVENT_RXFIFO_FULL);

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_RXFULL;
            }

            if( saved_lisr & RFM2G_LISR_ROGUE_PKT)
            {
                nonDmaInterrupts(unit, RFM2GEVENT_ROGUE_PKT);

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_ROGUE_PKT;
            }

            if( saved_lisr & RFM2G_LISR_RXAFULL)
            {
                nonDmaInterrupts(unit, RFM2GEVENT_RXFIFO_AFULL);

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_RXAFULL;
            }

            if( saved_lisr & RFM2G_LISR_SNCLOST)
            {
                RFM2G_UINT32 disable_value;
                nonDmaInterrupts(unit, RFM2GEVENT_SYNC_LOSS);

                /* Disable this until read to prevent flooding the interrupt handler */
                disable_value = readl( (char *)(csbase + rfm2g_lier) );
                writel( (disable_value & ~RFM2G_LIER_SNCLOST), (char *)(csbase + rfm2g_lier) );

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_SNCLOST;
            }

            if( saved_lisr & RFM2G_LISR_WRINIB)
            {
                nonDmaInterrupts(unit, RFM2GEVENT_MEM_WRITE_INHIBITED);

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_WRINIB;
            }

            if( saved_lisr & RFM2G_LISR_PTYERR)
            {
                nonDmaInterrupts(unit, RFM2GEVENT_LOCAL_MEM_PARITY_ERR);

                /* Clear the status bit */
                saved_lisr &= ~RFM2G_LISR_PTYERR;
            }

            /* Clear the needed LISR register bits */
            writel( saved_lisr, (char *)(csbase + rfm2g_lisr) );
            mb();
        }
    } while (0);

    /* flush PCI write fifo */
    readl( (char *)(orbase + rfmor_intcsr) );

    /* Release lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s Releasing lock\n",
               devname, unit, me);
    }
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

    WHENDEBUG(RFM2G_DBTRACE | RFM2G_DBINTR)
    {
        printk(KERN_CRIT"%s%d: Exiting %s\n", devname, unit, me);
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
    return;
#else
    return(IRQ_RETVAL(handled));
#endif
}   /* End of rfm2g_interrupt() */


/******************************************************************************
*
*  FUNCTION:   nonDmaInterrupts
*
*  PURPOSE:    Service interrupts which are not related to DMA
*  PARAMETERS: unit: (I) Which RFM board to service
*              int_type: (I) The type of interrupt to service
*  RETURNS:    void
*  GLOBALS:    devname, rfm2gDeviceInfo
*
******************************************************************************/

void
nonDmaInterrupts( RFM2G_UINT8 unit, RFM2G_UINT8 int_type )
{
    static char *me = "nonDmaInterrupts()";
    RFM2GCONFIGLINUX  *cfg;         /* This RFM's config struct             */
    rfm2gEventQueue_t *queue;       /* This RFM's interrupt queue           */
    rfm2gEventQueueHeader_t *qheader;
    RFM2G_UINT8       sender_id = 0;/* Local copy of SIDx contents          */
    RFM2G_UINT8 *      csbase;      /* Base address of RFM's memory space   */
    RFM2G_UINT32      ext_data = 0; /* Extended interrupt data              */


    WHENDEBUG(RFM2G_DBTRACE | RFM2G_DBINTR)
    {
        printk(KERN_CRIT"%s%d: Entering %s, type = %d\n",
            devname, unit, me, int_type);
    }

    /* Get local pointers to structures within rfm2gDeviceInfo */
    cfg = &rfm2gDeviceInfo[unit].Config;
    queue = &rfm2gDeviceInfo[unit].EventQueue[int_type];
    qheader = &queue->req_header;
    csbase = (RFM2G_UINT8 *)cfg->pCsRegisters;

    /* Count the interrupt */
    qheader->reqh_counter++;

    /* Add this event to the queue */
    if( qheader->reqh_maxSize > 0 )
    {
		switch(int_type)
		{
			case RFM2GEVENT_INTR1:
			case RFM2GEVENT_INTR2:
			case RFM2GEVENT_INTR3:
			case RFM2GEVENT_INTR4:

				/* Is the queue going to overflowed? */
				if( ++(qheader->reqh_size) >= qheader->reqh_maxSize )
				{
	                RFM2G_UINT32 disable_value;

					/* Disable the interrupt so we don't lose any */
					switch (int_type)
					{
						case RFM2GEVENT_INTR1:
							disable_value = readl( (char *)(csbase + rfm2g_lier) );
							writel( (disable_value & ~RFM2G_LIER_INT1) , (char *)(csbase + rfm2g_lier) );
							break;
						case RFM2GEVENT_INTR2:
							disable_value = readl( (char *)(csbase + rfm2g_lier) );
							writel( (disable_value & ~RFM2G_LIER_INT2) , (char *)(csbase + rfm2g_lier) );
							break;
						case RFM2GEVENT_INTR3:
							disable_value = readl( (char *)(csbase + rfm2g_lier) );
							writel( (disable_value & ~RFM2G_LIER_INT3) , (char *)(csbase + rfm2g_lier) );
							break;
						case RFM2GEVENT_INTR4:
							disable_value = readl( (char *)(csbase + rfm2g_lier) );
							writel( (disable_value & ~RFM2G_LIER_INITINT) , (char *)(csbase + rfm2g_lier) );
							break;

						default:
							/* Software failure */
							break;
					}
					/* Flush out write to disable interrupt */
					mb();
				}

				if ((qheader->reqh_size) > qheader->reqh_maxSize )
				{
					/* this should not happen, the interrupt was disabled on the previous round */
					qheader->reqh_flags |= QUEUE_OVERFLOW;
					qheader->reqh_size = qheader->reqh_maxSize;
					if( ++(qheader->reqh_head) == qheader->reqh_maxSize )
					{
						/* Rollover */
						qheader->reqh_head = 0;
					}

					WHENDEBUG(RFM2G_DBINTR)
					{
						printk(KERN_CRIT"%s%d: %s Queue overflow on, "
							"event %d\n", devname, unit, me, int_type );
					}
				}
				break;

			default:
				/* Has the queue overflowed? */
				if( ++(qheader->reqh_size) > qheader->reqh_maxSize )
				{
					qheader->reqh_flags |= QUEUE_OVERFLOW;
					qheader->reqh_size = qheader->reqh_maxSize;
					if( ++(qheader->reqh_head) == qheader->reqh_maxSize )
					{
						/* Rollover */
						qheader->reqh_head = 0;
					}

					WHENDEBUG(RFM2G_DBINTR)
					{
						printk(KERN_CRIT"%s%d: %s Queue overflow on, "
							"event %d\n", devname, unit, me, int_type );
					}
				}
				break;
		}
		/* If a fiber interrupt occured, get the Node ID and extended data */
		switch (int_type)
		{
			case  RFM2GEVENT_INTR1:
			{
				ext_data = readl( (char *)(csbase + rfm2g_isd1) );
				sender_id = readb( (char *)(csbase + rfm2g_sid1) ); /* Always read this last! */
				break;
			}
			case RFM2GEVENT_INTR2:
			{
				ext_data = readl( (char *)(csbase + rfm2g_isd2) );
				sender_id = readb( (char *)(csbase + rfm2g_sid2) ); /* Always read this last! */
				break;
			}
			case RFM2GEVENT_INTR3:
			{
				ext_data = readl( (char *)(csbase + rfm2g_isd3) );
				sender_id = readb( (char *)(csbase + rfm2g_sid3) ); /* Always read this last! */
				break;
			}
			case RFM2GEVENT_INTR4:
			{
				ext_data = readl( (char *)(csbase + rfm2g_initd) );
				sender_id = readb( (char *)(csbase + rfm2g_initn) ); /* Always read this last! */
				break;
			}
		}

        queue->req_info[qheader->reqh_tail].NodeId = sender_id;
        queue->req_info[qheader->reqh_tail].ExtendedInfo = ext_data;
        queue->req_info[qheader->reqh_tail].Qflags = 0;
		queue->req_info[qheader->reqh_tail].Event = int_type;

        if( ++(qheader->reqh_tail) == qheader->reqh_maxSize )
        {
            /* Rollover */
            qheader->reqh_tail = 0;
        }

        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_CRIT"%s: %s Unit %d, event %d enqueued.\n",
                devname, me, unit, int_type );
        }

        /* Remember the highest number of enqueued interrupts */
        if( qheader->reqh_size > qheader->reqh_qpeak )
        {
            qheader->reqh_qpeak = qheader->reqh_size;
        }
    }
    mb();

    /* Was user waiting on an interrupt? */
    if( qheader->reqh_flags & EVENT_AWAIT )
    {
        /* User was blocking, wake sleeping processes */
        qheader->reqh_flags &= ~(EVENT_TIMEDOUT);
        wake_up_interruptible( &qheader->reqh_wait );
        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: %s Awoke sleeping user process. "
                "event %d\n", devname, unit, me, int_type );
        }
    }

    WHENDEBUG(RFM2G_DBTRACE | RFM2G_DBINTR)
    {
        printk(KERN_CRIT"%s%d: Exiting %s\n", devname, unit, me);
    }

}   /* End of nonDmaInterrupts() */


/*******************************************************************************
*
*  FUNCTION:   EnableInterrupt
*
*  PURPOSE:    Enables one RFM interrupt and installs the interrupt handler
*                (if necessary)
*  PARAMETERS: eventinfo: (I) Info about the interrupt event to enable
*  RETURNS:    0  if no errors occur
*              -1 if this interrupt has already been enabled
*              -2 if any other error occurs
*  GLOBALS:    devname, rfm2gDeviceInfo, rfm2g_intr_counter
*
*******************************************************************************/

RFM2G_INT32
EnableInterrupt( RFM2GEVENTINFOLINUX *eventinfo, RFM2G_BOOL locked )
{
    static char *me = "EnableInterrupt()";
    RFM2GCONFIGLINUX  *cfg;    /* This RFM's config struct          */
    RFM2GEVENTQUEUE_HEADER qheader;
    RFM2G_UINT8 * csbase;
    RFM2G_UINT8 * orbase;
    RFM2G_UINT32 enable_value;
    RFM2G_UINT32 temp;
    RFM2G_UINT8  unit = eventinfo->Unit;
    unsigned long flags = 0;        /* Save and restore flags with spinlocks */

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s\n", devname,
                                   unit, me);

    /* Get a pointer to this interrupt queue header */
    qheader = &rfm2gDeviceInfo[unit].EventQueue[eventinfo->Event].req_header;

    /* Get a local pointer to this unit's rfm2gDeviceInfo structure */
    cfg = &rfm2gDeviceInfo[unit].Config;

    /* Get pointers to this board */
    csbase = (RFM2G_UINT8 *)cfg->pCsRegisters;
    orbase  = (RFM2G_UINT8 *)cfg->pOrRegisters;


    /* Set the bits in enable_value to enable the desired interrupt */
    switch( eventinfo->Event )
    {
        case RFM2GEVENT_RESET:
            enable_value = RFM2G_LIER_RSTREQ;
            break;

		case RFM2GEVENT_INTR1:
            enable_value = RFM2G_LIER_INT1;
            break;

        case RFM2GEVENT_INTR2:
            enable_value = RFM2G_LIER_INT2;
            break;

        case RFM2GEVENT_INTR3:
            enable_value = RFM2G_LIER_INT3;
            break;

        case RFM2GEVENT_INTR4:
            enable_value = RFM2G_LIER_INITINT;
            break;

        case RFM2GEVENT_BAD_DATA:
            enable_value = RFM2G_LIER_BAD_DATA;
            break;

        case RFM2GEVENT_RXFIFO_FULL:
            enable_value = RFM2G_LIER_RXFULL;
            break;

        case RFM2GEVENT_ROGUE_PKT:
            enable_value = RFM2G_LIER_ROGUE_PKT;
            break;

        case RFM2GEVENT_RXFIFO_AFULL:
            enable_value = RFM2G_LIER_RXAFULL;
            break;

        case RFM2GEVENT_SYNC_LOSS:
            enable_value = RFM2G_LIER_SNCLOST;
            break;

        case RFM2GEVENT_MEM_WRITE_INHIBITED:
            enable_value = RFM2G_LIER_WRINIB;
            break;

        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:
            enable_value = RFM2G_LIER_PTYERR;
            break;

        case RFM2GEVENT_DMADONE:  /* Enabled during DMA processing */

        default:
            WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: Cannot enable interrupt %d\n",
                    devname, unit, me, eventinfo->Event );
            }
            return(-2);
    }

    if (!locked)
    {
        /* Acquire lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s About to acquire interrupt spinlock on "
                "event %d\n", devname, unit, me, eventinfo->Event);
        }

        spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );


        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Acquired interrupt spinlock on "
                "event %d\n", devname, unit, me, eventinfo->Event);
        }
    }

    /* Now enable the interrupt */
    enable_value |= readl( (char *)(csbase + rfm2g_lier) );
    writel( enable_value, (char *)(csbase + rfm2g_lier) );

    temp = readl( (char *)(orbase + rfmor_intcsr) );
    temp |= RFMOR_INTCSR_ENABLEINT;
    temp |= RFMOR_INTCSR_ENABLELOCINT;
    writel( temp, (char *)(orbase + rfmor_intcsr) );

    qheader->reqh_flags |= EVENT_ENABLED;
    (cfg->IntrCount)++;

    if (!locked)
    {
        /* Release lock before returning */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock\n", devname, unit, me);
        }

        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
    }
    WHENDEBUG(RFM2G_DBINTR)
    {
        printk(KERN_ERR"%s%d: %s Enabled interrupt event %d\n",
            devname, unit, me, eventinfo->Event );
    }

    return(0);

}   /* End of EnableInterrupt() */


/******************************************************************************
*
*  FUNCTION:   DisableInterrupt
*
*  PURPOSE:    Disables one RFM interrupt and uninstalls the interrupt handler
*                (if necessary)
*  PARAMETERS: eventinfo: (I) Info about the interrupt event to disable
*  RETURNS:    0  if no errors occur
*              -1 if this interrupt has not already been enabled
*              -2 if any other error occurs
*  GLOBALS:    devname, rfm2gDeviceInfo
******************************************************************************/

RFM2G_INT32
DisableInterrupt( RFM2GEVENTINFOLINUX *eventinfo, RFM2G_BOOL locked )
{
    static char *me = "DisableInterrupt()";
    RFM2GCONFIGLINUX  *cfg;    /* This RFM's config struct          */
    RFM2GEVENTQUEUE_HEADER qheader;
    RFM2G_ADDR csbase;
    RFM2G_ADDR orbase;
    RFM2G_UINT32 disable_value;
    RFM2G_UINT8  unit = eventinfo->Unit;
    unsigned long flags = 0;        /* Save and restore flags with spinlocks */



    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s. pid:%d\n", devname,
                                    unit, me, current->pid);

    /* Get a pointer to this interrupt queue header */
    qheader = &rfm2gDeviceInfo[unit].EventQueue[eventinfo->Event].req_header;

    /* Get a local pointer to this unit's rfmDeviceInfo structure */
    cfg = &rfm2gDeviceInfo[unit].Config;

    /* Get pointers to this board */
    csbase = (RFM2G_ADDR)cfg->pCsRegisters;
    orbase  = (RFM2G_ADDR)cfg->pOrRegisters;


    /* Set the bits in enable_value to disable the desired interrupt */
    switch( eventinfo->Event )
    {
        case RFM2GEVENT_RESET:
            disable_value = RFM2G_LIER_RSTREQ;
            break;

		case RFM2GEVENT_INTR1:
            disable_value = RFM2G_LIER_INT1;
            break;

        case RFM2GEVENT_INTR2:
            disable_value = RFM2G_LIER_INT2;
            break;

        case RFM2GEVENT_INTR3:
            disable_value = RFM2G_LIER_INT3;
            break;

        case RFM2GEVENT_INTR4:
            disable_value = RFM2G_LIER_INITINT;
            break;

        case RFM2GEVENT_BAD_DATA:
            disable_value = RFM2G_LIER_BAD_DATA;
            break;

        case RFM2GEVENT_RXFIFO_FULL:
            disable_value = RFM2G_LIER_RXFULL;
            break;

        case RFM2GEVENT_ROGUE_PKT:
            disable_value = RFM2G_LIER_ROGUE_PKT;
            break;

        case RFM2GEVENT_RXFIFO_AFULL:
            disable_value = RFM2G_LIER_RXAFULL;
            break;

        case RFM2GEVENT_SYNC_LOSS:
            disable_value = RFM2G_LIER_SNCLOST;
            break;

        case RFM2GEVENT_MEM_WRITE_INHIBITED:
            disable_value = RFM2G_LIER_WRINIB;
            break;

        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:
            disable_value = RFM2G_LIER_PTYERR;
            break;

        case RFM2GEVENT_DMADONE:  /* Disabled during DMA processing */
        default:
            WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: Exiting %s: Cannot disable interrupt %d. pid:%d\n",
                    devname, unit, me, eventinfo->Event, current->pid );
            }

            return(-2);
    }

    if (!locked)
    {
        /* Acquire lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s About to acquire interrupt spinlock on "
                "event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid);
        }

        spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Acquired interrupt spinlock on "
                "event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid);
        }
    }

    /* Now disable the interrupt */
    disable_value = readl( (char *)(csbase + rfm2g_lier) ) & ~disable_value;
    writel( disable_value, (char *)(csbase + rfm2g_lier) );
    qheader->reqh_flags &= ~EVENT_ENABLED;
    (cfg->IntrCount)--;

    WHENDEBUG(RFM2G_DBINTR)
    {
        printk(KERN_ERR"%s%d: %s Disabled interrupt event %d. pid:%d\n",
            devname, unit, me, eventinfo->Event, current->pid );
    }


    if (!locked)
    {
        /* Release lock before returning */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        }

        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
    }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s. pid:%d\n", devname,
                                   unit, me, current->pid);

    return(0);

}   /* End of DisableInterrupt() */


/******************************************************************************
*
*  FUNCTION:   WaitForInterrupt
*
*  PURPOSE:    Block until an interrupt event occurs or timeout
*  PARAMETERS: eventinfo: (IO) Specifies event and timeout
*  RETURNS:    0  if interrupt is received
*              -1 if timeout value expires with no interrupt
*              -2 if notification was already requested for the event
*              -3 if this interrupt is not enabled
*  GLOBALS:    rfm2gDeviceInfo, devname
*
******************************************************************************/

RFM2G_INT32
WaitForInterrupt( RFM2GEVENTINFOLINUX *eventinfo, unsigned long ulFlags )
{
    RFM2G_INT32 status = 0;

    static char *me = "WaitForInterrupt()";
    rfm2gEventQueueHeader_t *qheader;
    rfm2gEventQueue_t *queue;  /* This RFM's interrupt queue        */
    unsigned long flags = ulFlags;   /* Save and restore flags with spinlocks */
    unsigned int  timeout = 0; /* Msecs converted into jiffies          */
    RFM2G_UINT8 unit = eventinfo->Unit;
    wait_queue_t wait;
    RFM2G_UINT8 * orbase;
    RFM2G_UINT8 * csbase;
    RFM2GCONFIGLINUX  *cfg;    /* This RFM's config struct          */

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s. pid:%d\n", devname,
                                    unit, me, current->pid);

    /* Get a local pointer to this unit's rfmDeviceInfo structure */
    cfg = &rfm2gDeviceInfo[unit].Config;

    /* Get the event queue header */
    queue = &rfm2gDeviceInfo[unit].EventQueue[eventinfo->Event];
    qheader = &queue->req_header;

    /* Get pointers to this board */
    csbase = (RFM2G_UINT8 *)cfg->pCsRegisters;
    orbase  = (RFM2G_UINT8 *)cfg->pOrRegisters;

    spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

    /* Go away if someone already wants this event */
    if( qheader->reqh_flags & EVENT_AWAIT )
    {
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        }
        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

        WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: %s Exiting.  Notification already requested "
                "for Event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid );
        }

        return(-EALREADY);
    }

    /* Clear out timeout and cancel flags possibly set from previous wait. */
    qheader->reqh_flags &= ~(EVENT_TIMEDOUT | EVENT_CANCELED);

    /* Do we need to wait for an event? */
    if( qheader->reqh_size == 0)
    {
		if (qheader->reqh_flags & EVENT_ENABLED)
		{
            RFM2G_UINT32 enable_value;

			/* Make sure the FIFO type events interrupt is enabled may have been
			   disabled if the FIFO filled up.  */
			switch (eventinfo->Event)
			{
				case RFM2GEVENT_INTR1:
					enable_value = readl( (char *)(csbase + rfm2g_lier) );
					if ((enable_value & RFM2G_LIER_INT1) == 0)
					{
						writel( (enable_value | RFM2G_LIER_INT1) , (char *)(csbase + rfm2g_lier) );
					}
					break;
				case RFM2GEVENT_INTR2:
					enable_value = readl( (char *)(csbase + rfm2g_lier) );
					if ((enable_value & RFM2G_LIER_INT2) == 0)
					{
						writel( (enable_value | RFM2G_LIER_INT2) , (char *)(csbase + rfm2g_lier) );
					}
					break;
				case RFM2GEVENT_INTR3:
					enable_value = readl( (char *)(csbase + rfm2g_lier) );
					if ((enable_value & RFM2G_LIER_INT3) == 0)
					{
						writel( (enable_value | RFM2G_LIER_INT3) , (char *)(csbase + rfm2g_lier) );
					}
					break;
				case RFM2GEVENT_INTR4:
					enable_value = readl( (char *)(csbase + rfm2g_lier) );
					if ((enable_value & RFM2G_LIER_INITINT) == 0)
					{
						writel( (enable_value | RFM2G_LIER_INITINT) , (char *)(csbase + rfm2g_lier) );
					}
					break;

				default:
					/* Nothing to do */
					break;
			}
		}

        /* We're going to wait */
        eventinfo->Qflags = 0;
        qheader->reqh_flags |= EVENT_AWAIT;
        qheader->reqh_flags |= EVENT_TIMEDOUT;

        /* Convert milliseconds into jiffies */
        timeout = (eventinfo->Timeout * HZ) / 1000;

        /* Anything less than 10 msec => 0 jiffies */
        if( timeout == 0)
        {
            timeout = 1;
        }

        /* Go to sleep until timing out or receiving the interrupt */
        init_waitqueue_entry(&wait, current);
        add_wait_queue( &qheader->reqh_wait, &wait );
        current->state = TASK_INTERRUPTIBLE;

        /* Release lock before really going to sleep */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        }

        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

        /* This while loop was added because we where seeing cases where we where waking up,
           but it was not for our interrupt or a signal_pending() */
		while (timeout && (qheader->reqh_flags & EVENT_TIMEDOUT) &&
                          (qheader->reqh_flags & EVENT_AWAIT) &&
                          !(qheader->reqh_flags & EVENT_CANCELED))
		{
            /* Put us to sleep */
	        current->state = TASK_INTERRUPTIBLE;

            /* Check the condition we are sleeping on prior to giving up the processor.  We will
            have a race condition if we fail to perform this check.  For additional information,
            refer to Linux Device Drivers 3rd Edition page 156.
             */
            if( qheader->reqh_size != 0)
            {
                /* The interrupt has come in, do not sleep.  Wake up the process.
                   Break out of while loop and remove process from the wait queue
                */
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s received interrupt without sleeping "
						"for interrupt type %d.. pid:%d\n", devname, unit, me,
						eventinfo->Event, current->pid );
				}

                current->state = TASK_RUNNING;
                break;
            }

            /* Check if we have a signal */
			if( signal_pending(current) )
            {
                status = -EINTR;
				WHENDEBUG(RFM2G_DBINTR)
				{
                    printk(KERN_ERR"%s%d: %s is signaled in schedule. pid:%d\n",
                                         devname, unit, me, current->pid);
				}
				break;
			}

			if( eventinfo->Timeout == INFINITE_TIMEOUT )
			{
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is sleeping with no timeout "
						"for interrupt type %d.. pid:%d\n", devname, unit, me,
						eventinfo->Event, current->pid );
				}

				schedule();
			}
			else
			{
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is sleeping %u msecs timeout %u for "
						"interrupt type %d.. pid:%d\n", devname, unit, me,
						eventinfo->Timeout, timeout, eventinfo->Event, current->pid );
				}

				timeout = schedule_timeout( timeout );
			}

			if( signal_pending(current) )
            {
                status = -EINTR;
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is signaled in schedule... pid:%d\n",
                                         devname, unit, me, current->pid);
				}
				break;
			}
		}

        remove_wait_queue( &qheader->reqh_wait, &wait );

        /* Acquire lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s About to acquire interrupt spinlock on "
                "event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid);
        }

        spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

        /* Awake now, did we time out or get a signal ? */
        if (qheader->reqh_flags & EVENT_CANCELED)
        {
            status = -EIDRM;
            WHENDEBUG(RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s Canceling. pid:%d\n", devname, unit, me, current->pid);
            }
        }
        else if( (qheader->reqh_flags & EVENT_TIMEDOUT) && (status == 0) )
        {
            status = -ETIMEDOUT;

            WHENDEBUG(RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s Timed out.. pid:%d\n", devname, unit, me, current->pid );
            }
        }
        else if (status == 0)
        {
            WHENDEBUG(RFM2G_DBINTR)
            {
                printk(KERN_ERR"%s%d: %s Received interrupt.. pid:%d\n", devname, unit,
                       me, current->pid );
            }
        } /* otherwise, we recieved a system signal */
    }

    /* Make sure we didn't timeout */
    if ((status == 0))
    {
		if (qheader->reqh_size == 0)
		{
			/* Release lock */
			WHENDEBUG(RFM2G_DBMUTEX)
			{
				printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
			}
            qheader->reqh_flags &= ~(EVENT_AWAIT);
			spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
			return (-ENOSYS);
		}

        /* There is at least one event in the queue, so dequeue the oldest */
        eventinfo->NodeId = queue->req_info[qheader->reqh_head].NodeId;
        eventinfo->ExtendedInfo =
            queue->req_info[qheader->reqh_head].ExtendedInfo;
        eventinfo->Qflags = queue->req_info[qheader->reqh_head].Qflags;

        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: %s dequeued interrupt type %d.. pid:%d\n",
                devname, unit, me, eventinfo->Event, current->pid );
        }

        /* Provide additional info about this interrupt */
        eventinfo->Qflags |= QFLAG_DEQUEUED;
        if( qheader->reqh_flags & QUEUE_OVERFLOW )
        {
            eventinfo->Qflags |= QFLAG_OVERFLOWED;
        }
        if( qheader->reqh_size >= qheader->reqh_maxSize/2 )
        {
            eventinfo->Qflags |= QFLAG_HALF_FULL;
        }

        if( ++(qheader->reqh_head) == qheader->reqh_maxSize )
        {
            /* Rollover */
            qheader->reqh_head = 0;
        }

        /* The queue has fewer elements now */
        --(qheader->reqh_size);

        qheader->reqh_flags &= ~(EVENT_TIMEDOUT);

    }

    qheader->reqh_flags &= ~(EVENT_AWAIT);
    mb();

    /* Release lock */
    WHENDEBUG(RFM2G_DBMUTEX)
    {
        printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
    }
    spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s status = %d. pid:%d\n", devname, unit, me, status, current->pid);

    return(status);

}   /* End of WaitForInterrupt() */

RFM2G_INT32
DoDMAWithWaitForInt( RFM2GEVENTINFOLINUX *eventinfo, unsigned long ulFlags )
{
	RFM2G_INT32 status = 0;
	static char *me = "DoDMAWithWaitForInt()";
	rfm2gEventQueueHeader_t *qheader;
	rfm2gEventQueue_t *queue;  /* This RFM's interrupt queue        */
	unsigned long flags = ulFlags;   /* Save and restore flags with spinlocks */
	unsigned int  timeout = 0; /* Msecs converted into jiffies          */
	RFM2G_UINT8 unit = eventinfo->Unit;
	wait_queue_t wait;
	RFM2G_UINT8 * orbase;
	RFM2G_UINT8 * csbase;
	RFM2GCONFIGLINUX  *cfg;    /* This RFM's config struct          */

    	WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s. pid:%d\n", devname,
							unit, me, current->pid);

	/* Get a local pointer to this unit's rfmDeviceInfo structure */
	cfg = &rfm2gDeviceInfo[unit].Config;

	/* Get the event queue header */
    	queue = &rfm2gDeviceInfo[unit].EventQueue[eventinfo->Event];
    	qheader = &queue->req_header;

    	/* Get pointers to this board */
    	csbase = (RFM2G_UINT8 *)cfg->pCsRegisters;
    	orbase  = (RFM2G_UINT8 *)cfg->pOrRegisters;

    	spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

    	/* Go away if someone already wants this event */
    	if( qheader->reqh_flags & EVENT_AWAIT )
    	{
        	WHENDEBUG(RFM2G_DBMUTEX)
        	{
            	printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        	}
        	spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

        	WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINTR)
        	{
            	printk(KERN_ERR"%s%d: %s Exiting.  Notification already requested "
                		"for Event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid );
        	}

        	return(-EALREADY);
    	}

    	/* Clear out timeout and cancel flags possibly set from previous wait. */
    	qheader->reqh_flags &= ~(EVENT_TIMEDOUT | EVENT_CANCELED);

    	/* Do we need to wait for an event? */
    	if( qheader->reqh_size == 0)
    	{
        	/* We're going to wait */
        	eventinfo->Qflags = 0;
        	qheader->reqh_flags |= EVENT_AWAIT;
        	qheader->reqh_flags |= EVENT_TIMEDOUT;

        	/* Convert milliseconds into jiffies */
        	timeout = (eventinfo->Timeout * HZ) / 1000;

        	/* Anything less than 10 msec => 0 jiffies */
        	if( timeout == 0)
        	{
            	timeout = 1;
        	}

        	/* Go to sleep until timing out or receiving the interrupt */
        	init_waitqueue_entry(&wait, current);
        	add_wait_queue( &qheader->reqh_wait, &wait );
        	current->state = TASK_INTERRUPTIBLE;

        	/* Release lock before really going to sleep */
        	WHENDEBUG(RFM2G_DBMUTEX)
        	{
            	printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        	}

        	spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

        	/* This while loop was added because we where seeing cases where we where waking up,
		but it was not for our interrupt or a signal_pending() */
		while (timeout && (qheader->reqh_flags & EVENT_TIMEDOUT) &&
                          (qheader->reqh_flags & EVENT_AWAIT) &&
                          !(qheader->reqh_flags & EVENT_CANCELED))
		{
            	/* Put us to sleep */
	        	current->state = TASK_INTERRUPTIBLE;

            	/* Check the condition we are sleeping on prior to giving up the processor.  We will
            	have a race condition if we fail to perform this check.  For additional information,
            	refer to Linux Device Drivers 3rd Edition page 156.
             	*/
            	if( qheader->reqh_size != 0)
            	{
                		/* The interrupt has come in, do not sleep.  Wake up the process.
                   	Break out of while loop and remove process from the wait queue
                		*/
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s received interrupt without sleeping "
						"for interrupt type %d.. pid:%d\n", devname, unit, me,
						eventinfo->Event, current->pid );
				}

                		current->state = TASK_RUNNING;
                		break;
            	}

            	/* Check if we have a signal */
			if( signal_pending(current) )
            	{
                		status = -EINTR;
		     		WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is signaled in schedule. pid:%d\n",
                                         devname, unit, me, current->pid);
				}
				break;
			}

			/* Start the DMA */
			spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );
			writeb( (RFMOR_DMACSR_ENABLE | RFMOR_DMACSR_START),
				(char *)(orbase + rfmor_Dma0Csr) );
			readb((char*)(orbase + rfmor_Dma0Csr));
			spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

			/* Wait for DMA Done Interrupt */
			if( eventinfo->Timeout == INFINITE_TIMEOUT )
			{
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is sleeping with no timeout "
						"for interrupt type %d.. pid:%d\n", devname, unit, me,
						eventinfo->Event, current->pid );
				}

				schedule();
			}
			else
			{
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is sleeping %u msecs timeout %u for "
						"interrupt type %d.. pid:%d\n", devname, unit, me,
						eventinfo->Timeout, timeout, eventinfo->Event, current->pid );
				}

				timeout = schedule_timeout( timeout );
			}

			if( signal_pending(current) )
            	{
				status = -EINTR;
				WHENDEBUG(RFM2G_DBINTR)
				{
					printk(KERN_ERR"%s%d: %s is signaled in schedule... pid:%d\n",
						devname, unit, me, current->pid);
				}
				break;
			}
		}

		remove_wait_queue( &qheader->reqh_wait, &wait );

		/* Acquire lock */
		WHENDEBUG(RFM2G_DBMUTEX)
		{
			printk(KERN_ERR"%s%d: %s About to acquire interrupt spinlock on "
                		"event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid);
		}

		spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

		/* Awake now, did we time out or get a signal ? */
		if (qheader->reqh_flags & EVENT_CANCELED)
		{
			status = -EIDRM;
			WHENDEBUG(RFM2G_DBINTR)
            	{
                		printk(KERN_ERR"%s%d: %s Canceling. pid:%d\n", devname, unit, me, current->pid);
            	}
        	}
        	else if( (qheader->reqh_flags & EVENT_TIMEDOUT) && (status == 0) )
        	{
            	status = -ETIMEDOUT;

            	WHENDEBUG(RFM2G_DBINTR)
            	{
                		printk(KERN_ERR"%s%d: %s Timed out.. pid:%d\n", devname, unit, me, current->pid );
            	}
        	}
        	else if (status == 0)
        	{
            	WHENDEBUG(RFM2G_DBINTR)
            	{
                		printk(KERN_ERR"%s%d: %s Received interrupt.. pid:%d\n", devname, unit,
                       		me, current->pid );
            	}
        	} /* otherwise, we recieved a system signal */
    	}

    	/* Make sure we didn't timeout */
    	if ((status == 0))
    	{
		if (qheader->reqh_size == 0)
		{
			/* Release lock */
			WHENDEBUG(RFM2G_DBMUTEX)
			{
				printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
			}
			qheader->reqh_flags &= ~(EVENT_AWAIT);
			spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
			return (-ENOSYS);
		}

		/* There is at least one event in the queue, so dequeue the oldest */
		eventinfo->NodeId = queue->req_info[qheader->reqh_head].NodeId;
		eventinfo->ExtendedInfo =
			queue->req_info[qheader->reqh_head].ExtendedInfo;
		eventinfo->Qflags = queue->req_info[qheader->reqh_head].Qflags;

		WHENDEBUG(RFM2G_DBINTR)
		{
			printk(KERN_ERR"%s%d: %s dequeued interrupt type %d.. pid:%d\n",
                		devname, unit, me, eventinfo->Event, current->pid );
		}

		/* Provide additional info about this interrupt */
		eventinfo->Qflags |= QFLAG_DEQUEUED;
		if( qheader->reqh_flags & QUEUE_OVERFLOW )
		{
			eventinfo->Qflags |= QFLAG_OVERFLOWED;
		}
		if( qheader->reqh_size >= qheader->reqh_maxSize/2 )
		{
			eventinfo->Qflags |= QFLAG_HALF_FULL;
		}

		if( ++(qheader->reqh_head) == qheader->reqh_maxSize )
		{
			/* Rollover */
            	qheader->reqh_head = 0;
		}

		/* The queue has fewer elements now */
		--(qheader->reqh_size);

		qheader->reqh_flags &= ~(EVENT_TIMEDOUT);

	}

    	qheader->reqh_flags &= ~(EVENT_AWAIT);
    	mb();

    	/* Release lock */
	WHENDEBUG(RFM2G_DBMUTEX)
    	{
        	printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
    	}
    	spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );

    	WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s status = %d. pid:%d\n", devname, unit, me, status, current->pid);

    	return(status);

}   /* End of DoDMAWithWaitForInt() */



/******************************************************************************
*
*  FUNCTION:   CancelInterrupt
*
*  PURPOSE:    Clear the EVENT_AWAIT flag and wake any process blocking
*                on this interrupt
*  PARAMETERS: eventinfo: (IO) Specifies unit and event
*  RETURNS:    0  if no errors occurred
*              -1 if this interrupt is not enabled
*              -2 if any other error occurs
*  GLOBALS:    rfm2gDeviceInfo, devname
******************************************************************************/

int
CancelInterrupt( RFM2GEVENTINFOLINUX *eventinfo, RFM2G_BOOL locked )
{
    static char        *me = "CancelInterrupt()";
    rfm2gEventQueueHeader_t *qheader; /* This RFM's interrupt queue header */
    rfm2gEventQueue_t *queue;  /* This RFM's interrupt queue        */
    RFM2G_UINT8 unit = eventinfo->Unit;
    RFM2GCONFIGLINUX  *cfg;    /* This RFM's config struct          */
    unsigned long flags = 0;        /* Save and restore flags with spinlocks */
    int status = 0;


    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Entering %s. pid:%d\n", devname,
                                    unit, me, current->pid);

    /* Get a local pointer to this unit's rfmDeviceInfo structure */
    cfg = &rfm2gDeviceInfo[unit].Config;

    /* Get the event queue header */
    queue = &rfm2gDeviceInfo[unit].EventQueue[eventinfo->Event];
    qheader = &queue->req_header;

    if (!locked)
    {
        /* Acquire lock */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s About to acquire interrupt spinlock on "
                "event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid);
        }

        spin_lock_irqsave( (spinlock_t*)(cfg->SpinLock), flags );

        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Acquired interrupt spinlock on "
                "event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid);
        }
    }

    /* Is this interrupt not in use? This test should be done with the spinlock
       because without the spinlock it is possible that the flag is being changed
       while we test it.
     */
    if( (qheader->reqh_flags & EVENT_AWAIT) == 0 )
    {
        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: Exiting %s: The interrupt on "
                "event %d is not in use, flags = 0x%X. pid:%d\n", devname, unit, me,
                eventinfo->Event, qheader->reqh_flags, current->pid );
        }
        status = -1;
    }
    else
    {
        /* User was blocking, so wake the sleeping process */
        qheader->reqh_flags |= EVENT_CANCELED;

        wake_up_interruptible( &qheader->reqh_wait );
        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: %s Awoke sleeping user process. "
                "Event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid );
        }

        /* The interrupt notification is canceled when it wakes up, which
           will occur after the spinlock is released.  It does not really matter
           where this message is because one does not know when the thread
           will wake up.
         */
        WHENDEBUG(RFM2G_DBINTR)
        {
            printk(KERN_ERR"%s%d: %s Interrupt notification canceled on "
                "event %d. pid:%d\n", devname, unit, me, eventinfo->Event, current->pid );
        }
    }

    if (!locked)
    {
        /* Release lock before returning */
        WHENDEBUG(RFM2G_DBMUTEX)
        {
            printk(KERN_ERR"%s%d: %s Releasing lock. pid:%d\n", devname, unit, me, current->pid);
        }

        spin_unlock_irqrestore( (spinlock_t*)(cfg->SpinLock), flags );
    }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s%d: Exiting %s. pid:%d\n", devname,
                                    unit, me, current->pid);

    return(status);

}  /* end of CancelInterrupt() */



