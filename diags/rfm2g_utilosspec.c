/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002, 2006, 2008, GE Intelligent Platforms, Inc.
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

    $Workfile: rfm2g_utilosspec.c $
    $Revision: 40621 $
    $Modtime: 9/03/10 1:43p $

-------------------------------------------------------------------------------
    Description: Diagnostic Utility Linux Specific for the PCI RFM2g Device
                 Driver
-------------------------------------------------------------------------------

===============================================================================
*/

static RFM2G_INT32  doReturn( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doFlushQueue( DIAGCMD *cmd, RFM2G_UINT8 usage );
static RFM2G_INT32  doStat( DIAGCMD *cmd, RFM2G_UINT8 usage );

/* A list of driver specific commands. */
static CMDINFO cmdlistdrvspec[] =
{
    /* name               description                 function    parcount repeatable display */
    { "flushqueue      ", "event",                    doFlushQueue,     1, 1, 1 },
    { "help            ", "[command]",                doHelp,           1, 0, 1 },
    { "repeat          ", "[-p] count cmd [arg...]",  doRepeat,         4, 0, 1 },
    { "return          ", " ",                        doReturn,         0, 0, 1 },
    { "stat            ", " ",                        doStat,           0, 2, 1 }
};

/**************************************************************************
*
* doDrvSpecific - Change to the driver specific menu
*
* Change to the driver specific menu
* cmd: (I) Command code and command parameters
* usage: (I) If TRUE, usage message is displayed
*
* RETURNS: 0 on Success, -1 on Error
*/

static RFM2G_INT32 doDrvSpecific
    (
    DIAGCMD *cmd,
    RFM2G_UINT8 usage
    )
    {
    if( usage || cmd->parmcount != cmd->pCmdInfoList[cmd->command].parmcount )
        {
        printf( "\n%s:  Go to driver specific menu.\n\n",
                cmd->pCmdInfoList[cmd->command].name );
        printf( "Usage:  %s\n\n", cmd->pCmdInfoList[cmd->command].name );

        return(0);
        }

    printf("Welcome to the driver specific menu\n");

    /* Place driver specific initialization here */

    /* Change menu to driver specific menu */
    cmd->pCmdInfoList = cmdlistdrvspec;
    cmd->NumEntriesCmdInfoList = (sizeof(cmdlistdrvspec) / sizeof(CMDINFO));
    sprintf(cmd->pUtilPrompt,"UTILDRVSPEC%d > ", cmd->deviceNum);

    /* return to menu control */

    return(0);
    }

/**************************************************************************
*
* doReturnDrvSpecific - Driver Specific cleanup on return to main menu
*
* Driver Specific cleanup on return to main menu
* cmd: (I) Command code and command parameters, unused
*
* RETURNS: 0 on Success, -1 on Error
*/

static RFM2G_INT32 doReturnDrvSpecific
    (
    DIAGCMD *cmd,
    RFM2G_UINT8 usage
    )
    {

    /*
     * Driver specific cleanup here, if any.  This function is called by
     * doReturn.
     */

    return(0);
    }

/**************************************************************************
*
* doExitDrvSpecific - Driver Specific cleanup on exit of utility
*
* Driver Specific cleanup on exit of utility
* cmd: (I) Command code and command parameters, unused
*
* RETURNS: 0 on Success, -1 on Error
*/

static RFM2G_INT32 doExitDrvSpecific
    (
    DIAGCMD *cmd,
    RFM2G_UINT8 usage
    )
    {

    /*
     * Driver specific cleanup here, if any.  This function is called by
     * doExit.
     */

    return(0);
    }

/**************************************************************************
*
* doInitDrvSpecific - Driver specific initialization on start of utility.
*
* Driver specific initialization on start of utility.
* cmd: (I) Command code and command parameters, unused
*
* RETURNS: 0 on Success, -1 on Error
*/

static RFM2G_INT32 doInitDrvSpecific
    (
    DIAGCMD *cmd,
    RFM2G_UINT8 usage
    )
    {

    /*
     * Driver specific initialization here, if any.  This function is called by
     * rfm2gUtil.
     */

    return(0);
    }

/******************************************************************************
*
*  FUNCTION:   doStat
*
*  PURPOSE:    Display statistics and queue info for all interrupt event types,
*              and allow event counters to be zeroed
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_names
*
******************************************************************************/

static RFM2G_INT32
doStat( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    int  index;        /* Selects appropriate nomenclature          */
    char *name;        /* Local pointer to command name             */
    char *desc;        /* Local pointer to command description      */
    int  i;            /* Loop variable                             */
    RFM2GQINFO qinfo;    /* Interrupt event queue                     */
    RFM2G_STATUS status;
    char *format = "  %-8lu\t%-22s   %d\t\t%d\t\t%s\n"; /* printf()   */

    if( usage )
    {
        index = cmd->command;
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Display statistics for all interrupt event types\n\n",
            name );
        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "Get the current interrupt event statistics by entering \"%s\"\n",
            name );
        printf( "  with no parameter.\n\n" );

        return(0);
    }

    /* Display statistics for all event types */
    printf( "\nCount\t\tEvent\t\t\t#Queued\t\tQueueSize  Overflowed\n" );

    printf( "-----------------------------------------------------------------------------\n" );
    for( i=0; i<RFM2GEVENT_LAST; i++ )
    {
        qinfo.Event = i;
        status = RFM2gGetEventStats(cmd->Handle, &qinfo);
        if( status != RFM2G_SUCCESS )
        {
            printf( "Could not get statistics on interrupt %d.\n", i );
            printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
            return(-1);
        }

        printf( format,
            qinfo.EventCount,
            event_name[i].name,
            qinfo.EventsQueued,
            qinfo.QueueSize,
            (qinfo.Overflowed ? "Yes" : "No ")  );
    }

    printf("\n");

    return(0);

}   /* End of doStat() */

/******************************************************************************
*
*  FUNCTION:   doFlushQueue
*
*  PURPOSE:    Flush one or all interrupt event queues
*  PARAMETERS: cmd: (I) Command code and command parameters
*              usage: (I) If TRUE, usage message is displayed
*  RETURNS:    0 on Success, -1 on Error
*  GLOBALS:    Handle, commandlist, event_name
*
******************************************************************************/

static RFM2G_INT32
doFlushQueue( DIAGCMD *cmd, RFM2G_UINT8 usage )
{
    int  index;           /* Selects appropriate nomenclature       */
    char *name;           /* Local pointer to command name          */
    char *desc;           /* Local pointer to command description   */
    RFM2GEVENTTYPE Event; /* Which event queue to flush             */
    int i;
    RFM2G_STATUS status;


    index = cmd->command;
    if( usage || cmd->parmcount != cmd->pCmdInfoList[index].parmcount )
    {
        name = cmd->pCmdInfoList[index].name;
        desc = cmd->pCmdInfoList[index].desc;

        printf( "\n%s:  Flush one or all interrupt event queues\n\n",
            name );

        printf( "Usage:  %s  %s\n\n", name, desc );

        printf( "  where \"event\" is one of the following interrupt events "
            "[0-%d]:\n", RFM2GEVENT_LAST );
        for( i=1; i<RFM2GEVENT_LAST; i++ )
        {
            printf( "    %d   %s\n", i, event_name[i].name );
        }

        printf( "    %d  ALL EVENT QUEUES\n", RFM2GEVENT_LAST );

        return(0);
    }

    /* Get the queue type */
    Event = strtoul( cmd->parm[0], 0, 0 );
    if( (Event > RFM2GEVENT_LAST) || (Event == 0) )
    {
        printf( "Invalid interrupt queue type \"%d\".\n", Event );
        return(-1);
    }

    /* Start flushing */
    status = RFM2gFlushEventQueue(cmd->Handle, Event);
    if( status == RFM2G_SUCCESS )
    {
        if( Event == RFM2GEVENT_LAST )
        {
            printf( "All interrupt event queues were flushed.\n" );
        }
        else
        {
            printf( "The \"%s\" interrupt event queue was flushed.\n",
                event_name[Event].name );
        }
    }
    else
    {
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(status));
        return(-1);
    }

    return(0);

}   /* End of doFlushQueue() */

