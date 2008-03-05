/**
 * Windows wireless interface.
 *
 * Licensed under a dual GPL/BSD license.  (See LICENSE file for more info.)
 *
 * \file cardif_windows_wireless.c
 *
 * \authors chris@open1x.org
 *
 * \par CVS Status Information:
 * \code
 * $Id: xsupconfcheck_profile.c,v 1.3 2007/10/17 07:00:39 galimorerpg Exp $
 * $Date: 2007/10/17 07:00:39 $
 * \endcode
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libxsupconfig/xsupconfig.h"
#include "libxsupconfig/xsupconfig_structs.h"
#include "src/context.h"
#include "src/error_prequeue.h"
#include "src/interfaces.h"
#include "xsupconfcheck.h"
#include "xsupconfcheck_profile.h"

/**
 * \brief Check to see if we have a password configured.
 *
 * @param[in] mypwd   The structure that contains data for "password only" EAP types.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_pwd_only(struct config_pwd_only *mypwd, config_profiles *prof, int log)
{
	if (mypwd == NULL)
	{
		if (log == TRUE) error_prequeue_add("There is no password configured.");
		return -1;
	}

	if ((mypwd->password == NULL) && (prof->temp_password == NULL))
	{
		if (log == TRUE) error_prequeue_add("There is no password configured.");
		return -1;
	}

	return 0;
}

/**
 * \brief Check to see if we have EAP-TLS configured correctly.
 *
 * @param[in] tls   The structure that contains data for the EAP-TLS authentication type.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_eap_tls(struct config_eap_tls *tls, config_profiles *prof, int log)
{
	int retval = 0;

	if (tls == NULL)
	{
		if (log == TRUE) error_prequeue_add("You must configure EAP-TLS first.");
		retval = -1;
	}

	if (tls->trusted_server == NULL)
	{
		if (log == TRUE) error_prequeue_add("You must define a trusted server to use EAP-TLS.");
		retval = -1;
	}

	if (tls->user_key == NULL)
	{
		if (log == TRUE) error_prequeue_add("A user key file must be specified to use EAP-TLS.");
		retval = -1;
	}

	if (tls->user_cert == NULL)
	{
		if (log == TRUE) error_prequeue_add("A user certificate file must be specified to use EAP-TLS.");
		retval = -1;
	}

	if ((tls->user_key_pass == NULL) && (prof->temp_password == NULL))
	{
		if (log == TRUE) error_prequeue_add("A user key password must be specified to use EAP-TLS.");
		retval = -1;
	}

	return retval;
}

/**
 * \brief Check to see if we have EAP-SIM configured correctly.
 *
 * @param[in] sim   The structure that contains data for the EAP-SIM authentication type.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_eap_sim(struct config_eap_sim *sim, config_profiles *prof, int log)
{
	int retval = 0;

	if (sim == NULL)
	{
		if (log == TRUE) error_prequeue_add("You must configure EAP-SIM before using it.");
		retval = -1;
	}

	if ((sim->password == NULL) && (prof->temp_password == NULL))
	{
		if (log == TRUE) error_prequeue_add("You must set a password before using EAP-SIM.");
		retval = -1;
	}

	return retval;
}

/**
 * \brief Check to see if we have EAP-TTLS configured correctly.
 *
 * @param[in] ttls   The structure that contains data for the EAP-TTLS authentication type.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_eap_ttls(struct config_eap_ttls *ttls, config_profiles *prof, int log)
{
	int retval = 0;

	if (ttls == NULL)
	{
		if (log == TRUE) error_prequeue_add("There is no TTLS configuration defined for this profile.");
		return -1;
	}

	if ((ttls->validate_cert == TRUE) && (ttls->trusted_server == NULL))
	{
		if (log == TRUE) error_prequeue_add("There is no trusted server defined even though you want to validate the server certificate.");
		retval = -1;
	}
	else
	{
		if (ttls->validate_cert == TRUE)
		{
			if (xsupconfcheck_trusted_server(ttls->trusted_server, log) != 0)
			{
				// No need to do anything else here.  The above call will have handled it.
				retval = -1;
			}
		}
	}

	if ((ttls->user_cert != NULL) && (ttls->user_key == NULL))
	{
		if (log == TRUE) error_prequeue_add("The profile is configured to use a user certificate with TTLS, but there is no user key file defined.");
		retval = -1;
	}

	if ((ttls->user_key != NULL) && (ttls->user_key_pass == NULL))
	{
		if (log == TRUE) error_prequeue_add("The profile is configured to use a user certificate with TTLS, but there is no password for the private key.");
		retval = -1;
	}

	if (ttls->phase2_data == NULL)
	{
//		if (log == TRUE) error_prequeue_add("There is no phase 2 configuration available!");
//		retval = -1;
		return retval;
	}


	switch (ttls->phase2_type)
	{
	case TTLS_PHASE2_PAP:
	case TTLS_PHASE2_CHAP:
	case TTLS_PHASE2_MSCHAP:
	case TTLS_PHASE2_MSCHAPV2:
		if ((((struct config_pwd_only *)ttls->phase2_data)->password == NULL) && (prof->temp_password))
		{
			if (log == TRUE) error_prequeue_add("There is no password defined for the TTLS phase 2 method.");
			retval = -1;
		}
		break;

	case TTLS_PHASE2_EAP:
		if (xsupconfcheck_profile_check_eap_method(ttls->phase2_data, prof, log) != 0)
		{
			// No need to do anything here.  It will be taken care of in the above call.
			retval = -1;
		}
		break;

	default:
		if (log == TRUE) error_prequeue_add("The TTLS phase 2 method defined is unknown.");
		retval = -1;
		break;
	}

	return retval;
}

/**
 * \brief Check to see if we have EAP-PEAP configured correctly.
 *
 * @param[in] peap   The structure that contains data for the EAP-PEAP authentication type.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_eap_peap(struct config_eap_peap *peap, config_profiles *prof, int log)
{
	int retval = 0;

	if (peap == NULL)
	{
		if (log == TRUE) error_prequeue_add("There is no configuration for EAP-PEAP.");
		return -1;
	}

	if ((peap->trusted_server == NULL) && (peap->validate_cert == TRUE))
	{
		if (log == TRUE) error_prequeue_add("There is no trusted server defined, but the profile is configured to validate the certificate.");
		retval = -1;
	}
	else
	{
		if (peap->validate_cert == TRUE)
		{
			if (xsupconfcheck_trusted_server(peap->trusted_server, log) != 0)
			{
				// No need to do anything else here.  The above call will have handled it.
				retval = -1;
			}
		}
	}

	if ((peap->user_cert != NULL) && (peap->user_key == NULL))
	{
		if (log == TRUE) error_prequeue_add("The profile is configured to use a user certificate with PEAP, but there is no user key file defined.");
		retval = -1;
	}

	if ((peap->user_key != NULL) && (peap->user_key_pass == NULL))
	{
		if (log == TRUE) error_prequeue_add("The profile is configured to use a user certificate with PEAP, but there is no password for the private key.");
		retval = -1;
	}

	if (xsupconfcheck_profile_check_eap_method(peap->phase2, prof, log) != 0)
	{
		// No need to do anything else here.  The above call will populate the errors.
		retval = -1;
	}

	return retval;
}

/**
 * \brief Check to see if we have EAP-MSCHAPv2 configured correctly.
 *
 * @param[in] mscv2   The structure that contains data for the EAP-MSCHAPv2 authentication type.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_eap_mschapv2(struct config_eap_mschapv2 *mscv2, config_profiles *prof, int log)
{
	int retval = 0;

	if (mscv2 == NULL)
	{
		if (log == TRUE) error_prequeue_add("There is no configuration for EAP-MSCHAPv2.");
		return -1;
	}

	if ((mscv2->nthash == NULL) && (mscv2->password == NULL) && (prof->temp_password == NULL))
	{
		if (log == TRUE) error_prequeue_add("There is no password set for EAP-MSCHAPv2.");
		retval = -1;
	}

	return retval;
}

/**
 * \brief Check to see if we have EAP-FAST configured correctly.
 *
 * @param[in] fast   The structure that contains data for the EAP-FAST authentication type.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_eap_fast(struct config_eap_fast *fast, config_profiles *prof, int log)
{
	int retval = 0;

	if (fast == NULL)
	{
		if (log == TRUE) error_prequeue_add("There is no configuration set for EAP-FAST.");
		return -1;
	}

	if (fast->pac_location == NULL)
	{
		if (log == TRUE) error_prequeue_add("Please specify a PAC file location.");
		retval = -1;
	}

	if (fast->phase2 == NULL)
	{
		if (log == TRUE) error_prequeue_add("There is no phase 2 configuration defined for EAP-FAST.");
		retval = -1;
	}

	if (xsupconfcheck_profile_check_eap_method(fast->phase2, prof, log) != 0)
	{
		// No need to put anything in the error queue here.  It will be taken care of.
		retval = -1;
	}

	return retval;
}

/**
 * \brief Check the EAP method structure, and check the related EAP methods.
 *
 * @param[in] myeap   The EAP method structure that contains information about which EAP method
 *                    we need to validate.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_check_eap_method(config_eap_method *myeap, config_profiles *prof, int log)
{
	int retval = 0;

	if (myeap->method_data == NULL)
	{
		if (log == TRUE) error_prequeue_add("No method data is configured for the EAP method.");
		return -1;
	}

	switch (myeap->method_num)
	{
	case EAP_TYPE_OTP:
	case EAP_TYPE_GTC:
		// Nothing to check.
		break;

	case EAP_TYPE_MD5:
	case EAP_TYPE_LEAP:
		if (xsupconfcheck_profile_pwd_only(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_TLS:
		if (xsupconfcheck_profile_eap_tls(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_SIM:
		if (xsupconfcheck_profile_eap_sim(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_TTLS:
		if (xsupconfcheck_profile_eap_ttls(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_AKA:
		// For now, it is the same settings as EAP-SIM, so use the same checks.
		if (xsupconfcheck_profile_eap_sim(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_PEAP:
		if (xsupconfcheck_profile_eap_peap(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_MSCHAPV2:
		if (xsupconfcheck_profile_eap_mschapv2(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	case EAP_TYPE_FAST:
		if (xsupconfcheck_profile_eap_fast(myeap->method_data, prof, log) != 0)
		{
			// No need to do anything here, the previous call did it all.
			retval = -1;
		}
		break;

	default:
		if (log == TRUE) error_prequeue_add("Unknown EAP type configured.  Please update xsupconfcheck_profile_check_eap_method().");
		retval = -1;
		break;
	}

	return retval;
}

/**
 * \brief Check a profile to make sure it is valid.
 *
 * @param[in] myprof   The profile structure that contains the data we want to check.
 *
 * \retval 0 on success
 * \retval -1 on failure
 **/
int xsupconfcheck_profile_check(struct config_profiles *myprof, int log)
{
	int retval = 0;

	// Verify that we have a valid identity set.
	if ((myprof->identity == NULL) && (myprof->temp_username == NULL))
	{
		if (log == TRUE) error_prequeue_add("Profile is missing a valid username.");
		retval = -1;
	}

	// Verify that the other data is valid.
	if (xsupconfcheck_profile_check_eap_method(myprof->method, myprof, log) != 0)
	{
		// No need to add anything to error_prequeue here.  The call above will have taken care of that for us.
		retval = -1;
	}

	return retval;
}
