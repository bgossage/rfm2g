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
$Workfile: rfm2g_types.h $
$Revision: 40623 $
$Modtime: 9/03/10 1:53p $
-------------------------------------------------------------------------------
    Description: Type Specifications for the RFM2G Library
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    11-OCT-2001  ML   1.0   Created.

===============================================================================
*/


#ifndef __RFM2G_TYPES_H
#define __RFM2G_TYPES_H


/*****************************************************************************/

/* Provide Type Definitions for basic portable types for
   this platform (LINUX)                        */
typedef unsigned char           RFM2G_UINT8;
typedef unsigned short int      RFM2G_UINT16;
typedef unsigned int            RFM2G_UINT32;
typedef unsigned long long int  RFM2G_UINT64;

typedef signed char             RFM2G_INT8;
typedef signed short int        RFM2G_INT16;
typedef signed int              RFM2G_INT32;
typedef signed long long int    RFM2G_INT64;

typedef unsigned char           RFM2G_BOOL;

typedef char                    RFM2G_CHAR;
typedef unsigned long           RFM2G_ADDR;

/* We also use void * and char *, but these are
   C standard types, and do not need to be declares */


/*****************************************************************************/

/* Add base constants for the Boolean */
#define RFM2G_TRUE  ((RFM2G_BOOL) 1)
#define RFM2G_FALSE ((RFM2G_BOOL) 0)


/*****************************************************************************/

/* Define the scalar types */

typedef RFM2G_UINT16      RFM2G_NODE;   /* Node ID values               */

/*****************************************************************************/

/* Define symbols to specify data width */
typedef enum rfm2gDataWidth
{
  RFM2G_BYTE      = 1,
  RFM2G_WORD      = 2,
  RFM2G_LONG      = 4,
  RFM2G_LONGLONG  = 8
} RFM2GDATAWIDTH;


/*****************************************************************************/

/* If needed, define the Maximum Path */
#define MAX_PATH 128


/*****************************************************************************/

/* Compiler support #defines */
#ifdef    __cplusplus
#define _LibDeclSpec extern "C"
#else
#define _LibDeclSpec
#endif    /* __cplusplus */

#define _CallDeclSpec

#endif  /* __RFM2G_TYPES_H */
