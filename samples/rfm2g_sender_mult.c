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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#define OFFSET_1_MULT001        0x1000
#define OFFSET_2_MULT001        0x2000
#define OFFSET_1_MULT002        0x3000
#define OFFSET_2_MULT002        0x4000

#define TIMEOUT         60000

RFM2G_BOOL loopAgain = RFM2G_TRUE;
int err = 0;

static pthread_t send_mult001_thread_id;
static pthread_t send_mult002_thread_id;


struct thread_struct{
    int loop_count;
    int offset_1;
    int offset_2;
    RFM2G_CHAR device[40];
    RFM2G_NODE otherNodeId;
    RFM2G_BOOL verbose;
    RFM2GEVENTTYPE first_event;
    RFM2GEVENTTYPE second_event;
};


static void *send_test_msg(void *mult_ptr)
{
    RFM2G_STATUS   result;                 /* Return codes from RFM2g API calls */
    RFM2G_UINT32   outbuffer[BUFFER_SIZE]; /* Data written to another node      */
    RFM2G_UINT32   inbuffer[BUFFER_SIZE];  /* Data read from another node       */
    RFM2G_INT32    i;                      /* Loop variable                     */
    RFM2GEVENTINFO EventInfo;              /* Info about received interrupts    */
    RFM2GHANDLE    Handle = 0;

    struct thread_struct* new_ptr = (struct thread_struct*) mult_ptr;
    struct thread_struct mult_struct = *((struct thread_struct*) mult_ptr);

    int offset_1 = mult_struct.offset_1;
    int offset_2 = mult_struct.offset_2;
    RFM2G_CHAR device[40];
    strcpy( device, mult_struct.device );
    RFM2G_NODE otherNodeId = mult_struct.otherNodeId;
    RFM2G_BOOL verbose = mult_struct.verbose;
    RFM2GEVENTTYPE first_event = mult_struct.first_event;
    RFM2GEVENTTYPE second_event = mult_struct.second_event;

    printf("\n  PCI RFM2g Sender\n\n");

    clearerr(stdin);

    /* Open the Reflective Memory device */
    result = RFM2gOpen( device, &Handle );
    if( result != RFM2G_SUCCESS )
    {
        printf( "ERROR: RFM2gOpen() failed.\n" );
        printf( "Error: %s.\n\n",  RFM2gErrorMsg(result));
        err = 1; return NULL;
    }

    // Send interupt
    result = RFM2gEnableEvent( Handle, second_event );
    if( result != RFM2G_SUCCESS )
    {
        printf("Error: %s\n", RFM2gErrorMsg(result) );
        RFM2gClose( &Handle );
        err = 1; return NULL;
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
                " at offset 0x%X:\n", offset_1 );
            for( i=0; i<BUFFER_SIZE; i++ )
            {
                printf( "Offset: 0x%X\tData: 0x%08X\n", offset_1 + i*4, outbuffer[i] );
            }
        }

        /* Write outbuffer into Reflective Memory starting at offset_1 */
        result = RFM2gWrite( Handle, offset_1, (void *)outbuffer, BUFFER_SIZE*4 );
        if( result == RFM2G_SUCCESS )
        {
            printf( "The data was written to Reflective Memory.  " );
        }
        else
        {
            printf( "ERROR: Could not write data to Reflective Memory.\n" );
            RFM2gClose( &Handle );
            err = 1; return NULL;
        }

        /* Send an interrupt to the other Reflective Memory board */
        result = RFM2gSendEvent( Handle, otherNodeId, first_event, 0);
        if( result == RFM2G_SUCCESS )
        {
            printf( "An interrupt was sent to Node %d.\n", otherNodeId );
        }
        else
        {
            printf("Error: %s\n", RFM2gErrorMsg(result) );
            RFM2gClose( &Handle );
            err = 1; return NULL;
        }

        /* Now wait on an interrupt from the other Reflective Memory board */
        printf( "\nWaiting %d seconds for an interrupt from Node %d ...  ",
            TIMEOUT/1000, otherNodeId );
        fflush( stdout );
        EventInfo.Event = second_event;  /* We'll wait on this interrupt */
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
            err = 1; return NULL;
        }

        /* Got the interrupt, now read data from the other board from offset_2 */
        result = RFM2gRead( Handle, offset_2, (void *)inbuffer, BUFFER_SIZE*4);
        if( result != RFM2G_SUCCESS )
        {
            printf( "\nERROR: Could not read data from Reflective Memory.\n" );
            RFM2gClose( &Handle );
            err = 1; return NULL;
        }

        if (verbose == RFM2G_TRUE)
        {
            /* Display contents of the inbuffer */
            printf( "\nThis buffer was read from Reflective Memory starting"
                " at offset 0x%X:\n", offset_2 );
            for( i=0; i<BUFFER_SIZE; i++ )
            {
                printf( "Offset: 0x%X\tData: 0x%08X\n", offset_2 + i*4, inbuffer[i] );
            }
        }

        printf( "message%d\n", new_ptr->loop_count++ );
        sleep(0.5);
    }
    while ( loopAgain == RFM2G_TRUE );

    printf( "\nSuccess!\n\n" );

    /* Close the Reflective Memory device */
    RFM2gClose( &Handle );

    return NULL;
}

#if (defined(RFM2G_VXWORKS))

RFM2G_INT32
rfm2g_sender(void)

#else

int
main( int argc, char *argv[] )

#endif

{
    struct thread_struct mult001_struct;
    struct thread_struct mult002_struct;

    mult001_struct.loop_count = 0;
    mult001_struct.offset_1 = OFFSET_1_MULT001;
    mult001_struct.offset_2 = OFFSET_2_MULT001;
    strcpy( mult001_struct.device, "/dev/rfm2g0");
    mult001_struct.otherNodeId = 0x00;
    mult001_struct.verbose = RFM2G_FALSE;
    mult001_struct.first_event = RFM2GEVENT_INTR1;
    mult001_struct.second_event = RFM2GEVENT_INTR2;

    mult002_struct.loop_count = 0;
    mult002_struct.offset_1 = OFFSET_1_MULT002;
    mult002_struct.offset_2 = OFFSET_2_MULT002;
    strcpy( mult002_struct.device, "/dev/rfm2g0");
    mult002_struct.otherNodeId = 0x00;
    mult002_struct.verbose = RFM2G_FALSE;
    mult002_struct.first_event = RFM2GEVENT_INTR3;
    mult002_struct.second_event = RFM2GEVENT_INTR4;

    pthread_create( &send_mult001_thread_id, NULL, send_test_msg, (void *)&mult001_struct);
    pthread_create( &send_mult002_thread_id, NULL, send_test_msg, (void *)&mult002_struct);

    while (1)
    {
        if (err)
        {
            printf("Error: Message Not Sent\n");
            break;
        }

        if (mult001_struct.loop_count >= 10 && mult002_struct.loop_count >= 10)
        {
            loopAgain = RFM2G_FALSE;
            break;
        }
        sleep(0.5);
    }

    pthread_join(send_mult001_thread_id, NULL);
    pthread_join(send_mult002_thread_id, NULL);
}