/**
 * Windows code to establish the supplicant as a service.
 *
 * Licensed under a dual GPL/BSD license.  (See LICENSE file for more info.)
 *
 * \file win_svc.c
 *
 * \authors chris@open1x.org
 *
 */  
    
#ifdef BUILD_SERVICE
#include <windows.h>
    
#include <dbt.h>
#include <devguid.h>
    
#include "xsup_debug.h"
#include "libxsupconfig/xsupconfig.h"
#include "context.h"
    



// Some service specific error codes.
#define SERVICE_ERROR_STOP_REQUESTED       1
#define SERVICE_ERROR_DUPLICATE_INSTANCE   2
#define SERVICE_ERROR_GLOBAL_DEINIT_CALLED 3
#define SERVICE_ERROR_FAILED_TO_INIT       4
#define SERVICE_ERROR_FAILED_TO_START_IPC  5
#define SERVICE_ERROR_BASIC_INIT_FAILED    6
    
// Used to determine when a network interface is plugged in.
    DEFINE_GUID(GUID_NDIS_LAN_CLASS, 0xad498944, 0x762f, 0x11d0, 0x8d, 0xcb, 0x00,
	    0xc0, 0x4f, 0xc3, 0x35, 0x8c);

			      

/**
 * \brief Handle device insertion/removal events that are coming in on the service
 *        event handler.
 *
 * \note Because we filter by the class of events we want, we don't need to do any extra
 *       checking in here.  (Unless someone messes with our filter. ;)
 *
 * @param[in] dwEventType   The event type that triggered this call.
 * @param[in] lpEventData   The data blob that came with the event.
 **/ 
void ProcessDeviceEvent(DWORD dwEventType, LPVOID lpEventData) 
{
	
	    (PDEV_BROADCAST_DEVICEINTERFACE) lpEventData;
	
		
	
		
		    // This check is largely pointless, but leave it here just to make sure nothing weird
		    // happens.
		    if (lpdb->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			
			
			    // The device name we care about will start with something like this :
			    //    \\?\Root#MS_PSCHEDMP#0008#
			    debug_printf(DEBUG_INT,
					 "Got a device insertion event for '%ws'.\n",
					 lpdb->dbcc_name);
			
			    // If it is the one we want, then process it, otherwise ignore it.
			    if (_wcsnicmp
				((wchar_t *) lpdb->dbcc_name,
				 L"\\\\?\\Root#MS_PSCHEDMP#", 21) == 0)
				
				
					      "Processing interface insertion!\n");
				
				    (lpdb->dbcc_name, TRUE);
				
			
		
	
		
		    // This check is largely pointless, but leave it here just to make sure nothing weird
		    // happens.
		    if (lpdb->dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			
			
			    // The device name we care about will start with something like this :
			    //    \\?\Root#MS_PSCHEDMP#0008#
			    debug_printf(DEBUG_INT,
					 "Got a device removal event for '%ws'.\n",
					 lpdb->dbcc_name);
			
			    // If it is the one we want, then process it, otherwise ignore it.
			    if (_wcsnicmp
				((wchar_t *) lpdb->dbcc_name,
				 L"\\\\?\\Root#MS_PSCHEDMP#", 21) == 0)
				
				
					      "Processing interface removal!\n");
				
				    (lpdb->dbcc_name, FALSE);
				
			
		
	
		
			      dwEventType);
		
		



			      
{
	
		
	
		
		
		
		
	
		
		
		
		
	
		
		
	
		
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Console connect. (Session : %d  Size : %d)\n",
				      
					lpEventData)->dwSessionId,
				      
					lpEventData)->cbSize);
			
			       lpEventData)->dwSessionId == 0)
				
				
				
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Console disconnect.(Session : %d  Size : %d)\n",
				      
					lpEventData)->dwSessionId,
				      
					lpEventData)->cbSize);
			
			       lpEventData)->dwSessionId == 0)
				
				
				
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>>  User logged on!  (Session : %d  Size : %d)\n",
				      
					lpEventData)->dwSessionId,
				      
					lpEventData)->cbSize);
			
			       lpEventData)->dwSessionId == 0)
				
				
				
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>>  User logged off! (Session : %d  Size : %d)\n",
				      
					lpEventData)->dwSessionId,
				      
					lpEventData)->cbSize);
			
			       lpEventData)->dwSessionId == 0)
				
				
				
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Remote connect.\n");
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Remote disconnect.\n");
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Session Lock\n");
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Session Unlock\n");
			
		
			
				      ">>>>>>>>>>>>>>>>>>>>>>>>>>> Session is under remote control.\n");
			
		
			
				      "Unknown event type %d.\n", dwEventType);
			
			
		
	
		
		    // There are a whole bunch of different states that can be signaled.  The ones
		    // below represent the ones that we either care about now, or might care about
		    // in the future.
		    //
		    //  If we don't process an event, we need to be sure to return NO_ERROR, to avoid
		    // running in to a situation where the OS believes that we want to block it from
		    // suspending.
		    switch ((int)dwEventType)
			
		
			
			    // We don't care about this one right now.
			    return NO_ERROR;
		
			
			    // This signal should be generated whenever we resume (sleep or hibernate)
			    event_core_waking_up_thread_ctrl();
			
		
			
			    // This signal gets triggered right before PBT_APMRESUMEAUTOMATIC.  So, we don't
			    // want to deal with it right now.
			    return NO_ERROR;
		
			
			    // This is the first indication that we are going in to suspend mode.  Most of the
			    // time it means that we are going to complete going in to suspend mode, however, if
			    // PBT_APMQUERYSUSPENDFAILD is triggered, then we won't suspend, so we need to kick
			    // everything back up.
			    event_core_going_to_sleep_thread_ctrl();
			
		
			
			    // Suspend failed, so turn everything back on.
			    event_core_cancel_sleep_thread_ctrl();
			
		
			
			    // We get this signal when the system is starting to go in to a suspend state.
			    // By the time we get here, it is probably too late to do much of anything.
			    return NO_ERROR;
			
			    // case PBT_WhatEver and so on.
			}
		
			      ((int)dwEventType));
		
		
	
		
		
	
	    // Report current status
	    SetServiceStatus(hStatus, &ServiceStatus);
	




{
	
	
	
	
	
	
	    // Start the control dispatcher thread for our service
	    StartServiceCtrlDispatcher(ServiceTable);

{
	
		
		
		
	
	
	



{
	
	
	
	

{
	

{
	
	
	
	    SERVICE_ERROR_DUPLICATE_INSTANCE;
	

{
	
	
	
	    SERVICE_ERROR_BASIC_INIT_FAILED;
	

{
	
	
	
		
		
		    SERVICE_ERROR_FAILED_TO_START_IPC;
		
	
	else
		
		
		    SERVICE_ERROR_FAILED_TO_INIT;
		
	



{
	
	    // The next part of the startup could see some lag that can confuse
	    // Windows about the state of the service.  So, we need to report the
	    // state earlier.  Technically we are in a fully running state here, 
	    // even though no interfaces are operational and no IPC channel is 
	    // available.
	    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	

{
	
	
	
	
	    
	    
	
	
	
	
	
						 (LPHANDLER_FUNCTION_EX)
						 ControlHandler, 
	
		
		
		    // Registering Control Handler failed
		    return -1;
		
	
	    // Register for device insert/remove notifications.
	    ZeroMemory(&devBcInterface, sizeof(devBcInterface));
	
	
	
		
	
	    RegisterDeviceNotification(hStatus, &devBcInterface,
				       DEVICE_NOTIFY_SERVICE_HANDLE);
	



#endif				// BUILD_SERVICE