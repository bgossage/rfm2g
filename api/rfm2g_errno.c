/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2003, 2006, 2008-10 GE Intelligent Platforms Embedded Systems, Inc.
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

    $Workfile: rfm2g_errno.c $
    $Revision: 40619 $
    $Modtime: 3/25/10 5:37p $

-------------------------------------------------------------------------------
    Description: RFM2G API Library Error Codes
-------------------------------------------------------------------------------

==============================================================================*/


#ifdef __cplusplus
extern "C"	{
#endif /* __cplusplus */

#if defined(WIN32)	
 _declspec( dllexport )	const char	*rfm2g_errlist[] =
 #else
				const char	*rfm2g_errlist[] =
 #endif
    {
    /*  0:RFM2G_SUCCESS               */  "No error",
    /*  1:RFM2G_NOT_IMPLEMENTED       */  "Capability not implemented",
    /*  2:RFM2G_DRIVER_ERROR          */  "Nonspecific error",
    /*  3:RFM2G_TIMED_OUT             */  "A wait timed out",
    /*  4:RFM2G_LOW_MEMORY            */  "A memory allocation failed",
    /*  5:RFM2G_MEM_NOT_MAPPED        */  "Memory is not mapped for this device",
    /*  6:RFM2G_OS_ERROR              */  "Function failed for other OS defined error",
    /*  7:RFM2G_EVENT_IN_USE          */  "The Event is already being waited on",
    /*  8:RFM2G_NOT_SUPPORTED         */  "Capability not supported by this particular Driver/Board",
    /*  9:RFM2G_NOT_OPEN              */  "Device not open",
    /* 10:RFM2G_NO_RFM2G_BOARD        */  "Driver did not find RFM2g device",
    /* 11:RFM2G_BAD_PARAMETER_1       */  "Function Parameter 1 in RFM2g API call is either NULL or invalid",
    /* 12:RFM2G_BAD_PARAMETER_2       */  "Function Parameter 2 in RFM2g API call is either NULL or invalid",
    /* 13:RFM2G_BAD_PARAMETER_3       */  "Function Parameter 3 in RFM2g API call is either NULL or invalid",
    /* 14:RFM2G_BAD_PARAMETER_4       */  "Function Parameter 4 in RFM2g API call is either NULL or invalid",
    /* 15:RFM2G_BAD_PARAMETER_5       */  "Function Parameter 5 in RFM2g API call is either NULL or invalid",
    /* 16:RFM2G_BAD_PARAMETER_6       */  "Function Parameter 6 in RFM2g API call is either NULL or invalid",
    /* 17:RFM2G_BAD_PARAMETER_7       */  "Function Parameter 7 in RFM2g API call is either NULL or invalid",
    /* 18:RFM2G_BAD_PARAMETER_8       */  "Function Parameter 8 in RFM2g API call is either NULL or invalid",
    /* 19:RFM2G_BAD_PARAMETER_9       */  "Function Parameter 9 in RFM2g API call is either NULL or invalid",
    /* 20:RFM2G_OUT_OF_RANGE          */  "Board offset/range exceeded",
    /* 21:RFM2G_MAP_NOT_ALLOWED       */  "Board Offset is not legal",
    /* 22:RFM2G_LINK_TEST_FAIL        */  "Link Test failed",
    /* 23:RFM2G_MEM_READ_ONLY         */  "Function attempted to change memory outside of User Memory area",
    /* 24:RFM2G_UNALIGNED_OFFSET      */  "Offset not aligned for width",
    /* 25:RFM2G_UNALIGNED_ADDRESS     */  "Address not aligned for width",
    /* 26:RFM2G_LSEEK_ERROR           */  "lseek operation failed",
    /* 27:RFM2G_READ_ERROR            */  "read operation failed",
    /* 28:RFM2G_WRITE_ERROR           */  "write operation failed",
    /* 29:RFM2G_HANDLE_NOT_NULL       */  "Cannot initialize a non-NULL handle pointer",
    /* 30:RFM2G_MODULE_NOT_LOADED     */  "Driver not loaded",
    /* 31:RFM2G_NOT_ENABLED           */  "An attempt was made to use an interrupt that has not been enabled",
    /* 32:RFM2G_ALREADY_ENABLED       */  "An attempt was made to enable an interrupt that was already enabled",
    /* 33:RFM2G_EVENT_NOT_IN_USE      */  "No process is waiting on the interrupt",
    /* 34:RFM2G_BAD_RFM2G_BOARD_ID    */  "Invalid RFM2G Board ID",
    /* 35:RFM2G_NULL_DESCRIPTOR       */  "RFM2G Handle is null",
    /* 36:RFM2G_WAIT_EVENT_CANCELLED  */  "Event Wait Cancelled",
    /* 37:RFM2G_DMA_FAILED            */  "DMA failed",
    /* 38:RFM2G_NOT_INITIALIZED       */  "User has not called RFM2gInit() yet",
    /* 39:RFM2G_UNALIGNED_LENGTH      */  "Data transfer length not 4 byte aligned",
    /* 40:RFM2G_SIGNALED              */  "Signal from OS",
    /* 41:RFM2G_NODE_ID_SELF          */  "Cannot send event to self",
    /* 41:RFM2G_MAX_ERROR_CODE        */  "Invalid error code"
    };

const int rfm2g_nerr = sizeof(rfm2g_errlist) / sizeof(char*);

#ifdef __cplusplus
};
#endif /* __cplusplus */
