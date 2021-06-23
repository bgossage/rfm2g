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
$Workfile: rfm2g_struct.h $
$Revision: 41413 $
$Modtime: 9/03/10 1:53p $
-------------------------------------------------------------------------------
    Description: RFM2G Library Structures and constants
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    12-OCT-2001  ML   1.0   Created.
    18 Jun 2008  EB   6.00  Changed type of the RFM2GDMAINFO struct BuffAddr
                            member to RFM2G_UINT64  This is to store the 64-bit
                            This change is in support of PAE (Physical Address
                            Extension)-enabled Linux.  Added mmapSem semaphore.
    06 Aug 2008  EB   6.04  Added SlidingWindowOffset to the RFM2GCONFIGLINUX
                            structure to keep a copy of the current sliding
                            window offset, needed when DMA is used with a
                            sliding window.
===============================================================================
*/


#ifndef __RFM2G_STRUCT_H
#define __RFM2G_STRUCT_H

#include "rfm2g_defs_linux.h"
#include "rfm2g_ioctl.h"

/*****************************************************************************/

#ifndef RFM2G_DYNAMIC_MAJOR_NUMBER
  #define RFM2G_DYNAMIC_MAJOR_NUMBER    0
#endif

#ifndef MAX_RFM2G_DEVICES
  #define MAX_RFM2G_DEVICES  5
#endif

/* The VMIC vendor ID */
#ifdef PCI_VENDOR_ID_VMIC
# undef PCI_VENDOR_ID_VMIC
#endif
#define PCI_VENDOR_ID_VMIC 0x114a

/* RFM2GEVENT_DMADONE is not in the RFM2GEVENTTYPE enumeration anymore, add it to the end. */
#define RFM2GEVENT_DMADONE (RFM2GEVENT_LAST)

/* INFINITE_TIMEOUT - RFM2G_INFINITE_TIMEOUT is defined in rfm2g_api.h */
#define INFINITE_TIMEOUT 0xFFFFFFFF

/* Linux has extra events, add 1 for RFM2GEVENT_DMADONE */
#define LINUX_RFM2G_NUM_OF_EVENTS (RFM2GEVENT_LAST + 1)

/*****************************************************************************/

/* Information about received or transmitted interrupt events */

typedef struct rfm2gLinuxEventInfo
{
    RFM2G_UINT32  ExtendedInfo;
    RFM2G_NODE    NodeId;
    RFM2G_UINT8   Event;
    RFM2G_UINT32  Timeout;
    RFM2G_UINT16  Unit;
    RFM2G_UINT16  Qflags;

} RFM2GEVENTINFOLINUX;

/*****************************************************************************/

/* Static Device Configuration Information */

typedef struct rfm2gConfigLinux
{
    RFM2G_UINT16   Size;               /* Size of structure                   */
    RFM2G_NODE     NodeId;             /* Node ID of board                    */
    RFM2G_UINT8    BoardId;            /* Board Type ID                       */
    RFM2G_UINT8    Unit;               /* Device unit number                  */
    RFM2G_UINT8    PlxRevision;
    RFM2G_UINT8    ExtendedAddressing; /* Are we using >16M addressing        */
    RFM2G_NODE     MaxNodeId;          /* Max Node ID supported by this board */
    RFM2G_UINT32   Capabilities;       /* Combination of RFMCAPFLAGS values   */
    RFM2G_UINT32   RegisterSize;       /* Size of Register Area               */
    RFM2G_UINT32   MemorySize;         /* Total Board Memory                  */
    char           Device[128];        /* Name of device file                 */
    RFM2G_UINT8    *pOrRegisters;      /* Mapped address of OR Registers      */
    RFM2G_UINT8    *pCsRegisters;      /* Mapped address of CS Registers      */
    RFM2G_UINT8    *pBaseAddress;      /* Mapped base address of RFM board    */
    RFM2G_UINT32   UserMemorySize;     /* Total Bytes of Available Memory     */
    RFM2G_UINT32   IntrCount;          /* Number of times Interrupts have been
                                          enabled                             */
    void*          SpinLock;           /* Protects critical sections          */
    RFM2GPCICONFIG PCI;                /* PCI Bus Specific Info               */
    RFM2G_BOOL     dmaByteSwap;        /* DMA byte swap on?                   */
    RFM2G_BOOL     pioByteSwap;        /* PIO byte swap on?                   */
    RFM2G_UINT8    RevisionId;         /* Current board revision/model        */
    RFM2G_UINT16   BuildId;   /* Build Id of the Current board revision/model */
    RFM2G_UINT32   SlidingWindowOffset; /* Offset of current sliding window   */
}  RFM2GCONFIGLINUX;

/* The bit flags for the RFM2GCONFIGLINUX Capabilities flag field */
#define RFM2G_SUPPORTS_DMA                (1<<0)
#define RFM2G_HAS_EXTENDED_ADDRESSING     (1<<1)
#define RFM2G_HAS_EXTENDED_INTERRUPTS     (1<<2)

/*****************************************************************************/

typedef struct rfm2gEventQueueHeader_s
{
    RFM2G_UINT8       reqh_head;         /* Queue Head                        */
    RFM2G_UINT8       reqh_tail;         /* Queue Tail                        */
    RFM2G_UINT8       reqh_size;         /* Current Size                      */
    RFM2G_UINT8       reqh_maxSize;      /* Maximum Size                      */
    RFM2G_UINT32      reqh_counter;      /* Number of received interrupts     */
    RFM2G_UINT32      reqh_timeout;      /* How long to wait on interrupt     */
    wait_queue_head_t reqh_wait;         /* For waiting on interrupts         */
    RFM2G_UINT8       reqh_qpeak;        /* Max number of enqueued interrupts */
    RFM2G_UINT8       reqh_flags;        /* Interrupt status flags            */
    RFM2G_UINT16      spare;             /* Alignment                         */
} rfm2gEventQueueHeader_t,*RFM2GEVENTQUEUE_HEADER;

/* reqh_flags */
#define EVENT_AWAIT     (1<<0)     /* User wants synchronous notification   */
#define EVENT_TIMEDOUT  (1<<1)     /* Notification timed out                */
#define EVENT_ENABLED   (1<<2)     /* Interrupt is enabled                  */
#define QUEUE_OVERFLOW  (1<<3)     /* Tail overwrote head                   */
#define EVENT_CANCELED  (1<<4)     /* Wait for Event Canceled               */

/*****************************************************************************/

/* Determine Max Queue Size based on header and elements. */
#define MAX_QUEUE_SIZE 1

/*****************************************************************************/

/* Now that we know the MAX_QUEUE_SIZE, define the interrupt event queue */
typedef struct rfm2gEventQueue_s
{
    rfm2gEventQueueHeader_t    req_header;
    RFM2GEVENTINFOLINUX        req_info[MAX_QUEUE_SIZE];

} rfm2gEventQueue_t,*RFM2GEVENTQUEUE;

/*****************************************************************************/

/* This structure contains all information about an RFM device */

typedef struct
{
    RFM2G_UINT32        Flags;
    RFM2G_UINT32        Instance;/*rfm2gOpen increments, rfm2gClose decrements*/
    RFM2GCONFIGLINUX    Config;
    void               *DMASem;	    /* one DMA at a time */
    void               *mmapSem; /* One process at a time may map DMA buffers */
    rfm2gEventQueue_t   EventQueue[LINUX_RFM2G_NUM_OF_EVENTS];

#ifdef CONFIG_DEVFS_FS
    devfs_handle_t      handle;     /* device handle for devfs */
#endif

    RFM2G_UINT8         unit; /*Minor device number, used as index in rfm2gDeviceInfo*/

} RFM2GDEVICEINFO,*PRFM2GDEVICEINFO;

/* Flags */
#define RFM2G_PRESENT     (1<<0)  /* Device is present                */
#define RFM2G_OPEN        (1<<1)  /* Device is opened                 */
#define RFM2G_TIMEDOUT    (1<<2)  /* Timed out waiting on an interrupt*/
#define RFM2G_DMA_ON      (1<<3)  /* DMA is turned on                 */
#define RFM2G_DMA_DONE    (1<<4)  /* DMA transfer is complete         */
#define RFM2G_IN_USE      (1<<5)  /* Sync for read/write              */

/****************************************************************************
   This structure contains the DMA settings.  The address of one is stored in
   filp->private_data so that each call to open() can have its own buffer and
   threshold
*/
typedef struct
{
    RFM2G_UINT32        BuffSize;
    RFM2G_UINT64        BuffAddr;
    RFM2G_UINT32        Threshold;
    RFM2G_UINT32        InUseAction;
    RFM2G_UINT32        VirtAddr;
} RFM2GDMAINFO,*PRFM2GDMAINFO;

/*** Prototypes **************************************************************/

/* Proc Page stuff */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
extern struct proc_dir_entry rfm2gProcEntry;
#else
extern struct file_operations rfm2gFOS;
#endif

/* PCI hardware probe stuff */
RFM2G_INT32 rfm2gCountDevices(RFM2G_INT32 maxCount, struct pci_dev *device_list[]);
RFM2G_INT32 rfm2gInitDevice(RFM2GDEVICEINFO *deviceInfo, struct pci_dev *device, RFM2G_UINT8 inst );
RFM2G_INT32 rfm2gSilenceDevice(RFM2GDEVICEINFO *deviceInfo, struct pci_dev *device, RFM2G_UINT8 inst );

/* Utility functions for interrupt support */
void InitEventQueues(RFM2G_UINT8 unit, RFM2G_UINT8 size);
RFM2G_INT32 EnableInterrupt(RFM2GEVENTINFOLINUX *eventinfo, RFM2G_BOOL locked);
RFM2G_INT32 DisableInterrupt(RFM2GEVENTINFOLINUX *eventinfo, RFM2G_BOOL locked);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
void rfm2g_interrupt(int irq, void *dev_id, struct pt_regs *regs);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
int rfm2g_interrupt(int irq, void *dev_id, struct pt_regs *regs);
#else
int rfm2g_interrupt(int irq, void *dev_id);
#endif

RFM2G_INT32 WaitForInterrupt(RFM2GEVENTINFOLINUX *eventinfo, unsigned long uiFlags);
RFM2G_INT32 DoDMAWithWaitForInt(RFM2GEVENTINFOLINUX *eventinfo, unsigned long uiFlags);
RFM2G_INT32 CancelInterrupt(RFM2GEVENTINFOLINUX *eventinfo, RFM2G_BOOL locked);

/* Standard file operations entry points */
int rfm2g_open(struct inode *inode, struct file *filp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
int rfm2g_ioctl( struct inode *inode, struct file *filp, unsigned int cmd,
    unsigned long arg );
#else
long rfm2g_ioctl(struct file *filp, unsigned int cmd, unsigned long arg );
#endif

int rfm2g_release(struct inode *inode, struct file *filp);
loff_t rfm2g_llseek( struct file *filp, loff_t offset, int whence );
ssize_t rfm2g_read( struct file *filp, char *buf, size_t count, loff_t *offset);
ssize_t rfm2g_write( struct file *filp, const char *buf, size_t count,
                     loff_t *offset );
int rfm2g_mmap(struct file *filp, struct vm_area_struct *vma);



/*** Globals *****************************************************************/

#ifdef ALLOC_DATA

  /* Our device name.  Make sure DEVNAME_SIZE is correct. */
  const char *devname = "rfm2g";

  /* Remember how many boards were detected in the system */
  RFM2G_INT32 rfm2g_device_count = 0;

  /* Maintain an array of RFM2GDEVICEINFO structures,
     indexed by minor number */
  RFM2GDEVICEINFO rfm2gDeviceInfo[MAX_RFM2G_DEVICES];

  /* PCI Device list of RFM2g devices */
  struct pci_dev *device_list[ MAX_RFM2G_DEVICES ];

  /* Dynamically assign a major device number */
  RFM2G_INT32 major_num = RFM2G_DYNAMIC_MAJOR_NUMBER;

#ifdef CONFIG_DEVFS_FS
  /* devfs auto dev creation support */
  devfs_handle_t        rfm2g_devfs_dir;
#endif

  /* Debug flags to control message verbosity */
  RFM2G_UINT32 rfm2gDebugFlags = 0;

  /* Our file operations table */
  struct file_operations rfm2g_fops =
  {
      owner:    THIS_MODULE,
      llseek:   rfm2g_llseek,
      read:     rfm2g_read,
      write:    rfm2g_write,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
      ioctl:    rfm2g_ioctl,
#else
      unlocked_ioctl: rfm2g_ioctl,
#endif
      mmap:     rfm2g_mmap,
      open:     rfm2g_open,
      release:  rfm2g_release
  };

#else
  extern char *devname;
  extern RFM2G_INT32 rfm2g_device_count;
  extern RFM2GDEVICEINFO rfm2gDeviceInfo[];
  extern RFM2G_INT32 major_num;
  extern RFM2G_UINT32 rfm2gDebugFlags;
  extern struct file_operations rfm2g_fops;
#endif

#define DEVNAME_SIZE 5

/*****************************************************************************/

typedef enum rfm2gDiagMode
{
  RFM2GDIAG_NORMAL,       /* Normal Ops   */
  RFM2GDIAG_MONITOR,      /* Monitor Mode */
  RFM2GDIAG_DISCONNECT    /* Disconnect   */
} RFM2GDIAGMODE;


/*****************************************************************************/

typedef struct rfm2gVersion
{
   RFM2G_UINT8 Size;
   RFM2G_UINT8 Major;
   RFM2G_UINT8 Minor;

}  RFM2GVERSION;


/*****************************************************************************/

/* Node Ranges and Special Values */

static const RFM2G_UINT32 RFM2G_DMA_THRESHOLD_MAX = 0xFFFFFFFF; /* default DMA threshold */

#define WHENDEBUG(x)        if( (x) & rfm2gDebugFlags )

#endif  /* __RFM2G_STRUCT_H */
