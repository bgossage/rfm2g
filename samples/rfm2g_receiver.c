/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002, 2006, 2008-10 GE Intelligent Platforms Embedded Systems, Inc.
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

    $Workfile: rfm2g_receiver.c $
    $Revision: 40632 $
    $Modtime: 3/19/10 11:22a $

-------------------------------------------------------------------------------
    Description: Sample program using RFM2g to swap data with another RFM node
-------------------------------------------------------------------------------

===============================================================================
*/

#if (defined(WIN32))
	#include "rfm2g_windows.h"
#endif

#include <stdio.h>
#include <string.h>
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

#define BUFFER_SIZE     256
#define OFFSET_1        0x1000
#define OFFSET_2        0x2000

#define TIMEOUT         60000

#if (defined(RFM2G_VXWORKS))

RFM2G_INT32
rfm2g_receiver(void)

#else

int
main( int argc, char *argv[] )

#endif


{
    RFM2G_STATUS   result;              /* Return codes from RFM2g API calls */
    RFM2G_UINT32   buffer[BUFFER_SIZE]; /* Data shared with another node     */
    RFM2G_INT32    i;                   /* Loop variable                     */
    RFM2G_NODE     otherNodeId;         /* Node ID of the other RFM board    */
    RFM2GEVENTINFO EventInfo;           /* Info about received interrupts    */
    RFM2G_CHAR     device[40];          /* Name of VME RFM2G device to use   */
    RFM2G_INT32    numDevice = 0;
    RFM2G_CHAR     selection[10];
    RFM2G_BOOL     loopAgain;
    RFM2G_BOOL     verbose;
    RFM2GHANDLE    Handle = 0;
    RFM2G_NODE     NodeId;

    printf("\n  PCI RFM2g Receiver\n\n");

    clearerr(stdin);

    printf("Please enter device number (0-4): ");

    while ((fgets( device, sizeof(device), stdin ) == (char *) NULL ) ||
           (strlen(device) < 2))
    {
    }

    sscanf(device, "%d", &numDevice);

    /* if sscanf fails, then numDevice will stay 0 */
    sprintf(device, "%s%d", DEVICE_PREFIX, numDevice);

    /* Open the Reflective Memory device */
    result = RFM2gOpen( device, &Handle );
    if( result != RFM2G_SUCCESS )
    {
        printf( "ERROR: RFM2gOpen() failed. \n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(result));
        return(-1);
    }

    /* Tell the user the NodeId of this device */
    result = RFM2gNodeID( Handle, &NodeId );
    if( result != RFM2G_SUCCESS )
    {
        printf( "Could not get the Node ID.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(result));
        return(-1);
    }

    printf("rfm2g_receiver:  NodeID = 0x%02x\n", NodeId);

    printf("\nDo you wish for receiver to loop continuously? (Y/N):");
    fgets( selection, sizeof(selection), stdin );
    if ( (selection[0] == 'Y') || (selection[0] == 'y') )
        {
        loopAgain = RFM2G_TRUE;
        }
    else
        {
        loopAgain = RFM2G_FALSE;
        }

    printf("\nDo you wish for receiver to be verbose? (Y/N):");
    fgets( selection, sizeof(selection), stdin );
    if ( (selection[0] == 'Y') || (selection[0] == 'y') )
        {
        verbose = RFM2G_TRUE;
        }
    else
        {
        verbose = RFM2G_FALSE;
        }

    EventInfo.Event = RFM2GEVENT_INTR1;  /* We'll wait on this interrupt */
    EventInfo.Timeout = TIMEOUT;       /* We'll wait this many milliseconds */
    result = RFM2gEnableEvent( Handle, RFM2GEVENT_INTR1 );
    if( result != RFM2G_SUCCESS )
    {
        printf("Error: %s\n", RFM2gErrorMsg(result) );
        RFM2gClose( &Handle );
        return(result);
    }

    do
    {
        /* Initialize the data buffer */
        for( i=0; i<BUFFER_SIZE; i++ )
        {
            buffer[i] = 0;
        }

        /* Wait on an interrupt from the other Reflective Memory board */
        printf( "\nWaiting %d seconds to receive an interrupt from "
            "the other Node ...  ", TIMEOUT/1000 );
        fflush( stdout );

        result = RFM2gWaitForEvent( Handle, &EventInfo );
        if( result == RFM2G_SUCCESS )
        {
            printf( "\nReceived the interrupt from Node %d.\n", EventInfo.NodeId );
        }
        else
        {
            printf("Error: %s\n", RFM2gErrorMsg(result) );
            RFM2gClose( &Handle );
            return(-1);
        }

        /* Got the interrupt, see who sent it */
        otherNodeId = EventInfo.NodeId;

        /* Now read data from the other board from OFFSET_1 */
        result = RFM2gRead( Handle, OFFSET_1, (void *)buffer, BUFFER_SIZE*4 );
        if( result == RFM2G_SUCCESS )
        {
            printf( "\nData was read from Reflective Memory.\n" );
        }
        else
        {
            printf( "\nERROR: Could not read data from Reflective Memory.\n" );
            RFM2gClose( &Handle );
            return(-1);
        }

        if (verbose == RFM2G_TRUE)
        {
            /* Display contents of the inbuffer */
            printf( "\nThis buffer was read from Reflective Memory starting"
                " at offset 0x%X:\n", OFFSET_1 );
            for( i=0; i<BUFFER_SIZE; i++ )
            {
                printf( "Offset: 0x%X\tData: 0x%08X\n", OFFSET_1 + i*4, buffer[i] );
            }
        }

        /* Now write the buffer into Reflective Memory starting at OFFSET_2 */
        result = RFM2gWrite( Handle, OFFSET_2, (void *)buffer, BUFFER_SIZE*4 );
        if( result == RFM2G_SUCCESS )
        {
            printf( "\nThe data was written to Reflective Memory"
                " starting at offset 0x%X.\n", OFFSET_2 );
        }
        else
        {
            printf( "\nERROR: Could not write data to Reflective Memory.\n" );
            RFM2gClose( &Handle );
            return(-1);
        }

        /* Send an interrupt to the other Reflective Memory board */
        result = RFM2gSendEvent( Handle, otherNodeId, RFM2GEVENT_INTR2, 0);
        if( result == RFM2G_SUCCESS )
        {
            printf( "\nAn interrupt was sent to Node %d.\n", otherNodeId );
        }
        else
        {
            printf( "\nERROR: Could not send interrupt to Node %d.\n", otherNodeId );
            RFM2gClose( &Handle );
            return(-1);
        }
	}
    while (loopAgain == RFM2G_TRUE);

    printf( "\nSuccess!\n\n" );

    /* Close the Reflective Memory device */
    RFM2gClose( &Handle );

    return(0);
}
