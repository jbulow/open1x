/**
 * Event core implementation for Windows.
 *
 * Licensed under a dual GPL/BSD license.  (See LICENSE file for more info.)
 *
 * \file event_core_win.c
 *
 * \author chris@open1x.org
 *
 * $Id: event_core_win.c,v 1.7 2008/01/30 21:07:01 galimorerpg Exp $
 * $Date: 2008/01/30 21:07:01 $
 **/

#ifndef WINDOWS
#error This event core is for Windows only!
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <winsock2.h>

#include "xsup_common.h"
#include "libxsupconfig/xsupconfig.h"
#include "libxsupconfig/xsupconfig_structs.h"
#include "context.h"
#include "wireless_sm.h"
#include "event_core_win.h"
#include "eapol.h"
#include "eap_sm.h"
#include "timer.h"
#include "xsup_debug.h"
#include "xsup_err.h"
#include "statemachine.h"
#include "frame_structs.h"
#include "platform/windows/cardif_windows_wmi.h"
#include "platform/windows/cardif_windows.h"
#include "platform/windows/wzc_ctrl.h"
#include "ipc_events.h"
#include "ipc_events_index.h"
#include "eap_sm.h"

#ifdef USE_EFENCE
#include <efence.h>
#endif

#ifdef USE_DIRECT_RADIUS
#error Direct RADIUS mode is not availble for Windows!
#endif

// Uncomment this to turn on really excessive debugging output!
//#define DEBUG_EVENT_HANDLER 1

typedef struct eventhandler_struct {
  HANDLE devHandle;
  char *name;
  context *ctx;
  int (*func_to_call)(context *, HANDLE);
  LPOVERLAPPED ovr;
  HANDLE hEvent;
  uint8_t flags;
} eventhandler;

eventhandler *events = NULL;  
uint16_t num_event_slots = 0;

int locate = 0;

time_t last_check = 0;
int terminate = 0;         // Is there a request to terminate ourselves.
int userlogon = 0;		   // (single shot) User logged on event.
int user_logged_on = FALSE;  // Is there a user currently logged on to windows?
void (*imc_notify_callback)() = NULL;
void (*imc_ui_connect_callback)() = NULL;
context *active_ctx = NULL;
int sleep_state = PWR_STATE_RUNNING;

void global_deinit();      // In xsup_driver.c

/**
 * Return the context that is currently being processed.
 **/
context *event_core_get_active_ctx()
{
	return active_ctx;
}

/**
 * We got a request to terminate ourselves.
 *
 *  *NOTE* : We won't terminate until the next time we call the event loop.  This means
 *           there is a maximum of 1 second latency following the request to terminate.
 **/
void event_core_terminate()
{
	terminate = 1;
	ipc_events_error(NULL, IPC_EVENT_ERROR_SUPPLICANT_SHUTDOWN, NULL);
}

int event_core_set_ovr(HANDLE devHandle, LPOVERLAPPED lovr)
{
	int i;

	for (i=0; i < num_event_slots; i++)
	{
#ifdef DEBUG_EVENT_HANDLER
		printf("%d == %d?\n", devHandle, events[i].devHandle);
#endif
		if (devHandle == events[i].devHandle)
		{
			events[i].ovr = lovr;
			return TRUE;
		}
	}

#ifdef DEBUG_EVENT_HANDLER
	debug_printf(DEBUG_EVENT_CORE, "Couldn't locate device handle!\n");
#endif
	return FALSE;
}

/**
 *  Get the OVERLAPPED structure that is being used with 'devHandle'.
 **/
LPOVERLAPPED event_core_get_ovr(HANDLE devHandle)
{
	int i;

	for (i=0; i < num_event_slots; i++)
	{
#ifdef DEBUG_EVENT_HANDLER
		printf("%d == %d?\n", devHandle, events[i].devHandle);
#endif
		if (devHandle == events[i].devHandle)
		{
			return events[i].ovr;
		}
	}

#ifdef DEBUG_EVENT_HANDLER
	debug_printf(DEBUG_EVENT_CORE, "Couldn't locate device handle!\n");
#endif
	return NULL;
}

/**
 * Bind an hEvent to our event structure so that the event_core() will pick it up.
 **/
int event_core_bind_hevent(HANDLE devHandle, HANDLE hEvent)
{
	int i;

	for (i=0; i < num_event_slots; i++)
	{
		if (devHandle == events[i].devHandle)
		{
#ifdef DEBUG_EVENT_HANDLER
			debug_printf(DEBUG_NORMAL, "Binding device handle %d to event handle %d.\n",
					devHandle, hEvent);
#endif
			events[i].hEvent = hEvent;
			events[i].ovr->hEvent = hEvent;
	
			return XENONE;
		}
	}

	return -1;
}

// To keep the compiler from complaining that it isn't defined.
void global_deinit();

/**
 *  This function gets called when certain console events happen.
 **/
BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
    char mesg[128];

    switch(CEvent)
    {
    case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		debug_printf(DEBUG_EVENT_CORE, "Caught a BREAK event.\n");
		event_core_terminate();
        break;
    case CTRL_CLOSE_EVENT:
        debug_printf(DEBUG_EVENT_CORE, "Caught a close event.\n");
		event_core_terminate();
        break;
    case CTRL_LOGOFF_EVENT:
		debug_printf(DEBUG_EVENT_CORE, "Caught a logoff event.\n");
        break;
    case CTRL_SHUTDOWN_EVENT:
		debug_printf(DEBUG_EVENT_CORE, "Caught a shutdown event.\n");
		event_core_terminate();
        break;

    }

    return TRUE;
}

/**
 * Set up a handler to catch CTRL-C/CTRL-BREAK and handle it correctly.  (Only useful when
 * running in console mode.)
 **/
void event_core_ctrl_c_handle()
{
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler,TRUE) == FALSE)
	{
		debug_printf(DEBUG_NORMAL, "Unable to install termination handler!\n");
	}
}

/**
 * \brief Set the defaults for a single event instance.
 *
 * @param[in] events   A pointer to the structure that we want to set the defaults on.
 **/
static void event_core_init_single_event(eventhandler *events)
{
      events->devHandle = INVALID_HANDLE_VALUE;
      events->name = NULL;
	  events->ctx = NULL;
	  events->flags = 0x00;
      events->func_to_call = NULL;
	  events->hEvent = INVALID_HANDLE_VALUE;
	  events->ovr = NULL;
}

/***********************************************************************
 *
 * Initialize the event core. -- This should make any generic calls needed
 * to set up the OS to provide us with events.
 *
 ***********************************************************************/
void event_core_init()
{
  int i;

  num_event_slots = STARTING_EVENTS;

  events = Malloc(sizeof(eventhandler) * num_event_slots);
  if (events == NULL)
  {
	  debug_printf(DEBUG_NORMAL, "Couldn't allocate memory to store event slots!\n");
	  _exit(4);
  }

  for (i=0; i < num_event_slots; i++)
    {
		event_core_init_single_event(&events[i]);
    }

  event_core_ctrl_c_handle();
  time(&last_check);   // Get our starting clock position.

  if (cardif_windows_wmi_init() != XENONE)
  {
	  debug_printf(DEBUG_NORMAL, "Couldn't establish a connection to WMI!  It is likely that "
			"things will be broken!  (We will try to continue anyway.)\n");
	  ipc_events_error(NULL, IPC_EVENT_ERROR_WMI_ATTACH_FAILED, NULL);
  }

  if (wzc_ctrl_connect() != XENONE)
  {
	  debug_printf(DEBUG_NORMAL, "Unable to initialize Windows Zero Config handle.  You should "
		  "go in to your network control panel, and clicked the check box for 'Use Windows to "
		  "configure my wireless settings.'\n");
	  ipc_events_error(NULL, IPC_EVENT_ERROR_WZC_ATTACH_FAILED, NULL);
  }
}

/***********************************************************************
 *
 * Deinit the event core. -- This should call deinit functions for anything
 * that was inited in event_core_init()
 *
 ***********************************************************************/
void event_core_deinit()
{
  int i;

  cardif_windows_wmi_deinit();

  for (i=0;i < num_event_slots; i++)
    {
      if (events[i].name)
	  {
		  event_core_deregister(events[i].devHandle);
	  }
    }

  wzc_ctrl_disconnect();

  FREE(events);
}

/**
 * \brief Populate a new event handler struct with the data needed.
 *
 * @param[in] events   A pointer to the events struct that we will populate.
 * @param[in] call_func   The function to call when this event is triggered.
 * @param[in] name   A name to give this event.
 *
 **/
static void event_core_fill_reg_struct(eventhandler *events, HANDLE devHandle, int(*call_func)(context *, HANDLE),
										context *ctx, char *name)
{
	      events->devHandle = devHandle;
	      events->name = _strdup(name);
		  events->ctx = ctx;
		  events->flags = 0x00;
	      events->func_to_call = call_func;
		  events->ovr = Malloc(sizeof(OVERLAPPED));
		  if (events->ovr == NULL)
		  {
			  debug_printf(DEBUG_NORMAL, "Couldn't allocate memory to store OVERLAPPED structure!\n");
			  ipc_events_malloc_failed(NULL);
		  }

		  if (ctx != NULL)
		  {
			  // Set up a receive.
			  cardif_setup_recv(events->ctx);
		  }
}

/*************************************************************************
 *
 *  Register a socket, and a function to call when that socket has something
 * to read.  If "hilo" is set to 0, we will register this socket to the 
 * highest priority handler available.  If it is set to 2, we will register
 * this socket to the lowest priority handler available.  If it is set to 1,
 * we will register it to whatever is available.
 *
 * Returns :
 *   -1 -- if there are no more slots available
 *    0 -- on success
 *
 *************************************************************************/
int event_core_register(HANDLE devHandle, context *ctx, 
		int(*call_func)(context *, HANDLE), 
		int hilo, char *name)
{
  int i = 0, done = FALSE;
  void *temp = NULL;

  if (!xsup_assert((call_func != NULL), "call_func != NULL", FALSE))
    return -1;

  if (!xsup_assert((name != NULL), "name != NULL", FALSE))
    return -1;

  if (hilo == 0)
    {
      while ((i < num_event_slots) && (done != TRUE))
	{
	  if (events[i].devHandle == INVALID_HANDLE_VALUE)
	    {
              debug_printf(DEBUG_EVENT_CORE, "Registered event handler '%s' in "
                           "slot %d.\n", name, i);

			  event_core_fill_reg_struct(&events[i], devHandle, call_func, ctx, name);
	      done = TRUE;
	    }
	  
	  i++;
	}
      
      if ((i >= num_event_slots) && (done == FALSE))
	{
	  debug_printf(DEBUG_NORMAL, "Not enough event handler slots "
		       "available!  (Increasing.)\n");

	  num_event_slots += EVENT_GROW;

	  temp = realloc(events, (sizeof(eventhandler) * num_event_slots));
	  if (temp == NULL)
	  {
		  num_event_slots -= EVENT_GROW;
		ipc_events_error(NULL, IPC_EVENT_ERROR_NO_IPC_SLOTS, NULL);
		return -1;
	  }

	  events = temp;

	  for (i = (num_event_slots-EVENT_GROW); i <num_event_slots; i++)
	  {
		  event_core_init_single_event(&events[i]);
	  }

	  // Try to fill the slot again.
	  i = 0;

        while ((i < num_event_slots) && (done != TRUE))
		{
		  if (events[i].devHandle == INVALID_HANDLE_VALUE)
		    {
	           debug_printf(DEBUG_EVENT_CORE, "Registered event handler '%s' in "
	                        "slot %d.\n", name, i);

			  event_core_fill_reg_struct(&events[i], devHandle, call_func, ctx, name);
		      done = TRUE;
		    }
	  
		  i++;
		}

	  }
    }
  else
    {
      i = num_event_slots - 1;
      while ((i >= 0) && (done != TRUE))
        {
          if (events[i].devHandle == INVALID_HANDLE_VALUE)
	    {
	      debug_printf(DEBUG_EVENT_CORE, "Registered event handler '%s' in "
			   "slot %d.\n", name, i);
		  event_core_fill_reg_struct(&events[i], devHandle, call_func, ctx, name);
              done = TRUE;
            }
	  
	  i--;
        }

      if ((i < 0) && (done == FALSE))
        {
          debug_printf(DEBUG_NORMAL, "Not enough event handler slots "
                        "available!  (Increasing)\n");

	  num_event_slots += EVENT_GROW;

	  temp = realloc(events, (sizeof(eventhandler) * num_event_slots));
	  if (temp == NULL)
	  {
		  num_event_slots -= EVENT_GROW;
		ipc_events_error(NULL, IPC_EVENT_ERROR_NO_IPC_SLOTS, NULL);
		return -1;
	  }

	  events = temp;

	  for (i = (num_event_slots-1); i >= (num_event_slots - EVENT_GROW); i--)
	  {
		  event_core_init_single_event(&events[i]);
	  }

	  i = num_event_slots -1;

        while ((i >= 0) && (done != TRUE))
        {
          if (events[i].devHandle == INVALID_HANDLE_VALUE)
			{
				debug_printf(DEBUG_EVENT_CORE, "Registered event handler '%s' in "
					"slot %d.\n", name, i);
				event_core_fill_reg_struct(&events[i], devHandle, call_func, ctx, name);
				done = TRUE;
            }
	  
			i--;
        }

	  return -1;
        }
    }

  return 0;
}

/***********************************************************************
 *
 * Deregister an event handler based on the socket id that we have.  When
 * an interface is deregistered, we need to destroy the context for that
 * interface.  (Assuming there is one.  In the case of an IPC interface,
 * there probably won't be.)
 *
 ***********************************************************************/
void event_core_deregister(HANDLE devHandle)
{
  int i;

  for (i=0;i < num_event_slots;i++)
    {
      if (events[i].devHandle == devHandle)
	{
	  debug_printf(DEBUG_EVENT_CORE, "Deregistering event handler '%s' in "
		       "slot %d.\n", events[i].name, i);

	  if (events[i].ctx != NULL) context_destroy(events[i].ctx);
	  events[i].ctx = NULL;

	  FREE(events[i].name);
	  events[i].devHandle = INVALID_HANDLE_VALUE;
	  events[i].hEvent = INVALID_HANDLE_VALUE;
	  events[i].func_to_call = NULL;
	  FREE(events[i].ovr);
	}
    }
}

/***********************************************************************
 *
 * Process any events that we may have received.  This includes processing
 * frames that may have come in.
 *
 ***********************************************************************/
void event_core()
{
  HANDLE *handles = NULL;
  int numhandles = 0, i;
  DWORD result;
  LPOVERLAPPED readOvr;
  struct eapol_header *eapol;
  wireless_ctx *wctx;
  ULONG bytesrx;
  time_t curtime;
  uint64_t uptime;
  long int err = 0;

  if (terminate == 1)
  {
	  debug_printf(DEBUG_NORMAL, "Got a request to terminate.\n");
	  global_deinit();
  }

  event_core_check_state();

  handles = Malloc(sizeof(HANDLE) * num_event_slots);
  if (handles == NULL)
  {
	  debug_printf(DEBUG_NORMAL, "Couldn't allocate memory to store interface handles.  Terminating.\n");
	  global_deinit();
  }

#if DEBUG_EVENT_HANDLER
  debug_printf(DEBUG_NORMAL, "Watching event handles :\n");
#endif

  cardif_windows_wmi_check_events();

  for (i = 0; i<num_event_slots; i++)
  {
	  if (events[i].ctx != NULL)
	  {
		  active_ctx = events[i].ctx;
		  cardif_check_events(events[i].ctx);
	  }

	  if ((events[i].hEvent != INVALID_HANDLE_VALUE) && (!TEST_FLAG(events[i].flags, EVENT_IGNORE_INT)))
	  {
		  // We have a handle, add it to our list of events to watch for, and check for events.
#if DEBUG_EVENT_HANDLER
		  debug_printf(DEBUG_NORMAL, "hEvent = %d  hdl = %d\n", events[i].hEvent, events[i].devHandle);
#endif
		  handles[numhandles] = events[i].hEvent;
		  numhandles++;
	  }
  }

  if (numhandles <= 0)
  {
	  debug_printf(DEBUG_NORMAL, "No handles available to watch.  Cannot continue!\n");
	  _exit(3);
  }

  result = WaitForMultipleObjectsEx(numhandles, handles, FALSE, 1000, 1);

  cardif_windows_wmi_check_events();

	  for (i=(num_event_slots-1); i>=0; i--)
	  {
		  if ((events[i].devHandle != INVALID_HANDLE_VALUE) && (HasOverlappedIoCompleted(events[i].ovr) == TRUE) &&
			  (!TEST_FLAG(events[i].flags, EVENT_IGNORE_INT)))
		  {
			  readOvr = events[i].ovr;

			  if (GetOverlappedResult(events[i].devHandle, readOvr, &bytesrx, FALSE) != 0)
			  {
				  debug_printf(DEBUG_EVENT_CORE, "Got data on handle %d (Size %d).\n", events[i].devHandle,
					  bytesrx);
				  if (events[i].ctx != NULL)
  				  {
					if (events[i].ctx->intType == ETH_802_11_INT)
					{
						if (events[i].ctx->intTypeData != NULL)
						{
							if (((wireless_ctx *)(events[i].ctx->intTypeData))->state != ASSOCIATED)
							{
								cardif_windows_wmi_check_events();
								wireless_sm_do_state(events[i].ctx);
							}
						}
					}

					if (events[i].ctx->recvframe != NULL)
					{
						if (events[i].ctx->recv_size != 0)
						{
							// If we have an unprocessed frame in the buffer, clear it out.
							events[i].ctx->recv_size = 0;
						}

						FREE(events[i].ctx->recvframe); 
						events[i].ctx->recvframe = NULL;
						events[i].ctx->eap_state->eapReqData = NULL;
					}

					events[i].ctx->recv_size = bytesrx;
					events[i].ctx->recvframe = ((struct win_sock_data *)(events[i].ctx->sockData))->frame;

					((struct win_sock_data *)(events[i].ctx->sockData))->frame = NULL;
					((struct win_sock_data *)(events[i].ctx->sockData))->size = 0;
					
					// Set up to receive the next frame.
					cardif_setup_recv(events[i].ctx);
				} 

				// Be sure to set active_ctx before calling the function below.  It is
				// used to allow upper layers to determine details of the lower layers.
				active_ctx = events[i].ctx;
				events[i].func_to_call(events[i].ctx, events[i].devHandle);
			  }
			  else
			  {
				  err = GetLastError();
				  if (GetLastError() == ERROR_BROKEN_PIPE)
				  {
					  active_ctx = events[i].ctx;
					  events[i].func_to_call(events[i].ctx, events[i].devHandle);
				  }

				  if ((err == ERROR_OPERATION_ABORTED) && (events[i].ctx != NULL))
				  {
					  active_ctx = events[i].ctx;

					  // We have a network interface that has disappeared, and we
					  // don't know what to do, so dump it's context and log a message.
					  debug_printf(DEBUG_VERBOSE, "The interface '%s' went in to a strange state.  We will terminate it's context.  If you want to "
						  "use this interface, you need to repair it, or unplug it and plug it back in.\n", events[i].ctx->desc);

		  			  ipc_events_ui(events[i].ctx, IPC_EVENT_INTERFACE_REMOVED, events[i].ctx->desc);

					  events[i].ctx->flags |= INT_GONE;
					  event_core_deregister(events[i].devHandle); 
				  }
			  }
		  }
	  }  

  // Clean up our handles array.
  FREE(handles);

  if (userlogon == TRUE)
  {
	  // Inform any IMCs that may need to know.
	  debug_printf(DEBUG_EVENT_CORE, ">>* Processed user logged on flag.\n");
	  if (imc_notify_callback != NULL) 
	  {
		  imc_notify_callback();
	  }
	  else
	  {
		  debug_printf(DEBUG_EVENT_CORE, ">>* Notify callback is NULL!\n");
	  }
	  userlogon = FALSE;   // Don't retrigger.
  }

  time(&curtime); 
  if (last_check > curtime)
  {
	  debug_printf(DEBUG_EVENT_CORE, "Let's do the time warp again.  Your clock has gone backward!\n");
	  last_check = curtime;
  }

  // If we got nailed with a bunch of events, make sure we still tick the clock.  This
  // method won't have exactly one second precision, but it should be close enough.
  if (curtime > last_check) 
  {
	  result = WAIT_TIMEOUT;
	  last_check = curtime;
  }

  if (result == WAIT_TIMEOUT)
  {
	  // See if logs need to be rolled.
	  xsup_debug_check_log_roll();

	  for (i=0; i<num_event_slots; i++)
	  {
		  if ((events[i].ctx != NULL) && (!TEST_FLAG(events[i].flags, EVENT_IGNORE_INT)))
		  {
			active_ctx = events[i].ctx;
			events[i].ctx->statemachine->tick = TRUE;
			events[i].ctx->tick = TRUE;
 
			// Tick clock.
			timer_tick(events[i].ctx);
		  }
	  }
  }

  for (i=0; i<num_event_slots; i++)
  {
	if (events[i].ctx != NULL)
	{
		active_ctx = events[i].ctx;
		if (events[i].ctx->intType != ETH_802_11_INT) 
		{
			if (!TEST_FLAG(events[i].flags, EVENT_IGNORE_INT))
			{
				if (events[i].ctx->conn != NULL)
				{
					statemachine_run(events[i].ctx);
				}
				else
				{
					if (events[i].ctx->eap_state != NULL)
					{
						events[i].ctx->eap_state->eap_sm_state = DISCONNECTED;
					}
				}
			}
		}
		else
		{
			if (!TEST_FLAG(events[i].flags, EVENT_IGNORE_INT))
			{
				wireless_sm_do_state(events[i].ctx);
			}
		}
	}
  }
}

/**
 *  \brief Find the context that matches the interface name string, and return it.  
 *
 * @param[in] matchstr   The OS specific name of the interface we wish to find the
 *                       context for.
 *
 * \retval ptr  Pointer to a context structure for the interface requested.
 * \retval NULL Couldn't locate the desired interface.
 **/
context *event_core_locate(char *matchstr)
{
  int i;

  if (!xsup_assert((matchstr != NULL), "matchstr != NULL", FALSE))
    return NULL;

  for (i = 0; i < num_event_slots; i++)
  {
	  if ((events[i].ctx != NULL) && (strcmp(events[i].ctx->intName, matchstr) == 0))
		  return events[i].ctx;
  }

  // Otherwise, we ran out of options.
  return NULL;
}

/**
 *  \brief Find the context that matches the interface description string, and return it.  
 *
 * @param[in] matchstr   The interface description we are looking for.
 *
 * \retval ptr  Pointer to a context structure for the interface requested.
 * \retval NULL Couldn't locate the desired interface.
 **/
context *event_core_locate_by_desc(char *matchstr)
{
  int i;

  if (!xsup_assert((matchstr != NULL), "matchstr != NULL", FALSE))
    return NULL;

  for (i = 0; i < num_event_slots; i++)
  {
	  if ((events[i].ctx != NULL) && (strcmp(events[i].ctx->desc, matchstr) == 0))
		  return events[i].ctx;
  }

  // Otherwise, we ran out of options.
  return NULL;
}

/**
 *  \brief Find the context that matches the interface description string, and return it.  
 *
 * @param[in] matchstr   The interface description we are looking for.
 *
 * \retval ptr  Pointer to a context structure for the interface requested.
 * \retval NULL Couldn't locate the desired interface.
 **/
context *event_core_locate_by_desc_strstr(char *matchstr)
{
  int i;

  if (!xsup_assert((matchstr != NULL), "matchstr != NULL", FALSE))
    return NULL;

  for (i = 0; i < num_event_slots; i++)
  {
	  if ((events[i].ctx != NULL) && (strstr(events[i].ctx->desc, matchstr) != NULL))
		  return events[i].ctx;
  }

  // Otherwise, we ran out of options.
  return NULL;
}

/**
 *  \brief Find the context that matches the interface caption string, and return it.  
 *
 * @param[in] matchstr   The WMI caption name to search for.
 *
 * \retval ptr  Pointer to a context structure for the interface requested.
 * \retval NULL Couldn't locate the desired interface.
 **/
context *event_core_locate_by_caption(wchar_t *matchstr, int exact)
{
  int i;
  struct win_sock_data *sockData = NULL;
  wchar_t *str = NULL;
  context *ctx = NULL;

  if (!xsup_assert((matchstr != NULL), "matchstr != NULL", FALSE))
    return NULL;

  for (i = 0; i < num_event_slots; i++)
  {
	  if (events[i].ctx != NULL)
	  {
		  sockData = events[i].ctx->sockData;
		  if (sockData != NULL)
		  {
			  // Depending on the type of event it is, Windows may send the full name with the "- Packet Scheduler Miniport"
			  // on the end, or just send the bare name.  In 99% of cases, you want to use an exact match to avoid any
			  // weird stuff..  But, for things like the connect and disconnect events, you need to use a substring
			  // match.   Always try to use an exact match first!!  (It will save you pain in the long run!)
			  if (exact == TRUE)
			  {
				  if (wcscmp(sockData->caption, matchstr) == 0)
					  return events[i].ctx;
			  }
			  else
			  {
				  str = wcsstr(sockData->caption, matchstr);
				  if (str != NULL)
				  {
					  if (wcscmp(str, matchstr) == 0)
						  return events[i].ctx;
				  }
			  }
		  }
	  }
  }

  // Otherwise, we ran out of options.
  return NULL;
}


void event_core_reset_locator()
{
	locate = -1;
}

context *event_core_get_next_context()
{
	int desired_ctx = 0;

	desired_ctx = ++locate;

	if (desired_ctx >= num_event_slots) return NULL;

	while ((desired_ctx < num_event_slots) && (events[desired_ctx].ctx == NULL))
	{
		desired_ctx++;
	}

	locate = desired_ctx;

	if (desired_ctx >= num_event_slots) return NULL;

	return events[desired_ctx].ctx;
}

/**
 * \brief Indicate that we need to take action to enter sleep state.
 *        (Sleep may also indicate hibernate, or other low power
 *        condition where things may happen that we could miss.)
 *
 * \warning This function will *ALWAYS* be called from the service
 *          control thread.  As such, nothing should be done that 
 *          could cause issues with the main thread!
 **/
void event_core_going_to_sleep_thread_ctrl()
{
	sleep_state = PWR_STATE_SLEEPING;
}

/**
 * \brief Another process indicated that going to sleep right now is
 *        unacceptible.   So, we need to turn everything back on again.
 *
 * \warning This function will *ALWAYS* be called from the service
 *          control thread.  As such, nothing should be done that 
 *          could cause issues with the main thread!
 **/
void event_core_cancel_sleep_thread_ctrl()
{
	sleep_state = PWR_STATE_SLEEP_CANCEL;
}

/**
 * \brief We have come out of sleep mode.  We need to take steps to
 *        make sure our configuration is still sane.
 *
 * \warning This function will *ALWAYS* be called from the service
 *          control thread.  As such, nothing should be done that 
 *          could cause issues with the main thread!
 **/
void event_core_waking_up_thread_ctrl()
{
	sleep_state = PWR_STATE_WAKING_UP;
}

/**
 * \brief This function will be called when an indication that we are going 
 *        to sleep is made.  There are certain steps that need to be taken :
 *
 *           1) All pending IO reads need to be canceled.  (This will prevent
 *              extra frames from being read, and screwing us up when we wake up.)
 *
 *           2) A "going to sleep" event needs to be sent to all connected UIs.
 *
 *           3) All contexts need to be flagged so that we don't process them.  (If
 *              we processed them we would undo the steps taken in #1. ;)
 *
 * \note NOTHING should be done that is totally perminent.  There is always a chance that
 *       the sleep request will be canceled.
 **/
void event_core_going_to_sleep()
{
	int i = 0;

	debug_printf(DEBUG_NORMAL, "XSupplicant got a request for the machine to enter a sleep state.\n");

	for (i= 0; i< num_event_slots; i++)
	{
		// Loop through each event slot, cancel the IO, flag the context.
		if (events[i].ctx != NULL)
		{
			cardif_cancel_io(events[i].ctx);
			events[i].flags |= EVENT_IGNORE_INT;
		}
	}

	// Then send a "going to sleep" message.
	ipc_events_ui(NULL, IPC_EVENT_UI_GOING_TO_SLEEP, NULL);

	sleep_state = PWR_STATE_HELD;
}

/**
 * \brief Cancel the sleep request.  We need to turn our frame listeners back on, and continue
 *        as if nothing happened.  There is a small chance that a reauth was taking place
 *        during the time the sleep request was received.  In this case, we will probably fail the
 *        authentication. :-/
 **/
void event_core_cancel_sleep()
{
	int i = 0;

	debug_printf(DEBUG_NORMAL, "Another process canceled the sleep request.\n");

	for (i= 0; i< num_event_slots; i++)
	{
		// Loop through each event slot, cancel the IO, flag the context.
		if (events[i].ctx != NULL)
		{
			cardif_restart_io(events[i].ctx);
			events[i].flags &= (~EVENT_IGNORE_INT);
		}
	}

	// Then send a "sleep cancelled" message.
	ipc_events_ui(NULL, IPC_EVENT_UI_SLEEP_CANCELLED, NULL);

	sleep_state = PWR_STATE_RUNNING;
}

/**
 * \brief We got a wake up event.   We need to sanity check all of our interfaces, reset state
 *        machines, and send notifications that we have come back to life.
 **/
void event_core_waking_up()
{
	int i = 0;
	wireless_ctx *wctx = NULL;

	debug_printf(DEBUG_NORMAL, "XSupplicant is coming out of a sleep state.\n");

	for (i= 0; i< num_event_slots; i++)
	{
		// Loop through each event slot, cancel the IO, flag the context.
		if (events[i].ctx != NULL)
		{
			cardif_restart_io(events[i].ctx);
			events[i].flags &= (~EVENT_IGNORE_INT);

			if (events[i].ctx->intType == ETH_802_11_INT)
			{
				wctx = events[i].ctx->intTypeData;
				memset(wctx->cur_bssid, 0x00, 6);
			}

			// Reset our auth count so that we do a new IP release/renew.  Just in case Windows beats us to the punch.
			events[i].ctx->auths = 0;
		}
	}

	// Then send a "waking up" message.
	ipc_events_ui(NULL, IPC_EVENT_UI_WAKING_UP, NULL);

	sleep_state = PWR_STATE_RUNNING;
}

/**
 * \brief Check the state of sleep/wake-up events.  Make any changes that may be
 *        needed for these events.
 **/
void event_core_check_state()
{
	switch (sleep_state)
	{
	default:
	case PWR_STATE_RUNNING:
	case PWR_STATE_HELD:
		break;

	case PWR_STATE_SLEEPING:
		event_core_going_to_sleep();
		break;

	case PWR_STATE_SLEEP_CANCEL:
		event_core_cancel_sleep();
		break;

	case PWR_STATE_WAKING_UP:
		event_core_waking_up();
		break;
	}
}

/**
 * \brief This is called when a user logs on.  It should be expected that this call
 *        may be called from a seperate thread.  So, it should do the minimum amount 
 *        of work possible.
 **/
void event_core_user_logged_on()
{
	if (user_logged_on == FALSE)
	{
		debug_printf(DEBUG_EVENT_CORE, ">>* Set user logged on flag!\n");
		userlogon = TRUE;
	}

	user_logged_on = TRUE;
}

/**
 * \brief This function gets called when a user logs off.  It should do any user specific
 *        cleanup that the engine needs.
 **/
void event_core_user_logged_off()
{
	user_logged_on = FALSE;
}

/**
 * \brief Register a callback to notify an IMC when a user logs on to the Windows console.
 *
 * @param[in] callback   The callback that we want to call when a user logs in.
 *
 * \warning Currently we only allow a single IMC to request this callback!  In the future
 *          we may need to extend this!
 *
 * \todo Extend this to allow more than one callback to register.
 *
 * \retval 0 on success
 * \retval 1 if another IMC has already registered.
 **/
uint32_t event_core_register_imc_logon_callback(void *callback)
{
	if (imc_notify_callback != NULL) return 1;

	imc_notify_callback = callback;

	return 0;
}

/**
 * \brief Register a callback to notify an IMC when a user logs on to the Windows console.
 *
 * @param[in] callback   The callback that we want to call when a user logs in.
 *
 * \warning Currently we only allow a single IMC to request this callback!  In the future
 *          we may need to extend this!
 *
 * \todo Extend this to allow more than one callback to register.
 *
 * \retval 0 on success
 * \retval 1 if another IMC has already registered.
 **/
uint32_t event_core_register_disconnect_callback(void *callback)
{
	if (imc_disconnect_callback != NULL) return 1;

	imc_disconnect_callback = callback;

	return 0;
}

/**
 * \brief Register a callback to notify an IMC when the UI connects to the supplicant.
 *
 * @param[in] callback   The callback that we want to call when the UI connects to the supplicant
 *
 * \warning Currently we only allow a single IMC to request this callback!  In the future
 *          we may need to extend this!
 *
 * \todo Extend this to allow more than one callback to register.
 *
 * \retval 0 on success
 * \retval 1 if another IMC has already registered.
 **/
uint32_t event_core_register_ui_connect_callback(void *callback)
{
	if (imc_ui_connect_callback != NULL) return 1;

	imc_ui_connect_callback = callback;

	return 0;
}

/**
 * \brief Drop all active connections.  (But leave them in a state that they should
 *        be able to reconnect.
 **/
void event_core_drop_active_conns()
{
	int i = 0;

	for (i= 0; i< num_event_slots; i++)
	{
		// Loop through each event slot, and disconnect it.
		if ((events[i].ctx != NULL) && (events[i].ctx->conn != NULL))
		{
			if (statemachine_change_state(events[i].ctx, LOGOFF) == 0)
			{
				events[i].ctx->auths = 0;                   // So that we renew DHCP on the next authentication.

				txLogoff(events[i].ctx);
			}

			if (events[i].ctx->intType = ETH_802_11_INT)
			{
				// Send a disassociate.
				cardif_disassociate(events[i].ctx, 0);
			}
		}
	}

}

