/*******************************************************************
 * EAPOL Function implementations for supplicant
 * 
 * Licensed under a dual GPL/BSD license.  (See LICENSE file for more info.)
 *
 * \file sim.h
 *
 * \author chris@open1x.org
 *
 *******************************************************************/

/*******************************************************************
 *
 * The development of the EAP/SIM support was funded by Internet
 * Foundation Austria (http://www.nic.at/ipa)
 *
 *******************************************************************/

#ifdef EAP_SIM_ENABLE

#ifndef _SIM_H_
#define _SIM_H_

#include "profile.h"
#include "eapsim.h"

int sim_build_start(struct eaptypedata *, uint8_t *, int *);
int sim_build_fullauth(char *, uint8_t *, int *, uint8_t *, int *);
int sim_at_version_list(char *, struct eaptypedata *, uint8_t *, int *, 
			uint8_t *, int *);
int sim_do_at_mac(eap_type_data *, struct eaptypedata *, uint8_t *, 
		  int, int *, uint8_t *, int *, char *);
int sim_do_v1_response(eap_type_data *, char *, int *, char *, 
		       char *);
int sim_do_at_rand(struct eaptypedata *, char *, char *, uint8_t *, int *,
		   uint8_t *, int *, char *);
int sim_skip_not_implemented(uint8_t *, int *);

#endif

#endif
