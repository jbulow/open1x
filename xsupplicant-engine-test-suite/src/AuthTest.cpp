#include <windows.h>
#include <process.h>
#include <iostream>

#include "Util.h"
#include "AuthTest.h"

	
#include "xsupgui_events.h"
};

{

{

{
	
		
		
		    ("Unable to establish a connection to the event handle!\n");
		
		
	
		return false;	// runConnectionTests() should have already screamed.
	



{
	
	
	
	      ((CONFIG_LOAD_GLOBAL | CONFIG_LOAD_USER),
	       &myEnum) != REQUEST_SUCCESS)
		
		
		
	
		
		
		
		
	
	



{
	
		
		
		
		
	



{
	
		
		
		
		
	



{
	
	
	
	
	
	
	
	
		
		
		
	
		
		
		    // We got an event.  So we need to look at the type it is, and
		    // request that it be processed properly.
		    switch (evttype)
			
		
			
			    // Process a log message.
			    result =
			    xsupgui_events_generate_log_string(&ints, &logline);
			
				
				
				
			
				free(ints);
			
				free(logline);
			
		
			
			    // Process an error message.
			    result = xsupgui_events_get_error(&uievt, &logline);
			
				
				
				
				
			
		
			
			    // Process a UI event.
			    result =
			    xsupgui_events_get_ui_event(&uievt, &ints, &arg);
			
				
				
				    Util::itos(uievt) + "\n";
				
					free(ints);
				
					free(arg);
				
			
			else
				
				
				
			
		
			
			    // Process a state machine message.
			    result =
			    xsupgui_events_get_state_change(&ints, &sm,
							    &oldstate,
							    &newstate,
							    &tncconnectionid);
			
			
			
			
		
			
			    // Process a scan complete event.
			    break;
		
			
			    // Process a password request event.
			    pwdrequest = 1;
			
		
			
		
			
		
			
		
			
			    // The xsupgui library notified us that it's event connection
			    // has been broken.  This is the right way to determine when the
			    // supplicant isn't going to send us more data, since it isn't
			    // platform specific.
			    printf
			    ("Communication with the supplicant has been broken.\n");
			
			
		
			
				result);
			
			
		
		    // Always free the event doc when you are done working with it.  
		    // Otherwise, you might end up leaking lots of memory.
		    xsupgui_free_event_doc();
		



{
	
	      i != connections.end(); ++i)
		
		
			return false;
		
	



{
	
	    /*
	       int retval = 0;
	       config_connection *myconnection = NULL;
	       
	       log = 1;
	       
	       cout << "Attempting to connect to : " << connName << endl;
	       
	       retval = xsupgui_request_get_connection_config(CONFIG_LOAD_GLOBAL, const_cast<char *>(connName.c_str()), &myconnection);
	       if (retval != REQUEST_SUCCESS)
	       {
	       cout << "Couldn't get the connection configuration!\n";
	       return false;
	       }
	       
	       retval = xsupgui_request_set_connection(myconnection->device, const_cast<char *>(connName.c_str()));
	       if (retval != REQUEST_SUCCESS)
	       {
	       cout << "Failed to request a change of connection to " + connName + "!  (Error : " + Util::itos(retval) + ")\n";
	       if (retval == 310)
	       {
	       cout << "It seems the desired interface isn't in the machine?\n";
	       return true;
	       }
	       
	       return false;
	       }
	       
	       state = DISCONNECTED;
	       
	       while ((state != AUTHENTICATED) && (failure == 0) && (pwdrequest == 0))
	       {
	       processEvent();
	       }
	       
	       if (xsupgui_request_disconnect_connection(myconnection->device) != REQUEST_SUCCESS)
	       {
	       cout << "Unable to disconnect connection " + connName + "!\n";
	       return false;
	       }
	       
	       xsupgui_request_free_connection_config(&myconnection);
	       
	       log = 0;
	     */ 
	    return true;
