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
$Workfile: device_probe.c $
$Revision: 40622 $
$Modtime: 9/03/10 1:16p $
-------------------------------------------------------------------------------
    Description: Detect Reflective Memory boards on the PCI bus (Linux)
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    13-NOV-2001  ML   1.0   Created.
    08-DEC-2004  ML   01.04 Ported to 2.6 Kernel. Use pci_find_device() instead
                            of walking obsolete pci_devices list.  pci_present()
                            also removed.

===============================================================================
*/
#include "rfm2g_driver.h"

# define pci_present pcibios_present

/*****************************************************************************/

/* Define a structure with device specific information */
typedef struct
{
    RFM2G_UINT16 deviceID;
}  RFM2G_BOARDTYPE_INFO;

/* Add the device IDs of newly supported boards here */
/* The PCI, CPCI, and PMC ID's are the same for each model number */
RFM2G_BOARDTYPE_INFO rfm2g_boardtype_info[] =
{
    { DEVICE_ID_PCI5565 }
};

/* Remember how many different boards we support */
RFM2G_INT32 rfm2g_boardtype_count = sizeof( rfm2g_boardtype_info ) /
    sizeof( RFM2G_BOARDTYPE_INFO );


/******************************************************************************
*
*  FUNCTION:   rfm2GCountDevices
*
*  PURPOSE:    Counts all Reflective Memory devices on the PCI bus and saves
*              a pointer to their pci_dev structures.
*  PARAMETERS: maxCount: (I) The max number of RFM boards controlled at one
*                  time by this driver.
*              device_list: (IO) A list of pointers to structures describing
*                  RFM boards
*  RETURNS:    The number of Reflective memory devices found on the PCI bus
*  GLOBALS:    devname, rfm2g_boardtype_count
*
******************************************************************************/

RFM2G_INT32
rfm2gCountDevices( RFM2G_INT32 maxCount, struct pci_dev *device_list[] )
{
    static char *me = "rfm2gCountDevices()";
    RFM2G_INT32 found = 0;           /* How many rfm devices found so far */
    struct pci_dev *dev_ptr;	   /* For current entry in list during traversal */

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s: Entering %s\n", devname, me);
    dev_ptr = NULL;

    while ( (dev_ptr = pci_get_device(PCI_VENDOR_ID_VMIC, DEVICE_ID_PCI5565, dev_ptr)) )
    {
        /* Remember the pointer to the pci_dev structure*/
        device_list[found] = dev_ptr;
        found++;
        if( found > maxCount )
            return( maxCount );
   }

    WHENDEBUG(RFM2G_DBTRACE) printk(KERN_ERR"%s: Exiting %s\n", devname, me);

    return( found );

}   /* End of rfm2gCountDevices() */
