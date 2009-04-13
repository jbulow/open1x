/**
 * Routines for checking the "completeness" of a piece of the configuration.
 *
 * Licensed under the dual GPL/BSD license.  (See LICENSE file for more info.)
 *
 * \file ipc_callout_helper.c
 *
 * \author chris@open1x.org
 *
 **/  
    
#ifdef WINDOWS
#include "stdintwin.h"
#else				/* 
#include <stdint.h>
#endif				/* 
    
#include <string.h>
#include <stdlib.h>
#include <libxml/parser.h>
    
#include "libxsupconfig/xsupconfig_structs.h"
#include "xsup_common.h"
#include "libxsupconfig/xsupconfig.h"
#include "libxsupconfig/xsupconfig_vars.h"
#include "context.h"
#include "xsup_debug.h"
#include "xsup_err.h"
#include "ipc_callout.h"
#include "logon_creds.h"
#include "eap_sm.h"
#include "platform/platform.h"
#include "ipc_callout_helper.h"
    
/**
 * \brief Build a list of available connections for a certain type.
 *
 * @param[in] config_type   The list of connections in memory to return.
 * @param[in] baseNode   The base node to build on.
 *
 * \retval XENONE on success
 **/ 
int ipc_callout_helper_build_connection_list(uint8_t config_type,
					     xmlNodePtr baseNode) 
{
	
	
	
	
	
	
	
	
	
		
		
		    // If we need to be admin, and aren't, skip this one.
		    if ((are_admin == FALSE)
			&& (ipc_callout_helper_connection_needs_admin(cur) ==
			    TRUE))
			
			
			
			
		
		    xmlNewChild(baseNode, NULL, (xmlChar *) "Connection", NULL);
		
			
			
			
		
		
		     (t, NULL, (xmlChar *) "Connection_Name",
		      (xmlChar *) temp) == NULL)
			
			
			
			
		
		
		
		     (t, NULL, (xmlChar *) "Config_Type",
		      (xmlChar *) res) == NULL)
			
			
			
		
		
		     (t, NULL, (xmlChar *) "SSID_Name",
		      (xmlChar *) temp) == NULL)
			
			
			
			
		
		
		
		     (t, NULL, (xmlChar *) "Priority", (xmlChar *) res) == NULL)
			
			
			
		
			
			
			
		
		else if (cur->association.association_type != ASSOC_OPEN)
			
			
			
		
		else if ((cur->association.association_type == ASSOC_OPEN)
			 && (cur->association.txkey != 0))
			
			
			
		
		else if (cur->association.association_type == ASSOC_OPEN)	// Which it should!
		{
			
				
				
				
			
			else
				
				
				
		
		
		else		// Shouldn't ever get here!
		{
			
		
		
		
		     (t, NULL, (xmlChar *) "Encryption",
		      (xmlChar *) res) == NULL)
			
			
			
		
			
			
			    // We don't explicitly have an auth type, so try to figure it out.
			    if (cur->profile != NULL)
				
				
				    // We have a profile, so we should be doing 802.1X.
				    sprintf((char *)&res, "%d", AUTH_EAP);
				
			
			else if ((cur->association.psk != NULL)
				 || (cur->association.psk_hex != NULL))
				
				
				    // We have a PSK, so we are probably doing WPA-PSK.
				    sprintf((char *)&res, "%d", AUTH_PSK);
				
			
			else
				
				
				    // We don't know.  So return 0.
				    sprintf((char *)&res, "0");
			
		
		else
			
			
				 cur->association.auth_type);
			
		
		      (t, NULL,
		       (xmlChar *) "Authentication", (xmlChar *) res) == NULL)
			
			
			
		
			  cur->association.association_type);
		
		      (t, NULL, (xmlChar *) "Association_Type",
		       (xmlChar *) res) == NULL)
			
			
			
		
		
	



/**
 * \brief Build a list of profiles that are available based on the config_type value passed in.
 *
 * @param[in] config_type   One of the configuration types that we want to get the list from.
 * @param[in/out] baseNode   The XML node that we want to build the list on.
 *
 * \retval XENONE on success, anything is a valid input to an ipc_callout_error() call.
 **/ 
int ipc_callout_helper_build_profile_list(uint8_t config_type,
					  xmlNodePtr baseNode) 
{
	
	
	
	
	
	
		
		
		
			
			
			
		
		
		     (t, NULL, (xmlChar *) "Profile_Name",
		      (xmlChar *) temp) == NULL)
			
			
			
			
		
		
		
		     (t, NULL, (xmlChar *) "Config_Type",
		      (xmlChar *) res) == NULL)
			
			
			
		
		
	



/**
 * \brief Build an XML list of trusted servers we know about.
 *
 * @param[in] config_type   Identifies which trusted server list we should be looking at. (System level, or user level.)
 * @param[in] baseNode   The XML node pointer to build the list under.
 *
 * \retval XENONE on success, anything is an error code suitable for use in an ipc_callout_create_error().
 **/ 
int ipc_callout_helper_build_trusted_server_list(uint8_t config_type,
						 xmlNodePtr baseNode) 
{
	
	
	
	
	
	
	
		return XENONE;
	
	
		
		
		
			
			
			
		
		
		     (t, NULL, (xmlChar *) "Server_Name",
		      (xmlChar *) temp) == NULL)
			
			
			
			
		
		
		
		     (t, NULL, (xmlChar *) "Config_Type",
		      (xmlChar *) res) == NULL)
			
			
			
		
		
	



/**
 * \brief Determine if our desired connection can use logon credentials (assuming we have any).
 *
 * \retval TRUE if the configuration allows the use of logon credentials, and the needed credentials
 *					are available.
 *
 * \retval FALSE if no credentials are available, or the configuration doesn't allow the use of
 *					logon credentials.
 **/ 
int ipc_callout_helper_can_use_logon_creds(context * ctx, char *conn_name) 
{
	
	
	
	
	      && (logon_creds_password_available() == FALSE))
		
	
	
		mycon = config_find_connection(CONFIG_LOAD_GLOBAL, conn_name);
	
		return FALSE;
	
	
		config_find_profile(CONFIG_LOAD_GLOBAL, mycon->profile);
	
		return FALSE;
	
	    eap_sm_creds_required(myprof->method->method_num,
				  myprof->method->method_data);
	
	      && (logon_creds_username_available() == FALSE))
		
	
	      && (logon_creds_password_available() == FALSE))
		
	
		return FALSE;
	
		return FALSE;
	



/**
 * \brief Determine if this connection needs administrative rights to be used.  Right now, it checks
 *			to see if machine authentication is enabled.  If it is, then the connection won't show up
 *			for normal users.
 *
 * @param[in] cur   A pointer to the connection structure we want to check.
 *
 * \retval TRUE if admin is needed
 * \retval FALSE if admin is NOT needed.
 **/ 
unsigned int ipc_callout_helper_connection_needs_admin(struct config_connection
						       *cur) 
{
	
	
	
	
		prof = config_find_profile(CONFIG_LOAD_GLOBAL, cur->profile);
	
		
		
			      "Couldn't find the profile used by connection '%s'!\n",
			      cur->name);
		
		
	
		return FALSE;
	
		return FALSE;	// We only do machine auth with PEAP right now.
	
	
		return TRUE;
	



/**
 * \brief Count the number of connections found in a config_connection list.
 *
 * @param[in] cur   A pointer to the list that contains a connection information.
 *
 * \retval int  Number of connections in the list.
 **/ 
unsigned int ipc_callout_helper_count_connections(struct config_connection *cur)
    
{
	
	
	
	
	
		
		
		
			
			
			
		
		else if (are_admin == TRUE)
			
			
			
		
		
	



/**
 * \brief Determine if a profile is in use in a specific connection list.
 *
 * @param[in] config_type   Should we be looking in the system level, or user level config?
 * @param[in] name   The name of the profile we are checking.
 *
 * \retval TRUE if the profile was found connected to a connection in the list.
 * \retval FALSE if the profile was not found in the connection list.
 **/ 
int ipc_callout_helper_is_profile_in_use(uint8_t config_type, char *name) 
{
	
	
		
	
	else
		
	
		 && ((cur->profile == NULL)
		     || (strcmp(cur->profile, name) != 0)))
		
		
		
	
		
		
		    // We found something.
		    return TRUE;
		
	



/**
 * \brief Based on the config_type determine if the named trusted server is in use by
 *        an existing profile.
 *
 * @param[in] config_type   One of CONFIG_LOAD_GLOBAL, or CONFIG_LOAD_USER.
 * @param[in] name   The name of the trusted server we want to look for.
 *
 * \retval TRUE if it is in use in this list.
 * \retval FALSE if it isn't in use in this list.
 **/ 
int ipc_callout_helper_is_trusted_server_in_use(uint8_t config_type,
						char *name) 
{
	
	
	
	
		
	
	else
		
	
		
		
		
		
			
			
			
		
		else
			
			
			
		
	
		
		
		    // We found something.
		    return TRUE;
		
	



/**
 * \brief Return the trusted server name that is bound to a profile.
 *
 * \note DO NOT FREE the resulting pointer!  It is a pointer to the
 *       source string, not a copied string.
 *
 * @param[in] cur   The profile to get the trusted server name for.
 *
 * \retval NULL if it isn't found, ptr otherwise.
 **/ 
char *ipc_callout_helper_get_tsname_from_profile(config_profiles * cur) 
{
	
	
		
		
		    // Only TLS, TTLS, and PEAP use trusted servers, so ignore the rest.
	case EAP_TYPE_TLS:
		
		    ((struct config_eap_tls *)(cur->method->
					       method_data))->trusted_server;
		
	
		
		    ((struct config_eap_ttls *)(cur->method->
						method_data))->trusted_server;
		
	
		
		    ((struct config_eap_peap *)(cur->method->
						method_data))->trusted_server;
		
		
	



								config_profiles
								*confprof,
								char *oldname,
								char *newname) 
{
	
	
		
		
		
		    // We should only switch on methods that actually use a trusted server.
		    // Others should be ignored, and a default should NOT be implemented!
		    switch (confprof->method->method_num)
			
		
			
				
				    ((struct config_eap_ttls
				      *)
				     (confprof->method->method_data))->trusted_server;
			
		
			
				
				    ((struct config_eap_tls
				      *)
				     (confprof->method->method_data))->trusted_server;
			
		
			
				
				    ((struct config_eap_peap
				      *)
				     (confprof->method->method_data))->trusted_server;
			
		
			
				
				    ((struct config_eap_fast
				      *)
				     (confprof->method->method_data))->trusted_server;
			
			
		
			
			
				
				
				    // We should only switch on methods that actually use a trusted server.
				    // Others should be ignored, and a default should NOT be implemented!
				    switch (confprof->method->method_num)
					
				
					
					     NULL)
						
						
							*)(confprof->
							   method->method_data))->trusted_server);
						
						   *)(confprof->
						      method->method_data))->trusted_server
			     = _strdup(newname);
						
					
				
					
					     NULL)
						
						
							*)(confprof->
							   method->method_data))->trusted_server);
						
						   *)(confprof->
						      method->method_data))->trusted_server
			      = _strdup(newname);
						
					
				
					
					     NULL)
						
						
							*)(confprof->
							   method->method_data))->trusted_server);
						
						   *)(confprof->
						      method->method_data))->trusted_server
			     = _strdup(newname);
						
					
				
					
					     NULL)
						
						
							*)(confprof->
							   method->method_data))->trusted_server);
						
						   *)(confprof->
						      method->method_data))->trusted_server
			     = _strdup(newname);
						
					
					
				
			
		
		


