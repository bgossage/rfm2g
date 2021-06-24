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
$Workfile: proc_page.c $
$Revision: 40801 $
$Modtime: 9/03/10 1:16p $
-------------------------------------------------------------------------------
    Description: Linux Proc Page for the PCI bus RFM2G device driver
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    15-OCT-2001  ML   1.0   Created.

===============================================================================
*/
#include "rfm2g_driver.h"

/******************************************************************************
*
*  FUNCTION:   RFM2gReadProcPage
*
*  PURPOSE:    Provides contents of the /proc/rmnet file
*  PARAMETERS: *buf:    (IO) Character buffer, passed in by OS
*              **start: Unused, passed in by OS
*              offset:  Unused, passed in by OS
*              len:     Unused, passed in by OS
*              unused:  Unused, passed in by OS
*              data_unused:  Unused, passed in by OS
*  RETURNS:    Number of bytes read
*  GLOBALS:    devname, rfmDeviceInfo, rfm_device_count
*
******************************************************************************/

RFM2G_INT32
RFM2gReadProcPage( char *buf, char **start, off_t offset, int len, int *unused,
                  void *data_unused)
{
    static char *me = "RFM2GReadProcPage()";
    RFM2G_INT32 bytesRead = 0;  /* Returned length                        */
    RFM2G_INT32 i;              /* Loop variable                          */
    char device_num[10];        /* Indicates minor number in text message */
    RFM2G_INT32 j;              /* Loop variable                          */

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s: Entering %s\n", devname, me);

    bytesRead = sprintf( buf, "\nWelcome to the RFM2G proc page!\n\n" );
    if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough! */

    bytesRead = sprintf( buf, "COPYRIGHT NOTICE\n" );
    if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough! */

    bytesRead = sprintf( buf, "Copyright (C) 2002 VMIC\n" );
    if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough! */

    bytesRead = sprintf( buf, "International Copyright Secured.  All Rights Reserved.\n" );
    if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough! */

    bytesRead += sprintf( buf+bytesRead, "MODULE_NAME                %s\n",
        devname );
    if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough! */

    bytesRead += sprintf( buf+bytesRead, "RFM2G_DEVICE_COUNT         %d\n\n",
        rfm2g_device_count );
    if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough! */

    /* Show the contents of the RFM2GDEVICEINFO structures */
    for( i=0; i<rfm2g_device_count; i++ )
    {
        sprintf( device_num, "DEVICE_%d", i );

        bytesRead += sprintf( buf+bytesRead, "%s_BUS               %d\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.bus );

        bytesRead += sprintf( buf+bytesRead, "%s_FUNCTION          %d\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.function );

        bytesRead += sprintf( buf+bytesRead, "%s_DEVFN             0x%08X\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.devfn );

        bytesRead += sprintf( buf+bytesRead, "%s_TYPE              0x%04X\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.type );

        bytesRead += sprintf( buf+bytesRead, "%s_REVISION          %d\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.revision );

        bytesRead += sprintf( buf+bytesRead, "%s_OR_REGISTER_BASE  0x%08X\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.rfm2gOrBase );

        bytesRead += sprintf( buf+bytesRead, "%s_OR_REGISTER_SIZE  %d\n",
            device_num,
            (int) rfm2gDeviceInfo[i].Config.PCI.rfm2gOrWindowSize );

        bytesRead += sprintf( buf+bytesRead, "%s_CS_REGISTER_BASE  0x%08X\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.rfm2gCsBase );

        bytesRead += sprintf( buf+bytesRead, "%s_CS_REGISTER_SIZE  %d\n",
            device_num,
            (int) rfm2gDeviceInfo[i].Config.PCI.rfm2gCsWindowSize );

        bytesRead += sprintf( buf+bytesRead, "%s_MEMORY_BASE       0x%08X\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.PCI.rfm2gBase );

        bytesRead += sprintf( buf+bytesRead, "%s_MEMORY_SIZE       %d\n",
            device_num, (int) rfm2gDeviceInfo[i].Config.MemorySize );

        bytesRead += sprintf( buf+bytesRead, "%s_INTERRUPT         %d\n",
            device_num,
            (int) rfm2gDeviceInfo[i].Config.PCI.interruptNumber );

        bytesRead += sprintf( buf+bytesRead, "rfm2gDebugFlags            0x%08X\n",
            rfm2gDebugFlags);

        bytesRead += sprintf( buf+bytesRead, "RFM Flags                  0x%08X\n",
            rfm2gDeviceInfo[i].Flags);

        bytesRead += sprintf( buf+bytesRead, "Capability Flags           0x%08X\n",
            rfm2gDeviceInfo[i].Config.Capabilities);

        bytesRead += sprintf( buf+bytesRead, "Instance counter           %d\n",
            rfm2gDeviceInfo[i].Instance);

        bytesRead += sprintf( buf+bytesRead, "Queue Flags = " );

        for(j=0; j<RFM2GEVENT_LAST; j++)
        {
            bytesRead += sprintf( buf+bytesRead, "0x%02X  ",
                rfm2gDeviceInfo[i].EventQueue[j].req_header.reqh_flags);
        }

        bytesRead += sprintf( buf+bytesRead, "\n" );

        if( bytesRead > PAGE_SIZE-80 ) return( bytesRead ); /* This is enough!*/
    }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s: Exiting %s\n", devname, me);

    return( bytesRead );

}   /* End of RFM2gReadProcPage() */



/*****************************************************************************/

/* Define the proc page support structure */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)

struct proc_dir_entry rfm2gProcEntry =
{
    low_ino:    0,                      /* Inode, set by kernel         */
    namelen:    DEVNAME_SIZE,           /* Size of name                 */
    name:       "rfm2g",                /* Name of page                 */
    mode:       S_IFREG | S_IRUGO,      /* Mode                         */
    nlink:      1,                      /* Nlinks                       */
    uid:        0,                      /* user                         */
    gid:        0,                      /* group                        */
    size:       0,                      /* Size (not used)              */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
    owner:      THIS_MODULE,            /* owner of the proc entry      */
#endif
    read_proc:  RFM2gReadProcPage,        /* Our read function            */

    /* The rest defaults to NULL */
};

#else

/*
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
*/
static char myBuff[1048];

static int rfm2gFSshow(struct seq_file *m, void *v)
{
	RFM2gReadProcPage(myBuff, 0, 0, 0, 0,0);
	seq_write(m, myBuff, strlen(myBuff));
	return 0;
}

static int Rfm2gFSOpen(struct inode *inode, struct  file *file)
{
	return single_open(file, rfm2gFSshow, NULL);
}

struct file_operations rfm2gFOS =
{
	.owner = THIS_MODULE,
	.open = Rfm2gFSOpen,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#endif
