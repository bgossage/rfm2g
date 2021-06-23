/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002, 2006, 2008 GE Intelligent Platforms, Inc.
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

    $Workfile: rfm2g_map.c $
    $Revision: 40632 $
    $Modtime: 9/03/10 2:05p $

-------------------------------------------------------------------------------
    Description: Sample program using PCI RFM2g to map memory for use by application

===============================================================================
*/

#if (defined(WIN32))
	#include "rfm2g_windows.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

#define BUFFER_SIZE     4

#if 0
	#define OFFSET_1        0x1000
	#define OFFSET_2        0x2000
#else
	#define OFFSET_1        0x0000
	#define OFFSET_2        0x0040
#endif

#define TIMEOUT         15000

#if (defined(RFM2G_VXWORKS))

RFM2G_INT32 rfm2g_map(void)

#else

int
main( int argc, char *argv[] )

#endif
{
    RFM2G_STATUS   result;                 /* Return codes from RFM2Get API calls  */
    volatile RFM2G_UINT32 * outbuffer;     /* Pointer mapped to RFM area           */
    RFM2G_UINT32   inbuffer[BUFFER_SIZE];  /* Data read from RFM area              */
    RFM2G_INT32    i;                      /* Loop variable                        */
    RFM2G_CHAR     device[40];             /* Name of RFM2G device to use          */
    RFM2GHANDLE    Handle = NULL;
    RFM2G_INT32    numDevice = 0;

    printf("\n  PCI RFM2g Map\n\n");

    clearerr(stdin);

    printf("Please enter device number: ");

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
        printf( "ERROR: RFM2gOpen() failed.\n" );
        printf( "ERROR MSG: %s\n", RFM2gErrorMsg(result));
        return(-1);
    }

    result = RFM2gUserMemory(Handle, (volatile void **)(&outbuffer), 0, 1);
    if( result != RFM2G_SUCCESS )
    {
        printf( "ERROR: RFM2gUserMemory() failed.\n" );
        printf( "ERROR MSG: %s\n", RFM2gErrorMsg(result));
        /* Close the Reflective Memory device */
        RFM2gClose( &Handle );
        return(-1);
    }


    /* Initialize the data buffers */
    for( i=0; i<BUFFER_SIZE; i++ )
    {
        outbuffer[i] = 0xa5a50000 + i;  /* Any data will do */
        inbuffer[i] = 0;
    }

    /* Use read to collect the data */
    result = RFM2gRead(Handle, 0, &inbuffer,
                       (BUFFER_SIZE * sizeof(RFM2G_UINT32)));

    /* Compare the data buffers */
    for( i=0; i<BUFFER_SIZE; i++ )
    {
        printf("Wrote: %08X, Read: %08X\n", outbuffer[i], inbuffer[i]);
        if(outbuffer[i] != inbuffer[i])
        {
            printf( "ERROR: Test failed, index %d, Read %08X, Expected %08X\n",
                    i, inbuffer[i], outbuffer[i] );

            /* Close the Reflective Memory device */
            RFM2gClose( &Handle );
            return(-1);
        }
    }

    printf( "\nSuccess!\n\n" );

    result = RFM2gUnMapUserMemory(Handle, (volatile void **)(&outbuffer), 1);
    if( result != RFM2G_SUCCESS )
    {
        printf( "ERROR: RFM2gUnMapUserMemory() failed.\n" );
        printf( "ERROR MSG: %s\n", RFM2gErrorMsg(result));

        /* Close the Reflective Memory device */
        RFM2gClose( &Handle );
        return(-1);
    }


    /* Close the Reflective Memory device */
    RFM2gClose( &Handle );

    return(0);
}
