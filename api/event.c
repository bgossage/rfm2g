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
$Workfile: event.c $
$Revision: 40619 $
$Modtime: 9/03/10 1:35p $
-------------------------------------------------------------------------------
    Description: RFM2g API functions for sending and receiving interrupt events
-------------------------------------------------------------------------------

    Revision History:
    Date         Who  Ver   Reason For Change
    -----------  ---  ----  ---------------------------------------------------
    29-OCT-2001  ML   1.0   Created.

===============================================================================
*/

#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/param.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "rfm2g_api.h"


/*****************************************************************************/
/* Local prototypes */

#ifdef __cplusplus
extern "C" {
#endif

void *CallbackDispatcher(void *callbackInfo);

#ifdef __cplusplus
}
#endif


/*****************************************************************************/

extern STDRFM2GCALL RFM2gCheckHandle(RFM2GHANDLE rh);

/******************************************************************************
*
*  FUNCTION:   RFM2gEnableEvent
*
*  PURPOSE:    Enables the reception of a (PCI bus) RFM interrupt event
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              EventType: (I) Which interrupt event to enable
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN if the driver is not open
*              RFM2G_BAD_PARAMETER_1 if rh is invalid
*              RFM2G_BAD_PARAMETER_2 for an invalid event type
*              RFM2G_EVENT_IN_USE if this RFM event is already enabled
*              RFM2G_NOT_SUPPORTED if the board does not support this
*                interrupt type.
*              RFM2G_DRIVER_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gEnableEvent( RFM2GHANDLE rh, RFM2GEVENTTYPE EventType )
{
    RFM2GEVENTINFO  event;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    event.Event = EventType;

    /* Enable the interrupt */
    if( ioctl(rh->fd, IOCTL_RFM2G_ENABLE_EVENT, &event) )
    {
        switch( errno )
        {
            case ENOSYS:
                return( RFM2G_NOT_SUPPORTED );
                break;

            case EINVAL:
                return( RFM2G_BAD_PARAMETER_2 );
                break;

            case EACCES:
                return( RFM2G_EVENT_IN_USE );
                break;

            case EFAULT:  /* Fall thru */
            default:
                return( RFM2G_DRIVER_ERROR );
                break;
        }
    }

    return( RFM2G_SUCCESS );

}  /* End of RFM2gEnableEvent() */


/******************************************************************************
*
*  FUNCTION:   RFM2gDisableEvent
*
*  PURPOSE:    Disables the reception of a (PCI bus) RFM interrupt event
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              EventType: (I) Which interrupt event to disable
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN if the driver is not open
*              RFM2G_BAD_PARAMETER_1 if rh is invalid
*              RFM2G_BAD_PARAMETER_2 for an invalid event type
*              RFM2G_NOT_ENABLED if this RFM event was not already enabled
*              RFM2G_NOT_SUPPORTED if the board does not support this
*                interrupt type.
*              RFM2G_DRIVER_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gDisableEvent( RFM2GHANDLE rh, RFM2GEVENTTYPE EventType )
{
    RFM2GEVENTINFO  event;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    event.Event = EventType;

    /* Disable the interrupt */
    if( ioctl(rh->fd, IOCTL_RFM2G_DISABLE_EVENT, &event) )
    {
        switch( errno )
        {
            case ENOSYS:
                return( RFM2G_NOT_SUPPORTED );
                break;

            case EINVAL:
                return( RFM2G_BAD_PARAMETER_2 );
                break;

            case EACCES:
                return( RFM2G_NOT_ENABLED );
                break;

            case EFAULT:  /* Fall thru */
            default:
                return( RFM2G_DRIVER_ERROR );
                break;
        }
    }

    return( RFM2G_SUCCESS );

}  /* End of RFM2gDisableEvent() */


/******************************************************************************
*
*  FUNCTION:   RFM2gSendEvent
*
*  PURPOSE:    Send an interrupt event to one or all other nodes
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              ToNode: (I) Who will receive the interrupt event
*              EventType: (I) Which type of interrupt event to send
*              ExtendedData: (I) User-defined data (not supported on all boards)
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_BAD_PARAMETER_3
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gSendEvent( RFM2GHANDLE rh, RFM2G_NODE ToNode, RFM2GEVENTTYPE EventType,
    RFM2G_UINT32 ExtendedData )
{
    RFM2GEVENTINFO  event;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify the EventType */
    switch( EventType )
    {
        case RFM2GEVENT_RESET: break;
        case RFM2GEVENT_INTR1: break;
        case RFM2GEVENT_INTR2: break;
        case RFM2GEVENT_INTR3: break;
        case RFM2GEVENT_INTR4: break;
        default:
            return( RFM2G_BAD_PARAMETER_3 );
    }
    event.Event = EventType;

    /* Don't attempt to interrupt yourself. It can't happen */
    if( ToNode == rh->Config.NodeId )
    {
        return( RFM2G_NODE_ID_SELF );
    }

	if ( ( ToNode > RFM2G_NODE_MAX ) && ( ToNode != RFM2G_NODE_ALL ) )
	{
        return( RFM2G_BAD_PARAMETER_2 );
	}

    event.NodeId = ToNode;
    event.ExtendedInfo = ExtendedData;

    /* Send the interrupt event */
    if( ioctl(rh->fd, IOCTL_RFM2G_SEND_EVENT, &event) != 0 )
    {
        return( RFM2G_DRIVER_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gSendEvent() */


/******************************************************************************
*
*  FUNCTION:   RFM2gWaitForEvent
*
*  PURPOSE:    Wait for an interrupt event to be received, or time out
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              EventInfo: (IO) Indicates which interrupt to wait on, and
*              returns info about the received interrupt
*  RETURNS:    RFM2G_SUCCESS if interrupt is received
*              RFM2G_BAD_PARAMETER_1 for invalid handles
*              RFM2G_BAD_PARAMETER_2 for invalid RFM2GEVENTINFO structures
*              RFM2G_TIMED_OUT if the interrupt did not occur
*              RFM2G_EVENT_IN_USE if notification has already been requested
*              RFM2G_NOT_ENABLED if the interrupt was not enabled
*              RFM2G_DRIVER_ERROR if the ioctl(2) fails
*              RFM2G_WAIT_EVENT_CANCELED if teh wait was canceled
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gWaitForEvent( RFM2GHANDLE rh, RFM2GEVENTINFO *EventInfo )
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Check the EventInfo pointer */
    if( EventInfo == (RFM2GEVENTINFO *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Verify the EventInfo */
    switch( EventInfo->Event )
    {
        case RFM2GEVENT_RESET:                break;
        case RFM2GEVENT_INTR1:                break;
        case RFM2GEVENT_INTR2:                break;
        case RFM2GEVENT_INTR3:                break;
        case RFM2GEVENT_INTR4:                break;
        case RFM2GEVENT_BAD_DATA:             break;
        case RFM2GEVENT_RXFIFO_FULL:          break;
        case RFM2GEVENT_ROGUE_PKT:            break;
        case RFM2GEVENT_RXFIFO_AFULL:         break;
        case RFM2GEVENT_SYNC_LOSS:            break;
        case RFM2GEVENT_MEM_WRITE_INHIBITED:  break;
        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR: break;
        default:
            return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Now wait for the event */
    if((ioctl(rh->fd, IOCTL_RFM2G_WAIT_FOR_EVENT, EventInfo)) )
    {
        switch( errno )
        {
            case ETIMEDOUT:
                return( RFM2G_TIMED_OUT );
                break;

            case EALREADY:
                return( RFM2G_EVENT_IN_USE );
                break;

            case ENOSYS:
                return( RFM2G_NOT_ENABLED );
                break;

            case EINTR:
                return( RFM2G_SIGNALED );
                break;

            case EIDRM:
                return( RFM2G_WAIT_EVENT_CANCELED );
                break;

            default:

                return( RFM2G_DRIVER_ERROR );
                break;
        }
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gWaitForEvent() */


/******************************************************************************
*
*  FUNCTION:   RFM2gCancelWaitForEvent
*
*  PURPOSE:    Cancels any pending RFM2gWaitForEvent() calls on an event
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              EventInfo: (I) Indicates which interrupt to canceln
*  RETURNS:    RFM2G_SUCCESS if no errors occurred
*              RFM2G_BAD_PARAMETER_1 for invalid handles
*              RFM2G_BAD_PARAMETER_2 for invalid RFM2GEVENTINFO structures
*              RFM2G_NOT_ENABLED if the interrupt has not been enabled
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gCancelWaitForEvent( RFM2GHANDLE rh, RFM2GEVENTTYPE EventType )
{
    RFM2G_STATUS stat;
    RFM2GEVENTINFO EventInfo;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify the EventType */
    switch( EventType )
    {
        case RFM2GEVENT_RESET:                break;
        case RFM2GEVENT_INTR1:                break;
        case RFM2GEVENT_INTR2:                break;
        case RFM2GEVENT_INTR3:                break;
        case RFM2GEVENT_INTR4:                break;
        case RFM2GEVENT_BAD_DATA:             break;
        case RFM2GEVENT_RXFIFO_FULL:          break;
        case RFM2GEVENT_ROGUE_PKT:            break;
        case RFM2GEVENT_RXFIFO_AFULL:         break;
        case RFM2GEVENT_SYNC_LOSS:            break;
        case RFM2GEVENT_MEM_WRITE_INHIBITED:  break;
        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR: break;
        default:
            return( RFM2G_BAD_PARAMETER_2 );
    }

    EventInfo.Event = EventType;

    /* Cancel any pending RFM2gWaitForInterrupt() call */
    if( ioctl(rh->fd, IOCTL_RFM2G_CANCEL_EVENT, &EventInfo) )
    {
        switch( errno )
        {
            case ENOSYS:
                return( RFM2G_EVENT_NOT_IN_USE );
                break;

            case EFAULT:  /* Fall thru */
            default:
                return( RFM2G_OS_ERROR );
                break;
        }
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gCancelWaitForEvent() */


/******************************************************************************
*
*  FUNCTION:   RFM2gEnableEventCallback
*
*  PURPOSE:    Enables asynchronous interrupt notification for one event on
*              one board.  The user's callback function will be called upon
*              receipt of the interrupt, and runs in a child thread.
*  PARAMETERS: rh (I): Handle to an open RFM2g device
*              Event (I): Specifies the interrupt event to wait on
*              pEventFunc (I): User-written handler for the interrupt
*  RETURNS:    RFM2G_SUCCESS if no errors occurred
*              RFM2G_BAD_PARAMETER_1 if rh is a NULL pointer
*              RFM2G_NOT_OPEN if the device driver has not been opened
*              RFM2G_BAD_PARAMETER_2 for an invalid interrupt Event type
*              RFM2G_BAD_PARAMETER_3 for a NULL pEventFunc pointer
*              RFM2G_EVENT_IN_USE if notification has already been enabled
*                for this channel's interrupt
*              RFM2G_NOT_ENABLED if this interrupt has not been enabled
*              RFM2G_OS_ERROR if the pthread_create() or ioctl() fails
*  GLOBALS:    board
*
******************************************************************************/

STDRFM2GCALL
RFM2gEnableEventCallback( RFM2GHANDLE rh, RFM2GEVENTTYPE Event,
    RFM2G_EVENT_FUNCPTR pEventFunc )
{
    pthread_attr_t  attr;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Verify the Event type */
    switch( Event )
    {
        case RFM2GEVENT_RESET:                break;
        case RFM2GEVENT_INTR1:                break;
        case RFM2GEVENT_INTR2:                break;
        case RFM2GEVENT_INTR3:                break;
        case RFM2GEVENT_INTR4:                break;
        case RFM2GEVENT_BAD_DATA:             break;
        case RFM2GEVENT_RXFIFO_FULL:          break;
        case RFM2GEVENT_ROGUE_PKT:            break;
        case RFM2GEVENT_RXFIFO_AFULL:         break;
        case RFM2GEVENT_SYNC_LOSS:            break;
        case RFM2GEVENT_MEM_WRITE_INHIBITED:  break;
        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR: break;
        default:
            return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Verify pEventFunc */
    if( pEventFunc == NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

    /* Make sure Interrupt Notification has not already been enabled */
    if( rh->callbackInfo[Event].Callback != NULL )
    {
        /* Interrupt Notification has already been enabled */
        return( RFM2G_EVENT_IN_USE );
    }

    rh->callbackInfo[Event].Callback = (void*) pEventFunc;

    /* Initialize the child thread attributes */
    if( pthread_attr_init( &attr ) != 0 )
    {
        printf("ERROR: pthread_attr_init() failed\n");
        return( RFM2G_OS_ERROR );
    }

    /* The child thread will run in the detached state */
    if( pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED ) != 0 )
    {
        printf("ERROR: pthread_attr_setdetachstate() failed\n");
        return( RFM2G_OS_ERROR );
    }

    /* Child inherits the parent's scheduling policy and parameters */
    if( pthread_attr_setinheritsched( &attr, PTHREAD_INHERIT_SCHED) != 0 )
    {
        printf("ERROR: pthread_attr_setinheritsched() failed\n");
        return( RFM2G_OS_ERROR );
    }

	/* Create a semaphore */
	if (sem_init( &(rh->callbackInfo[Event].callbackdispachSemId), 0, 0) != 0)
	{
	    printf("ERROR: sem_init( &(rh->callbackInfo[Event].callbackdispachSemId) failed\n");
		return( RFM2G_OS_ERROR );
	}

    /* Create the child thread that will wait for the interrupt */
    if( pthread_create( &(rh->callbackInfo[Event].thread), &attr,
        CallbackDispatcher, (void *) &(rh->callbackInfo[Event]) )  != 0 )
    {
        printf("ERROR: pthread_create() failed\n");

		/* Destroy the semaphore, we are done with it */
		if (sem_destroy(&(rh->callbackInfo[Event].callbackdispachSemId)) != 0)
		{
			printf("ERROR: sem_destroy(rh->callbackInfo[%d].callbackdispachSemId) failed\n", Event);
		}
        return( RFM2G_OS_ERROR );
    }

	/* CallbackDispatcher thread has been started */

	/* Wait for semaphore here to be released by thread created above */
	if (sem_wait(&(rh->callbackInfo[Event].callbackdispachSemId)) != 0)
	{
        printf("ERROR: sem_wait(&(rh->callbackInfo[%d].callbackdispachSemId)) failed\n", Event);
        return( RFM2G_OS_ERROR );
	}

    /* Return the current status of the child thread */
    switch( rh->callbackInfo[Event].CallbackStat )
    {
        case RFM2G_SUCCESS:
            break;

        case RFM2G_OS_ERROR:            /* Fall thru */
        case RFM2G_EVENT_IN_USE:        /* Fall thru */
        default:
            /* Delete the callback function */
            rh->callbackInfo[Event].Callback = NULL;
            break;
    }

	/* Destroy the semaphore, we are done with it */
	if (sem_destroy(&(rh->callbackInfo[Event].callbackdispachSemId)) != 0)
	{
        printf("ERROR: sem_destroy(rh->callbackInfo[%d].callbackdispachSemId) failed\n", Event);
        return( RFM2G_OS_ERROR );
	}
    return( rh->callbackInfo[Event].CallbackStat );

}  /* end of RFM2gEnableEventCallback() */


/******************************************************************************
*
*  FUNCTION:   CallbackDispatcher
*
*  PURPOSE:    Calls the user's callback function upon receipt of an interrupt.
*              This function runs in a child thread.  There will be one
*              thread running this function for each interrupt that is enabled
*              for asynchronous notification.
*  PARAMETERS: board (I): Contains handle and and event type of the
*                interrupt source
*  RETURNS:    board
*  GLOBALS:    board
*
******************************************************************************/

void *
CallbackDispatcher( void *callbackInfo )
{
    RFM2GCALLBACKINFO pCallbackInfo = (RFM2GCALLBACKINFO) callbackInfo;
    RFM2GHANDLE     hndl  = pCallbackInfo->Handle;
    RFM2G_UINT16    event = pCallbackInfo->Event;
    int             done;
    RFM2GEVENTINFO  intr;

    intr.Event   = event;
    intr.Timeout = RFM2G_NO_WAIT;

	/* Test to see if we get RFM2G_EVENT_IN_USE */
    if( (ioctl(hndl->fd, IOCTL_RFM2G_WAIT_FOR_EVENT, &intr) == 0 ) )
	{
		/* Well one was pending as we where testing for an error condition */
        /* Got the interrupt, call the user's callback function */
        if( pCallbackInfo->Callback != NULL )
        {
            ( *(pCallbackInfo->Callback) )( hndl, &intr );
        }
	}
	else if (errno != ETIMEDOUT)
	{
        /* An error occurred, or the notification was canceled */
        switch( errno )
        {
            case ENOSYS: /* Interrupt was not previously enabled */
                done = 1;
                pCallbackInfo->CallbackStat = RFM2G_NOT_ENABLED;
                break;

            case EFAULT:  /* fall thru */
            default:
                done = 1;
                pCallbackInfo->CallbackStat = RFM2G_OS_ERROR;
                break;

            case EALREADY: /* This event is already being used */
                done = 1;
                pCallbackInfo->CallbackStat = RFM2G_EVENT_IN_USE;
                break;
        }
        /* NULLify the callback function */
        pCallbackInfo->Callback = NULL;

		sem_post(&(pCallbackInfo->callbackdispachSemId));
		return(0);
	}

    pCallbackInfo->CallbackStat = RFM2G_SUCCESS;
	sem_post(&(pCallbackInfo->callbackdispachSemId));

    intr.Timeout = RFM2G_INFINITE_TIMEOUT;

    /* Wait for interrupts, and call the user's callback function whenever
       an interrupt is received */
    for( done=0; !done; )
    {
	if ( ioctl(hndl->fd, IOCTL_RFM2G_WAIT_FOR_EVENT, &intr) == 0)
        {
            /* Got the interrupt, call the user's callback function */
            if( pCallbackInfo->Callback != NULL )
            {
                ( *(pCallbackInfo->Callback) )( hndl, &intr );
            }

            continue;
        }
        else
        {
            /* An error occurred, or the notification was canceled */
            switch( errno )
            {

               case ENOSYS: /* Interrupt was not previously enabled */
                    done = 1;
                    pCallbackInfo->CallbackStat = RFM2G_NOT_ENABLED;
                    break;

                case EFAULT:  /* fall thru */
                default:
                    done = 1;
                    pCallbackInfo->CallbackStat = RFM2G_OS_ERROR;
                    break;

                case EALREADY: /* This event is already being used */
                    done = 1;
                    pCallbackInfo->CallbackStat = RFM2G_EVENT_IN_USE;
                    break;

                case ETIMEDOUT: /* This event was canceled */
                    done = 1;
                    pCallbackInfo->CallbackStat = RFM2G_TIMED_OUT;
                    break;

                case EIDRM: /* This event was canceled */
                    done = 1;
                    pCallbackInfo->CallbackStat =  RFM2G_WAIT_EVENT_CANCELED;
                    break;
            }

            /* NULLify the callback function */
            pCallbackInfo->Callback = NULL;
        	sem_post(&(pCallbackInfo->callbackdispachSemId));
        }
    }

    /* This child thread dies at this return */
    return( 0 );

}  /* End of CallbackDispatcher() */


/******************************************************************************
*
*  FUNCTION:   RFM2gDisableEventCallback
*
*  PURPOSE:    Disables asynchronous interrupt notification for one event on
*              one board.
*  PARAMETERS: rh (I): Handle to an open RFM2g device
*              Event (I): Specifies which interrupt notification to disable
*  RETURNS:    RFM2G_SUCCESS if no errors occurred
*              RFM2G_BAD_PARAMETER_1 if rh is a NULL pointer
*              RFM2G_NOT_OPEN if the device driver has not been opened
*              RFM2G_BAD_PARAMETER_2 for an invalid interrupt Event type
*              RFM2G_NOT_ENABLED if this interrupt has not been enabled
*              RFM2G_OS_ERROR if the ioctl(2) fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gDisableEventCallback( RFM2GHANDLE rh, RFM2GEVENTTYPE Event )
{
    RFM2GEVENTINFO EventInfo;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Verify the Event type */
    switch( Event )
    {
        case RFM2GEVENT_RESET:                break;
        case RFM2GEVENT_INTR1:                break;
        case RFM2GEVENT_INTR2:                break;
        case RFM2GEVENT_INTR3:                break;
        case RFM2GEVENT_INTR4:                break;
        case RFM2GEVENT_BAD_DATA:             break;
        case RFM2GEVENT_RXFIFO_FULL:          break;
        case RFM2GEVENT_ROGUE_PKT:            break;
        case RFM2GEVENT_RXFIFO_AFULL:         break;
        case RFM2GEVENT_SYNC_LOSS:            break;
        case RFM2GEVENT_MEM_WRITE_INHIBITED:  break;
        case RFM2GEVENT_LOCAL_MEM_PARITY_ERR: break;
        default:
            return( RFM2G_BAD_PARAMETER_2 );
    }

    /* NULLify the callback pointer */
    rh->callbackInfo[Event].Callback = NULL;

    EventInfo.Event = Event;

    /* Cancel the interrupt */
    if( ioctl(rh->fd, IOCTL_RFM2G_CANCEL_EVENT, &EventInfo) != 0 )
    {
        switch( errno )
        {
            case ENOSYS:
                return( RFM2G_NOT_ENABLED );
                break;

            case EFAULT:  /* fall thru */
            default:
                return( RFM2G_OS_ERROR );
                break;
        }
    }

    return( RFM2G_SUCCESS );

}  /* End of RFM2gDisableEventCallback() */


/******************************************************************************
*
*  FUNCTION:   RFM2gGetEventStats
*
*  PURPOSE:    Get count of received interrupt events, and details about
*              the interrupt queue
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              qinfo: (O) Event statistics returned to caller
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_DRIVER_ERROR if the ioctl(2) call fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetEventStats( RFM2GHANDLE rh, RFM2GQINFO *qinfo )
{
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Validate qinfo */
    if( qinfo == (RFM2GQINFO *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Validate the event type */
    if( qinfo->Event >= RFM2GEVENT_LAST )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Now get the stats */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_EVENT_STATS, qinfo) != 0 )
    {
        return( RFM2G_DRIVER_ERROR );
    }

    return( RFM2G_SUCCESS );

}   /* End of RFM2gGetEventStats() */


/******************************************************************************
*
*  FUNCTION:   RFM2gClearEventStats
*
*  PURPOSE:    Clear the event counter for an interrupt event type
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              type: (I) The event type
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_DRIVER_ERROR if the ioctl(2) call fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gClearEventStats( RFM2GHANDLE rh, RFM2GEVENTTYPE EventType )
{
    int i;

    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Validate the event type */
    if( (EventType > RFM2GEVENT_LAST) )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Now clear the stats */
    /* Should all queues be cleared? */
    if( EventType == RFM2GEVENT_LAST )
    {
        /* Clear all event queues */
        for( i=RFM2GEVENT_RESET; i<RFM2GEVENT_LAST; i++ )
        {
            if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_EVENT_STATS, i) != 0 )
            {
                return( RFM2G_DRIVER_ERROR );
            }
        }
    }
    else
    {
        if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_EVENT_STATS, EventType) != 0 )
        {
            return( RFM2G_DRIVER_ERROR );
        }
    }
    return( RFM2G_SUCCESS );

}   /* End of RFM2gClearEventStats() */

/******************************************************************************
*
*  FUNCTION:   RFM2gClearEventCount
*
*  PURPOSE:    Clear the event counter for an interrupt event type
*  PARAMETERS: rh: (I) Handle to open RFM2g device
*              type: (I) The event type
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_DRIVER_ERROR if the ioctl(2) call fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gClearEventCount( RFM2GHANDLE rh, RFM2GEVENTTYPE EventType )
{
    int i;

    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}


    /* Validate the event type */
    if( (EventType > RFM2GEVENT_LAST) )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Now clear the count */
    if( EventType == RFM2GEVENT_LAST )
    {
        /* Clear all event queues */
        for( i=RFM2GEVENT_RESET; i<RFM2GEVENT_LAST; i++ )
        {
            if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_EVENT_COUNT, i) != 0 )
            {
                return( RFM2G_DRIVER_ERROR );
            }
        }
    }
    else
    {
        if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_EVENT_COUNT, EventType) != 0 )
        {
            return( RFM2G_DRIVER_ERROR );
        }
    }
    return( RFM2G_SUCCESS );

}   /* End of RFM2gClearEventCount() */

/******************************************************************************
*
*  FUNCTION:   RFM2gFlushEventQueue
*
*  PURPOSE:    Flush pending interrupt events from the specified queue
*  PARAMETERS: rh: (I) Handle to open RFM2G device
*              EventType: (I) Which interrupt event queue to flush
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_DRIVER_ERROR if the ioctl(2) call fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gFlushEventQueue( RFM2GHANDLE rh, RFM2GEVENTTYPE EventType )
{
    /* Linux specific function, call the Common RFM2g API function */
    return( RFM2gClearEvent(rh, EventType) );

}   /* End of RFM2gFlushEventQueue() */

/******************************************************************************
*
*  FUNCTION:   RFM2gClearEvent
*
*  PURPOSE:    Flush pending interrupt events from the RFM2g device
*  PARAMETERS: rh: (I) Handle to open RFM2G device
*              EventType: (I) Which interrupt event to clear
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_DRIVER_ERROR if the ioctl(2) call fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gClearEvent(RFM2GHANDLE rh, RFM2GEVENTTYPE EventType)
{
    int i;  /* Loop variable */

    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Validate the event type */
    if( (EventType > RFM2GEVENT_LAST) )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Should all events be cleared */
    if( EventType == RFM2GEVENT_LAST )
    {
        /* Clear all events from device */
        for( i=RFM2GEVENT_RESET; i<RFM2GEVENT_LAST; i++ )
        {
			/* Clear event on H/W */
            if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_EVENT, i) != 0 )
            {
                return( RFM2G_DRIVER_ERROR );
            }
        }
    }
    else
    {
        /* Clear event from device */
        if( ioctl(rh->fd, IOCTL_RFM2G_CLEAR_EVENT, EventType) != 0 )
        {
            return( RFM2G_DRIVER_ERROR );
        }
    }

    return( RFM2G_SUCCESS );

} /* End of RFM2gClearEvent() */

/******************************************************************************
*
*  FUNCTION:   RFM2gGetEventCount
*
*  PURPOSE:    Get the number of interrupt events
*  PARAMETERS: rh: (I) Handle to open RFM2G device
*              EventType: (I) Which interrupt event to get number of
*  RETURNS:    RFM2G_SUCCESS on success
*              RFM2G_NOT_OPEN
*              RFM2G_BAD_PARAMETER_1
*              RFM2G_BAD_PARAMETER_2
*              RFM2G_DRIVER_ERROR if the ioctl(2) call fails
*  GLOBALS:    None
*
******************************************************************************/

STDRFM2GCALL
RFM2gGetEventCount(RFM2GHANDLE rh, RFM2GEVENTTYPE EventType, RFM2G_UINT32 *Count)
{
	RFM2GQINFO qinfo;
    RFM2G_STATUS stat;

    stat = RFM2gCheckHandle(rh);

	if ( stat != RFM2G_SUCCESS ) {
		return (stat);
	}

    /* Validate the event type */
    if( (EventType >= RFM2GEVENT_LAST) )
    {
        return( RFM2G_BAD_PARAMETER_2 );
    }

    /* Check the Count pointer */
    if( Count == (RFM2G_UINT32 *) NULL )
    {
        return( RFM2G_BAD_PARAMETER_3 );
    }

	qinfo.Event = EventType;

    /* Now get the stats */
    if( ioctl(rh->fd, IOCTL_RFM2G_GET_EVENT_STATS, &qinfo) != 0 )
    {
        return( RFM2G_DRIVER_ERROR );
    }

	*Count = qinfo.EventCount;

    return( RFM2G_SUCCESS );
}


