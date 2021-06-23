/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2001, 2006-2010 GE Intelligent Platforms Embedded Systems, Inc.
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

    $Workfile: rfm2g_util.c $
    $Revision: 40671 $
    $Modtime: 2/25/10 6:21p $

-------------------------------------------------------------------------------
    Description: Diagnostic Utility for the PCI RFM2g Device Driver
-------------------------------------------------------------------------------

===============================================================================
*/

#if defined(RFM2G_VXWORKS)
    #include "vxWorks.h"
#endif

#if defined(WIN32)
    #include "rfm2g_windows.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "rfm2g_api.h"

#if (defined(RFM2G_LINUX))

#ifdef CONFIG_DEVFS_FS
    #define DEVICE_PREFIX   "/dev/rfm2g/"
#else
    #define DEVICE_PREFIX   "/dev/rfm2g"
#endif

    #define PROCFILE         "/proc/rfm2g"

#elif defined(RFM2G_VXWORKS)

    #define DEVICE_PREFIX   "RFM2G_"

#elif defined(SOLARIS)

    #define DEVICE_PREFIX   "/dev/rfm2g"

#elif defined(WIN32)

    #define DEVICE_PREFIX   "\\\\.\\rfm2g"
#else

    #error Please define DEVICE_PREFIX for your driver
#endif

#define CMD_PROMPT_HDR_SIZE 20
#define LINES_PER_PAGE  16

#define MAXPARMCOUNT    12
#define MAXPARMSIZE     24

#define MEMBUF_SIZE     0x200000
#define RFM2G_FIXED_DATA			0x00
#define RFM2G_INCADDR_DATA			0x01
#define RFM2G_INC_DATA				0x02
#define RFM2G_INVINCADDR_DATA		0x03

#define RFM2G_SUPPORT_PERSONNEL_MSG "\nNote:  This command should not be used unless \
advised to do so by\nGE Intelligent Platforms Embedded Systems, Inc. support personnel.\n"


/*****************************************************************************/

/* A structure used for validating data widths */
typedef struct width_s
{
    RFM2G_UINT32 addrmask;
    RFM2G_UINT32 valmask;
    RFM2G_UINT32 width;
    char         type[16];
    RFM2G_UINT8  parm;

} WIDTH;

/*** Debug Flags *************************************************************/

typedef struct debug_info
{
    char       *name;
    RFM2G_UINT32 flag;

} DEBUGINFO;

static DEBUGINFO debugflags[] =
{
    { "allflags", 0xffffffff       },
    { "close",    RFM2G_DBCLOSE    },
	{ "dma",      RFM2G_DBDMA	   },
    { "error",    RFM2G_DBERROR    },
    { "init",     RFM2G_DBINIT     },
    { "intr",     RFM2G_DBINTR     },
    { "ioctl",    RFM2G_DBIOCTL    },
    { "minphys",  RFM2G_DBMINPHYS  },
    { "mmap",     RFM2G_DBMMAP     },
    { "mutex",    RFM2G_DBMUTEX    },
    { "not_intr", RFM2G_DBINTR_NOT },
    { "open",     RFM2G_DBOPEN     },
    { "peek",     RFM2G_DBPEEK     },
    { "poke",     RFM2G_DBPOKE     },
    { "read",     RFM2G_DBREAD     },
    { "slow",     RFM2G_DBSLOW     },
    { "strat",    RFM2G_DBSTRAT    },
    { "timer",    RFM2G_DBTIMER    },
    { "trace",    RFM2G_DBTRACE    },
    { "write",    RFM2G_DBWRITE    },
    { "",         0                }  /* Must have even number of flags */
};

typedef struct eventName_info
{
    char       *name;
    RFM2GEVENTTYPE EventType;

} EVENTNAMEINFO;

/* Names for the interrupt event types */
#define event_name_array_num_elements  RFM2GEVENT_LAST

#define DEBUGFLAGCOUNT  (sizeof( debugflags ) / sizeof( DEBUGINFO ))

/* Keep the following structure in sync with vmeRfm2g.h */
static const EVENTNAMEINFO event_name[] =
{
    { "RESET",                  RFM2GEVENT_RESET },
    { "NETWORK INT 1",          RFM2GEVENT_INTR1 },
    { "NETWORK INT 2",          RFM2GEVENT_INTR2 },
    { "NETWORK INT 3",          RFM2GEVENT_INTR3 },
    { "NETWORK INT 4",          RFM2GEVENT_INTR4 },
    { "BAD DATA",               RFM2GEVENT_BAD_DATA },
    { "RX FIFO FULL",           RFM2GEVENT_RXFIFO_FULL},
    { "ROGUE PACKET",           RFM2GEVENT_ROGUE_PKT},
    { "RX FIFO ALMOST FULL",    RFM2GEVENT_RXFIFO_AFULL},
    { "SYNC LOSS",              RFM2GEVENT_SYNC_LOSS},
    { "MEM WRITE INHIBITED",    RFM2GEVENT_MEM_WRITE_INHIBITED},
    { "LOCAL MEM PARITY ERROR", RFM2GEVENT_LOCAL_MEM_PARITY_ERR},
    { "ALL EVENTS",             RFM2GEVENT_LAST}
};

/* Counter used by callback function. */
static RFM2G_UINT32 callbackCounter[RFM2GEVENT_LAST];


/*****************************************************************************/

/* The shape of a command message */

typedef struct diag_cmd_s
{
    RFM2G_UINT8    MemBuf[MEMBUF_SIZE];    /* Buffer for reads and writes */
    RFM2G_UINT8    command;
    RFM2G_UINT8    parmcount;
    RFM2G_UINT16   spare;
    char           parm[MAXPARMCOUNT][MAXPARMSIZE];

    RFM2GHANDLE     Handle;
    RFM2G_INT32     running; /* Set RFM2G_TRUE upon startup, set RFM2G_FALSE to exit main()*/
    RFM2G_INT32     drvspecrun;
    RFM2G_UINT32    DmaBase;  /* Physical address of DMA buffer stored here */
    RFM2G_UINT32    DmaRange; /* Size of DMA buffer stored here (bytes) */
    RFM2G_UINT32    DmaPages; /* # Pages mapped */
    volatile char  *MappedDmaBase;  /* Mapped pointer to DMA buffer base stored here*/
    char            notify_state[RFM2GEVENT_LAST]; /* Enabled async notify */
    char            enable_state[RFM2GEVENT_LAST]; /* Enabled async notify */
    struct cmdinfo* pCmdInfoList;
    RFM2G_INT32     NumEntriesCmdInfoList;
    char*           pUtilPrompt;
    RFM2G_UINT8     deviceNum;
} DIAGCMD;

/* The command names, function bindings, and descriptions */

typedef struct cmdinfo
{
    char * name;
    char * desc;
    RFM2G_INT32  (*func)(DIAGCMD *, RFM2G_UINT8 usage);
    RFM2G_UINT8  parmcount;
    RFM2G_UINT8  repeatable;
	RFM2G_UINT8  display;

} CMDINFO;

/*** Local Prototypes ********************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/* Function prototypes for commands */
/* See implementation for a simple example of how to use
   each command */
static RFM2G_INT32  doBoardId( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doCancelWait( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doConfig( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doCheckRing( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doClearOwnData( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doClearEventCount( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doClearReg( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDevName( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDisableEvent( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDisableCallback( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDisplayReg( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDllVersion( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDriverVersion( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doEnableEvent( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doEnableCallback( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doErrorMsg( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doExit( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doFirst( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doClearEvent( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetDarkOnDark( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetDebug( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetDmaByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetEventCount( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetLed( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetLoopback( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetMemoryOffset( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetParityEnable( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetPioByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetThreshold( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doGetTransmit( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doHelp( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doMapUser( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doMapUserBytes( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doNodeId( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPeek8( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPeek16( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPeek32( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPeek64( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPoke8( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPoke16( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPoke32( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doPoke64( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doRegRead( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doRegWrite( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doRead( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doRepeat( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSend( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetDarkOnDark( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetDebug( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetDmaByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetLed( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetLoopback( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetMemoryOffset( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetParityEnable( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetPioByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetThreshold( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetTransmit( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSetReg( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSize( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSlidingWindowGet( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doSlidingWindowSet( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doUnMapUser( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doUnMapUserBytes( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doWait( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doWrite( DIAGCMD *cmd, RFM2G_UINT8 usage );

/* The following is not an API function, but provides the user
   a simple performance tool */
static RFM2G_INT32  doPerformance( DIAGCMD *cmd, RFM2G_UINT8 usage );

/* The following are not API functions, but provide the user
   useful functionality for testing */
static RFM2G_INT32  doDump( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doMemop( DIAGCMD *cmd, RFM2G_UINT8 usage );

static RFM2G_INT32  doFlashCard( DIAGCMD *cmd, RFM2G_UINT8 usage );

/* Function prototypes for internal utilities */
static RFM2G_INT32  GetCommand( DIAGCMD *cmd );
static RFM2G_INT32  ValidCmd(DIAGCMD *cmd, char *comand, RFM2G_INT32 matches[] );
static RFM2G_INT32  ValidFlag( char *cmd, RFM2G_INT32 matches[], RFM2G_INT32 *state );
static RFM2G_INT32  ValidWidth( WIDTH *w );
#if (defined(RFM2G_LINUX))
static char*        RfmBoardName( RFM2G_UINT8 bid );
#endif
static void         EventCallback( RFM2GHANDLE rh, RFM2GEVENTINFO *EventInfo );
static RFM2G_INT32  get64bitvalue(char* pValue64Str, RFM2G_UINT64* pValue64);

/* Common rfm2g_utilosspec.c function prototypes */
static RFM2G_INT32  doExitDrvSpecific( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doReturnDrvSpecific( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doDrvSpecific( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doInitDrvSpecific( DIAGCMD *cmd, RFM2G_UINT8 usage );

#ifdef __cplusplus
}
#endif

#include "rfm2g_utilosspec.c"

/*****************************************************************************/

/* A list of commands. */
static CMDINFO commandlist[] =
{
	/* name               description                 function    parcount repeatable display */
    { "boardid         ", " ",                        doBoardId,        0, 1, 1 },
    { "cancelwait      ", "event",                    doCancelWait,     1, 0, 1 },
    { "checkring       ", " ",                        doCheckRing,      0, 2, 1 },
    { "clearevent      ", "event",                    doClearEvent,     1, 1, 1 },
    { "cleareventcount ", "event",                    doClearEventCount,1, 1, 1 },
    { "clearowndata    ", " ",                        doClearOwnData,   0, 1, 1 },
    { "clearreg        ", "regset offset width mask", doClearReg,       4, 2, 0 },
    { "config          ", " ",                        doConfig,         0, 2, 1 },
    { "devname         ", " ",                        doDevName,        0, 1, 1 },
    { "disableevent    ", "event",                    doDisableEvent,   1, 0, 1 },
    { "disablecallback ", "event",                    doDisableCallback,1, 0, 1 },
    { "dllversion      ", " ",                        doDllVersion,     0, 1, 1 },
    { "driverversion   ", " ",                        doDriverVersion,  0, 1, 1 },
    { "drvspecific     ", " ",                        doDrvSpecific,    0, 0, 1 },
    { "dump            ", "offset width length",      doDump,           3, 2, 1 },
    { "displayreg      ", "regset offset width length", doDisplayReg,   4, 2, 0 },
    { "enableevent     ", "event",                    doEnableEvent,    1, 0, 1 },
    { "enablecallback  ", "event",                    doEnableCallback, 1, 0, 1 },
    { "errormsg        ", "ErrorCode",                doErrorMsg,       1, 0, 1 },
    { "exit            ", " ",                        doExit,           0, 0, 1 },
    { "first           ", " ",                        doFirst,          0, 1, 1 },
    { "getdarkondark   ", " ",                        doGetDarkOnDark,  0, 1, 1 },
    { "getdebug        ", " ",                        doGetDebug,       0, 1, 1 },
    { "getdmabyteswap  ", " ",                        doGetDmaByteSwap, 0, 1, 1 },
    { "geteventcount   ", "event",                    doGetEventCount,  1, 1, 1 },
    { "getled          ", " ",                        doGetLed,         0, 1, 1 },
    { "getmemoryoffset ", " ",                        doGetMemoryOffset,0, 1, 1 },
    { "getloopback     ", " ",                        doGetLoopback,    0, 1, 1 },
    { "getparityenable ", " ",                        doGetParityEnable,0, 1, 1 },
    { "getpiobyteswap  ", " ",                        doGetPioByteSwap, 0, 1, 1 },
    { "getslidingwindow", " ",                        doSlidingWindowGet, 0, 1, 1},
    { "getthreshold    ", " ",                        doGetThreshold,   0, 1, 1 },
    { "gettransmit     ", " ",                        doGetTransmit,    0, 1, 1 },
    { "help            ", "[command]",                doHelp,           1, 0, 1 },
    { "mapuser         ", "offset pages",             doMapUser,        2, 0, 1 },
    { "mapuserbytes    ", "offset bytes",             doMapUserBytes,   2, 0, 1 },
    { "memop           ", "pattern offset width length verify float patterntype", doMemop,7, 2, 1 },
    { "nodeid          ", " ",                        doNodeId,         0, 1, 1 },
    { "peek8           ", "offset",                   doPeek8,          1, 2, 1 },
    { "peek16          ", "offset",                   doPeek16,         1, 2, 1 },
    { "peek32          ", "offset",                   doPeek32,         1, 2, 1 },
    { "peek64          ", "offset",                   doPeek64,         1, 2, 1 },
    { "performancetest ", " ",                        doPerformance,    0, 2, 1 },
    { "poke8           ", "value offset",             doPoke8,          2, 2, 1 },
    { "poke16          ", "value offset",             doPoke16,         2, 2, 1 },
    { "poke32          ", "value offset",             doPoke32,         2, 2, 1 },
    { "poke64          ", "value offset",             doPoke64,         2, 2, 1 },
    { "quit            ", " ",                        doExit,           0, 0, 1 },
    { "read            ", "offset width length display",doRead,         4, 2, 1 },
    { "readreg         ", "regset offset width",      doRegRead,        3, 2, 0 },
    { "repeat          ", "[-p] count cmd [arg...]",  doRepeat,         3, 0, 1 },
    { "send            ", "event tonode [ext_data]",  doSend,           3, 2, 1 },
    { "setdarkondark   ", "state",                    doSetDarkOnDark,  1, 1, 1 },
    { "setdebug        ", "flag",                     doSetDebug,       1, 1, 1 },
    { "setdmabyteswap  ", "state",                    doSetDmaByteSwap, 1, 1, 1 },
    { "setled          ", "state",                    doSetLed,         1, 1, 1 },
    { "setloopback     ", "state",                    doSetLoopback,    1, 1, 1 },
    { "setmemoryoffset ", "offset",                   doSetMemoryOffset,1, 1, 1 },
    { "setparityenable ", "state",                    doSetParityEnable,1, 1, 1 },
    { "setpiobyteswap  ", "state",                    doSetPioByteSwap, 1, 1, 1 },
    { "setreg          ", "regset offset width mask", doSetReg,         4, 2, 0 },
    { "setslidingwindow", "offset",                   doSlidingWindowSet, 1, 1, 1},
    { "setthreshold    ", "value",                    doSetThreshold,   1, 1, 1 },
    { "settransmit     ", "state",                    doSetTransmit,    1, 1, 1 },
    { "size            ", " ",                        doSize,           0, 1, 1 },
    { "unmapuser       ", " ",                        doUnMapUser,      0, 0, 1 },
    { "unmapuserbytes  ", " ",                        doUnMapUserBytes, 0, 0, 1 },
    { "wait            ", "event timeout",            doWait,           2, 2, 1 },
    { "write           ", "value offset width length",doWrite,          4, 2, 1 },
    { "writereg        ", "regset offset width value",doRegWrite,       4, 2, 0 },
	{ "flashcard       ", "",						  doFlashCard,      0, 0, 0 }
};

/******************************************************************************
*
*  FUNCTION:   main
*
*  PURPOSE:    Process commands entered by the user
*  PARAMETERS: argc, argv (unused)
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, running
*
******************************************************************************/

#if (defined(RFM2G_VXWORKS))

RFM2G_INT32
rfm2g_util(void)

#else

int
main( int argc, char *argv[] )

#endif
{
    DIAGCMD*        pCmd;           /* Commandline info entered by user         */
    char            device[80];     /* Name of PCI RFM2G device to use          */
    RFM2G_INT32     i;              /* Loop variable                            */
    RFM2G_STATUS    status;
    int             numDevice = 0;

#if (defined(RFM2G_LINUX))
    FILE *          fp;             /* Open the PROCFILE and get device count   */
    RFM2G_INT32     count;
    char            temp[80];
    char            string[80];
	RFM2GHANDLE		Handle;
#endif

    pCmd = malloc(sizeof(DIAGCMD));
    if (NULL == pCmd)
        {
        printf("pCmd = NULL, malloc(%d) failed!\n", (int) sizeof(DIAGCMD) );
        return (-1);
        }
    memset( (void *)pCmd,  0, sizeof(DIAGCMD)  );

    /* initialize the DMA buffer variables to NULL */
    pCmd->DmaBase = 0;
    pCmd->DmaRange = 0;
    pCmd->DmaPages = 0;
    pCmd->MappedDmaBase = NULL;
    for (i=0; i < event_name_array_num_elements; i++)
    {
        pCmd->notify_state[i] = 0;
        pCmd->enable_state[i] = 0;
        callbackCounter[i]  = 0;
    }

    /* Initialize pCmdInfoList to main menu */
    pCmd->pCmdInfoList = commandlist;
    pCmd->NumEntriesCmdInfoList = sizeof(commandlist) / sizeof(CMDINFO);
    pCmd->pUtilPrompt = malloc(CMD_PROMPT_HDR_SIZE);
    if (NULL == pCmd->pUtilPrompt)
        {
        printf("pCmd->pUtilPrompt = NULL, malloc(%d) failed!\n", CMD_PROMPT_HDR_SIZE );
        free (pCmd);
        return (-1);
        }
    memset( (void *) pCmd->pUtilPrompt,  0, CMD_PROMPT_HDR_SIZE );

    printf("\n  PCI RFM2g Commandline Diagnostic Utility\n\n");

#if (defined(RFM2G_LINUX))
    /* Open the proc file */
    fp = fopen( PROCFILE, "r" );
    if( fp == 0 )
    {
        printf( "Unable to open \"%s\".\n", PROCFILE );
        return(-1);
    }

    while( fgets(string, sizeof(string), fp) != NULL )
    {
        if( strncmp(string, "RFM2G_DEVICE_COUNT", 16) == 0 )
        {
           sscanf( string, "%s %d", temp, &count );
            break;
        }
    }

    fclose(fp);

	/* Display a list of RFM2g  devices */
	printf( "  Available devices:\n\n" );
	for( i=0; i<count; i++ )
	{
        sprintf( string, "%s%d", DEVICE_PREFIX, i);

		/* Open the RFM2g driver */
		status = RFM2gOpen( string, &Handle);
		if( status != RFM2G_SUCCESS )
		{
			printf( "Unable to open \"%s\".\n", string );
			printf( "Error: %d.\n\n",  status);
	        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
			return(-1);
		}

		printf( "    %d.  %s  (%s, Node %d)\n", i, string,
			RfmBoardName( Handle->Config.BoardId ),
			Handle->Config.NodeId);

		RFM2gClose( &Handle );
	}

#endif

    clearerr(stdin);

    printf("Please enter device number: ");

    while ((fgets( device, sizeof(device), stdin ) == (char *) NULL ) || (strlen(device) < 2))
    {
    }

    sscanf(device, "%i", &numDevice);

    /* if sscanf fails, then numDevice will stay 0 */
    sprintf(device, "%s%d", DEVICE_PREFIX, numDevice);

    pCmd->deviceNum = numDevice;
    pCmd->Handle = 0;

    sprintf(pCmd->pUtilPrompt,"UTIL%d > ", pCmd->deviceNum);

    /* Open the PCI RFM2G driver */
    status = RFM2gOpen( device, &pCmd->Handle);
    if( status != RFM2G_SUCCESS )
    {
        printf( "Unable to open \"%s\".\n", device );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        free(pCmd->pUtilPrompt);
        free(pCmd);
        return(-1);
    }

    if (pCmd->Handle == NULL)
    {
        printf( "Error opening \"%s\".\n", device );
        printf( "Handle returned is NULL!\n\n");
        free(pCmd->pUtilPrompt);
        free(pCmd);
        return(-1);
    }

    /* Call driver specific initialization */
    doInitDrvSpecific(pCmd, 0);

    /* Process user input */
    for( pCmd->running=1; pCmd->running; )
    {
        GetCommand(pCmd);
        if (pCmd->running)
        {
             (pCmd->pCmdInfoList[pCmd->command].func)( pCmd, 0 );
        }
    }

    printf("\n");

    status = RFM2gClose( &pCmd->Handle );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Unable to close \"%s\".\n", device );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
	    pCmd->Handle = 0;
        free(pCmd->pUtilPrompt);
        free(pCmd);
        return(-1);
    }
    else
    {
	    pCmd->Handle = 0;
        free(pCmd->pUtilPrompt);
        free(pCmd);
        return(0);
    }

}   /* End of main() */


/******************************************************************************
*
*  FUNCTION:   GetCommand
*
*  PURPOSE:    Accepts a command from the user
*  PARAMETERS: cmd: (O) Command code and command parameters
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    commandlist
*
******************************************************************************/

static RFM2G_INT32
GetCommand( DIAGCMD *cmd )
{
    static char string[80];          /* User input                         */
    static char *args[MAXPARMCOUNT]; /* Pointers to commandline tokens     */
    RFM2G_INT32  length;             /* Length on input string             */
    RFM2G_INT32  i;                  /* Loop variable                      */
    RFM2G_INT32  count;              /* Number of commandline tokens       */
    char *c;                         /* Temp pointer to one token          */
    RFM2G_INT32  done = 0;           /* A valid command stops the while    */
    RFM2G_INT32  match;              /* Incremented on valid commands      */
    RFM2G_INT32  matches[20];        /* commandlist indices matching input */

    while( !done )
    {
        /* Initialization */
        memset( (void *)string,  0, sizeof(string)  );
        memset( (void *)args,    0, sizeof(args) );
        memset( (void *)matches, 0, sizeof(matches) );
        cmd->command   = 0;
        cmd->parmcount = 0;
        cmd->spare     = 0;

        /* Get user input */
        fflush(stdin);

        printf( "%s", cmd->pUtilPrompt );

        if( fgets( string, sizeof(string), stdin ) == (char *) NULL )
        {
            cmd->running = 0;
            return(-1);
        }

        /* Remove the newline from the end of the string */
        length = (RFM2G_INT32)strlen( string );
        string[ --length ] = '\0';
        if( length == 0 )
        {
            continue;
        }

        /* Parse the user input */
        for( i=0, count=0; (i < length) && (count < MAXPARMCOUNT); i++ )
        {
            /* Move to the next nonspace character */
            while( isspace( (int) string[i]) )
            {
                i++;
            }

            /* Save a pointer to the beginning of the token */
            if( string[i] != '\0' )
            {
                args[count++] = &string[i];
            }

            /* Move to the next whitespace character */
            while( string[i] && !isspace( (int) string[i]) )
            {
                i++;
            }
        }

        /* Terminate the tokens */
        for( i=0; i<count; i++ )
        {
            c = args[i];

            while( *c && !isspace( (int) *c) )
            {
                c++;
            }

            *c = '\0';
        }

        /* Validate the command */
        if( count > 0 )
        {
            match = ValidCmd( cmd, args[0], matches);
            if( match == 1)
            {
                int argcount;
                cmd->command = (RFM2G_UINT8)matches[0];
                cmd->parmcount = (RFM2G_UINT8)count-1;

                /* Initialize parameters for the command */
                for (argcount = 1; argcount < MAXPARMCOUNT; argcount++)
                {
                    if ( args[argcount] )
                    {
                        strncpy( cmd->parm[argcount - 1], args[argcount], MAXPARMSIZE );
                    }
                }

                done = 1;
            }
        }
    }

    return(0);

}   /* End of GetCommand() */

/******************************************************************************
*
*  FUNCTION:   ValidCmd
*
*  PURPOSE:    Validates cmd by comparing against commandlist names.
*  PARAMETERS: cmd: (I) string containing command name
*              matches: (O) Array of indices of successful matches
*  RETURNS:    Number of successful matches
*  GLOBALS:    commandlist
*
******************************************************************************/

static RFM2G_INT32
ValidCmd(DIAGCMD *cmd, char *command, RFM2G_INT32 matches[])
{
    RFM2G_INT32 i;
    RFM2G_INT32 match;
    char *pcmd;


    /* The command should be lowercase before doing the validation */
    for( i=0; i < (RFM2G_INT32)strlen(command); i++ )
    {
        command[i] = tolower( (int)command[i] );
    }


    pcmd = &command[0];

    /* Compare input to valid command names */
    for( i=0, match=0; i < cmd->NumEntriesCmdInfoList; i++ )
    {
        if( ( strncmp( cmd->pCmdInfoList[i].name, pcmd, strlen(pcmd) ) == 0 ) )
        {
			/* test the next char as we may have a match with a longer name */
			if (cmd->pCmdInfoList[i].name[strlen(pcmd)] == ' ')
			{
				/* We have the exact command now - return */
				matches[0] = i;
				return 1;
			}

			if( matches != (RFM2G_INT32 *) NULL )
            {
                matches[match] = i;
            }

            ++match;
        }
    }

    if( match == 0 )
    {
        printf("Invalid command \"%s\".\n", pcmd );
    }
    else if( match > 1 )
    {
        printf( "Ambiguous command \"%s\".  This could be\n", pcmd );
        for( i=0; i<match; i++ )
        {
            printf( "  \"%s\"\n", cmd->pCmdInfoList[matches[i]].name );
        }

    }

    return( match );

}   /* End of ValidCmd() */

/******************************************************************************
*
*  FUNCTION:   ValidFlag
*
*  PURPOSE:    Validates flag by comparing against debug flag names.
*  PARAMETERS: flag: (I) string containing command name
*              matches: (O) Array of indices of successful matches
*              state: (O) Indicates if the flag is to be set or cleared
*  RETURNS:    Number of successful matches
*  GLOBALS:    debugflags
*
******************************************************************************/

static RFM2G_INT32
ValidFlag( char *flag, RFM2G_INT32 matches[], RFM2G_INT32 *state )
{
    RFM2G_INT32 i;
    RFM2G_INT32 match;
    char *string;


    /* The command should be lowercase before doing the validation */
    for( i=0; i < (RFM2G_INT32)strlen(flag); i++ )
    {
        flag[i] = tolower( (int)flag[i] );
    }

    /* Check for a minus sign to indicate this flag will be cleared */
    if( flag[0] == '-' )
    {
        *state = 0;
        string = &flag[1];
    }
    else
    {
        *state = 1;
        string = &flag[0];
    }

    /* Compare input to valid command names */
    for( i=0, match=0; i<DEBUGFLAGCOUNT; i++ )
    {
        if( strncmp( debugflags[i].name, string, strlen(string) ) == 0 )
        {
            if( matches != (RFM2G_INT32 *) NULL )
            {
                matches[match] = i;
            }

            ++match;
        }
    }

    if( match == 0 )
    {
        printf("Invalid flag \"%s\".\n", string );
    }
    else if( match > 1 )
    {
        printf( "Ambiguous flag \"%s\".  This could be\n", string );
        for( i=0; i<match; i++ )
        {
            printf( "  \"%s\"\n", debugflags[matches[i]].name );
        }

    }

    return( match );

}   /* End of ValidFlag() */


/******************************************************************************
*
*  FUNCTION:   ValidWidth
*
*  PURPOSE:    Validate a width value and set various masks accordingly
*  PARAMETERS: w: (IO) Width structure is initialized if the parm is valid
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    None
*
******************************************************************************/

static RFM2G_INT32
ValidWidth( WIDTH *w )
{
    if( w == (WIDTH *) NULL )
    {
        return(1);
    }

    switch( w->parm )
    {
        case 1:
            w->addrmask = 0;
            w->valmask  = 0xff;
            strcpy( w->type, "byte" );
            w->width = RFM2G_BYTE;
            break;
        case 2:
            w->addrmask = 1;
            w->valmask  = 0xffff;
            strcpy( w->type, "word" );
            w->width = RFM2G_WORD;
            break;
        case 4:
            w->addrmask = 3;
            w->valmask  = 0xffffffff;
            strcpy( w->type, "longword" );
            w->width = RFM2G_LONG;
            break;
        case 8:
            w->addrmask = 4;
            w->valmask  = 0xffffffff;
            strcpy( w->type, "longlongword" );
            w->width = RFM2G_LONGLONG;
            break;
        default:
            return(-1);
    }

    return(0);

}   /* End of ValidWidth() */


/******************************************************************************
*
*  FUNCTION:   RfmBoardName
*
*  PURPOSE:    Returns model number of PCI RFM board based on board ID
*  PARAMETERS: bid: (I) Board ID value
*  RETURNS:    Pointer to string containing RFM model number
*  GLOBALS:    None
*
******************************************************************************/
#if (defined(RFM2G_LINUX))
static char *
RfmBoardName( RFM2G_UINT8 bid )
{
    static char *name;

    switch( bid )
    {
        case 0x65: name = "VMIPCI-5565"; break;
        default:   name = "UNKNOWN"; break;
    }

    return( name );

}   /* End of RfmBoardName() */
#endif

/* Commnand function implementations */

/******************************************************************************
*
*  FUNCTION:   doBoardId
*
*  PURPOSE:    Display the VME RFM's Board Id
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doBoardId( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_UINT8 BoardId = 0;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the Reflective Memory board's Board ID\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get the Board ID */
    Status = RFM2gBoardID( cmd->Handle, &BoardId );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the Board ID.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print Board ID and description of board */
    printf ("    Board ID                   0x%02X    (", BoardId);
    switch( BoardId )
    {
        case 0x65:
            printf ("VMIPCI-5565");
            break;
        default:
            printf("unknown");
            return(-1);
    }
    printf(")\n\n");

    return(0);

}   /* End of doBoardId() */

/******************************************************************************
*
*  FUNCTION:   doCancelWait
*
*  PURPOSE:    Cancel pending RFM2gWaitForEvent for one event
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doCancelWait( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;  /* Selects appropriate nomenclature          */
    char *          name;   /* Local pointer to command name             */
    char *          desc;   /* Local pointer to command description      */
    RFM2G_INT32     i;      /* Loop variables                            */
    unsigned int    event;  /* Interrupt event                       */
    RFM2G_STATUS    status;


    index = cmd->command;
    name = cmd->pCmdInfoList[index].name;
    desc = cmd->pCmdInfoList[index].desc;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &event) != 1) ||
        (event >= event_name_array_num_elements))
    {
        printf( "\n%s:  Cancel pending RFM2gWaitForEvent\n", name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "Cancel pending RFM2gWaitForEvent by entering "
            "\"%s  event\"\n  where \"event\" is one of the ", name );
        printf( "following interrupt events [0-%d]:\n\n", event_name_array_num_elements-1 );

        for( i=RFM2GEVENT_RESET; i < event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %s\n", i, event_name[i].name );
        }
        printf( "\n");

        return(0);
    }

    status = RFM2gCancelWaitForEvent ( cmd->Handle, event_name[event].EventType );
    if( status != RFM2G_SUCCESS)
    {
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }
    else
    {
        printf( "RFM2gWaitForEvent has been "
            "canceled for the\n   \"%s\" event.\n",
            event_name[event].name );
    }

    return(0);

}   /* End of doCancelWait() */

/******************************************************************************
*
*  FUNCTION:   doClearEventCount
*
*  PURPOSE:    Clear one or all interrupt event counts
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doClearEventCount( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    int  index;            /* Selects appropriate nomenclature       */
    char *name;            /* Local pointer to command name          */
    char *desc;            /* Local pointer to command description   */
    RFM2GEVENTTYPE Event;  /* Which event to clear                   */
    int i;
    RFM2G_STATUS status;


    index = cmd->command;
    if( usage || cmd->parmcount != commandlist[index].parmcount )
    {
        name = commandlist[index].name;
        desc = commandlist[index].desc;

        printf( "\n%s:  Clear one or all interrupt event counts\n\n",
            name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following interrupt events "
            "[%d-%d]:\n", RFM2GEVENT_RESET, RFM2GEVENT_LAST );
        for( i=RFM2GEVENT_RESET; i<RFM2GEVENT_LAST; i++ )
        {
            printf( "    %2d   %s\n", i, event_name[i].name );
        }

        printf( "    %d   ALL EVENTS\n", RFM2GEVENT_LAST );

        return(0);
    }

    /* Get the event type */
    Event = strtoul( cmd->parm[0], 0, 0 );

    /* Start clearing */
    status = RFM2gClearEventCount(cmd->Handle, Event);
    if( status == RFM2G_SUCCESS )
    {
        if( Event == RFM2GEVENT_LAST )
        {
            printf( "All interrupt event counts were cleared.\n" );
        }
        else
        {
            printf( "The \"%s\" interrupt event count cleared.\n",
                event_name[Event].name );
        }
    }
    else
    {
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    return(0);

}   /* End of doClearEventCount() */

/******************************************************************************
*
*  FUNCTION:   doClearReg
*
*  PURPOSE:    Clear bits in a register
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doClearReg( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32      index;      /* Selects appropriate nomenclature      */
    char *           name;       /* Local pointer to command name         */
    char *           desc;       /* Local pointer to command description  */
    WIDTH            width;      /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     mask = 0;
	RFM2GREGSETTYPE  regset;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

		printf(RFM2G_SUPPORT_PERSONNEL_MSG);

        printf( "\n%s:  Write a value to a register window\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"regset\" is one of the following (0, 1, 2 or 3):\n" );
        printf( "    0  BAR 0 mem - Local Config, Runtime and DMA Registers\n" );
        printf( "    1  BAR 1 I/O - Local Config, Runtime and DMA Registers\n" );
        printf( "    2  BAR 2 mem - RFM Control and Status Registers \n" );
        printf( "    3  BAR 3 mem - reflective memory RAM\n" );
        printf( "    4  BAR 4 Reserved\n" );
        printf( "    5  BAR 5 Reserved\n" );

        printf( "  \"offset\" is the offset into the window to read.\n" );

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );

        printf( "  \"mask\" is a constant byte, word, or longword that specifies\n" );
        printf( "    the bits to clear in the register specified by offset.\n\n" );
        return(0);
    }

    /* Get the regset */
    regset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[2] );
        return(-1);
    }

    /* Get the value */
    mask = strtoul( cmd->parm[3], 0, 0 );

    /* Write the value */
    status = RFM2gClearReg( cmd->Handle, regset, offset, width.parm, mask);

    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not do Register Write to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Write completed.\n" );

    return(0);

}   /* End of doClearReg() */


/******************************************************************************
*
*  FUNCTION:   doConfig
*
*  PURPOSE:    Display the VME RFM's Configuration information
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, memregisters, biddesc
*
******************************************************************************/

static RFM2G_INT32
doConfig( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;   /* Selects appropriate nomenclature      */
    char *name;           /* Local pointer to command name         */
    char *desc;           /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2GCONFIG Config;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the Reflective Memory board's Configuration\n",
            name );
        printf( "  information\n\n" );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Get the board Configuration */
    Status = RFM2gGetConfig( cmd->Handle, &Config );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the board Configuration.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print board configuration */
    printf ("    Driver Part Number     \"%s\"\n", Config.Name);
    printf ("    Driver Version         \"%s\"\n", Config.DriverVersion);
    printf ("    Device Name            \"%s\"\n", Config.Device);
    printf ("    Board Instance         %d\n",     Config.Unit);
    printf ("    Board ID               0x%02X\n", Config.BoardId);
    printf ("    Node ID                0x%02X\n", Config.NodeId);
    printf ("    Installed Memory       %ud (0x%08X)\n",
            Config.MemorySize, Config.MemorySize);
    printf ("    Memory Offset:         ");

	switch (Config.Lcsr1 & 0x0030000 )
	{
		case 0x00000000:
				printf ("0x00000000\n");
				break;

		case 0x00010000:
				printf ("0x04000000\n");
				break;

		case 0x00020000:
				printf ("0x08000000\n");
				break;

		case 0x00030000:
				printf ("0x0c000000\n");
				break;

		default: /* S/W Error */
				printf("\n");
	}

    printf ("    Board Revision         0x%02X\n", Config.BoardRevision);
    printf ("    Build Id               0x%04X\n", Config.BuildId);
    printf ("    PLX Revision           0x%02X\n", Config.PlxRevision);

    /* Display Board Configuration */
    printf ("    Config.Lcsr1           0x%08x\n", Config.Lcsr1);

    printf("RFM2g Configuration:\n");

    if (Config.Lcsr1 & 0x01000000)
    {
        printf ("        Rogue Master 0 Enabled\n");
    }

    if (Config.Lcsr1 & 0x02000000)
    {
        printf ("        Rogue Master 1 Enabled\n");
    }

    if (Config.Lcsr1 & 0x04000000)
    {
        printf ("        Redundant Mode\n");
    }
    else
    {
        printf ("        Fast Mode\n");
    }

    if (Config.Lcsr1 & 0x08000000)
    {
        printf ("        Local Bus Parity Enabled\n");
    }

    if (Config.Lcsr1 & 0x10000000)
    {
        printf ("        Loopback Enabled\n");
    }

    if (Config.Lcsr1 & 0x20000000)
    {
        printf ("        Dark-on-Dark Enabled\n");
    }

    if (Config.Lcsr1 & 0x40000000)
    {
        printf ("        Transmitter Disabled\n");
    }

    /* PCI Configuration Info */
    printf("RFM2g PCI Configuration:\n");
    printf ("   bus                 0x%02x\n", Config.PciConfig.bus);
    printf ("   function            0x%02x\n", Config.PciConfig.function);
    printf ("   type                0x%04x\n", Config.PciConfig.type);
    printf ("   devfn               0x%08x\n", Config.PciConfig.devfn);
    printf ("   revision            0x%02x\n", Config.PciConfig.revision);
    printf ("   rfm2gOrBase         0x%08x\n", Config.PciConfig.rfm2gOrBase);
    printf ("   rfm2gOrWindowSize   0x%08x\n", Config.PciConfig.rfm2gOrWindowSize);
    printf ("   rfm2gOrRegSize      0x%08x\n", Config.PciConfig.rfm2gOrRegSize);
    printf ("   rfm2gCsBase         0x%08x\n", Config.PciConfig.rfm2gCsBase);
    printf ("   rfm2gCsWindowSize   0x%08x\n", (unsigned int)Config.PciConfig.rfm2gCsWindowSize);
    printf ("   rfm2gCsRegSize      0x%08x\n", Config.PciConfig.rfm2gCsRegSize);
    printf ("   rfm2gBase           0x%08x\n", (unsigned int)Config.PciConfig.rfm2gBase);
    printf ("   rfm2gWindowSize     0x%08x\n", Config.PciConfig.rfm2gWindowSize);
    printf ("   interruptNumber     0x%02x\n", Config.PciConfig.interruptNumber);

    return(0);

}   /* End of doConfig() */


/******************************************************************************
*
*  FUNCTION:   doCheckRing
*
*  PURPOSE:    Perform the "own-bit" test to determine if the Reflective
*              Memory fiber link is intact.
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    None
*
******************************************************************************/

static RFM2G_INT32
doCheckRing( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Tests the RFM ring continuity\n\n",
            name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* If the ring is intact, the "own-data" bit will be set */
    status = RFM2gCheckRingCont(cmd->Handle);
    if( status == RFM2G_SUCCESS )
    {
        printf( "The Reflective Memory link is intact.\n" );
    }
    else
    {
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
    }

    return(0);

}   /* End of doCheckRing() */



/******************************************************************************
*
*  FUNCTION:   doDevName
*
*  PURPOSE:    Display the RFM's Board's Device name
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doDevName( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    char DeviceName[80];

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the Reflective Memory board's Device Name\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get the Device Name */
    DeviceName[0] = 0;
    Status = RFM2gDeviceName( cmd->Handle, DeviceName );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the Device Name.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print Device Name */
    printf ("    Device Name:               \"%s\"\n\n", DeviceName);

    return(0);

}   /* End of doDevName() */


/******************************************************************************
*
*  FUNCTION:   doDisableEvent
*
*  PURPOSE:    Disable reception of a Reflective Memory interrupt event
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doDisableEvent( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;  /* Selects appropriate nomenclature      */
    char *          name;   /* Local pointer to command name         */
    char *          desc;   /* Local pointer to command description  */
    unsigned int    event;  /* Interrupt event to disable            */
    RFM2G_INT32     i;      /* Loop variables                        */
    RFM2G_STATUS    status;


    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &event) != 1) ||
        (event >= event_name_array_num_elements))
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Disable the reception of a Reflective Memory "
            "interrupt event\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following [0-%d]:\n",
            event_name_array_num_elements-1 );

        for( i=RFM2GEVENT_RESET; i < event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %-25s %s\n", i, event_name[i].name, cmd->enable_state[i]? "enabled" : "disabled" );
        }

        printf("\n");
        return(0);
    }

    /* Disable the interrupt */
    status = RFM2gDisableEvent(cmd->Handle, event_name[event].EventType);
    switch(status)
    {
        case RFM2G_SUCCESS:
            printf( "Interrupt event \"%s\" is disabled.\n",
                event_name[event].name );
            cmd->enable_state[event] = RFM2G_DISABLE;
            break;

        case RFM2G_NOT_ENABLED:
            printf( "Interrupt event \"%s\" is already disabled.\n",
                event_name[event].name );
            cmd->enable_state[event] = RFM2G_DISABLE;
            return(-1);
            break;

        case RFM2G_NOT_SUPPORTED:
            printf( "Interrupt event \"%s\" is not supported by this board "
                "type.\n", event_name[event].name );
            return(-1);
            break;

        default:
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
            return(-1);
            break;
    }

    return(0);

}   /* End of doDisableEvent() */



/******************************************************************************
*
*  FUNCTION:   doDisableCallback
*
*  PURPOSE:    Disables asynchronous event notification for one event
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doDisableCallback( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;  /* Selects appropriate nomenclature          */
    char *          name;   /* Local pointer to command name             */
    char *          desc;   /* Local pointer to command description      */
    RFM2G_INT32     i;      /* Loop variables                            */
    unsigned int    event;  /* Interrupt event                       */
    RFM2G_STATUS    status;


    index = cmd->command;
    name = cmd->pCmdInfoList[index].name;
    desc = cmd->pCmdInfoList[index].desc;

    if ( ( usage ) || ( (cmd->parmcount == 1) &&
                        ( (sscanf(cmd->parm[0], "%i", &event) != 1) ||
                        ( event >= event_name_array_num_elements)
                        ) ) )
    {
        printf( "\n%s:  Disable asynchronous event notification,"
            "\n  or get the current state\n\n", name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "Get the current state of asynchronous event notification "
            "by entering\n  \"%s\" with no parameter.\n\n", name );

        printf( "Disable asynchronous event notification by entering "
            "\"%s  event\"\n  where \"event\" is one of the ", name );
        printf( "following interrupt events [0-%d]:\n\n", event_name_array_num_elements-1 );

        for( i=RFM2GEVENT_RESET; i < event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %s\n", i, event_name[i].name );
        }
        printf( "\n");

        return(0);
    }

    if( cmd->parmcount == 0 )
    {
        /* Display the status of async event notification */
        printf("\nAsynchronous Event Notification states:\n\n");
        for( i=RFM2GEVENT_RESET; i < event_name_array_num_elements; i++ )
        {
            printf( "   %2d  %-25s %s\n",i, event_name[i].name,
                cmd->notify_state[i]? "enabled" : "disabled" );
        }

        printf("\n");
        return(0);
    }

    if( cmd->parmcount == 1 )
    {
        /* The async notification should be disabled */
        status = RFM2gDisableEventCallback( cmd->Handle, event_name[event].EventType );

        switch(status)
        {
            case RFM2G_SUCCESS:
                printf( "Asynchronous Event Notification has been "
                    "disabled for the\n   \"%s\" event.\n",
                    event_name[event].name );
                cmd->notify_state[event] = 0;
                break;

            case RFM2G_EVENT_NOT_IN_USE:
                printf( "Asynchronous Event Notification \"%s\" is already disabled.\n",
                    event_name[event].name );
                cmd->notify_state[event] = 0;
                return(-1);
                break;

            default:
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                return(-1);
                break;
        }
    }
    else
    {
        printf( "Usage:  %s  %s\n\n", name, desc );
    }

    return(0);

}   /* End of DisableCallback() */


/******************************************************************************
*
*  FUNCTION:   doDllVersion
*
*  PURPOSE:    Display the library file version number
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doDllVersion( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    char DllVersion[80];

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the driver library file version\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get the DllVersion */
    DllVersion[0] = 0;
    Status = RFM2gDllVersion( cmd->Handle, DllVersion );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the DllVersion.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print Dll Version */
    printf ("    Dll Version:               \"%s\"\n\n", DllVersion);

    return(0);

}   /* End of doDllVersion() */


/******************************************************************************
*
*  FUNCTION:   doDriverVersion
*
*  PURPOSE:    Display the driver version number
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doDriverVersion( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    char DriverVersion[80];

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the driver version\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get the DriverVersion */
    DriverVersion[0] = 0;
    Status = RFM2gDriverVersion( cmd->Handle, DriverVersion );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the DriverVersion.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print Driver Version */
    printf ("    Driver Version:            \"%s\"\n\n", DriverVersion);

    return(0);

}   /* End of doDriverVersion() */

/******************************************************************************
*
*  Helper routine for doRead, doWrite, Dump commands
*
*******************************************************************************/

static void
displayRange(RFM2G_UINT32 PIOLength, RFM2G_UINT32 DMALength)
{
     printf( "  \"length\" is the number of \"width\" units to display.\n" );
     printf( "    Maximum length for bytes for PIO is 0x%08X and DMA is 0x%08X\n",
          PIOLength, DMALength);
     printf( "    Maximum length for words for PIO is 0x%08X and DMA is 0x%08X\n",
          PIOLength/2, DMALength/2 );
     printf( "    Maximum length for 32 bit longs for PIO is 0x%08X and DMA is 0x%08X\n",
          PIOLength/4, DMALength/4 );
     printf( "    Maximum length for 64 bit longs for PIO is 0x%08X and DMA is 0x%08X\n\n",
          PIOLength/8, DMALength/8 );
}

/******************************************************************************
*
*  FUNCTION:   doDump
*
*  PURPOSE:    Reads and displays an area of RFM memory
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doDump( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_UINT32     index;          /* Selects appropriate nomenclature      */
    char *           name;           /* Local pointer to command name         */
    char *           desc;           /* Local pointer to command description  */
    RFM2G_UINT32     perline;        /* How many data to display per line    */
    WIDTH            width;          /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     length;
    RFM2G_UINT32     i;
    RFM2G_UINT32     j;              /* Loop variables                   */
    char             charArray[20];
    RFM2G_UINT8      value8;
    RFM2G_UINT16     value16;
    RFM2G_UINT32     value32;
    RFM2G_UINT64     value64;
    RFM2G_UINT32     skipped;
    RFM2G_UINT32     startAddress;

    RFM2G_UINT32 windowSize;  /* Size of current sliding window */
    RFM2G_UINT32 windowOffset; /* offset of current sliding window */

    index = cmd->command;

    /* Get info about the current sliding window */
    status = RFM2gGetSlidingWindow(cmd->Handle, &windowOffset, &windowSize);
    if( status == RFM2G_NOT_SUPPORTED )
    {
        windowOffset = 0;
        RFM2gSize(cmd->Handle, &windowSize);
    }
    else if (status != RFM2G_SUCCESS)
    {
        printf("Could not get info about current Sliding Window.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Peek and display an area of Reflective Memory\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the width-aligned beginning offset to read.\n" );

        RFM2gSize( cmd->Handle, &length );
        printf( "    Valid offsets for PIO are in the range 0x00000000 to 0x%08X.\n",
           windowSize - 1);
        printf( "    Valid offsets for DMA are in the range 0x00000000 to 0x%08X.\n\n",
           length - windowOffset - 1);

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );
        printf( "    8  for Longlong (64 bits)\n\n" );

        displayRange(windowSize, (length-windowOffset));

        return(0);
    }

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[1], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[1] );
        return(-1);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the length and convert to bytes */
    length = strtoul( cmd->parm[2], 0, 0 );
    length *= width.width;

    /* display the buffer */
    perline = 16/width.width;

    printf("\n %*c           0 ", 2*width.width, ' ');
    for (i = 1; i < perline; i++)
    {
        printf("%*X ", 2*width.width, i);
    }
    printf("\n");

    startAddress = offset - (offset % (width.width * perline));

    printf( "0x%08X:  ", startAddress );

    /* How many elements do we skip before we start printing out elements? */
    skipped = (offset - startAddress) / width.width;

    /* Aligned the printout with the header */
    if (skipped != 0)
    {
        printf( "%*c", (skipped * 2 * width.width) + skipped, ' ' );
    }

    for( i=offset, j=skipped; i<offset+length; i+=width.width, j++ )
    {
        if ( j == perline )
        {
            j = 0;
            printf( "0x%08X:  ", i );
        }

        switch (width.width)
        {
            case 1:
            status = RFM2gPeek8(cmd->Handle, i, &value8);
            if( status != RFM2G_SUCCESS )
            {
                printf( "Could not do peek8 from offset 0x%X.\n", i );
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                return(-1);
            }

            charArray[j] = (char) value8;
            printf( "%0*X ", 2*width.width, (int) value8 );
            break;

            case 2:
            status = RFM2gPeek16(cmd->Handle, i, &value16);
            if( status != RFM2G_SUCCESS )
            {
                printf( "Could not do peek16 from offset 0x%X.\n", i );
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                return(-1);
            }
            charArray[j*2]   = ((char*) &value16)[0];
            charArray[j*2+1] = ((char*) &value16)[1];
            printf( "%0*X ", 2*width.width, (int) value16 );
            break;

            case 4:
            status = RFM2gPeek32(cmd->Handle, i, &value32);
            if( status != RFM2G_SUCCESS )
            {
                printf( "Could not do peek32 from offset 0x%X.\n", i );
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                return(-1);
            }
            charArray[j*4]   = ((char*) &value32)[0];
            charArray[j*4+1] = ((char*) &value32)[1];
            charArray[j*4+2] = ((char*) &value32)[2];
            charArray[j*4+3] = ((char*) &value32)[3];
            printf( "%0*X ", 2*width.width, (int) value32 );
            break;

            case 8:
            status = RFM2gPeek64(cmd->Handle, i, &value64);
            if( status != RFM2G_SUCCESS )
            {
                printf( "Could not do peek64 from offset 0x%X.\n", i );
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                return(-1);
            }
            charArray[j*4]   = ((char*) &value64)[0];
            charArray[j*4+1] = ((char*) &value64)[1];
            charArray[j*4+2] = ((char*) &value64)[2];
            charArray[j*4+3] = ((char*) &value64)[3];
            charArray[j*4+4] = ((char*) &value64)[4];
            charArray[j*4+5] = ((char*) &value64)[5];
            charArray[j*4+6] = ((char*) &value64)[6];
            charArray[j*4+7] = ((char*) &value64)[7];

            #if defined(RFM2G_VXWORKS)

                printf( "%08X%08X ", ((RFM2G_UINT32*) &value64)[0], ((RFM2G_UINT32*) &value64)[1]);

            #elif defined(RFM2G_LINUX)

                printf( "%0*llX ", 2*width.width, value64 );

            #elif defined(WIN32)

                printf( "%0*I64X ", 2*width.width, value64 );

            #else

                printf( "%0*lX ", 2*width.width, value64 );

            #endif
            break;

            default:
                return(-1);
       }

        /* Print out ASCII char of memory */
        if ( (j == (perline - 1) ) || ((i + width.width) == offset+length) )
        {
            RFM2G_UINT32 index;

            if (j != (perline - 1))
            {
                for (index = j; index < (perline - 1); index++)
                {
                printf("%*s ", 2*width.width, " ");
                }
            }
            /* Print out ASCII version of data */
            printf( "|" );
            for( index = 1; index <= ((j+1) * width.width); index++ )
            {
                if (isprint((int) charArray[index-1]))
                {
                    printf("%c", charArray[index-1]);
                }
                else
                {
                    printf("%c", '.');
                }

                if ( (width.width > 1) &&  /* do not insert space between characters on bytes */
                     ( (index % width.width) == 0 ) &&
                     ( index != ((j+1) * width.width)) ) /* We do not want a trailing space */
                {
                    printf(" ");
                }
            }
            printf( "|\n" );
        }
    }

    printf("\n\n");

    return(0);

}   /* End of doDump() */

/******************************************************************************
*
*  FUNCTION:   doDisplayReg
*
*  PURPOSE:    Reads and displays an area of RFM registers
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doDisplayReg( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32      index;          /* Selects appropriate nomenclature      */
    char *           name;           /* Local pointer to command name         */
    char *           desc;           /* Local pointer to command description  */
    RFM2G_UINT32     perline;        /* How many data to display per line    */
    WIDTH            width;          /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     length;
    RFM2G_UINT32     value   = 0;
    RFM2G_UINT32     i;
    RFM2G_UINT32     j;              /* Loop variables                   */
    volatile RFM2G_UINT8 *    pMappedMemory = NULL;
    char *           pChar       = NULL;
    volatile RFM2G_UINT8 *    pByte       = NULL;
    volatile RFM2G_UINT16 *   pWord       = NULL;
    volatile RFM2G_UINT32 *   pLong       = NULL;
    volatile RFM2G_UINT64 *   pLonglong   = NULL;
    RFM2G_UINT32     skipped;
    RFM2G_UINT32     startAddress;
	RFM2GREGSETTYPE  regset;
    RFM2GCONFIG      Config;

    index = cmd->command;

    if( usage || !( (cmd->parmcount == (cmd->pCmdInfoList[index].parmcount - 1)) ||
                   (cmd->parmcount == cmd->pCmdInfoList[index].parmcount)
                 )
      )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

		printf(RFM2G_SUPPORT_PERSONNEL_MSG);

        printf( "\n%s:  Display RFM Device Registers\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"regset\" is one of the following (0, 1, 2 or 3):\n" );
        printf( "    0  BAR 0 mem - Local Config, Runtime and DMA Registers\n" );
        printf( "    1  BAR 1 I/O - Local Config, Runtime and DMA Registers\n" );
        printf( "    2  BAR 2 mem - RFM Control and Status Registers \n" );
        printf( "    3  BAR 3 mem - reflective memory RAM\n" );
        printf( "    4  BAR 4 Reserved\n" );
        printf( "    5  BAR 5 Reserved\n" );

        printf( "  \"offset\" is the beginning offset to read.\n" );

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );
        printf( "    8  for Longlong (64 bits)\n\n" );

        RFM2gSize( cmd->Handle, &length );
        printf( "  \"length\" is the number of \"width\" units to display.\n" );
        printf( "    Maximum length for bytes is %d (0x%X)\n",
            length, length );
        printf( "    Maximum length for words is %d (0x%X)\n",
            length/2, length/2 );
        printf( "    Maximum length for 32 bit longs is %d (0x%X)\n",
            length/4, length/4 );
        printf( "    Maximum length for 64 bit longs is %d (0x%X)\n\n",
            length/8, length/8 );

        return(0);
    }

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[2] );
        return(-1);
    }

    /* Get the regset */
    regset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Get the board Configuration */
    status = RFM2gGetConfig( cmd->Handle, &Config );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not get the board Configuration.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    /* Get the length and convert to bytes */
    length = strtoul( cmd->parm[3], 0, 0 );
    length *= width.width;

    /* Validate parameters */
    switch (regset)
    {
    case RFM2GCFGREGMEM:
        if (offset + length > Config.PciConfig.rfm2gOrRegSize)
        {
            printf("offset (0x%08x) + length (0x%08x) = 0x%08x is greater then the memory space size (0x%08x)\n",
                offset, length, offset+length, Config.PciConfig.rfm2gOrRegSize);
            return(-1);
        }
        break;

    case RFM2GCTRLREGMEM:
        if (offset + length > Config.PciConfig.rfm2gCsRegSize)
        {
            printf("offset (0x%08x) + length (0x%08x) = 0x%08x is greater then the memory space size (0x%08x)\n",
                offset, length, offset+length, Config.PciConfig.rfm2gCsRegSize);
            return(-1);
        }
        break;

    case RFM2GMEM:
        if (offset + length > Config.PciConfig.rfm2gWindowSize)
        {
            printf("offset (0x%08x) + length (0x%08x) = 0x%08x is greater then the memory space size (0x%08x)\n",
                offset, length, offset+length, Config.PciConfig.rfm2gWindowSize);
            return(-1);
        }
        break;

    default:
        /* Let driver function return failure */
        break;
    }

    /* Map in the register space */
    status = RFM2gMapDeviceMemory( cmd->Handle, regset, (volatile void**) &pMappedMemory);
    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not map device memory regset 0x%08x\n", regset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    /* set up width size pointers */
    pByte     = pMappedMemory + offset;
    pChar     = (char*) pByte;
    pWord     = (RFM2G_UINT16*) pByte;
    pLong     = (RFM2G_UINT32*) pByte;
    pLonglong = (RFM2G_UINT64*) pByte;

    if (length > MEMBUF_SIZE)
    {
        length = MEMBUF_SIZE;
        printf("Read length limited to MEMBUF_SIZE.\n");
    }

    /* display the buffer */
    perline = 16/width.width;

    printf("\nSystem memory buffer contents after RFM2gRead\n");

    printf("\n %*c           0 ", 2*width.width, ' ');
    for (i = 1; i < perline; i++)
    {
        printf("%*X ", 2*width.width, i);
    }
    printf("\n");
    startAddress = offset - (offset % (width.width * perline));
    printf( "0x%08X:  ", startAddress );

    /* How many elements do we skip before we start printing out elements? */
    skipped = (offset - startAddress) / width.width;

    /* Aligned the printout with the header */
    if (skipped != 0)
    {
        printf( "%*c", (skipped * 2 * width.width) + skipped, ' ' );
    }

    for( i=offset, j=skipped; i<offset+length; i+=width.width, j++ )
    {
        if ( j == perline )
        {
            j = 0;
            printf( "0x%08X:  ", i );
        }

        switch (width.width)
        {
            case 1:
            value = (RFM2G_UINT32)(*pByte);
            pByte++;
            printf( "%0*X ", 2*width.width, value );
            break;

            case 2:
            value = (RFM2G_UINT32)(*pWord);
            pWord++;
            printf( "%0*X ", 2*width.width, value );
            break;

            case 4:
            value = *pLong;
            pLong++;
            printf( "%0*X ", 2*width.width, value );
            break;

            case 8:

            #if defined(RFM2G_VXWORKS)

                printf( "%08X%08X ", ((RFM2G_UINT32*) pLonglong)[0], ((RFM2G_UINT32*) pLonglong)[1]);

		    #elif defined(RFM2G_LINUX)

                printf( "%0*llX ", 2*width.width, *pLonglong );

            #elif defined(WIN32)

                printf( "%0*I64X ", 2*width.width, *pLonglong );

            #else

                printf( "%0*lX ", 2*width.width, *pLonglong );

            #endif

            pLonglong++;

            break;
        }

        /* Print out ASCII char of memory */
        if ( (j == (perline - 1) ) || ((i + width.width) == offset+length) )
        {
            RFM2G_UINT32 index;

            if (j != (perline - 1))
            {
                for (index = j; index < (perline - 1); index++)
                {
                printf("%*s ", 2*width.width, " ");
                }
            }
            /* Print out ASCII version of data */
            printf( "|" );
            for( index = 1; index <= ((j+1) * width.width); index++ )
            {
                if (isprint((int) *pChar))
                {
                    printf("%c", *pChar);
                }
                else
                {
                    printf("%c", '.');
                }

                if ( (width.width > 1) &&  /* do not insert space between characters on bytes */
                     ( (index % width.width) == 0 ) &&
                     ( index != ((j+1) * width.width)) ) /* We do not want a trailing space */
                {
                    printf(" ");
                }

                pChar++;
            }
            printf( "|\n" );
        }
    }

    printf("\n\n");

    /* Unmap the register space */
    status = RFM2gUnMapDeviceMemory( cmd->Handle, regset, (volatile void**) &pMappedMemory);
    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not unmap device memory regset 0x%08x pMappedMemory = %p\n", regset, pMappedMemory );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    return(0);

}   /* End of doDump() */

/******************************************************************************
*
*  FUNCTION:   doEnableEvent
*
*  PURPOSE:    Enable reception of a Reflective Memory interrupt event
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doEnableEvent( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;      /* Selects appropriate nomenclature      */
    char *          name;       /* Local pointer to command name         */
    char *          desc;       /* Local pointer to command description  */
    unsigned int    event;      /* Interrupt event to enable             */
    RFM2G_INT32     i;          /* Loop variables                        */
    RFM2G_STATUS    status;

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &event) != 1) ||
        (event >= event_name_array_num_elements))
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Enable the reception of a Reflective Memory "
            "interrupt event\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following [0-%d]:\n",
            event_name_array_num_elements-1);

        for( i=RFM2GEVENT_RESET; i < event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %-25s %s\n", i, event_name[i].name, cmd->enable_state[i]? "enabled" : "disabled" );
        }

        printf("\n");
        return(0);
    }

    /* Enable the interrupt */
    status = RFM2gEnableEvent(cmd->Handle, event_name[event].EventType);
    switch( status )
    {
        case RFM2G_SUCCESS:
            printf( "Interrupt event \"%s\" is enabled.\n",
                event_name[event].name );
            cmd->enable_state[event] = RFM2G_ENABLE;
            break;

        case RFM2G_ALREADY_ENABLED:
            printf( "Interrupt event \"%s\" is already enabled.\n",
                event_name[event].name );
            cmd->enable_state[event] = RFM2G_ENABLE;
            return(-1);
            break;

        case RFM2G_NOT_SUPPORTED:
            printf( "Interrupt event \"%s\" is not supported by this board "
                "type.\n", event_name[event].name );
            return(-1);
            break;

        default:
            printf("Error: %s\n", RFM2gErrorMsg(status) );
            return(-1);
            break;
    }

    return(0);

}   /* End of doEnableEvent() */


/******************************************************************************
*
*  FUNCTION:   doEnableCallback
*
*  PURPOSE:    Enables asynchronous event notification for one event
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doEnableCallback( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;  /* Selects appropriate nomenclature          */
    char *          name;   /* Local pointer to command name             */
    char *          desc;   /* Local pointer to command description      */
    RFM2G_INT32     i;      /* Loop variables                            */
    unsigned int    event;  /* Interrupt event                       */
    RFM2G_STATUS    status;

    index = cmd->command;
    name = cmd->pCmdInfoList[index].name;
    desc = cmd->pCmdInfoList[index].desc;

    if( usage )
    {
        printf( "\n%s:  Enable asynchronous event notification,"
            "\n  or get the current state\n\n", name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "Get the current state of asynchronous event notification "
            "by entering\n  \"%s\" with no parameter.\n\n", name );

        printf( "Enable asynchronous event notification by entering "
            "\"%s  event\"\n  where \"event\" is one of the ", name );
        printf( "following interrupt events [1-%d]:\n\n", event_name_array_num_elements );

        for( i=0; i < event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %s\n", i, event_name[i].name );
        }
        printf( "\n");

        return(0);
    }

    if( cmd->parmcount == 0 )
    {
        /* Display the status of async event notification */
        printf("\nAsynchronous Event Notification states:\n\n");
        for( i=RFM2GEVENT_RESET; i < event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %-25s %s\n",i, event_name[i].name,
                cmd->notify_state[i]? "enabled" : "disabled" );
        }

        printf("\n");
        return(0);
    }

    if( cmd->parmcount == 1 )
    {
        /* The async notification should be enabled */
        /* Get and verify the event */
        if( (sscanf(cmd->parm[0], "%i", &event) != 1) || (event >= event_name_array_num_elements) )
        {
            printf( "Invalid Event number\n" );
            return(-1);
        }

        callbackCounter[event] = 0;

        status = RFM2gEnableEventCallback( cmd->Handle, event_name[event].EventType, EventCallback );
        if( status != RFM2G_SUCCESS)
        {
            printf("Error: %s\n", RFM2gErrorMsg(status) );
            return(-1);
        }
        else
        {
            printf( "Asynchronous Event Notification has been "
                "enabled for the\n   \"%s\" event.\n",
                event_name[event].name );

            cmd->notify_state[event] = 1;
        }
    }
    else
    {
        printf( "Usage:  %s  %s\n\n", name, desc );
    }

    return(0);

}   /* End of doEnableCallback() */

/******************************************************************************
*
*  FUNCTION:   doErrorMsg
*
*  PURPOSE:    Displays RFM2g error message corresponding to ErrorCode
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doErrorMsg( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;   /* Selects appropriate nomenclature          */
    char *       name;    /* Local pointer to command name             */
    char *       desc;    /* Local pointer to command description      */
    RFM2G_STATUS ErrorCode;

    index = cmd->command;
    name = cmd->pCmdInfoList[index].name;
    desc = cmd->pCmdInfoList[index].desc;

    if( usage )
    {
        printf("\n%s:  Display error message corresponding to ErrorCode\n\n",
               name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        return(0);
    }

    if( cmd->parmcount == 1 )
    {
        /* The async notification should be enabled */
        /* Get and verify the event */
        ErrorCode = strtoul( cmd->parm[0], 0, 0 );

        printf("ErrorCode = %d, Msg = %s\n", ErrorCode, RFM2gErrorMsg(ErrorCode));
    }

    return(0);
}

/******************************************************************************
*
*  FUNCTION:   doExit
*
*  PURPOSE:    Clears the running flag, forcing loop in main() to break
*  PARAMETERS: cmd: (I) Command code and command parameters, unused
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:    running, commandlist
*
******************************************************************************/

static RFM2G_INT32
doExit( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    char c;
    char *name;     /* Local pointer to command name */


    if( usage )
    {
        name = cmd->pCmdInfoList[cmd->command].name;
        printf( "\n%s:  Terminates this program.\n\n", name );
        printf( "Usage:  %s\n\n", name );

        return(0);
    }

    printf( "Exit? (y/n) : " );
    c = getchar();

    if( tolower((int)c) == 'y' )
    {
        doExitDrvSpecific(cmd, 0);
        cmd->running = 0;
    }

    return(0);

}   /* End of doExit() */

/******************************************************************************
*
*  FUNCTION:   doFirst
*
*  PURPOSE:    Display the Offset to first usable area of RFM Memory
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doFirst( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    int  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_UINT32 First = 0;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the first usable offset in the RFM memory\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get First */
    Status = RFM2gFirst( cmd->Handle, &First );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the First Offset.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print First */
    printf ("    First                      0x%08X\n\n", First);

    return(0);

}   /* End of doFirst() */

/******************************************************************************
*
*  FUNCTION:   doClearEvent
*
*  PURPOSE:    Clear one or all interrupt event(s)
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doClearEvent( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;    /* Selects appropriate nomenclature       */
    char *          name;     /* Local pointer to command name          */
    char *          desc;     /* Local pointer to command description   */
    unsigned int    event;    /* Which event to clear                   */
    RFM2G_INT32     i;
    RFM2G_STATUS    status;

    index = cmd->command;
    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &event) != 1))
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Clear one interrupt event, if FIFO event, all events cleared from FIFO.\n\n",
            name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following interrupt events "
            "[%d-%d]:\n", RFM2GEVENT_RESET, RFM2GEVENT_LAST );
        for( i=RFM2GEVENT_RESET; i <= RFM2GEVENT_LAST; i++ )
        {
            printf( "    %2d   %s\n", i, event_name[i].name );
        }
        return(0);
    }

    /* Start clearing */
    status = RFM2gClearEvent(cmd->Handle, event);
    if ( status == RFM2G_SUCCESS )
    {
        if (event != RFM2GEVENT_LAST)
        {
            printf( "The \"%s\" interrupt event was cleared.\n",
                event_name[event].name );
        }
        else
        {
            printf( "All events were Cleared.\n" );
        }
    }
    else
    {
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    return(0);

}   /* End of doClearEvent() */

/******************************************************************************
*
*  FUNCTION:   doGetDarkOnDark
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's Dark on Dark functionality
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetDarkOnDark( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  Dark On Dark feature\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Get the current state of the Fail LED */
    Status = RFM2gGetDarkOnDark( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current Dark on Dark state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Dark On Dark feature is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doGetDarkOnDark() */

/******************************************************************************
*
*  FUNCTION:   doGetDebug
*
*  PURPOSE:    Get the driver's debug flags
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, debugflags, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetDebug( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    RFM2G_INT32  i;            /* Loop variable                         */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 flags;  /* Current debug flags                   */


    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current state of the Debug Flags.\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        return(0);
    }

    /* Get the current debug flags */
    status = RFM2gGetDebugFlags( cmd->Handle, &flags );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not get the Debug Flags.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Current Debug Flags: 0x%08X\n", flags );
    for( i=1; i<DEBUGFLAGCOUNT; i++ )
    {
        if( flags & debugflags[i].flag )
        {
            printf( "  %s\n", debugflags[i].name );
        }
    }

    return(0);

}   /* End of doGetDebug() */

/******************************************************************************
*
*  FUNCTION:   doGetDmaByteSwap
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's DMA Byte Swap
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetDmaByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  DMA Byte Swap\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    Status = RFM2gGetDMAByteSwap( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current DMA Byte Swap state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's DMA Byte Swap is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}

/******************************************************************************
*
*  FUNCTION:   doGetTransmit
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's Transmitter
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetTransmit( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  Transmitter\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Get the current state of the Fail LED */
    Status = RFM2gGetTransmit( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current Transmitter state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Transmitter is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doGetTransmit() */

/******************************************************************************
*
*  FUNCTION:   doGetEventCount
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's Status LED
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetEventCount( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;      /* Status return from RFM2g API call     */
    unsigned int event;       /* Which event to get count on           */
    RFM2G_INT32     i;        /* Loop variable                         */
	RFM2G_UINT32 count;       /* Event count                           */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &event) != 1) ||
        (event > event_name_array_num_elements))
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current event count for event "
            "Reflective Memory board's \n  Status LED\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following interrupt events "
            "[%d-%d]:\n", RFM2GEVENT_RESET, RFM2GEVENT_LAST );
        for( i=RFM2GEVENT_RESET; i <= RFM2GEVENT_LAST; i++ )
        {
            printf( "    %2d   %s\n", i, event_name[i].name );
        }

        printf("\n");
        return(0);
    }

	if (event_name[event].EventType != RFM2GEVENT_LAST)
	{
		Status = RFM2gGetEventCount( cmd->Handle, event_name[event].EventType, &count);
		if( Status != RFM2G_SUCCESS )
		{
			printf( "Could not get the current event count.\n" );
			printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
			return(-1);
		}
	    printf( "The event count for %s is %d\n", event_name[event].name , count);
	}
	else
	{
        /* Display counts for all event types */
        printf( "\nEvent                        Count\n" );

        printf( "-----------------------------------------------------------------------------\n" );
        for( i=RFM2GEVENT_RESET; i<RFM2GEVENT_LAST; i++ )
        {
			Status = RFM2gGetEventCount( cmd->Handle, event_name[i].EventType, &count);
			if( Status != RFM2G_SUCCESS )
			{
				printf( "Could not get the current event count.\n" );
				printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
				return(-1);
			}
            printf("  %-26s %-8d\n", event_name[i].name, count);
		}
		printf("\n");
	}
    return(0);

}   /* End of doGetEventCount() */


/******************************************************************************
*
*  FUNCTION:   doGetLed
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's Status LED
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetLed( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  Status LED\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Get the current state of the Fail LED */
    Status = RFM2gGetLed( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current Status LED state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Status LED is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doGetLed() */


/******************************************************************************
*
*  FUNCTION:   doClearOwnData
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's OWN data LED and turn it off.
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doClearOwnData( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  Own Data LED and clear it\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Get the current state of the Fail LED */
    Status = RFM2gClearOwnData( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current Own Data LED state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Own Data LED was %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doClearOwnData() */

/******************************************************************************
*
*  FUNCTION:   doGetLoopback
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's transmit loopback
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetLoopback( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  transmit loopback\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    Status = RFM2gGetLoopback( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current transmit loopback state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's transmit loopback is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doGetLoopback() */

/******************************************************************************
*
*  FUNCTION:   doGetMemoryOffset
*
*  PURPOSE:    Get the current memory offset of the Reflective Memory
*
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetMemoryOffset( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
	RFM2G_MEM_OFFSETTYPE offset;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current memory offset "
            "Reflective Memory board's \n  \n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    Status = RFM2gGetMemoryOffset( cmd->Handle, &offset );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current memory offset.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's memory offset is ");

	switch (offset)
	{
		case RFM2G_MEM_OFFSET0: printf(" 0x00000000\n");
				break;
		case RFM2G_MEM_OFFSET1: printf(" 0x04000000\n");
				break;
		case RFM2G_MEM_OFFSET2: printf(" 0x08000000\n");
				break;
		case RFM2G_MEM_OFFSET3: printf(" 0x0c000000\n");
				break;
		default: printf(" ERROR offset = %d\n", offset);
	}

    return(0);

}   /* End of doGetMemoryOffset() */

/******************************************************************************
*
*  FUNCTION:   doGetParityEnable
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's transmit loopback
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetParityEnable( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  Parity Enable\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    Status = RFM2gGetParityEnable( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current parity enable state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's parity enable is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doGetParityEnable() */


/******************************************************************************
*
*  FUNCTION:   doGetPioByteSwap
*
*  PURPOSE:    Get the current on/off state of the Reflective Memory
*              board's DMA Byte Swap
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetPioByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;       /* Selects appropriate nomenclature      */
    char *       name;        /* Local pointer to command name         */
    char *       desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_BOOL   State;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the current on/off state of the "
            "Reflective Memory board's \n  PIO Byte Swap\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    Status = RFM2gGetPIOByteSwap( cmd->Handle, &State );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the current DMA Byte Swap state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's PIO Byte Swap is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}

/******************************************************************************
*
*  FUNCTION:   doGetThreshold
*
*  PURPOSE:    Get the RFM board's DMA transfer threshold
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doGetThreshold( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_UINT32 Threshold;
    int  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;


    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get the DMA transfer threshold\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Get the DMA Threshold */
    status =  RFM2gGetDMAThreshold( cmd->Handle, &Threshold );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not get the DMA Threshold.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Current DMA Threshold: \"%u\"\n", Threshold );

    return(0);

}   /* End of doGetThreshold() */

/******************************************************************************
*
*  FUNCTION:   doHelp
*
*  PURPOSE:    Displays the list of commands for the diag utility
*  PARAMETERS: cmd: (I) Command code and command parameters, unused
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:    commandlist
*
******************************************************************************/

static RFM2G_INT32
doHelp( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32 i;          /* Loop variable */
    RFM2G_INT32 counter;    /* Regulates number of lines printed to screen  */
    RFM2G_INT32 matches[20];
    RFM2G_INT32  index;     /* Selects appropriate nomenclature             */
    char *name;     /* Local pointer to command name                */
    char *desc;     /* Local pointer to command description         */


    index = cmd->command;

    if( usage )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display a list of all commands, or display details about one command.\n", name );

        printf( "\nUsage:  %s  %s\n\n", name, desc );
        return(0);
    }

    if( cmd->parmcount == 0 )
    {
        /* Print a list of commands */
        for(i=0, counter=0; i<cmd->NumEntriesCmdInfoList; i++)
        {
            if( counter == 0 )
            {
                printf( "\n  COMMAND           PARAMETERS\n" );
                printf( "  -----------------------------------------------------\n" );
            }

			if (cmd->pCmdInfoList[i].display != 0)
			{
				printf( "  %s  %s\n", cmd->pCmdInfoList[i].name, cmd->pCmdInfoList[i].desc );

				if( ++counter == LINES_PER_PAGE )
				{
					counter = 0;
					printf( "\nPress ENTER for more commands ... " );
					getchar();
				}
			}
        }
        printf("\n");
    }
    else
    {
        if( ValidCmd( cmd,  cmd->parm[0] ,matches ) == 1 )
        {
            int cmdIndex = matches[0];

            /* Modify the command to reflect the command we want help for */
            cmd->command = cmdIndex;

            /* Call this command's usage routine */
            (cmd->pCmdInfoList[cmdIndex].func)( cmd, 1 );
        }
    }

    return(0);

}   /* End of doHelp() */

/******************************************************************************
*
*  FUNCTION:   doMapUser
*
*  PURPOSE:    Get or Set the DMA buffer base and range
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/
static RFM2G_INT32
doMapUser( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32   index;      /* Selects appropriate nomenclature      */
    char *        name;       /* Local pointer to command name         */
    char *        desc;       /* Local pointer to command description  */
    RFM2G_UINT32  Pages;
    volatile void *UserMemoryPtr = (volatile void *)cmd->MappedDmaBase;
    RFM2G_UINT64  Offset;
    RFM2G_STATUS  status;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get Set the User buffer offset and pages\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the beginning offset to map.\n" );

        printf( "  \"pages\" is the number of pages of memory to map.\n" );

        printf("\n");
        return(0);
    }
    else
    {
        get64bitvalue(cmd->parm[0], &Offset);
        Pages = strtoul( cmd->parm[1], 0, 0 );
    }

#if (defined(RFM2G_LINUX))

    /* Set flag in the offset to indicate DMA buffer map request. */
    Offset |= RFM2G_DMA_MMAP_OFFSET;

#endif

    /* Allow only one mapping at a time */
    if (UserMemoryPtr != NULL)
    {
        printf("Error:  User memory has already been mapped; only one mapping is supported\n");
        printf("        with this utility.  Use \"%s\" to free the memory before\n",
            (cmd->DmaPages != 0)? "unmapuser" : "unmapuserbytes");
        printf("        attempting to run \"mapuser\".\n");
        return -1;
    }

    /* Call RFM2gUserMemory.  This function maps the region to a virtual pointer. */
    status = RFM2gUserMemory(cmd->Handle, (void volatile **)(&UserMemoryPtr), Offset, Pages);
    if( status  != RFM2G_SUCCESS )
    {
        printf( "RFM2gUserMemory returned Error. Offset = 0x%llX, Pages = 0x%08x\n",
                Offset,  Pages);
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        UserMemoryPtr = NULL;
        return(-1);
    }

#if defined(RFM2G_VXWORKS) || defined(RFM2G_LINUX) /* Add OS's here that prepend '0x' on %p */

        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf("\nRFM2gUserMemory assigned UserMemoryPtr = %p\n\n", UserMemoryPtr);

#else

        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf("\nRFM2gUserMemory assigned UserMemoryPtr = 0x%p\n\n", UserMemoryPtr);

#endif

	cmd->MappedDmaBase = UserMemoryPtr;
    cmd->DmaPages      = Pages;
    cmd->DmaRange      = 0; /* This forces user to call unmapuser instead of unmapuserbytes */

    return(0);

}   /* End of doMapUser() */

/******************************************************************************
*
*  FUNCTION:   doMapUserBytes
*
*  PURPOSE:    Get or Set the DMA buffer base and range
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/
static RFM2G_INT32
doMapUserBytes( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32   index;      /* Selects appropriate nomenclature      */
    char *        name;       /* Local pointer to command name         */
    char *        desc;       /* Local pointer to command description  */
    RFM2G_UINT32  Bytes;
    volatile void *UserMemoryPtr = (volatile void *)cmd->MappedDmaBase;
    RFM2G_UINT64  Offset;
    RFM2G_STATUS  status;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Get Set the User buffer offset and pages\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the beginning offset to map.\n" );

        printf( "  \"bytes\" is the number of bytes of memory to map.\n" );

        printf("\n");
        return(0);
    }
    else
    {
        get64bitvalue(cmd->parm[0], &Offset);
        Bytes = strtoul( cmd->parm[1], 0, 0 );
    }

#if (defined(RFM2G_LINUX))

    /* Set flag in the offset to indicate DMA buffer map request. */
    Offset |= RFM2G_DMA_MMAP_OFFSET;

#endif

    /* Allow only one mapping at a time */
    if (UserMemoryPtr != NULL)
    {
        printf("Error:  User memory has already been mapped; only one mapping is supported\n");
        printf("        with this utility.  Use \"%s\" to free the memory before\n",
            (cmd->DmaPages != 0)? "unmapuser" : "unmapuserbytes");
        printf("        attempting to run \"mapuserbytes\".\n");
        return -1;
    }

    /* Call RFM2gUserMemory.  This function maps the region to a virtual pointer. */
    status = RFM2gUserMemoryBytes(cmd->Handle, (void volatile **)(&UserMemoryPtr), Offset, Bytes);
    if( status  != RFM2G_SUCCESS )
    {
        printf( "RFM2gUserMemoryBytes returned Error. Offset = 0x%llu, Bytes = 0x%08x\n",
                Offset,  Bytes);

        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

#if defined(RFM2G_VXWORKS) || defined(RFM2G_LINUX) /* Add OS's here that prepend '0x' on %p */

        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf("\nRFM2gUserMemoryBytes assigned UserMemoryPtr = %p\n\n", UserMemoryPtr);

#else

        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf("\nRFM2gUserMemoryBytes assigned UserMemoryPtr = 0x%p\n\n", UserMemoryPtr);

#endif

	cmd->MappedDmaBase = UserMemoryPtr;
    cmd->DmaRange      = Bytes;
    cmd->DmaPages      = 0; /* This forces user to call unmapuserbytes instead of unmapuser */

    return(0);

}   /* End of doMapUserBytes() */

/******************************************************************************
*
*  FUNCTION:   doNodeId
*
*  PURPOSE:    Display the RFM's Node Id
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doNodeId( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_NODE NodeId = 0;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the Reflective Memory board's Node ID\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get the Node ID */
    Status = RFM2gNodeID( cmd->Handle, &NodeId );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the Node ID.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print Node ID and description of board */
    printf ("    Node ID                    0x%02X\n\n", NodeId);

    return(0);

}   /* End of doNodeId() */


/******************************************************************************
*
*  FUNCTION:   doPeek8
*
*  PURPOSE:    Performs one byte read from an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPeek8( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;    /* Selects appropriate nomenclature      */
    char *       name;     /* Local pointer to command name         */
    char *       desc;     /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT8  value;

    index = cmd->command;

    if ( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one byte read from an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the Reflective Memory byte-aligned offset to read.\n" );

        printf("\n");
        return(0);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[0], 0, 0 );

    status = RFM2gPeek8( cmd->Handle, offset, &value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do peek8 from offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Data: 0x%02X\tRead from Offset: 0x%08X\n", value, offset );

    return(0);

}   /* End of doPeek8() */


/******************************************************************************
*
*  FUNCTION:   doPeek16
*
*  PURPOSE:    Performs one word (short) read from an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPeek16( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT16 value;


    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one word (short) read from an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the Reflective Memory word (short)-aligned offset to read.\n" );

        printf("\n");
        return(0);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[0], 0, 0 );

    status = RFM2gPeek16( cmd->Handle, offset, &value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do peek16 from offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Data: 0x%04X\tRead from Offset: 0x%08X\n", value, offset );

    return(0);

}   /* End of doPeek16() */


/******************************************************************************
*
*  FUNCTION:   doPeek32
*
*  PURPOSE:    Performs one longword read from an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPeek32( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT32 value;


    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one longword read from an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the Reflective Memory longword-aligned offset to read.\n" );

        printf("\n");
        return(0);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[0], 0, 0 );

    status = RFM2gPeek32( cmd->Handle, offset, &value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do peek32 from offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Data: 0x%08X\tRead from Offset: 0x%08X\n", value, offset );

    return(0);

}   /* End of doPeek32() */


/******************************************************************************
*
*  FUNCTION:   doPeek64
*
*  PURPOSE:    Performs one longword read from an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPeek64( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;     /* Selects appropriate nomenclature      */
    char *       name;      /* Local pointer to command name         */
    char *       desc;      /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT64 value;


    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one 64 bit longword read from an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the Reflective Memory longword-aligned offset to read.\n" );

        printf("\n");
        return(0);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[0], 0, 0 );

    status = RFM2gPeek64( cmd->Handle, offset, &value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do peek64 from offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    #if defined(RFM2G_VXWORKS)

        printf( "Data: 0x%08X%08X\tRead from Offset: 0x%08X\n",
                ((RFM2G_UINT32*) &value)[0], ((RFM2G_UINT32*) &value)[1], offset );

    #elif defined(RFM2G_LINUX)

		printf( "Data: 0x%llX\tRead from Offset: 0x%08X\n", value, offset );

    #elif defined(WIN32)

		printf( "Data: 0x%I64X\tRead from Offset: 0x%08X\n", value, offset );

    #else

		printf( "Data: 0x%lX\tRead from Offset: 0x%08X\n", value, offset );

    #endif

    return(0);

}   /* End of doPeek64() */

/******************************************************************************
*
*  FUNCTION:   doPoke8
*
*  PURPOSE:    Performs one byte write to an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPoke8( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT8 value;


    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one byte write to an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"value\" is the data to be written.\n\n" );
        printf( "  \"offset\" is the Reflective Memory byte-aligned offset to write.\n" );

        printf("\n");
        return(0);
    }

    /* Get the value */
    value = (RFM2G_UINT8)(strtoul( cmd->parm[0], 0, 0 ));

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Poke the value */
    status = RFM2gPoke8( cmd->Handle, offset, value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do poke8 to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Data: 0x%02X\tWritten to Offset: 0x%08X\n", value, offset );

    return(0);

}   /* End of doPoke8() */


/******************************************************************************
*
*  FUNCTION:   doPoke16
*
*  PURPOSE:    Performs one word (short) write to an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPoke16( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT16 value;


    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one word (short) write to an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"value\" is the data to be written.\n\n" );
        printf( "  \"offset\" is the Reflective Memory word(short)-aligned offset to write.\n" );

        printf("\n");
        return(0);
    }

    /* Get the value */
    value = (RFM2G_UINT16)(strtoul( cmd->parm[0], 0, 0 ));

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Poke the value */
    status = RFM2gPoke16( cmd->Handle, offset, value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do poke16 to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Data: 0x%04X\tWritten to Offset: 0x%08X\n", value, offset );

    return(0);

}   /* End of doPoke16() */


/******************************************************************************
*
*  FUNCTION:   doPoke32
*
*  PURPOSE:    Performs one long write to an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPoke32( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT32 value;


    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one long write to an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"value\" is the data to be written.\n\n" );
        printf( "  \"offset\" is the Reflective Memory long-aligned offset to write.\n" );

        printf("\n");
        return(0);
    }

    /* Get the value */
    value = (RFM2G_UINT32)(strtoul( cmd->parm[0], 0, 0 ));

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Poke the value */
    status = RFM2gPoke32( cmd->Handle, offset, value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do poke32 to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Data: 0x%08X\tWritten to Offset: 0x%08X\n", value, offset );

    return(0);

}   /* End of doPoke32() */


/******************************************************************************
*
*  FUNCTION:   doPoke64
*
*  PURPOSE:    Performs one 64 bit long write to an offset
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPoke64( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;
    RFM2G_UINT64 value;


    index = cmd->command;

    if ( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount))
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Perform one long write to an offset\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"value\" is the data to be written.\n\n" );
        printf( "  \"offset\" is the Reflective Memory long-aligned offset to write.\n" );

        printf("\n");
        return(0);
    }

    if (get64bitvalue(cmd->parm[0], &value) != 0)
    {
        return(-1);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Poke the value */
    status = RFM2gPoke64( cmd->Handle, offset, value );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not do poke64 to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    #if defined(RFM2G_VXWORKS)

        printf( "Data: 0x%08X%08X\tWritten to Offset: 0x%08X\n",
                ((RFM2G_UINT32*) &value)[0], ((RFM2G_UINT32*) &value)[1], offset);

    #elif defined(RFM2G_LINUX)

        printf( "Data: 0x%016llX\tWritten to Offset: 0x%08X\n", value, offset );

    #elif defined(WIN32)

        printf( "Data: 0x%I64X\tWritten to Offset: 0x%08X\n", value, offset );

    #else

        printf( "Data: 0x%016lX\tWritten to Offset: 0x%08X\n", value, offset );

    #endif

    return(0);

}   /* End of doPoke64() */

/******************************************************************************
*
*  FUNCTION:   doRead
*
*  PURPOSE:    Reads and displays an area of RFM memory
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doRead( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32      index;          /* Selects appropriate nomenclature      */
    char *           name;           /* Local pointer to command name         */
    char *           desc;           /* Local pointer to command description  */
    RFM2G_UINT32     perline;        /* How many data to display per line    */
    WIDTH            width;          /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     length;
    RFM2G_UINT32     value   = 0;
    RFM2G_UINT32     i;
    RFM2G_UINT32     j;              /* Loop variables                   */
    char *           pChar       = NULL;
    RFM2G_UINT8 *    pByte       = NULL;
    RFM2G_UINT16 *   pWord       = NULL;
    RFM2G_UINT32 *   pLong       = NULL;
    RFM2G_UINT64 *   pLonglong   = NULL;
    RFM2G_UINT32     skipped;
    RFM2G_UINT32     startAddress;
    RFM2G_BOOL       display     = RFM2G_TRUE;
    RFM2G_UINT32 windowSize;  /* Size of current sliding window */
    RFM2G_UINT32 windowOffset; /* offset of current sliding window */

    index = cmd->command;

    /* Get info about the current sliding window */
    status = RFM2gGetSlidingWindow(cmd->Handle, &windowOffset, &windowSize);
    if( status == RFM2G_NOT_SUPPORTED )
    {
        windowOffset = 0;
        RFM2gSize(cmd->Handle, &windowSize);
    }
    else if (status != RFM2G_SUCCESS)
    {
        printf("Could not get info about current Sliding Window.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    if( usage || !( (cmd->parmcount == (cmd->pCmdInfoList[index].parmcount - 1)) ||
                   (cmd->parmcount == cmd->pCmdInfoList[index].parmcount) ) )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Read and display an area of Reflective Memory\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"offset\" is the beginning offset to read.\n" );

        RFM2gSize( cmd->Handle, &length );
        printf( "    Valid offsets for PIO are in the range 0x00000000 to 0x%08X.\n",
           windowSize - 1);
        printf( "    Valid offsets for DMA are in the range 0x00000000 to 0x%08X.\n\n",
           length - windowOffset - 1);

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );
        printf( "    8  for Longlong (64 bits)\n\n" );

        RFM2gSize( cmd->Handle, &length ); /* get installed RFM memory size */

        displayRange(windowSize, (length-windowOffset));

        printf( "  \"display\" 0 do not display data. 1 display data. (optional) \n" );

        return(0);
    }

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[1], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[1] );
        return(-1);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the length and convert to bytes */
    length = strtoul( cmd->parm[2], 0, 0 );
    length *= width.width;

    if (cmd->parmcount == (cmd->pCmdInfoList[index].parmcount))
    {
        display = (RFM2G_BOOL) strtoul( cmd->parm[3], 0, 0 );

        switch (display)
        {
        case RFM2G_TRUE:
        case RFM2G_FALSE:
            break;
        default:
            printf( "Invalid display \"%s\".\n", cmd->parm[3] );
            return(-1);
            break;
        }
    }

#if (defined(RFM2G_LINUX))

    {
        RFM2G_UINT32 Threshold = 0;

        /* see if DMA threshold and buffer are intialized */
        RFM2gGetDMAThreshold( cmd->Handle, &Threshold );

        /* see if DMA threshold and buffer are intialized */
        if ((cmd->MappedDmaBase != NULL) && (length >= Threshold))
        {
            printf("Read used DMA.\n");

            /* set up width size pointers */
            pChar = (char*) cmd->MappedDmaBase;
            pByte = (RFM2G_UINT8*) cmd->MappedDmaBase;
            pWord = (RFM2G_UINT16*) cmd->MappedDmaBase;
            pLong = (RFM2G_UINT32*) cmd->MappedDmaBase;
            pLonglong = (RFM2G_UINT64*) cmd->MappedDmaBase;
        }
        else
        {
            printf("DMA will not be used.\n");

            /* set up width size pointers */
            pChar     = (char*) &cmd->MemBuf;
            pByte     = (RFM2G_UINT8*)  &cmd->MemBuf;
            pWord     = (RFM2G_UINT16*) &cmd->MemBuf;
            pLong     = (RFM2G_UINT32*) &cmd->MemBuf;
            pLonglong = (RFM2G_UINT64*) &cmd->MemBuf;

            if (length > sizeof(cmd->MemBuf))
            {
                length = sizeof(cmd->MemBuf);
                printf("Read length limited to MEMBUF_SIZE %d bytes.\n", length);
            }
        }
    }

#else

    /* set up width size pointers */
    pChar     = (char*) &cmd->MemBuf;
    pByte     = (RFM2G_UINT8*)  &cmd->MemBuf;
    pWord     = (RFM2G_UINT16*) &cmd->MemBuf;
    pLong     = (RFM2G_UINT32*) &cmd->MemBuf;
    pLonglong = (RFM2G_UINT64*) &cmd->MemBuf;

    if (length > MEMBUF_SIZE)
    {
        length = MEMBUF_SIZE;
        printf("Read length limited to MEMBUF_SIZE.\n");
    }

#endif

    /* Read from the board */
    status = RFM2gRead( cmd->Handle, offset, pByte, length );
    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not do Read from offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    /* Does the user want to display the data read? */
    if (display == RFM2G_FALSE)
        {
        printf("\n\n");
        return(0);
        }

    /* display the buffer */
    perline = 16/width.width;

    printf("\nSystem memory buffer contents after RFM2gRead\n");

    printf("\n %*c           0 ", 2*width.width, ' ');
    for (i = 1; i < perline; i++)
        {
        printf("%*X ", 2*width.width, i);
        }
    printf("\n");
    startAddress = offset - (offset % (width.width * perline));
    printf( "0x%08X:  ", startAddress );

    /* How many elements do we skip before we start printing out elements? */
    skipped = (offset - startAddress) / width.width;

    /* Aligned the printout with the header */
    if (skipped != 0)
    {
        printf( "%*c", (skipped * 2 * width.width) + skipped, ' ' );
    }

    for( i=offset, j=skipped; i<offset+length; i+=width.width, j++ )
    {
        if ( j == perline )
        {
            j = 0;
            printf( "0x%08X:  ", i );
        }

        switch (width.width)
        {
            case 1:
            value = (RFM2G_UINT32)(*pByte);
            pByte++;
            printf( "%0*X ", 2*width.width, value );
            break;

            case 2:
            value = (RFM2G_UINT32)(*pWord);
            pWord++;
            printf( "%0*X ", 2*width.width, value );
            break;

            case 4:
            value = *pLong;
            pLong++;
            printf( "%0*X ", 2*width.width, value );
            break;

            case 8:

            #if defined(RFM2G_VXWORKS)

                printf( "%08X%08X ", ((RFM2G_UINT32*) pLonglong)[0], ((RFM2G_UINT32*) pLonglong)[1]);

		    #elif defined(RFM2G_LINUX)

                printf( "%0*llX ", 2*width.width, *pLonglong );

            #elif defined(WIN32)

                printf( "%0*I64X ", 2*width.width, *pLonglong );

            #else

                printf( "%0*lX ", 2*width.width, *pLonglong );

            #endif

            pLonglong++;

            break;
        }

        /* Print out ASCII char of memory */
        if ( (j == (perline - 1) ) || ((i + width.width) == offset+length) )
        {
            RFM2G_UINT32 index;

            if (j != (perline - 1))
            {
                for (index = j; index < (perline - 1); index++)
                {
                printf("%*s ", 2*width.width, " ");
                }
            }
            /* Print out ASCII version of data */
            printf( "|" );
            for( index = 1; index <= ((j+1) * width.width); index++ )
            {
                if (isprint((int) *pChar))
                {
                    printf("%c", *pChar);
                }
                else
                {
                    printf("%c", '.');
                }

                if ( (width.width > 1) &&  /* do not insert space between characters on bytes */
                     ( (index % width.width) == 0 ) &&
                     ( index != ((j+1) * width.width)) ) /* We do not want a trailing space */
                {
                    printf(" ");
                }

                pChar++;
            }
            printf( "|\n" );
        }
    }

    printf("\n\n");

    return(0);

}   /* End of doRead() */

/******************************************************************************
*
*  FUNCTION:   doRegRead
*
*  PURPOSE:    Reads and displays a value from a memory window
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doRegRead( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32      index;          /* Selects appropriate nomenclature      */
    char *           name;           /* Local pointer to command name         */
    char *           desc;           /* Local pointer to command description  */
    WIDTH            width;          /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     value   = 0;
    RFM2G_UINT8 *    pByte       = NULL;
    RFM2G_UINT16 *   pWord       = NULL;
    RFM2G_UINT32 *   pLong       = NULL;
	RFM2GREGSETTYPE  regset;
    index = cmd->command;

    if( usage || !( (cmd->parmcount == (cmd->pCmdInfoList[index].parmcount - 1)) ||
                   (cmd->parmcount == cmd->pCmdInfoList[index].parmcount)
                 )
      )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

		printf(RFM2G_SUPPORT_PERSONNEL_MSG);

        printf( "\n%s:  Read and display a value from a memory window.\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"regset\" is one of the following (0, 1, 2 or 3):\n" );
        printf( "    0  BAR 0 mem - Local Config, Runtime and DMA Registers\n" );
        printf( "    1  BAR 1 I/O - Local Config, Runtime and DMA Registers\n" );
        printf( "    2  BAR 2 mem - RFM Control and Status Registers \n" );
        printf( "    3  BAR 3 mem - reflective memory RAM\n" );
        printf( "    4  BAR 4 Reserved\n" );
        printf( "    5  BAR 5 Reserved\n" );

        printf( "  \"offset\" is the offset into the window to read.\n" );

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );
        return(0);
    }

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[1] );
        return(-1);
    }

    /* Get the regset */
    regset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* set up width size pointers */
    pByte     = (RFM2G_UINT8*)  &cmd->MemBuf;
    pWord     = (RFM2G_UINT16*) &cmd->MemBuf;
    pLong     = (RFM2G_UINT32*) &cmd->MemBuf;

    /* Read from the board */
    status = RFM2gReadReg( cmd->Handle, regset, offset, width.parm, (void*) pByte );
    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not do Read from offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    switch (width.width)
    {
        case 1:
        value = (RFM2G_UINT32)(*pByte);
        break;

        case 2:
        value = (RFM2G_UINT32)(*pWord);
        break;

        case 4:
        value = *pLong;
        break;
    }

    printf( "Value = 0x%0*X \n\n", 2*width.width, value );

    return(0);

}   /* End of doRegRead() */

/******************************************************************************
*
*  FUNCTION:   doRegWrite
*
*  PURPOSE:    Write a value to a register memory window
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doRegWrite( DIAGCMD *cmd, RFM2G_UINT8 usage )

{
    RFM2G_INT32      index;      /* Selects appropriate nomenclature      */
    char *           name;       /* Local pointer to command name         */
    char *           desc;       /* Local pointer to command description  */
    WIDTH            width;      /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     value = 0;
	RFM2GREGSETTYPE  regset;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

		printf(RFM2G_SUPPORT_PERSONNEL_MSG);

        printf( "\n%s:  Write a value to a register window\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"regset\" is one of the following (0, 1, 2 or 3):\n" );
        printf( "    0  BAR 0 mem - Local Config, Runtime and DMA Registers\n" );
        printf( "    1  BAR 1 I/O - Local Config, Runtime and DMA Registers\n" );
        printf( "    2  BAR 2 mem - RFM Control and Status Registers \n" );
        printf( "    3  BAR 3 mem - reflective memory RAM\n" );
        printf( "    4  BAR 4 Reserved\n" );
        printf( "    5  BAR 5 Reserved\n" );

        printf( "  \"offset\" is the offset into the window to read.\n" );

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );

        printf( "  \"value\" is a constant byte, word, or longword that will be\n" );
        printf( "    written to offset specified by offset.\n\n" );
        return(0);
    }

    /* Get the regset */
    regset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[2] );
        return(-1);
    }

    /* Get the value */
    value = strtoul( cmd->parm[3], 0, 0 );

    /* Write the value */
    status = RFM2gWriteReg( cmd->Handle, regset, offset, width.parm, value);

    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not do Register Write to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Write completed.\n" );

    return(0);

}   /* End of doRegWrite() */


/******************************************************************************
*
*  FUNCTION:   doRepeat
*
*  PURPOSE:    Repeats the command 1 or more times
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:    commandlist
*
******************************************************************************/

static RFM2G_INT32
doRepeat( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    static char *args[MAXPARMCOUNT]; /* Pointers to commandline tokens       */
    RFM2G_INT32  match;              /* Incremented on valid commands        */
    RFM2G_INT32  matches[20];        /* commandlist indices matching input   */
    RFM2G_UINT32 count = 0;    /* The number of times to repeat the command  */
    RFM2G_UINT32 i;            /* Loop variable                              */
    RFM2G_UINT8  pSwitch;      /* indicates if the count print switch is on  */
    RFM2G_INT32  index;        /* Selects appropriate nomenclature           */
    RFM2G_INT32  curParm;
    char *       name;         /* Local pointer to command name              */
    char *       desc;         /* Local pointer to command description       */
    RFM2G_INT32  parmcount;

    /* did the use specify the -p option? */
    pSwitch = 0;
    curParm = 1;
    if (cmd->parm[0] != NULL)
    {
        if ((cmd->parm[0][0] == '-') && (cmd->parm[0][1] == 'p'))
        {
            pSwitch = 1;
            curParm = 2;
        }
    }

    /* check for usage and proper parameter count */
    if( usage || (cmd->parmcount < (2 + pSwitch)))
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Repeat a command one or more times\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf( "  \"[-p]\" indicates printing of the current command count. \n" );
        printf( "  \"count\" is the number of times to repeat the command.\n" );
        printf( "  \"cmd\" is the command to be repeated.\n" );
        printf( "  \"[arg...]\" is the parameter list for the command to be repeated.\n\n" );

        printf( "Note that some commands cannot be repeated, or cannot be repeated\n" );
        printf( "  more than once.\n\n");

        return(0);
    }

    /* initialize variables */
    match = 0;
    memset( (void *) args,    0, sizeof(args) );
    memset( (void *) matches, 0, sizeof(matches) );

    /* build param list for command to be repeated */
    for(index = 0; index < (MAXPARMCOUNT - curParm); index++)
        {
        args[index] = cmd->parm[index + curParm];
        }

    /* validate the command */
    match = ValidCmd(cmd, args[0], matches);
    if( match == 1)
    {
        cmd->command = (RFM2G_UINT8)matches[0];
        parmcount = cmd->parmcount - pSwitch - 2;
        cmd->parmcount = (RFM2G_UINT8)parmcount;

        /* Some commands shouldn't be repeated */
        if( !cmd->pCmdInfoList[cmd->command].repeatable )
        {
            printf("Cannot repeat the \"%s\" command.\n",
                cmd->pCmdInfoList[cmd->command].name );
        }
        else
        {
            RFM2G_UINT32 argcount;

            if (cmd->pCmdInfoList[cmd->command].repeatable == 1)
            {
                /* Command can be repeated only once */
                count = 1;
            }
            else if( cmd->pCmdInfoList[cmd->command].repeatable == 2)
            {
                /* Command can be repeated multiple times */
                count = strtoul( cmd->parm[pSwitch], 0, 0 );
                if (count == 0)
                {
                    count = 1;
                }
            }

            /* Initialize parameters for command to be repeated. */
            for (argcount = 1; argcount < (MAXPARMCOUNT - 2); argcount++)
            {
                if ( args[argcount] )
                {
                    strncpy( cmd->parm[argcount - 1], args[argcount], MAXPARMSIZE );
                }
            }

            for(i=1; i<=count; i++)
            {
		        if( pSwitch )
                {
			        printf( "%u\t", i );
                }

                if ((cmd->pCmdInfoList[cmd->command].func)( cmd, 0 )  != 0)
                {
                    printf("Repeat stopped because of command failure\n");
                    return(-1);
                }
            }
	        printf( "\n" );
        }
    }

    return(0);
}   /* End of doRepeat() */

/******************************************************************************
*
*  FUNCTION:   doSend
*
*  PURPOSE:    Send an interrupt event to another Reflective Memory node
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSend( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32    index;       /* Selects appropriate nomenclature          */
    char *         name;        /* Local pointer to command name             */
    char *         desc;        /* Local pointer to command description      */
    RFM2G_STATUS   status;
    RFM2G_INT32    i;
    RFM2GEVENTINFO EventInfo;
    int            nodeId;      /* Local nodeId, used with scanf             */
    int            event;

    index = cmd->command;

    /* Get the interrupt type */
    EventInfo.Event = strtoul( cmd->parm[0], 0, 0 );

    if  (   ( usage || ((cmd->parmcount != cmd->pCmdInfoList[index].parmcount) &&
            (cmd->parmcount != (cmd->pCmdInfoList[index].parmcount - 1))) ) ||
            (sscanf(cmd->parm[0], "%i", &event) != 1) ||  /* Get event */
            (event >= event_name_array_num_elements) ||   /* event out of range? */
            (sscanf(cmd->parm[1], "%i", &nodeId) != 1))   /* Get nodeId */
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Send an interrupt event to another Reflective Memory node\n\n",
            name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following interrupt events [%d-%d]:\n",
              RFM2GEVENT_RESET, RFM2GEVENT_INTR4 );

        for( i=RFM2GEVENT_RESET; i<=RFM2GEVENT_INTR4; i++ )
        {
            printf( "    %2d  %s\n", i, event_name[i].name );
        }

        printf( "\n  and \"tonode\" is the node number of the Reflective Memory"
            " board to\n    receive the interrupt.\n\n" );

        printf( "\n  and \"[Ext_data]\" is the 32bit Extended data to send."
            "\n If not present 0 is sent for Extended data. \n\n" );

        return(0);
    }

    /* Get the event type */
    EventInfo.Event = event_name[event].EventType;

    /* Get the target node number */
    EventInfo.NodeId = (RFM2G_NODE) nodeId;

    /* Get the extende data */
    if (cmd->parmcount == 3)
    {
        EventInfo.ExtendedInfo = strtoul( cmd->parm[2], 0, 0 );
    }
    else
    {
        EventInfo.ExtendedInfo = 0;
    }

    /* Now send the interrupt event */
    status = RFM2gSendEvent( cmd->Handle, EventInfo.NodeId, EventInfo.Event, EventInfo.ExtendedInfo );
    if( status == RFM2G_SUCCESS )
    {
        printf( "\"%s\" interrupt event was sent to node %d (0x%X).\n",
            event_name[EventInfo.Event].name, EventInfo.NodeId, EventInfo.NodeId );
    }
    else
    {
        printf( "\nERROR: Could not send \"%s\" interrupt to node %d (0x%X).\n",
            event_name[EventInfo.Event].name, EventInfo.NodeId, EventInfo.NodeId );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));

        return(-1);
    }

    return(0);

}   /* End of doSend() */

/******************************************************************************
*
*  FUNCTION:   doSetDarkOnDark
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's Dark On Dark feature
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetDarkOnDark( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;   /* Selects appropriate nomenclature      */
    char *       name;    /* Local pointer to command name         */
    char *       desc;    /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;   /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  Dark On Dark feature\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    /* Set the  LED */
    if( State == 0)
    {
        Status = RFM2gSetDarkOnDark( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetDarkOnDark( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current Dark On Dark state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Dark On Dark feature is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doSetDarkOnDark() */

/******************************************************************************
*
*  FUNCTION:   doSetDebug
*
*  PURPOSE:    Set the driver's debug flags
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, debugflags, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetDebug( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  state;        /* Set of clear state of a debug flag    */
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    RFM2G_INT32  i;            /* Loop variable                         */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_INT32 matches[20];
    RFM2G_UINT32 flags;  /* Current debug flags                   */
    RFM2G_STATUS status;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Change the state of one Debug Flag\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "Set a debug flag by entering   \"%s  flag\", or\n",
            name );

        printf( "Clear a debug flag by entering \"%s  -flag\",\n",
            name );

        printf( "  where \"flag\" is one of the following:\n" );

        for( i=0; i<DEBUGFLAGCOUNT/2; i++ )
        {
            printf( "    %s   \t%s\n",
                debugflags[i].name,
                debugflags[i+(DEBUGFLAGCOUNT/2)].name );
        }

        printf("\n");
        return(0);
    }

    /* Validate flag name */
    if( ValidFlag( cmd->parm[0], matches, &state ) != 1 )
    {
        /* Invalid or ambiguous flag name */
        return(-1);
    }

    index = matches[0];

    /* Get the current debug flags */
    status = RFM2gGetDebugFlags( cmd->Handle, &flags );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not get the Debug Flags.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    /* Change the state of the flag */
    if( state )
    {
        flags |= debugflags[index].flag;
    }
    else
    {
        flags &= ~(debugflags[index].flag);
    }

    /* Set the debug flags */
    status = RFM2gSetDebugFlags( cmd->Handle, flags );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not set the Debug Flags.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Debug Flag \"%s\" was %s.\n", debugflags[index].name,
        (state? "set" : "cleared") );

    return(0);

}   /* End of doSetDebug() */

/******************************************************************************
*
*  FUNCTION:   doSetDmaByteSwap
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's DMA Byte Swap (4 byte swap)
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetDmaByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;    /* Selects appropriate nomenclature      */
    char *       name;     /* Local pointer to command name         */
    char *       desc;     /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;    /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;



    if ( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  DMA Byte Swap\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    /* Set the DMA Byte Swap */
    if( State == 0)
    {
        Status = RFM2gSetDMAByteSwap( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetDMAByteSwap( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current DMA Byte Swap state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's DMA Byte Swap is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}

/******************************************************************************
*
*  FUNCTION:   doSetLed
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's Status LED
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetLed( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;   /* Selects appropriate nomenclature      */
    char *       name;    /* Local pointer to command name         */
    char *       desc;    /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;   /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  Status LED\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    /* Set the  LED */
    if( State == 0)
    {
        Status = RFM2gSetLed( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetLed( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current Status LED state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Status LED is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doSetLed() */


/******************************************************************************
*
*  FUNCTION:   doSetLoopback
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's transmit loopback
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetLoopback( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;   /* Selects appropriate nomenclature      */
    char *       name;    /* Local pointer to command name         */
    char *       desc;    /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;   /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  transmit loopback\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    if( State == 0)
    {
        Status = RFM2gSetLoopback( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetLoopback( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current transmit loopback state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's transmit loopback is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doSetLoopback() */



/******************************************************************************
*
*  FUNCTION:   doSetPioByteSwap
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's PIO Byte Swap (4 byte swap)
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetPioByteSwap( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;    /* Selects appropriate nomenclature      */
    char *name;            /* Local pointer to command name         */
    char *desc;            /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;    /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )

    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  PIO Byte Swap\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    /* Set the PIO Byte Swap */
    if( State == 0)
    {
        Status = RFM2gSetPIOByteSwap( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetPIOByteSwap( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current DMA Byte Swap state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's PIO Byte Swap is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}


/******************************************************************************
*
*  FUNCTION:   doSetMemoryOffset
*
*  PURPOSE:    Set the current memory offset of the Reflective Memory
*
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetMemoryOffset( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;    /* Selects appropriate nomenclature      */
    char *name;            /* Local pointer to command name         */
    char *desc;            /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int offset;    /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &offset) != 1)  )

    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current memory offset of the "
            "Reflective Memory\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"offset\" is one of the following (0-3):\n" );

        printf( "    0  for No offset\n" );
        printf( "    1  for 0x4000000 offset\n" );
        printf( "    2  for 0x8000000 offset\n" );
        printf( "    3  for 0xc000000 offset\n" );

        printf("\n");
        return(0);
    }

    Status = RFM2gSetMemoryOffset( cmd->Handle, (RFM2G_MEM_OFFSETTYPE) offset );

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current Memory offset.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    return(0);

}

/******************************************************************************
*
*  FUNCTION:   doSetParityEnable
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's Parity Enable
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetParityEnable( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;    /* Selects appropriate nomenclature      */
    char *name;            /* Local pointer to command name         */
    char *desc;            /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;    /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )

    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  Parity Enable\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    /* Set the PIO Byte Swap */
    if( State == 0)
    {
        Status = RFM2gSetParityEnable( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetParityEnable( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current Parity Enable state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Parity Enable is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}

/******************************************************************************
*
*  FUNCTION:   doSetReg
*
*  PURPOSE:    Clear bits in a register
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetReg( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32      index;      /* Selects appropriate nomenclature      */
    char *           name;       /* Local pointer to command name         */
    char *           desc;       /* Local pointer to command description  */
    WIDTH            width;      /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     mask = 0;
	RFM2GREGSETTYPE  regset;

    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

		printf(RFM2G_SUPPORT_PERSONNEL_MSG);

        printf( "\n%s:  Write a value to a register window\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"regset\" is one of the following (0, 1, 2 or 3):\n" );
        printf( "    0  BAR 0 mem - Local Config, Runtime and DMA Registers\n" );
        printf( "    1  BAR 1 I/O - Local Config, Runtime and DMA Registers\n" );
        printf( "    2  BAR 2 mem - RFM Control and Status Registers \n" );
        printf( "    3  BAR 3 mem - reflective memory RAM\n" );
        printf( "    4  BAR 4 Reserved\n" );
        printf( "    5  BAR 5 Reserved\n" );

        printf( "  \"offset\" is the offset into the window to read.\n" );

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );

        printf( "  \"mask\" is a constant byte, word, or longword that specifies\n" );
        printf( "    the bits to set in the register specified by offset.\n\n" );
        return(0);
    }

    /* Get the regset */
    regset = strtoul( cmd->parm[0], 0, 0 );

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[2] );
        return(-1);
    }

    /* Get the value */
    mask = strtoul( cmd->parm[3], 0, 0 );

    /* Write the value */
    status = RFM2gSetReg( cmd->Handle, regset, offset, width.parm, mask);

    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not do Register Write to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Write completed.\n" );

    return(0);

}   /* End of doSetReg() */

/******************************************************************************
*
*  FUNCTION:   doSetThreshold
*
*  PURPOSE:    Set the RFM board's DMA transfer threshold
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetThreshold( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_UINT32 Threshold;
    int  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS status;

    if( usage || (cmd->parmcount == 0))
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the DMA transfer threshold\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    /* Set the DMA threshold */
    Threshold = strtoul( cmd->parm[0], 0, 0 );

    status = RFM2gSetDMAThreshold( cmd->Handle, Threshold );
    if( status != RFM2G_SUCCESS )
    {
        printf( "Could not set the DMA threshold.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "The DMA Threshold is %d.\n", Threshold);

    return(0);

}   /* End of doSetThreshold() */

/******************************************************************************
*
*  FUNCTION:   doSetTransmit
*
*  PURPOSE:    Set the current on/off state of the Reflective Memory
*              board's Transmitter
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSetTransmit( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;   /* Selects appropriate nomenclature      */
    char *       name;    /* Local pointer to command name         */
    char *       desc;    /* Local pointer to command description  */
    RFM2G_STATUS Status;
    int          State;   /* Keep as int type as it is passed by reference to sscanf */

    index = cmd->command;

    if( usage || (cmd->parmcount != cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &State) != 1) ||
        !( (State == 0) || (State == 1) ) )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Set the current on/off state of the "
            "Reflective Memory board's \n  Transmitter\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "\n  \"state\" is one of the following (0-1):\n" );

        printf( "    0  for OFF\n" );
        printf( "    1  for ON\n" );

        printf("\n");
        return(0);
    }

    /* Set the  LED */
    if( State == 0)
    {
        Status = RFM2gSetTransmit( cmd->Handle, RFM2G_OFF );
    }
    else
    {
        Status = RFM2gSetTransmit( cmd->Handle, RFM2G_ON );
    }

    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not set the current Transmitter state.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    printf( "The Reflective Memory board's Transmitter is %s.\n",
        (State? "ON" : "OFF") );

    return(0);

}   /* End of doSetTransmit() */

/******************************************************************************
*
*  FUNCTION:   doSize
*
*  PURPOSE:    Display the size of the RFM Memory
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doSize( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_STATUS Status;
    RFM2G_UINT32 Size = 0;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display the size of the RFM memory\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get Size */
    Status = RFM2gSize( cmd->Handle, &Size );
    if( Status != RFM2G_SUCCESS )
    {
        printf( "Could not get the Size Offset.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
        return(-1);
    }

    /* Print Size */
    printf ("    Size                       %d (0x%08X)\n\n", Size, Size);

    return(0);

}   /* End of doSize() */

/******************************************************************************
*
*  FUNCTION:   doUnMapUser
*
*  PURPOSE:    Unmap the User buffer
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/
static RFM2G_INT32
doUnMapUser( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32   index = cmd->command; /* Selects appropriate nomenclature  */
    char *        name;     			/* Local pointer to command name     */
    char *        desc;             /* Local pointer to command description  */
    RFM2G_STATUS  status;
    RFM2G_UINT32  Pages;
    volatile void *UserMemoryPtr;

    if (usage)
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Unmap the User buffer pointer returned by the \n", name);
        printf( "  mapuser command.\n\n");
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    UserMemoryPtr = (volatile void *)cmd->MappedDmaBase;
    Pages = cmd->DmaPages;

    /* Don't attempt to unmap memory that hasn't been mapped */
    if (UserMemoryPtr == NULL)
    {
        printf("Error: There is no mapped memory to unmap.\n");
        return -1;
    }

    /* Don't attempt to unmap memory that was mapped with mapuserbytes
       instead of mapuser */
    if (Pages == 0)
    {
        printf("Error: Cannot unmap memory originally mapped with mapuserbytes.\n");
        printf("       Use \"unmapuserbytes\" instead.\n");
        return -1;
    }

    status = RFM2gUnMapUserMemory(cmd->Handle, (void volatile **)(&UserMemoryPtr), Pages);

    if( status  != RFM2G_SUCCESS )
    {
#if defined(RFM2G_VXWORKS) || defined(RFM2G_LINUX) /* Add OS's here that prepend '0x' on %p */
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "RFM2gUnMapUserMemory returned Error. UserMemoryPtr = %p, %d Page%s\n",
                UserMemoryPtr,  Pages, (Pages > 1)? "s" : "");
#else
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "RFM2gUnMapUserMemory returned Error. UserMemoryPtr = 0x%p, %d Page%s\n",
                UserMemoryPtr,  Pages, (Pages > 1)? "s" : "");
#endif
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

#if defined(RFM2G_VXWORKS) || defined(RFM2G_LINUX) /* Add OS's here that prepend '0x' on %p */
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "UserMemoryPtr %p (%d Page%s) successfully unmapped.\n",
            cmd->MappedDmaBase,  Pages, (Pages > 1)? "s" : "");
#else
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "UserMemoryPtr 0x%p (%d Page%s) successfully unmapped.\n",
            cmd->MappedDmaBase,  Pages, (Pages > 1)? "s" : "");
#endif

	cmd->MappedDmaBase = NULL;
    cmd->DmaPages = 0;
    cmd->DmaRange = 0;

    return(0);

}   /* End of doUnMapUser() */

/******************************************************************************
*
*  FUNCTION:   doUnMapUserBytes
*
*  PURPOSE:    Unmap the User buffer
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/
static RFM2G_INT32
doUnMapUserBytes( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32   index = cmd->command; /* Selects appropriate nomenclature  */
    char *        name;     			/* Local pointer to command name     */
    char *        desc;             /* Local pointer to command description  */
    RFM2G_STATUS  status;
    RFM2G_UINT32  Bytes;
    volatile void *UserMemoryPtr;


    if (usage)
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Unmap the User buffer pointer returned by the \n", name);
        printf( "  mapuserbytes command.\n\n");
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf("\n");
        return(0);
    }

    UserMemoryPtr = (volatile void *)cmd->MappedDmaBase;
    Bytes = cmd->DmaRange;

    /* Don't attempt to unmap memory that hasn't been mapped */
    if (UserMemoryPtr == NULL)
    {
        printf("Error: There is no mapped memory to unmap.\n");
        return -1;
    }

    /* Don't attempt to unmap memory that was mapped with mapuser
       instead of mapuserbytes */
    if (Bytes == 0)
    {
        printf("Error: Cannot unmap memory originally mapped with mapuser.\n");
        printf("       Use \"unmapuser\" instead.\n");
        return -1;
    }

    status = RFM2gUnMapUserMemoryBytes(cmd->Handle, (void volatile **)(&UserMemoryPtr), Bytes);

    if( status  != RFM2G_SUCCESS )
    {
#if defined(RFM2G_VXWORKS) || defined(RFM2G_LINUX) /* Add OS's here that prepend '0x' on %p */
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "RFM2gUnMapUserMemoryBytes returned Error. UserMemoryPtr = %p, 0x%08x Byte%s\n",
                UserMemoryPtr,  Bytes, (Bytes > 1)? "s" : "");
#else
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "RFM2gUnMapUserMemoryBytes returned Error. UserMemoryPtr = 0x%p, 0x%08x Byte%s\n",
                UserMemoryPtr,  Bytes, (Bytes > 1)? "s" : "");
#endif
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

#if defined(RFM2G_VXWORKS) || defined(RFM2G_LINUX) /* Add OS's here that prepend '0x' on %p */
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "UserMemoryPtr %p (0x%X Byte%s) successfully unmapped.\n",
            cmd->MappedDmaBase,  Bytes, (Bytes > 1)? "s" : "");
#else
        /* VxWorks printf %p prepends '0x', Solaris does not. */
        printf( "UserMemoryPtr 0x%p (0x%X Byte%s) successfully unmapped.\n",
            cmd->MappedDmaBase,  Bytes, (Bytes > 1)? "s" : "");
#endif

	cmd->MappedDmaBase = NULL;
    cmd->DmaPages = 0;
    cmd->DmaRange = 0;

    return(0);

}   /* End of doUnMapUserBytes() */

/******************************************************************************
*
*  FUNCTION:   doWait
*
*  PURPOSE:    Wait for an event to be received, or time out
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doWait( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32     index;      /* Selects appropriate nomenclature       */
    char *          name;       /* Local pointer to command name          */
    char *          desc;       /* Local pointer to command description   */
    RFM2GEVENTINFO  EventInfo;  /* Info about the received interrupt */
    RFM2G_INT32     i;
    unsigned int    event;
    int             timeout;

    index = cmd->command;

    if( usage || cmd->parmcount != (cmd->pCmdInfoList[index].parmcount) ||
        (sscanf(cmd->parm[0], "%i", &event) != 1) ||
        (event >= event_name_array_num_elements)  ||
        (sscanf(cmd->parm[1], "%i", &timeout) != 1))
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Wait for an event to be received, or time out\n\n",
            name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following interrupt events [0-%d]:\n",
            event_name_array_num_elements-1);

        for( i=RFM2GEVENT_RESET; i<event_name_array_num_elements; i++ )
        {
            printf( "    %2d  %s\n", i, event_name[i].name );
        }

        printf( "\n  and \"timeout\" is the number of milliseconds to wait.\n\n" );

        return(0);
    }

    /* Get the interrupt type */
    EventInfo.Event = event_name[event].EventType;

    /* Get the timeout */
    EventInfo.Timeout = timeout;

    /* Now wait on the interrupt */
    printf("Waiting for event ... ");
    fflush(stdout);
    switch( i=RFM2gWaitForEvent( cmd->Handle, &EventInfo ) )
    {
        case RFM2G_SUCCESS:
            printf( "\nReceived \"%s\" event from node %d.\n",
                event_name[EventInfo.Event].name, EventInfo.NodeId );

            switch (EventInfo.Event)
            {
                case RFM2GEVENT_INTR1:
                case RFM2GEVENT_INTR2:
                case RFM2GEVENT_INTR3:
                case RFM2GEVENT_INTR4:
                    printf("  This event's extended data is:  0x%08X.\n",
                        EventInfo.ExtendedInfo );
                    break;

                default:

                    break;
            }
            break;

        case RFM2G_TIMED_OUT:
            printf( "Timed out.\n" );
            break;

        case RFM2G_EVENT_IN_USE:
            printf( "Notification for this event has already been requested.\n" );
            break;

        default:
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(i));
            break;
    }

    return(0);

}   /* End of doWait() */

/******************************************************************************
*
*  FUNCTION:   doWrite
*
*  PURPOSE:    Writes a range of RFM memory with a byte, word, or long constant
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doWrite( DIAGCMD *cmd, RFM2G_UINT8 usage )

{
    RFM2G_INT32      index;      /* Selects appropriate nomenclature      */
    char *           name;       /* Local pointer to command name         */
    char *           desc;       /* Local pointer to command description  */
    WIDTH            width;      /* Initialization based on data width    */
    RFM2G_STATUS     status;
    RFM2G_UINT32     offset;
    RFM2G_UINT32     length;
    RFM2G_UINT32     bytes;
    RFM2G_UINT32     value = 0;
#if defined(RFM2G_VXWORKS)
	RFM2G_UINT64     value64 = 0LL;
#else
	RFM2G_UINT64     value64 = 0;
#endif

    RFM2G_UINT32     j;          /* Loop variables                     */

    RFM2G_UINT8	*	 pByte = NULL;
    RFM2G_UINT8 *    pBuffer     = NULL;
    RFM2G_UINT16 *   pWord       = NULL;
    RFM2G_UINT32 *   pLong       = NULL;
    RFM2G_UINT64 *   pLonglong   = NULL;

    RFM2G_UINT32 windowSize;  /* Size of current sliding window */
    RFM2G_UINT32 windowOffset; /* offset of current sliding window */

    index = cmd->command;

    /* Get info about the current sliding window */
    status = RFM2gGetSlidingWindow(cmd->Handle, &windowOffset, &windowSize);
    if( status == RFM2G_NOT_SUPPORTED )
    {
        windowOffset = 0;
        RFM2gSize(cmd->Handle, &windowSize);
    }
    else if (status != RFM2G_SUCCESS)
    {
        printf("Could not get info about current Sliding Window.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Write an area of Reflective Memory with a constant value\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"value\" is a constant byte, word, or longword that will be\n" );
        printf( "    written to the range specified by offset and length.\n\n" );
        printf( "  \"offset\" is the width-aligned beginning offset to write.\n" );

        RFM2gSize( cmd->Handle, &length );
        printf( "    Valid offsets for PIO are in the range 0x00000000 to 0x%08X.\n",
           windowSize - 1);
        printf( "    Valid offsets for DMA are in the range 0x00000000 to 0x%08X.\n\n",
           length - windowOffset - 1);

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );
        printf( "    8  for Longlong (64 bits)\n\n" );

        displayRange(windowSize, (length-windowOffset));

        return(0);
    }

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[2] );
        return(-1);
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[1], 0, 0 );

    /* Get the length */
    length = strtoul( cmd->parm[3], 0, 0 );

    /* Check for overflow in bytes calculation */
    if (length > (0xFFFFFFFF / width.width))
    {
        printf( "Invalid length \"%s\".\n", cmd->parm[3] );
        return(-1);
    }

    bytes = length * width.width;

    /* Is the offset+length out of range? Let the driver validate this and return
       an error code */

    /* Get the value */
    if (width.width != 8)
    {
        value = strtoul( cmd->parm[0], 0, 0 );
    }
    else
    {
        if (get64bitvalue(cmd->parm[0], &value64) != 0)
        {
            return(-1);
        }
    }

#if (defined(RFM2G_LINUX))

    {
        RFM2G_UINT32 Threshold = 0;

        /* see if DMA threshold and buffer are intialized */
        RFM2gGetDMAThreshold( cmd->Handle, &Threshold );

        /* see if DMA threshold and buffer are intialized */
        if ((cmd->MappedDmaBase != NULL) && (length >= Threshold))
        {
            printf("Write used DMA.\n");

            /* set up width size pointers */
            pByte = (RFM2G_UINT8*) cmd->MappedDmaBase;
            pWord = (RFM2G_UINT16*)cmd->MappedDmaBase;
            pLong = (RFM2G_UINT32*)cmd->MappedDmaBase;
        }
        else
        {
            printf("DMA will not be used.\n");

            /* set up width size pointers */
            pByte     = (RFM2G_UINT8*)  &cmd->MemBuf;
            pWord     = (RFM2G_UINT16*) &cmd->MemBuf;
            pLong     = (RFM2G_UINT32*) &cmd->MemBuf;
            pLonglong = (RFM2G_UINT64*) &cmd->MemBuf;
        }
    }

#else

    /* set up width size pointers */
    pByte     = (RFM2G_UINT8*)  &cmd->MemBuf;
    pWord     = (RFM2G_UINT16*) &cmd->MemBuf;
    pLong     = (RFM2G_UINT32*) &cmd->MemBuf;
    pLonglong = (RFM2G_UINT64*) &cmd->MemBuf;

#endif

    if (bytes > MEMBUF_SIZE)
    {
        bytes = MEMBUF_SIZE;
        length = bytes / width.width;
        printf("Write length limited to MEMBUF_SIZE %d bytes.\n", bytes);
    }

    /* Initialize pBuffer pointer to begining of buffer */
    pBuffer = pByte;

    /* fill the buffer */
    for( j=0; j<length; j++ )
    {
        switch (width.width)
        {
            case 1:
            *pByte = (RFM2G_UINT8) value;
            pByte++;
            break;

            case 2:
            *pWord = (RFM2G_UINT16) value;
            pWord++;
            break;

            case 4:
            *pLong = (RFM2G_UINT32) value;
            pLong++;
            break;

            case 8:
            *pLonglong = value64;
            pLonglong++;
            break;
        }
    }

    /* Write the buffer */
    status = RFM2gWrite( cmd->Handle, offset, pBuffer, bytes);

    if (status != RFM2G_SUCCESS)
    {
        printf( "Could not do Write to offset 0x%X.\n", offset );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    printf( "Write completed.\n" );

    return(0);

}   /* End of doWrite() */


/******************************************************************************
*
*  FUNCTION:   EventCallback
*
*  PURPOSE:    Sample event handler used with asynchronous event
*                notification
*  PARAMETERS: EventInfo: (I) Information about the received event
*  RETURNS:    void
*  GLOBALS:    event_name
*
******************************************************************************/

static void
EventCallback( RFM2GHANDLE rh, RFM2GEVENTINFO *EventInfo )
{
    RFM2G_NODE NodeId;

    RFM2gNodeID(rh,  &NodeId);

    switch( EventInfo->Event )
    {
        case RFM2GEVENT_INTR1:
        case RFM2GEVENT_INTR2:
        case RFM2GEVENT_INTR3:
        case RFM2GEVENT_INTR4:
            callbackCounter[EventInfo->Event]++;
            printf("EventCallback:  counter = %ld ", (unsigned long int)callbackCounter[EventInfo->Event]);
            printf("\nnode %d Received \"%s\" interrupt from node %d\n", NodeId,
                event_name[EventInfo->Event].name, EventInfo->NodeId );
            printf("Extended Data = 0x%08X\n", EventInfo->ExtendedInfo);
            break;

        case RFM2GEVENT_BAD_DATA:
        case RFM2GEVENT_RXFIFO_FULL:
        case RFM2GEVENT_ROGUE_PKT:
        case RFM2GEVENT_RXFIFO_AFULL:
        case RFM2GEVENT_SYNC_LOSS:
        case RFM2GEVENT_MEM_WRITE_INHIBITED:
        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR:
        case RFM2GEVENT_RESET:

            callbackCounter[EventInfo->Event]++;
            printf("EventCallback:  counter = %ld ", (unsigned long int)callbackCounter[EventInfo->Event]);
            printf("\nnode %d Received \"%s\" interrupt\n", NodeId,
                event_name[EventInfo->Event].name );
            break;

        default:
            printf("\nnode %d Received unknown interrupt event %d\n", NodeId,
                EventInfo->Event );
            break;
    }

    fflush(stdout);

}   /* End of EventCallback() */

/******************************************************************************
*
*  FUNCTION:   doPerformance
*
*  PURPOSE:    Display the RFM's Board's PCI Performance
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doPerformance( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32    index;  /* Selects appropriate nomenclature      */
    char *         name;   /* Local pointer to command name         */
    char *         desc;   /* Local pointer to command description  */
    RFM2G_STATUS   Status;
    time_t         time1;  /* ANSI C time */
    double         mBytePerSec   = 0;
    int            numBytes      = 0;
    int            numIterations = 10000;
    int            count;
    int            timeToRunTest = 2;  /* Time to run tests in seconds */
    RFM2G_UINT32   dmaThreshold;

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Displays the Reflective Memory board's Performance\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );
        printf("\n");
        return(0);
    }

    /* Get the DMA Threshold */
    RFM2gGetDMAThreshold(cmd->Handle, &dmaThreshold);

    printf("\nRFM2g Performance Test (DMA Threshold is %u)\n", dmaThreshold);
    printf("---------------------------------------------------\n");
    printf("   Bytes     Read IOps   Read MBps     Write IOps     Write MBps\n");

    for (numBytes = 0; numBytes < MEMBUF_SIZE; )
    {
        if (numBytes < 32)
        {
            numBytes += 4;
        }
        else if (numBytes < 128)
        {
            numBytes += 32;
        }
        else if (numBytes < 2048)
        {
            numBytes += 128;
        }
        else if (numBytes < 4096)
        {
            numBytes += 512;
        }
        else if (numBytes < 16384)
        {
            numBytes += 4096;
        }
        else if (numBytes < 131072)
        {
            numBytes += 16384;
        }
        else if (numBytes < 262144)
        {
            numBytes += 65536;
        }
        else
        {
            numBytes += 262144;
        }

        numIterations = 0;

        if (time(&time1) == -1)
            {
            printf("ANSI C time function returned ERROR\n");
            return(-1);
            }

        /* Wait for the timer to get to the begining of a second */
        while (difftime(time(0), time1) == 0)
            {
            /* Let's wait */
            count++;
            }

        /* Get the start time */
        time(&time1);

        /* The accuracy of the results is dependent on the amount of time
           the test runs and the Number of IO's per second.  This is not a
           precise test, the priority of this task along with the limited
           resolution (1 Sec) of the ANSI C time function affects the
           precision.  */

        while (difftime(time(0), time1) < timeToRunTest)
        {
#if (defined(RFM2G_LINUX))
            /* Use DMA if a preallocated DMA buffer exists and the data is
               larger than the threshold */
            if (cmd->MappedDmaBase != NULL && numBytes > dmaThreshold)
            {
                /* Using DMA buffer */
                Status = RFM2gRead(cmd->Handle,
                             0, /* rfmOffset */
                             (void*) cmd->MappedDmaBase,
                             numBytes);
            }
            else
            {
                Status = RFM2gRead(cmd->Handle,
                             0, /* rfmOffset */
                             (void*) cmd->MemBuf,
                             numBytes);
            }

#else
            Status = RFM2gRead(cmd->Handle,
                         0, /* rfmOffset */
                         (void*) cmd->MemBuf,
                         numBytes);
#endif

            if (Status != RFM2G_SUCCESS)
            {
                printf("RFM2gRead : Failure\n");
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
                return(-1);
            }

            numIterations++;

        }

        /* Calculate MByte/Sec = Total number of bytes transferred / (Time
           in seconds * 1024 * 1024) */
        mBytePerSec = (double)(numBytes * numIterations) / ( timeToRunTest *
                      1024.0 * 1024.0);

        printf("%8d    %10d      %6.1f", numBytes,
              (numIterations / timeToRunTest), mBytePerSec);

        numIterations = 0;

        time(&time1);

        /* Wait for the timer to get to the begining of a second */
        while (difftime(time(0), time1) == 0)
            {
            /* Let's wait */
            count++;
            }

        /* Get the start time */
        time(&time1);

        /* Perform the IO until the elapsed time occurs */
        while (difftime(time(0), time1) < timeToRunTest)
        {
#if (defined(RFM2G_LINUX))
            /* Use DMA if a preallocated DMA buffer exists and the data is
               larger than the threshold */
            if (cmd->MappedDmaBase != NULL && numBytes > dmaThreshold)
            {
                /* Using DMA buffer */
                Status = RFM2gWrite(cmd->Handle,
                              0, /* rfmOffset */
                              (void*) cmd->MappedDmaBase,
                              numBytes);
            }
            else
            {
                Status = RFM2gWrite(cmd->Handle,
                              0, /* rfmOffset */
                              (void*) cmd->MemBuf,
                              numBytes);
            }
#else
            Status = RFM2gWrite(cmd->Handle,
                          0, /* rfmOffset */
                          (void*) cmd->MemBuf,
                          numBytes);
#endif

            if (Status != RFM2G_SUCCESS)
            {
                printf( "RFM2gWrite : Failure\n");
                printf( "Error: %s.\n\n",  RFM2gErrorMsg(Status));
                return(-1);
            }

            numIterations++;

        }

        mBytePerSec = (double) (numBytes * numIterations) / (timeToRunTest *
                      1024.0 * 1024.0);

        printf("     %10d         %6.1f\n",
               (numIterations / timeToRunTest), mBytePerSec);

    } /* for */

    return(0);
}

/******************************************************************************
*
*  FUNCTION:   doReturn
*
*  PURPOSE:    Clears the drvspecrun flag, forcing loop in doDrvSpecific() to break
*  PARAMETERS: cmd: (I) Command code and command parameters, unused
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:    running, commandlist
*
******************************************************************************/

static RFM2G_INT32
doReturn( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    char *name;     /* Local pointer to command name */

    if( usage || cmd->parmcount != cmd->pCmdInfoList[cmd->command].parmcount )
    {
        name = cmd->pCmdInfoList[cmd->command].name;
        printf( "\n%s:  Return to main menu.\n\n", name );
        printf( "Usage:  %s\n\n", name );

        return(0);
    }

    cmd->pCmdInfoList = commandlist;
    cmd->NumEntriesCmdInfoList = sizeof(commandlist) / sizeof(CMDINFO);
    sprintf(cmd->pUtilPrompt,"UTIL%d > ", cmd->deviceNum);

    doReturnDrvSpecific(cmd, 0);

    printf("Welcome to the main menu\n");

    return(0);
}

typedef union	{
	unsigned long	ul;
	float		f;
} rfm2gObject_t, *RFM2GOBJECT;

/******************************************************************************
*
*  FUNCTION:   doMemop
*
*  PURPOSE:    fill memory or verify expected contents
*  PARAMETERS: cmd: (I) Command code and command parameters, unused
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:
*
******************************************************************************/

static RFM2G_INT32
doMemop( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    RFM2G_INT32  index;          /* Selects appropriate nomenclature      */
    char *       name;           /* Local pointer to command name         */
    char *       desc;           /* Local pointer to command description  */
    RFM2G_UINT32 pattern   = 0;
    RFM2G_UINT64 pattern64 = 0;
    RFM2G_UINT32 offset    = 0;
    RFM2G_UINT32 length    = 0;
    RFM2G_UINT32 patterntype = RFM2G_FIXED_DATA; /* default if no arguement is on cmd line*/
    RFM2G_BOOL   onlyCheck;
    WIDTH        width;
    RFM2G_BOOL   isFloat;
    RFM2G_STATUS status;
    RFM2G_UINT32 starting_pattern  = 0x0;
    RFM2G_UINT32 save_pattern = 0x0;
    index = cmd->command;

    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Fill or verify an area of Reflective Memory\n\n", name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  \"pattern\" is the pattern to write or verify.\n" );

        printf( "  \"offset\" is the beginning offset to begin writing or verifying.\n" );

        printf( "  \"width\" is one of the following (1, 2, 4 or 8):\n" );
        printf( "    1  for Byte     (8 bits)\n" );
        printf( "    2  for Word     (16 bits)\n" );
        printf( "    4  for Longword (32 bits)\n" );
        printf( "    8  for Longlong (64 bits)\n\n" );

        RFM2gSize( cmd->Handle, &length );
        printf( "  \"length\" is the number of \"width\" units to write or verify.\n" );
        printf( "    Maximum length for bytes is %d (0x%X)\n",
            length, length );
        printf( "    Maximum length for words is %d (0x%X)\n",
            length/2, length/2 );
        printf( "    Maximum length for 32 bit longs is %d (0x%X)\n",
            length/4, length/4 );
        printf( "    Maximum length for 64 bit longs is %d (0x%X)\n\n",
            length/8, length/8 );

        printf( "  \"verify\" 0 write pattern to memory, 1 verify pattern in memory.\n" );

        printf( "  \"float\" 0 pattern is not a float, 1 pattern is a floating point value.\n" );


		printf( "  \"patterntype\" is one of the following (0, 1, 2 or 3):\n" );
        printf( "    0  for Fixed Data\n" );
        printf( "    1  for Incrementing Address\n" );
        printf( "    2  for Incrementing transfers count\n" );
		printf( "    3  for Inverted Incrementing Address\n" );


        return(0);
    }

    isFloat = (RFM2G_BOOL) strtoul( cmd->parm[5], 0, 0 );

    switch (isFloat)
    {
    case RFM2G_TRUE:
    case RFM2G_FALSE:
        break;
    default:
        printf( "Invalid float \"%s\" option.\n", cmd->parm[5] );
        return(-1);
        break;
    }

    onlyCheck = (RFM2G_BOOL) strtoul( cmd->parm[4], 0, 0 );

    switch (onlyCheck)
    {
    case RFM2G_TRUE:
    case RFM2G_FALSE:
        break;
    default:
        printf( "Invalid verify \"%s\" option.\n", cmd->parm[4] );
        return(-1);
        break;
    }

    /* Get the width */
    width.parm = (RFM2G_UINT8)strtoul( cmd->parm[2], 0, 0 );
    if( ValidWidth( &width) != 0 )
    {
        printf( "Invalid width \"%s\".\n", cmd->parm[2] );
        return(-1);
    }

    /* Get the pattern */
    if( isFloat )
    {
        rfm2gObject_t	u;

        if (width.width != sizeof(float))
        {
            printf( "Invalid float width \"%s\".\n", cmd->parm[2] );
            return(-1);
        }
        u.f = (float) strtod( cmd->parm[0], (char **) 0 );
        pattern = u.ul;
    }
    else
    {
        if (width.width != 8)
        {
            pattern = strtoul( cmd->parm[0], (char **) 0, 0 );
			starting_pattern = pattern;
        }
        else
        {
            if (get64bitvalue(cmd->parm[0], &pattern64) != 0)
            {
                return(-1);
            }
        }
    }

    /* Get the offset */
    offset = strtoul( cmd->parm[1], (char **) 0, 0 );

    /* Get the length */
    length = strtoul( cmd->parm[3], (char **) 0, 0 );

    /* convert length to bytes */
    length *= width.width;

	if( (offset % width.width) != 0 )	{
		printf(
		"FOR ALIGNMENT, OFFSET MUST BE MULTIPLE OF DATA WIDTH!\n"
		);
		return( -1 );
	}
	if( (length % width.width) != 0 )	{
		printf(
		"FOR ALIGNMENT, LENGTH MUST BE MULTIPLE OF DATA WIDTH!\n"
		);
		return( -1 );
	}

    /* Get the pattern type - Fixed, Incrementing address */
    patterntype = strtoul( cmd->parm[6], (char **) 0, 0 );

    switch (patterntype)
    {
    case RFM2G_FIXED_DATA:
    case RFM2G_INCADDR_DATA:
    case RFM2G_INC_DATA:
    case RFM2G_INVINCADDR_DATA:
        break;

    default:
        printf( "Invalid patterntype \"%s\" option.\n", cmd->parm[6] );
        return(-1);
        break;
    }

	if( onlyCheck != RFM2G_FALSE )
    {
		/* Validate memory contents				 */
		RFM2G_BOOL   ok       = RFM2G_TRUE;
		RFM2G_UINT32 actual   = 0;
		RFM2G_UINT32 expected = 0;
		switch( width.width )
        {
		default:
			printf( "%u-BYTE DATA NOT SUPPORTED!\n", width.width );
			return( -1 );
		case 1:
			{
				RFM2G_UINT8  cscratch;
				RFM2G_UINT8* c_dataval = &cscratch;

				RFM2G_STATUS status;
                for( ; length != 0; length -= 1, offset += 1 )
                {
	                status = RFM2gPeek8( cmd->Handle, offset, c_dataval );

                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do peek8 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
	                else
                    {
                        if(*c_dataval != (RFM2G_UINT8) pattern)
                        {
                            actual = *c_dataval;
                            expected = (RFM2G_UINT8) pattern;
                            ok = RFM2G_FALSE;
                            break;
                        }
                    }
                }
			}
			break;
		case 2:
			{
				RFM2G_UINT16  aWord     = (RFM2G_UINT16) pattern;
				RFM2G_UINT16  sscratch;
				RFM2G_UINT16* s_dataval = &sscratch;
				RFM2G_STATUS  status;

				for( ; length != 0; length -= 2, offset += 2 )
                {
					status = RFM2gPeek16( cmd->Handle, offset, s_dataval);
                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do peek16 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
                    else
                    {
                        if (*s_dataval != aWord)
                        {
                            actual = *s_dataval;
                            expected = (RFM2G_UINT16) pattern;
                            ok = RFM2G_FALSE;
                            break;
                        }
                    }
                }
            }
			break;
		case 4:
			{
				RFM2G_UINT32  aLong     = pattern;
				RFM2G_UINT32  lscratch;
				RFM2G_UINT32* l_dataval = &lscratch;
				RFM2G_STATUS  status;

				save_pattern = starting_pattern;

				for( ; length != 0; length -= 4, offset += 4 )
                {

                    status = RFM2gPeek32( cmd->Handle, offset, l_dataval );

                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do peek32 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
                    else
                    {
                        if(patterntype == RFM2G_INVINCADDR_DATA)
                        {
                            aLong = ~save_pattern;
                        }

                        if(*l_dataval != aLong)
                        {
                            actual = *l_dataval;
                            expected = pattern;
                            ok = RFM2G_FALSE;
                            break;
                        }
						/* Check if user selected incrementing address pattern */
						if(patterntype == RFM2G_INCADDR_DATA){
							aLong += 0x04;
						}
						else if(patterntype == RFM2G_INC_DATA){
							aLong += 0x01;
								if(aLong == 0xffffffff){
									aLong = starting_pattern;
								}
						}
						else if(patterntype == RFM2G_INVINCADDR_DATA){

							save_pattern += 4;
							aLong = ~(save_pattern);
						}
                    }

                }
            }
			break;
		case 8:
			{
				RFM2G_UINT64  temp64;
				RFM2G_STATUS  status;

				for( ; length != 0; length -= 8, offset += 8 )
                {

                    status = RFM2gPeek64( cmd->Handle, offset, &temp64 );

                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do peek64 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
                    else
                    {
                        if(temp64 != pattern64)
                        {

                            #if defined(RFM2G_VXWORKS)

                            printf( "RFM[%u/%u]=0x%08X%08X, EXPECTED=0x%08X%08X\n",
	                            offset,
                                width.width,
                                ((RFM2G_UINT32*) &temp64)[0], ((RFM2G_UINT32*) &temp64)[1],
                                ((RFM2G_UINT32*) &pattern64)[0], ((RFM2G_UINT32*) &pattern64)[1]);

						    #elif defined(RFM2G_LINUX)

                            printf( "RFM[%u/%u]=0x%llX, EXPECTED=0x%llX\n",
	                            offset, width.width, temp64, pattern64 );

                            #elif defined(WIN32)

                            printf( "RFM[%u/%u]=0x%I64X, EXPECTED=0x%I64X\n",
	                            offset, width.width, temp64, pattern64 );

                            #else

                            printf( "RFM[%u/%u]=0x%lX, EXPECTED=0x%lX\n",
	                            offset, width.width, temp64, pattern64 );

                            #endif

                            return(-1);
                            break;
                        }
                    }

                }
            }
			break;
		}

		if ( ok == RFM2G_FALSE )
        {
            printf( "VERIFY FAILED!\n");
			if( isFloat != RFM2G_FALSE)
            {
                rfm2gObject_t	uActual;
                rfm2gObject_t	uExpected;
                uActual.ul    = actual;
                uExpected.ul  = expected;
                printf( "RFM[%u/%u]=%g, EXPECTED=%g\n",
	                offset, width.width, uActual.f, uExpected.f );
			}
            else
            {
                printf( "RFM[%u/%u]=0x%lX, EXPECTED=0x%lX\n",
	                offset, width.width, (long unsigned int) actual, (long unsigned int) expected );
			}
			return( -1 );
		}
        else
        {
            printf( "VERIFY PASSED!\n");
        }
	}
    else
    {
		/* Set new memory contents				 */
		switch( width.width )
        {
            default:
                printf( "%u-BYTE DATA NOT SUPPORTED!\n", width.width );
                return( -1 );

            case 1:
            {
	            for( ; length != 0; length -= 1, offset += 1 )
                {
		            status = RFM2gPoke8( cmd->Handle, offset, (RFM2G_UINT8) pattern );
                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do poke8 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
	            }
            }
            break;
            case 2:
            {
	            for( ; length != 0; length -= 2, offset += 2 )
                {
		            status = RFM2gPoke16( cmd->Handle, offset, (RFM2G_UINT16) pattern );
                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do poke16 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
	            }
            }
            break;
            case 4:
            {
				save_pattern = starting_pattern;

	            for( ; length != 0; length -= 4, offset += 4 )
                {
                    if(patterntype == RFM2G_INVINCADDR_DATA)
                    {
                        pattern = ~save_pattern;
                    }

	                status = RFM2gPoke32( cmd->Handle, offset, pattern );
                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do poke32 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
					if(patterntype == RFM2G_INCADDR_DATA){
							pattern += 4;
					}
					else if(patterntype == RFM2G_INC_DATA){
						  pattern += 0x01;
							if(pattern == 0xffffffff){
								pattern = starting_pattern;
							}
					}
					else if(patterntype == RFM2G_INVINCADDR_DATA){
							save_pattern += 4;
							pattern = ~(save_pattern);
					}
	            }
            }
            break;
            case 8:
            {
	            for( ; length != 0; length -= 8, offset += 8 )
                {
	                status = RFM2gPoke64( cmd->Handle, offset, pattern64 );
                    if( status != RFM2G_SUCCESS )
                    {
                        printf( "Could not do poke64 from offset 0x%X.\n", offset );
                        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
                        return(-1);
                    }
	            }
            }
            break;

            }
        printf( "Memory contents written.\n");

	}
	return( 0 );
}


/******************************************************************************
*
*  FUNCTION:   doSlidingWindowGet
*
*  PURPOSE:    Display the offset of the current Sliding Window
*  PARAMETERS: cmd: (I) Command code and command parameters, unused
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:
*
******************************************************************************/

static RFM2G_INT32
doSlidingWindowGet(DIAGCMD *cmd, RFM2G_UINT8 usage)
{
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_INT32  index;        /* Selects appropriate nomenclature      */
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;  /* Offset of current sliding window   */
    RFM2G_UINT32 windowSize;  /* Size of current sliding window */


    if (usage)
    {
        index = cmd->command;
        name  = cmd->pCmdInfoList[index].name;
        desc  = cmd->pCmdInfoList[index].desc;

        printf("\n%s:  Get the offset and size of the current Sliding Window.\n\n", name);
        printf("Usage:  %s  %s\n\n", name, desc);
        return 0;
    }

    /* Get info about the current sliding window */
    status = RFM2gGetSlidingWindow(cmd->Handle, &offset, &windowSize);
    if( status != RFM2G_SUCCESS )
    {
        printf("Could not get info about current Sliding Window.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    printf("The %d MByte Sliding Window begins at offset 0x%08X.\n",
        windowSize / 0x00100000, offset);

    return 0;
}


/******************************************************************************
*
*  FUNCTION:   doSlidingWindowSet
*
*  PURPOSE:    Set the offset of the Sliding Window
*  PARAMETERS: cmd: (I) Contains the offset
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0
*  GLOBALS:
*
******************************************************************************/

static RFM2G_INT32
doSlidingWindowSet( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    char *name;        /* Local pointer to command name         */
    char *desc;        /* Local pointer to command description  */
    RFM2G_INT32  index = cmd->command;
    RFM2G_STATUS status;
    RFM2G_UINT32 offset;  /* Offset of sliding window   */
    RFM2G_UINT32 windowSize;  /* Size of current sliding window */
    RFM2G_UINT32 memSize;  /* Size of installed reflective memory */


    /* Get info about the current sliding window */
    status = RFM2gGetSlidingWindow(cmd->Handle, &offset, &windowSize);
    if( status != RFM2G_SUCCESS )
    {
        printf("Could not get info about current Sliding Window.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    /* Get the size of the installed reflective memory */
    status = RFM2gSize(cmd->Handle, &memSize);
    if( status != RFM2G_SUCCESS )
    {
        printf("Could not get the size of the installed reflective memory.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    if (usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount)
    {
        name  = cmd->pCmdInfoList[index].name;
        desc  = cmd->pCmdInfoList[index].desc;

        printf("\n%s:  Set the offset of the Sliding Window.\n\n", name);
        printf("Usage:  %s  %s\n\n", name, desc);

        printf("Total installed reflective memory is %d MBytes.\n",
            memSize / 0x00100000);
        printf("The configured Sliding Window size is %d MBytes (0x%08X bytes).\n",
            windowSize / 0x00100000, windowSize);

        if ((memSize/windowSize) == 1)
        {
            printf("There is one possible Sliding Window offset.\n");
        }
        else
        {
            printf("There are %d possible Sliding Window offsets.\n",
                memSize / windowSize);
        }

        printf("The offset must be a multiple of the Sliding Window size.\n\n");

        switch (memSize / windowSize)
        {
        case 1:
            printf("The only valid offset is 0x00000000.\n\n");
            break;
        case 2:
            printf("The only valid offsets are 0x00000000 and 0x%08X.\n\n",
                windowSize);
            break;
        default:
            printf("Examples:  0x00000000, 0x%08X, 0x%08X, etc.\n\n",
                windowSize, windowSize*2);
        }
        return 0;
    }

    /* Get the offset */
    offset = (RFM2G_UINT32)(strtoul( cmd->parm[0], 0, 0 ));

    /* Set the offset for this sliding window */
    status = RFM2gSetSlidingWindow(cmd->Handle, offset);
    if (status != RFM2G_SUCCESS)
    {
        printf("Could not set the Sliding Window offset.\n");
        printf("Error: %s.\n\n",  RFM2gErrorMsg(status));
        return -1;
    }

    printf("The %d MByte Sliding Window begins at offset 0x%08X.\n",
        windowSize / 0x00100000, offset);

    return 0;
}


static RFM2G_INT32 get64bitvalue(char* pValue64Str, RFM2G_UINT64* pValue64)
{

#if defined (RFM2G_VXWORKS)

        char  valueHigh32[20];
        char  valueLow32[20];
        char* pValueStr;
        int   valueStrLen;

        bzero(valueHigh32, 20);
        bzero(valueLow32, 20);

        pValueStr = strstr(pValue64Str, "0x");

        if (pValueStr == NULL)
            {
            pValueStr = strstr(pValue64Str, "0X");
            if (pValueStr == NULL)
                {
                printf("For 64 bit type, hex format must be used, starting with 0x or 0X\n");
                return(-1);
                }
            }

        valueStrLen = strlen(pValueStr);

        if (valueStrLen <= 10)  /* strtoul can handle a 32 bit value ok */
            {
            *pValue64 = (RFM2G_UINT64) strtoul( pValue64Str, 0, 0 );
            }
        else
            {
            strcpy(valueLow32, "0x");

            /* copy the most significant part of the number
               max string length is 18, 16 for the number and 2 for the 0x */
            strncpy(valueHigh32, pValue64Str, (valueStrLen - 8));

            /* copy the least significant part of the number */
            strncpy((valueLow32+2), (pValue64Str + (valueStrLen - 8)), 8);

            ((RFM2G_UINT32*) pValue64)[0] = (RFM2G_UINT32) strtoul( valueHigh32, 0, 0 );
            ((RFM2G_UINT32*) pValue64)[1] = (RFM2G_UINT32) strtoul( valueLow32, 0, 0 );

            }

#elif defined(WIN32)

		sscanf(pValue64Str, "%I64X", pValue64);

#elif defined(RFM2G_LINUX)

		sscanf(pValue64Str, "%llX", pValue64);

#else

        *pValue64 = strtoul( pValue64Str, 0, 0 );

#endif

    return 0;
}

/**************************************************/
/* flash library                                  */
/**************************************************/

#define MAX_BOARDS	10 /* max devices in system */
#define MAXLEN		0x100
#ifndef _1M
#define _1M			0x100000
#define _2M			0x200000
#define _64K		0x10000
#endif

/* flash commands */
#define FL_AUTOSEL		0x90	/* Device identification */
#define FL_RESET		0xf0	/* Return to DATA mode */
#define FL_ERASE		0x80
#define FL_ERASE_CHIP	0x10
#define FL_CHIP		0x10
#define FL_SECT		0x30
#define FL_PGM			0xa0
#define FL_SUSPEND		0xb0	/* Erase Suspend */
#define FL_RESUME		0x30	/* Erase Resume */

/* flash offsets */
#define AMD_CMDOFFS1	(0xAAA <<0)
#define AMD_CMDOFFS2	(0x555 <<0)

/* flash status */
#define ENB			1
#define LAT			2
#define CE			4
#define OE			8
#define WE			0x10
#define RDY			0x20

typedef struct {
	volatile RFM2G_INT32 *pldareg;
	volatile RFM2G_INT32 *rfmreg;
	volatile RFM2G_INT32 *rfmmem;
	volatile RFM2G_INT32 *flashbase;
	RFM2GHANDLE rh;
} XLADDR, * XLADDR_PTR;

#define SWAP32( data) ( ((data & 0xff) <<24) | ((data & 0xff00) <<8) | ((data & 0xff0000) >> 8) | (data & 0xff000000) >> 24 )

/* functions */

/* flash driver */
static RFM2G_UINT32 read_long( volatile RFM2G_INT32* addr);
static void write_long(volatile RFM2G_INT32 *addr, RFM2G_UINT32 data);
static RFM2G_UINT8 fl_inb (volatile RFM2G_INT32 *pfxl, RFM2G_UINT32 addr);
static int fl_outb (volatile RFM2G_INT32 *pfxl, RFM2G_UINT32 addr , RFM2G_UINT8 data);
static int fl_reset_amd (void);
static int readFlashId(void);
static int fl_program_amd(RFM2G_UINT32 pa, RFM2G_UINT8 *pd, int len);
static int fl_isbusy_amd(RFM2G_UINT32 offset);
static int fl_wait_busy_amd(void);
static int fl_erase_sector_amd(RFM2G_UINT32 offset);
static RFM2G_UINT32 fbOffset;

/* flash api */
static int flashMain();
static int flashTerm();
static void flashMenu( void );
static void flashDisplayDeviceInfo(int i);
static int readFlash(void);
static int copyFlashToFile(void);
static int verifyFlash(void);
static int programFlash(void);
static int eraseFlash(void);
static int eraseFlashBySector();
static void flashSelect();

/* flash variables */
static volatile RFM2G_INT32 *pldaregs; /* plda register pointer */
static volatile RFM2G_INT32 *csr5565;  /* rfm register pointer */
static volatile RFM2G_INT32 *refmem5565; /* rfm memory pointer */
static volatile RFM2G_INT32 *flashbase; /* selected flash base pointer */
static XLADDR xladdr[MAX_BOARDS]; /* flash device data */
static RFM2G_BOOL fPPC = RFM2G_TRUE; /* PPC mode */
static char user[MAXLEN]; /* user string */
static RFM2G_UINT32 selected_brd; /* selected flash memory device */
static RFM2G_UINT32 num_brds; /* number of flash devices found in system */

#define FLASH_DATA (volatile RFM2G_INT32*)(flashbase)
#define FLASH_ADDR (volatile RFM2G_INT32*)((char*)flashbase+4)
#define FLASH_CTRL (volatile RFM2G_INT32*)((char*)flashbase+8)

/* flash driver routines */

/* read flash register that supports byte swapping */
static RFM2G_UINT32 read_long( volatile RFM2G_INT32* addr)
{
    RFM2G_UINT32 data;

    data = *addr;

    if(fPPC)
    {
        data = SWAP32(data);

    }
    return (data);
}

/* write flash register that supports byte swapping */
static void write_long(volatile RFM2G_INT32 *addr, RFM2G_UINT32 data)
{
    if(fPPC)
    {
        *addr = SWAP32(data);
    }
    else
    {
        *addr = data;
    }
}

/* flash 'in' command */
static RFM2G_UINT8 fl_inb (volatile RFM2G_INT32 *pfxl, RFM2G_UINT32 addr)
{
    RFM2G_UINT8 data;
    write_long(FLASH_ADDR, addr);
    read_long(FLASH_ADDR);
    write_long(FLASH_CTRL, ENB | CE | OE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE | OE | LAT);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE | OE | LAT);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE | OE | LAT);
    read_long(FLASH_CTRL);
    data = (RFM2G_UINT8) read_long(FLASH_DATA);
    write_long(FLASH_CTRL, ENB | CE | OE | LAT);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, 0);
    read_long(FLASH_CTRL);

    return(data);
}

/* flash 'out' command */
static int fl_outb (volatile RFM2G_INT32 *pfxl, RFM2G_UINT32 addr , RFM2G_UINT8 data)
{
    write_long(FLASH_ADDR, addr);
    read_long(FLASH_ADDR);
    write_long(FLASH_DATA, data);
    read_long(FLASH_DATA);
    write_long(FLASH_CTRL, ENB | CE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE | WE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE | WE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, ENB | CE);
    read_long(FLASH_CTRL);
    write_long(FLASH_CTRL, 0);
    read_long(FLASH_CTRL);

    return(0);
}

/* reset flash device */
static int fl_reset_amd(void)
{
    fl_outb(flashbase, 0, FL_RESET);
    return(0);
}

/* program flash device */
static int fl_program_amd(RFM2G_UINT32 pa, RFM2G_UINT8 *pd, int len)
{
    int stat;
    while(len--)
    {
        fl_outb(flashbase, AMD_CMDOFFS1, 0xAA);
        fl_outb(flashbase, AMD_CMDOFFS2, 0x55);
        fl_outb(flashbase, AMD_CMDOFFS1, 0xA0);
        fl_outb(flashbase, pa, *pd);
        do {
            stat = fl_isbusy_amd(pa);
        } while(stat == 1);
        pa++;
        pd++;
    }
    return(0);
}

/* is busy? return when done */
static int fl_isbusy_amd(RFM2G_UINT32 offset)
{
    int busy;
    for (;;) {
        RFM2G_UINT8 poll1, poll2;
        poll1 = fl_inb(flashbase, offset);
        poll2 = fl_inb(flashbase, offset);
        if ((poll1 ^ poll2) & 0x40) {   /* toggle */
            if (poll2 & 0x20) {     /* DQ5 = 1 */
                /* read twice */
                poll1 = fl_inb(flashbase,  offset);
                poll2 = fl_inb(flashbase,  offset);
                if ((poll1 ^ poll2) & 0x40) {   /* toggle */
                    busy = -1;
                    break;
                }
                else {
                    busy = 0;
                    break;   /* program completed */
                }
            }
            else {
                poll1 = poll2;
                continue;
            }
        }
        else {
            busy = 0;
            break;   /* program completed */
        }
    }
    return(busy);
}

/* erase sector */
static int fl_erase_sector_amd(RFM2G_UINT32 offset)
{
    fl_outb(flashbase, AMD_CMDOFFS1, 0xAA);
    fl_outb(flashbase, AMD_CMDOFFS2, 0x55);
    fl_outb(flashbase, AMD_CMDOFFS1, FL_ERASE);
    fl_outb(flashbase, AMD_CMDOFFS1, 0xAA);
    fl_outb(flashbase, AMD_CMDOFFS2, 0x55);
    fl_outb(flashbase, offset, FL_SECT);
    return(0);
}

/* wait for flash busy */
static int fl_wait_busy_amd(void)
{
    int stat;
    do {
        stat = fl_isbusy_amd(0);
    } while(stat == 1);
    return(0);
}

/* flash API routines */

/* Get flash mfg id and chip id */
static int readFlashId(void)
{
    RFM2G_UINT8 mfgid;
    RFM2G_UINT8 chipid;
    fl_reset_amd();
    fl_outb(flashbase, AMD_CMDOFFS1, 0xAA);
    fl_outb(flashbase, AMD_CMDOFFS2, 0x55);
    fl_outb(flashbase, AMD_CMDOFFS1, FL_AUTOSEL);
    mfgid = fl_inb(flashbase, 0);
    chipid = fl_inb(flashbase, 2);
    printf("     Flash Mfg %2x, Id %2x\n", mfgid, chipid);
    return(mfgid<<8| chipid);
}

/* Display memu commands */
static void flashMenu( void )
{
    printf("\n\n");
    printf("C   - Copy flash data to file, C filename\n");
    printf("R   - Read flash, prompted for address and range\n");
    printf("P   - Program flash from file, P filename\n");
    printf("V   - Verify flash against file, V filename\n");
    printf("S   - Select which board, 0 selects all boards, S 1,2,3...10\n");
    printf("H/? - Menu\n");
    printf("X   - Exit\n\n");

}

/* display plda, rfm2g, and flash base adresses */
static void flashDisplayDeviceInfo(int i)
{
    printf("\n5565 Card %d\n", i+1);
    printf("     PLDA = %p\n", pldaregs);
    printf("     RFM csr = %p\n",csr5565);
    printf("     RFM memory = %p\n",refmem5565);
    printf("     Flash Base = %p\n",flashbase);
}

/* Read flash memory from prompted address and range */
static int readFlash(void)
{
    RFM2G_UINT8 data;
    unsigned long addr; /* Type must match parameter in sscanf below, %lX, ANSI-C */
    unsigned long i = 0;
    unsigned long j = 0;
    unsigned long size = 0; /* Type must match parameter in sscanf below, %lX, ANSI-C */
    RFM2G_UINT8 *array;
    RFM2G_UINT8 *p;

    if( selected_brd == 0)
    {
        printf("Warning: Must select one board to use this function\n");
        return -1;
    }

    printf("Enter address: ");
    while ((fgets( user, sizeof(user), stdin ) == (char *) NULL ) || (strlen(user) < 2))
    {
    }
    sscanf( user, "%lX", &addr );

    printf("Enter address range: ");
    while ((fgets( user, sizeof(user), stdin ) == (char *) NULL ) || (strlen(user) < 2))
    {
    }
    sscanf( user, "%lX", &size );
    if(size == 0)
    {
        sscanf( user, "%lX", &size );
    }

    array = malloc(size);
    if(array == NULL)
    {
        printf("readFlash: ERROR: Failed to malloc buffer\n");
        return -1;
    }

    p = array;
    for(i=0;i<size;i++)
    {
        data = (unsigned char)fl_inb(flashbase, addr+i);
        *p++ = data;
    }

    p = array;
    for(i=0;i<size;i+=16)
    {
        printf("%6.6lX:", addr+i);
        for(j=0;j<16;j++)
        {
            if(i+j >= size)
                break;
            printf(" %.2X", (int) (*p++));
        }
        printf("\n");
    }

    free(array);
    return (0);
}

/* Copy flash memory to file */
static int copyFlashToFile(void)
{
    char	name[256];
    unsigned char data;
    RFM2G_UINT32 addr = 0;
    RFM2G_UINT32 i = 0;
    size_t size = _2M;
    char *array;
    char *p;
    FILE *infile;
    int prtCnt = 0;

    if( selected_brd == 0)
    {
        printf("Warning: Must select one board to use this function\n");
        return -1;
    }

    sscanf( user, "%*c %s", name);
    infile = fopen(name, "wb");
    if (infile == NULL)
    {
        printf("copyFlashToFile: ERROR: Failed to open file named %s\n", name);
        return -1;
    }

    array = malloc(size);
    if(array == NULL)
    {
        printf("copyFlashToFile: ERROR: Failed to malloc buffer\n");
        return -1;
    }

    printf("Please wait while copying flash to file");
    fflush(stdout);

    p = array;
    for(i=0;i<size;i++)
    {
        data = fl_inb(flashbase,addr+i);
        *p++ = data;

        if(prtCnt++ >= 0x8000)
        {
            printf(".");
            fflush(stdout);
            prtCnt = 0;
        }
    }

    printf("*");
    fflush(stdout);

    if(fwrite(array, 1, size, infile) < size)
    {
        printf("copyFlashToFile: ERROR: Failed to write flash data to file %s", name);
        free(array);
        fclose(infile);
        return -1;
    }

    free(array);
    fclose(infile);

    printf("...done\n");

    return 0;
}

/* Verify flash memory against file */
static int verifyFlash(void)
{
    char name[MAXLEN];
    int stat;
    RFM2G_UINT32 brd;
    RFM2G_UINT32 i;
    RFM2G_UINT8 data;
    RFM2G_UINT8 vdata;
    FILE *infile;

    sscanf( user, "%*c %s", name);
    infile = fopen(name, "rb");
    if (infile == NULL)
    {
        printf("verifyFlash can not open %s\n", name);
        return -1;
    }
    fclose(infile);

    for (brd = 0; brd < num_brds; brd++)
    {
        if ( (selected_brd != 0) && (selected_brd-1 != brd) ) {
            continue;
        }
        flashbase = xladdr[brd].flashbase;
        if (flashbase == 0 ) {
            printf("verifyFlash on address\n");
            break;
        }
        printf("VerifyFlash bdn %d at %p\n", brd+1, flashbase);

        infile = fopen(name, "rb");
        fl_reset_amd();
        i = 0;
        while ( (stat = (RFM2G_INT32)fread(&data, 1, 1, infile)) > 0){
            if (stat < 0 ) {
                printf("verifyFlash failed fread at 0x%X\n", i);
                fclose(infile);
                return  stat;
            }

            if ( (i % 0x1000) == 0) {
                printf("Verifying address 0x%X to address 0x%X\r", i, (unsigned int)(i + 0xFFF));
                fflush(stdout);
            }
            vdata = fl_inb(flashbase,i);
            if ( data != vdata) {
                printf("\nverifyFlash failed at 0x%X expected 0x%X found 0x%X\n", i, vdata & 0xff, data);
                fclose(infile);
                return (-1);
            }
            i++;
        }
        fclose(infile);
        printf("\nVerifyFlash brd %d passed %d bytes \n", brd+1, i);
    }

    return (0);
}

/* Program flash memory from file */
static int programFlash(void)
{
    char name[MAXLEN];
    int stat;
    RFM2G_UINT32 brd;
    RFM2G_UINT32 i;
    RFM2G_UINT8 data;
    RFM2G_UINT8 vdata;
    FILE *infile;

    sscanf( user, "%*c %s", name);
    infile = fopen(name, "rb");
    if (infile == NULL) {
        printf("programFlash can not open %s\n", name);
        return -1;
    }
    fclose(infile);
    if ( eraseFlash() != 0) {
        printf("programFlash failed erase\n");
        return (-1);
    }

    for (brd = 0; brd < num_brds; brd++)
    {
        if ( (selected_brd != 0) && (selected_brd-1 != brd) )
        {
            continue;
        }
        flashbase = xladdr[brd].flashbase;
        if (flashbase == 0 )
        {
            printf("programFlash on address\n");
            break;
        }
        printf("programFlash bdn %d at %p \n", brd+1, flashbase);
        infile = fopen(name, "rb");
        fl_reset_amd();

        i = 0;
        while ( (stat = (RFM2G_INT32)fread(&data, 1, 1, infile)) > 0)
        {
            if (stat < 0 ) {
                printf("programFlash failed fread at 0x%X \n", i);
                fclose(infile);
                return  stat;
            }

            if ( (i % 0x1000) == 0)
            {
                printf("Programing address 0x%X to address 0x%X\r", i, (unsigned int)(i + 0xFFF));
                fflush(stdout);
            }
            stat = fl_program_amd(i, &data, 1);
            if ( stat < 0 )
            {
                printf("\nprogramFlash failed at 0x%X writing 0x%X\n", i, data);
                fclose(infile);
                return (-1);
            }
            vdata = fl_inb(flashbase, i);
            if ( data != vdata)
            {
                printf("\nprogramFlash failed at 0x%X expected 0x%X found 0x%X\n", i, data, vdata);
                fclose(infile);
                return (-1);
            }
            i++;
            if(i>=_1M)
                break;
        }
        fclose(infile);
        printf("programFlash passed wrote %d bytes          \n", i);
    }
    return (0);
}

/* Erase all the flash memory */
static int eraseFlash(void)
{
    int stat;
    RFM2G_UINT32 brd;

    for (brd = 0; brd < num_brds; brd++)
    {
        if ( (selected_brd != 0) && (selected_brd-1 != brd) ) {
            continue;
        }
        flashbase = xladdr[brd].flashbase;
        if (flashbase == 0 ) {
            printf("eraseFlash on address\n");
            break;
        }

        printf("eraseFlash bdn %d at 0x%lX \n", brd+1, (unsigned long) flashbase);
        stat = eraseFlashBySector();

        if (stat < 0) {
            printf("eraseFlash failed stat = 0x%x \n", stat);
            return stat;
        }
    }

    for (brd = 0; brd < num_brds; brd++)
    {
        if ( (selected_brd != 0) && (selected_brd-1 != brd) ) {
            continue;
        }
        flashbase = xladdr[brd].flashbase;
        if (flashbase == 0 ) {
            printf("eraseFlash on address\n");
            break;
        }
        printf("eraseFlash Waiting bdn %d at 0x%lX \n", brd+1, (unsigned long) flashbase);
        fl_wait_busy_amd();
    }

    printf("eraseFlash passed \n");
    return (0);
}

/* Erase all the flash memory */
static int eraseFlashBySector()
{
    int stat;
    int i;
    int size = _1M;

    printf("Please wait while the sectors are being erased");
    fflush(stdout);

    fl_reset_amd();
    for(i=0;i<size;i+=_64K)
    {
        stat = fl_erase_sector_amd(i);
        if (stat < 0) {
            printf("eraseFlashSector failed stat = 0x%x \n", stat);
            return stat;
        }
        fl_wait_busy_amd();
        printf(".");
        fflush(stdout);
    }
    printf("done\n");

    return (0);
}

/* select flash device */
static void flashSelect()
{
    unsigned int brd;
    sscanf( user, "%*c %d", &brd);
    if (brd > num_brds)
    {
        printf("Invalid board selection selection stays at %d\n", selected_brd);
    }
    else
    {
        selected_brd = brd;
    }

    if( selected_brd != 0)
    {
        if(fbOffset == 0x1F0L)
        {
            flashbase = (volatile RFM2G_INT32*)((char *)xladdr[selected_brd-1].pldareg+fbOffset);
        }
        else
        {
            flashbase = (volatile RFM2G_INT32*)((char*)csr5565+fbOffset);
        }

        printf("Selected board %d at %p\n",selected_brd, flashbase);
    }
    else
    {
        printf("All boards selected\n");
    }
}

/*
 * Intialize handles, map device memory and enter a command loop.  This routine support
 * up to 10 devices.  The command loop supports P (program flash from file), C (copy flash to file),
 * R (read flash memory), E (erase flash memory), I (get mfg id and chip id), V (verify flash
 * to file), and S (select device).
 */
static int flashMain()
{
    int stop;
    int i = 0;
    RFM2G_STATUS status;
    static char device[80];
    int flash_id;
    RFM2G_UINT32 endianData = 0x30201000;
    RFM2G_UINT8 *pEndianCheck;
    volatile RFM2G_INT32 *pFA;
    RFM2GCONFIG config;

    num_brds = 0;
    for( i=0; i<MAX_BOARDS; i++ )
    {
        xladdr[i].rh = 0;
    }

    for( i = 0; i < MAX_BOARDS; i++ )
    {
        sprintf(device, "%s%d", DEVICE_PREFIX, i);
        status = RFM2gOpen( device, &xladdr[num_brds].rh);
        if( status != RFM2G_SUCCESS )
        {
            continue;
        }

        status = RFM2gGetConfig( xladdr[num_brds].rh, &config );
        if( status != RFM2G_SUCCESS )
        {
            printf( "Failed to get the board Configuration.\n" );
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
            return(-1);
        }

        if(config.PlxRevision != 0xC0)
        {
            printf( "\nCard %d is not supported by the Flash command\n", i+1);
            return -1;
        }

        /* get plx base address from BAR0 */
        status = RFM2gMapDeviceMemory(xladdr[num_brds].rh, RFM2GCFGREGMEM, (volatile void**)&xladdr[num_brds].pldareg);
        if(status != RFM2G_SUCCESS)
        {
            printf("Failed to get the plx base address, BAR0, pointer\n");
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
            return -1;
        }

        /* get reflective reg address from BAR2 */
        status = RFM2gMapDeviceMemory(xladdr[num_brds].rh, RFM2GCTRLREGMEM,  (volatile void**)&xladdr[num_brds].rfmreg);
        if(status != RFM2G_SUCCESS)
        {
            printf("Failed to get the reflective reg address, BAR2, pointer\n");
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
            return -1;
        }

        /* get reflective memory address from BAR3 */
        status = RFM2gMapDeviceMemory(xladdr[num_brds].rh, RFM2GMEM,  (volatile void**)&xladdr[num_brds].rfmmem);
        if(status != RFM2G_SUCCESS)
        {
            printf("Failed to get the reflective memory address, BAR3, pointer\n");
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
            return -1;
        }

        fbOffset = 0x1f0L; /* piorc */

        pldaregs = xladdr[num_brds].pldareg;
        csr5565 = xladdr[num_brds].rfmreg;
        refmem5565 = xladdr[num_brds].rfmmem;
        flashbase = (volatile RFM2G_INT32*)((char *)pldaregs+fbOffset);

        /* check endian mode of CPU */
        pFA = FLASH_ADDR;
        *pFA = endianData;
        pEndianCheck = (RFM2G_UINT8*)&pFA[0];
        if (pEndianCheck[0] == 0x00)
        {
            /* Little Endian */
            fPPC = RFM2G_FALSE;
            printf ("\nCPU is Little Endian\n");
        }
        else
        {
            if (pEndianCheck[0] == 0x30)
            {
                /* Big Endian */
                fPPC = RFM2G_TRUE;
                printf ("\nCPU is Big Endian\n");
            }
            else
            {
                /* Don't Know */
                printf ("\nERROR:  Could not determine the endianess of the CPU\n");
                return (-1);
            }
        }

        flashDisplayDeviceInfo(num_brds);

        flash_id = readFlashId();
        if ( (flash_id != 0x01da) && (flash_id != 0x01c4) )
        {
            fbOffset = 0x80L; /* piorc */

            flashbase = (volatile RFM2G_INT32*)((char*)csr5565+fbOffset);
            flash_id = readFlashId();
            if ( (flash_id != 0x01da) && (flash_id != 0x02c4) )
            {
                printf(" Wrong flash ID skipping board\n");
                continue;
            }
        }

        xladdr[num_brds].flashbase = flashbase;
        num_brds++;
    }

    if (num_brds == 0)
    {
        printf("no 5565s found exiting..\n");
        return -1;
    }

    if(flashbase == 0)
    {
        flashbase = xladdr[0].flashbase;
    }
    pFA = FLASH_ADDR;

    selected_brd = 0;
    printf("\nAll boards selected\n");

    flashMenu();
    stop = RFM2G_FALSE;
    do
    {
        printf("[[> ");

        if( fgets( user, sizeof(user), stdin ) == (char *) NULL )
        {
            /* beep */
            putc( 7, stdout );
        }

        switch( toupper( (int) user[0] ) )
        {
        case 'C': /* copy flash to file */
            fl_reset_amd();
            copyFlashToFile();
            break;

        case 'R': /* read flash memory */
            fl_reset_amd();
            readFlash();
            break;

        case 'P': /*program flash memory */
            programFlash();
            break;

        case 'V': /* verify flash memory */
            verifyFlash();
            break;

        case 'S': /* select flash device */
            flashSelect();
            break;

        case 'H': /* display help */
        case '?':
            flashMenu();
            break;

        case 'X': /* exit flash command loop */
            stop =  RFM2G_TRUE;
            break;

        default: /* unsupported commands */
            putc( 7, stdout );
        }

    } while( !stop );

    return 0;
}

/*
 * Closing all handles and free any allocated memory
 */
static int flashTerm()
{
	int nRet = 0;
	int i = 0;

	for( i = 0; i < MAX_BOARDS; i++ )
	{
		if(xladdr[i].rh != 0)
			RFM2gClose(&xladdr[i].rh);
	}

	return nRet;
}

/******************************************************************************
*
*  FUNCTION:   doFlashCard
*
*  PURPOSE:    Flash the card with new firmware
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist
*
******************************************************************************/

static RFM2G_INT32
doFlashCard( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
	printf(RFM2G_SUPPORT_PERSONNEL_MSG);

	printf( "\nAre you sure you want to continue? (y/n) : " );

    if( fgets( user, sizeof(user), stdin ) != (char *) NULL )
    {
	    if( toupper( (int) user[0] ) == 'Y' )
	    {
		    flashMain();
	    }
    }
	flashTerm();
	return(0);

}   /* End of doFlashCard() */

/* end flash library */

