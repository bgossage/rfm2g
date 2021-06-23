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
$Workfile: driver_init.c $
$Revision: 40801 $
$Modtime: 9/03/10 1:16p $
-------------------------------------------------------------------------------
    Description: Initialization of driver and hardware upon loading of module.
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    11-OCT-2001  ML   1.0   Created.
    08-DEC-2004  ML   01.04 Added new module_init() and module_exit() macros
    20 Jun 2008  EB   6.00  Added mmapSem semaphore.
                            This change is in support of PAE (Physical Address
                            Extension)-enabled Linux.
===============================================================================
*/

#define ALLOC_DATA
#include "rfm2g_driver.h"

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

MODULE_AUTHOR("GE   Embedded Systesms <www.gefanuc.com>");
MODULE_DESCRIPTION("RFM2g PCI driver");

/******************************************************************************
*
*  FUNCTION:   rfm2g_init_module
*
*  PURPOSE:    Standard entry point to handle registration of the RFM2G
*              driver module
*  PARAMETERS: void
*  RETURNS:    0      : Success
*              -ENODEV: No Reflective Memory detected
*              -ENOMEM: Memory allocation failed
*              Negative values for other errors
*  GLOBALS:    devname, rfm2g_device_count, rfm2gDeviceInfo, rfm2g_fops, major_num
*
******************************************************************************/

#define  MYRFM_DEBUG_FLAGS (RFM2G_DBERROR | RFM2G_DBINIT | RFM2G_DBINTR | RFM2G_DBIOCTL | RFM2G_DBIOCTL | RFM2G_DBMMAP | RFM2G_DBOPEN | RFM2G_DBCLOSE | RFM2G_DBREAD | RFM2G_DBWRITE | RFM2G_DBPEEK | RFM2G_DBPOKE | RFM2G_DBSTRAT | RFM2G_DBTIMER | RFM2G_DBTRACE | RFM2G_DBMUTEX | RFM2G_INTR_NOT | RFM2G_DBSLOW | DBMINPHYS | RFM2G_DBDMA)

static RFM2G_INT32
rfm2g_init_module(void)
{
    static char *me = "rfm_init_module()";
    RFM2G_INT32 result;
    RFM2G_INT32 i;       /* Loop variable */

    /* Default debug flags */
    rfm2gDebugFlags = RFM2G_DBERROR;
/*	rfm2gDebugFlags = 0xffff; */

    WHENDEBUG(RFM2G_DBINIT)printk(KERN_ERR"%s: Entering %s\n", devname, me);

    for( i=0; i<MAX_RFM2G_DEVICES; i++ )
    {
        device_list[i] = (struct pci_dev *) NULL;
    }
    /* Check to see if we have any reflective memory devices */
    rfm2g_device_count = rfm2gCountDevices( MAX_RFM2G_DEVICES, device_list );
    if( rfm2g_device_count <= 0 )
    {
        /* No devices, so there's no point in loading this module! */
        WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINIT)
        {
            printk(KERN_ERR"%s:Exiting %s: No Reflective Memory devices "
                "found\n", devname, me );
        }

        return( -ENODEV );
    }

    /* Initialize the rfmDeviceInfo array */
    memset( (void *) rfm2gDeviceInfo, 0, (size_t) sizeof(rfm2gDeviceInfo) );

    /* Now initialize each device */
    for( i=0; i<rfm2g_device_count; i++ )
    {
        rfm2gDeviceInfo[i].unit = i;
        result = rfm2gInitDevice( &(rfm2gDeviceInfo[i]), device_list[i], i );
        if( result < 0 )
        {
            WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINIT)
            {
                printk(KERN_ERR"%s:Exiting %s: rfm2gInitDevice() failed\n",
                    devname, me );
            }

            return(result);
        }

        /* Initialize the interrupt queues for this device */
        InitEventQueues( i, MAX_QUEUE_SIZE );
    }
#ifdef CONFIG_DEVFS_FS
    /*  Use the register function to auto create the entries under /dev
     *     without having to run mknod from the command line
     */
    rfm2g_devfs_dir = devfs_mk_dir(NULL, devname, NULL);
    if (!rfm2g_devfs_dir)
        return -EBUSY; /* problem */

    for( i=0; i<rfm2g_device_count; i++ )
    {
        char mindevname[8];
        sprintf (mindevname, "%i", i);
        rfm2gDeviceInfo[i].handle = devfs_register( rfm2g_devfs_dir, mindevname,
                    0,//Flags, was DEVFS_FL_AUTO_DEVNUM
                    major_num, i, S_IFCHR | S_IRUGO | S_IWUGO,
                    &rfm2g_fops,
                    &rfm2gDeviceInfo[i]);
    }
#else
    /* Register this driver with the kernel */
    result = register_chrdev( major_num, devname, &rfm2g_fops );
    if( result < 0 )
    {
        WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINIT)
        {
            printk(KERN_ERR"%s:Exiting %s: register_chrdev() failed\n",
                devname, me );
        }

        return( result );
    }
#endif

    /* We succeeded, so store our new major device number */
    if( major_num == RFM2G_DYNAMIC_MAJOR_NUMBER )
    {
        major_num = result;
    }

    /* Register our /proc/rfm2g page */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
    create_proc_read_entry(rfm2gProcEntry.name, rfm2gProcEntry.mode, &proc_root,
				rfm2gProcEntry.read_proc, NULL);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    create_proc_read_entry(rfm2gProcEntry.name, rfm2gProcEntry.mode, NULL,
                           rfm2gProcEntry.read_proc, NULL);
#else
	proc_create("rfm2g", 0, NULL, &rfm2gFOS);
#endif
#endif

    /* Don't export any global symbols */
   /* EXPORT_NO_SYMBOLS;*/

    WHENDEBUG(RFM2G_DBINIT)printk(KERN_ERR"%s: Exiting %s\n", devname, me);
    return(0);

}   /* End of init_module() */

/******************************************************************************
*
*  FUNCTION:   rfm2g_cleanup_module
*
*  PURPOSE:    Standard entry point to handle unregistration of the RFM2G
*              driver module
*  PARAMETERS: void
*  RETURNS:    void
*  GLOBALS:    devname, major_num
*
******************************************************************************/

static void
rfm2g_cleanup_module(void)
{
    static char *me = "rfm_cleanup_module()";
    RFM2GCONFIGLINUX  *cfg;         /* Local pointer to config structure         */
    int i;
    RFM2G_INT32 result;

    WHENDEBUG(RFM2G_DBINIT) printk(KERN_ERR"%s: Entering %s\n", devname, me);
    /* Now silence each device */
    for( i=0; i<rfm2g_device_count; i++ )
    {
        rfm2gDeviceInfo[i].unit = i;
        result = rfm2gSilenceDevice( &(rfm2gDeviceInfo[i]), device_list[i], i );
        if( result < 0 )
        {
            WHENDEBUG(RFM2G_DBERROR | RFM2G_DBINIT)
            {
                printk(KERN_ERR"%s%d:Error %s: rfm2gSilenceDevice() failed\n",
                    devname, rfm2gDeviceInfo[i].unit, me );
            }
        }
    }

    /* Release our major device number */
    unregister_chrdev( major_num, devname );

    /* Release our /proc/rfm2g page */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
    remove_proc_entry( rfm2gProcEntry.name, &proc_root);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
    remove_proc_entry( rfm2gProcEntry.name, NULL);
#else
	remove_proc_entry("rfm2g", NULL);
#endif
#endif

    /* Now Free spinlock structure for each device */
    for( i=0; i<rfm2g_device_count; i++ )
    {
#ifdef CONFIG_RFM2G
        devfs_unregister(rfm2gDeviceInfo[i].handle);
#endif

        cfg = &(rfm2gDeviceInfo[i].Config);

        if (cfg->SpinLock != NULL)
        {
            kfree(cfg->SpinLock);
            cfg->SpinLock = NULL;
        }

        if (rfm2gDeviceInfo[i].DMASem != NULL)
        {
            kfree(rfm2gDeviceInfo[i].DMASem);
            rfm2gDeviceInfo[i].DMASem = NULL;
        }

        if (rfm2gDeviceInfo[i].mmapSem != NULL)
        {
            kfree(rfm2gDeviceInfo[i].mmapSem);
            rfm2gDeviceInfo[i].mmapSem = NULL;
        }
    }

    WHENDEBUG(RFM2G_DBINIT) printk(KERN_ERR"%s: Exiting %s\n", devname, me);
    return;

}   /* End of cleanup_module() */


/* Macros to define standard entry and exit points */
module_init(rfm2g_init_module);
module_exit(rfm2g_cleanup_module);
