/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002, 2006, 2008-10 GE Intelligent Platforms Embedded Systems, Inc.
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

    $Workfile: rfm2g_sender.c $
    $Revision: 40632 $
    $Modtime: 3/19/10 11:22a $

-------------------------------------------------------------------------------
    Description: Sample program using PCI RFM2g to swap data with another RFM node
-------------------------------------------------------------------------------

===============================================================================
*/

#if (defined(WIN32))
	#include "rfm2g_windows.h"
#endif

#include <stdio.h>
#include <stdlib.h>
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
rfm2g_sender(void)

#else

int
main( int argc, char *argv[] )

#endif

{
    RFM2G_STATUS   result;                 /* Return codes from RFM2g API calls */
    RFM2G_UINT32   outbuffer[BUFFER_SIZE]; /* Data written to another node      */
    RFM2G_UINT32   inbuffer[BUFFER_SIZE];  /* Data read from another node       */
    RFM2G_INT32    i;                      /* Loop variable                     */
    RFM2G_NODE     otherNodeId;            /* Node ID of the other RFM board    */
    RFM2G_CHAR     string[40];             /* User input                        */
    RFM2GEVENTINFO EventInfo;              /* Info about received interrupts    */
    RFM2G_CHAR     device[40];             /* Name of PCI RFM2G device to use   */
    RFM2G_INT32    numDevice = 0;
    RFM2G_CHAR     selection[10];
    RFM2G_BOOL     loopAgain;
    RFM2G_BOOL     verbose;
    RFM2GHANDLE    Handle = 0;

    printf("\n  PCI RFM2g Sender\n\n");

    clearerr(stdin);

    printf("Please enter device number: ");

    while ((fgets( device, sizeof(device), stdin ) == (char *) NULL ) || (strlen(device) < 2))
    {
    }

    sscanf(device, "%d", &numDevice);

    /* if sscanf fails, then numDevice will stay 0 */
    sprintf(device, "%s%d", DEVICE_PREFIX, numDevice);

    printf("\nDo you wish for sender to loop continuously? (Y/N):");
    fgets( selection, sizeof(selection), stdin );
    if ( (selection[0] == 'Y') || (selection[0] == 'y') )
        {
        loopAgain = RFM2G_TRUE;
        }
    else
        {
        loopAgain = RFM2G_FALSE;
        }

    printf("\nDo you wish for sender to be verbose? (Y/N):");
    fgets( selection, sizeof(selection), stdin );
    if ( (selection[0] == 'Y') || (selection[0] == 'y') )
        {
        verbose = RFM2G_TRUE;
        }
    else
        {
        verbose = RFM2G_FALSE;
        }

    /* Open the Reflective Memory device */
    result = RFM2gOpen( device, &Handle );
    if( result != RFM2G_SUCCESS )
    {
        printf( "ERROR: RFM2gOpen() failed.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(result));
        return(-1);
    }

    /* Get the other Reflective Memory board's node ID */
    printf( "\nWhat is the Reflective Memory Node ID of the computer running\n"
        "the \"RFM2G_receiver\" program?  " );
    if( fgets( string, sizeof(string), stdin ) == (char *) NULL )
    {
        printf( "ERROR: Could not get user input.\n" );
        RFM2gClose( &Handle );
        return(-1);
    }
#if (defined(WIN32))
    otherNodeId = (RFM2G_INT16)strtol( string, 0, 0 );
#else
	otherNodeId = strtol( string, 0, 0 );
#endif

    /* Prompt user to start the RFM2G_receiver program */
    printf( "\nStart the \"RFM2G_receiver\" program on the other computer.\n" );
    printf( "  Press RETURN to continue ...  " );
    getchar();
    printf( "\n" );

    result = RFM2gEnableEvent( Handle, RFM2GEVENT_INTR2 );
    if( result != RFM2G_SUCCESS )
    {
        printf("Error: %s\n", RFM2gErrorMsg(result) );
        RFM2gClose( &Handle );
        return(result);
    }

    do
    {
        /* Initialize the data buffers */
        for( i=0; i<BUFFER_SIZE; i++ )
        {
            outbuffer[i] = 0xa5a50000 + i;  /* Any fake data will do */
            inbuffer[i] = 0;
        }

        if (verbose == RFM2G_TRUE)
        {
            /* Display contents of the outbuffer */
            printf( "\nThis buffer will be written to Reflective Memory starting"
                " at offset 0x%X:\n", OFFSET_1 );
            for( i=0; i<BUFFER_SIZE; i++ )
            {
                printf( "Offset: 0x%X\tData: 0x%08X\n", OFFSET_1 + i*4, outbuffer[i] );
            }
        }

        /* Write outbuffer into Reflective Memory starting at OFFSET_1 */
        result = RFM2gWrite( Handle, OFFSET_1, (void *)outbuffer, BUFFER_SIZE*4 );
        if( result == RFM2G_SUCCESS )
        {
            printf( "The data was written to Reflective Memory.  " );
        }
        else
        {
            printf( "ERROR: Could not write data to Reflective Memory.\n" );
            RFM2gClose( &Handle );
            return(-1);
        }

        /* Send an interrupt to the other Reflective Memory board */
        result = RFM2gSendEvent( Handle, otherNodeId, RFM2GEVENT_INTR1, 0);
        if( result == RFM2G_SUCCESS )
        {
            printf( "An interrupt was sent to Node %d.\n", otherNodeId );
        }
        else
        {
            printf("Error: %s\n", RFM2gErrorMsg(result) );
            RFM2gClose( &Handle );
            return(-1);
        }

        /* Now wait on an interrupt from the other Reflective Memory board */
        printf( "\nWaiting %d seconds for an interrupt from Node %d ...  ",
            TIMEOUT/1000, otherNodeId );
        fflush( stdout );
        EventInfo.Event = RFM2GEVENT_INTR2;  /* We'll wait on this interrupt */
        EventInfo.Timeout = TIMEOUT;       /* We'll wait this many milliseconds */

        result = RFM2gWaitForEvent( Handle, &EventInfo );
        if( result == RFM2G_SUCCESS )
        {
            printf( "Received the interrupt from Node %d.\n", EventInfo.NodeId );
        }
        else
        {
            printf( "\nTimed out waiting for the interrupt.\n" );
            RFM2gClose( &Handle );
            return(-1);
        }

        /* Got the interrupt, now read data from the other board from OFFSET_2 */
        result = RFM2gRead( Handle, OFFSET_2, (void *)inbuffer, BUFFER_SIZE*4);
        if( result != RFM2G_SUCCESS )
        {
            printf( "\nERROR: Could not read data from Reflective Memory.\n" );
            RFM2gClose( &Handle );
            return(-1);
        }

        if (verbose == RFM2G_TRUE)
        {
            /* Display contents of the inbuffer */
            printf( "\nThis buffer was read from Reflective Memory starting"
                " at offset 0x%X:\n", OFFSET_2 );
            for( i=0; i<BUFFER_SIZE; i++ )
            {
                printf( "Offset: 0x%X\tData: 0x%08X\n", OFFSET_2 + i*4, inbuffer[i] );
            }
        }
    }
    while (loopAgain == RFM2G_TRUE);

    printf( "\nSuccess!\n\n" );

    /* Close the Reflective Memory device */
    RFM2gClose( &Handle );

    return(0);
}
