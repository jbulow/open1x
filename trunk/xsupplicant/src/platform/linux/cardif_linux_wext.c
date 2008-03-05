/**
 * Linux wireless extensions interface.
 *
 * Licensed under a dual GPL/BSD license.  (See LICENSE file for more info.)
 *
 * \file cardif_linux_wext.c
 *
 * \authors Chris.Hessing@utah.edu
 *
 **/

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <iwlib.h>
#include <linux/if_packet.h>
#include <linux/netlink.h>

#include "libxsupconfig/xsupconfig.h"
#include "context.h"
#include "config_ssid.h"
#include "xsup_common.h"
#include "xsup_debug.h"
#include "xsup_err.h"
#include "wpa.h"
#include "wpa2.h"
#include "wpa_common.h"
#include "platform/cardif.h"
#include "platform/linux/cardif_linux.h"
#include "platform/linux/cardif_linux_wext.h"
#include "wireless_sm.h"
#include "platform/linux/cardif_linux_rtnetlink.h"
#include "timer.h"
#include "wpa.h"
#include "wpa2.h"

#ifdef USE_EFENCE
#include <efence.h>
#endif

// Old versions of wireless.h may not have this defined.
#ifndef IW_ENCODE_TEMP
#define IW_ENCODE_TEMP            0x0400
#endif

/**
 * Cancel our scan timer, if we get here before something else cancels
 * it for us. ;)
 **/
void cardif_linux_wext_cancel_scantimer(context *ctx)
{
  timer_cancel(ctx, SCANCHECK_TIMER);
}

/**
 * Tell the wireless card to start scanning for wireless networks.
 **/
int cardif_linux_wext_scan(context *thisint, char passive)
{
  struct lin_sock_data *sockData;
  struct iwreq iwr;
#if WIRELESS_EXT > 17
  struct iw_scan_req iwsr;
#endif
  struct config_globals *globals;
  wireless_ctx *wctx = NULL;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEGENERROR;

  if (!xsup_assert((thisint->intTypeData != NULL), "thisint->intTypeData != NULL",
		   FALSE))
    return XEMALLOC;

  wctx = (wireless_ctx *)thisint->intTypeData;

  if (thisint->intType != ETH_802_11_INT)
    {
      debug_printf(DEBUG_INT, "%s is not a wireless interface!\n", 
		   thisint->intName);
      return XENOWIRELESS;
    }

  memset(&iwr, 0x00, sizeof(iwr));

  sockData = thisint->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  if (sockData->sockInt <= 0)
    {
      debug_printf(DEBUG_INT, "No socket available to start scan!\n");
      return XENOSOCK;
    }

  cardif_linux_wext_wpa_state(thisint, 1);

  debug_printf(DEBUG_INT, "Issuing %s scan request for interface %s!\n",
	       passive ? "passive":"active", thisint->intName);

#if WIRELESS_EXT > 17  
  // Build our extended scan structure.
  memset(&iwsr, 0x00, sizeof(iwsr));

  if (passive)
    {
      iwsr.scan_type = IW_SCAN_TYPE_PASSIVE;
            iwsr.scan_type = IW_SCAN_TYPE_ACTIVE;

      // If we are doing a passive scan, then we only care about other APs
      // that are on this SSID.  Otherwise, we might end up picking an SSID
      // later that isn't in the same layer2/3 space.
      //      iwr.u.data.flags = IW_SCAN_THIS_ESSID | IW_SCAN_ALL_FREQ | 
      //	IW_SCAN_THIS_MODE | IW_SCAN_ALL_RATE;

      //      iwr.u.data.flags = IW_SCAN_DEFAULT;
      iwr.u.data.flags = IW_SCAN_THIS_ESSID;
    }
  else
    {
      // Some cards won't do a full scan if they are associated.
      //      cardif_linux_wext_set_bssid(thisint, all4s);

      iwsr.scan_type = IW_SCAN_TYPE_ACTIVE;
      iwr.u.data.flags = IW_SCAN_DEFAULT;
    }

  // We aren't looking for a specific BSSID.
  memset(iwsr.bssid.sa_data, 0xff, 6);

  iwr.u.data.length = sizeof(iwsr);
  iwr.u.data.pointer = (caddr_t) &iwsr;

#else
  iwr.u.data.length = 0;
  iwr.u.data.pointer = NULL;
  iwr.u.data.flags = IW_SCAN_DEFAULT;
#endif
  
  Strncpy((char *)&iwr.ifr_name, sizeof(iwr.ifr_name), thisint->intName,
	  strlen(thisint->intName)+1);

  if (ioctl(sockData->sockInt, SIOCSIWSCAN, &iwr) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Error with SCAN ioctl!  (Perhaps your card "
		   "doesn't support scanning, or isn't up?)\n");
      debug_printf(DEBUG_NORMAL, "Error was (%d) : %s\n", errno, strerror(errno));

      return -1;
    }

  SET_FLAG(wctx->flags, WIRELESS_SCANNING);

  globals = config_get_globals();
  
  if (!globals)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't get a handle to the scan timeout "
		   "variable!  (Perhaps the configuration isn't "
		   "initalized?)\n");
      debug_printf(DEBUG_NORMAL, "Scanning will commence, but will only be "
		   "successful on cards that send scan complete events.\n");
      return XENONE;
    }

  timer_add_timer(thisint, SCANCHECK_TIMER, globals->assoc_timeout, 
		  cardif_linux_rtnetlink_scancheck, 
		  cardif_linux_wext_cancel_scantimer);

  return XENONE;
}

/**
 * Set all of the keys to 0s.
 **/
void cardif_linux_wext_set_zero_keys(context *thisint)
{
  char zerokey[13];
  char keylen = 13;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return;

  debug_printf(DEBUG_INT, "Setting keys to zeros!\n");

  memset(zerokey, 0x00, 13);

  // We set the key index to 0x80, to force key 0 to be set to all 0s,
  // and to have key 0 be set as the default transmit key.
  cardif_set_wep_key(thisint, (uint8_t *)&zerokey, keylen, 0x80);
  cardif_set_wep_key(thisint, (uint8_t *)&zerokey, keylen, 0x01);
  cardif_set_wep_key(thisint, (uint8_t *)&zerokey, keylen, 0x02);
  cardif_set_wep_key(thisint, (uint8_t *)&zerokey, keylen, 0x03);
}

/**
 * If we have detected, or forced this interface to reset keys, then
 * we need to reset them.  Otherwise, we will just ignore the fact that
 * we changed APs, and return.
 **/
void cardif_linux_wext_zero_keys(context *thisint)
{
  wireless_ctx *wctx = NULL;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return;

  if (!xsup_assert((thisint->intTypeData != NULL), "thisint->intTypeData != NULL",
		   FALSE))
    return;

  wctx = (wireless_ctx *)thisint->intTypeData;

  if (TEST_FLAG(wctx->flags, WIRELESS_ROAMED))
    {
      return;
    }

  SET_FLAG(wctx->flags, WIRELESS_ROAMED);

  cardif_linux_wext_set_zero_keys(thisint);
  cardif_linux_wext_enc_open(thisint);
}

/**
 * Disable encryption on the wireless card.  This is used in cases
 * where we roam to a different AP and the card needs to have WEP
 * disabled.
 **/
int cardif_linux_wext_enc_disable(context *thisint)
{
  int rc = 0;
  struct iwreq wrq;
  struct lin_sock_data *sockData;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  memset((struct iwreq *)&wrq, 0x00, sizeof(struct iwreq));

  sockData = thisint->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  if (sockData->sockInt <= 0)
    return XENOSOCK;

  Strncpy(wrq.ifr_name, sizeof(wrq.ifr_name), thisint->intName, 
	  strlen(thisint->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  // We got some data, so see if we have encryption or not.
#ifdef IW_ENCODE_NOKEY
  wrq.u.encoding.flags = (IW_ENCODE_DISABLED | IW_ENCODE_NOKEY);
#else
  wrq.u.encoding.flags = IW_ENCODE_DISABLED;
#endif

  wrq.u.encoding.length = 0;
  wrq.u.encoding.pointer = (caddr_t)NULL;

  rc = ioctl(sockData->sockInt, SIOCSIWENCODE, &wrq);
  if (rc < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't disable encryption!\n");
    } else {
      debug_printf(DEBUG_INT, "Encryption disabled!\n");
    }
 
  return rc;
}

/**
 * Create the WPA2 Information Element.
 **/
int cardif_linux_wext_get_wpa2_ie(context *thisint, char *iedata, int *ielen)
{
  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((iedata != NULL), "iedata != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((ielen != NULL), "ielen != NULL", FALSE))
    return XEMALLOC;

#if WIRELESS_EXT > 17

  // Should we use capabilities here?
  wpa2_gen_ie(thisint, iedata, ielen);

  debug_printf(DEBUG_INT, "Setting WPA2 IE : ");
  debug_hex_printf(DEBUG_INT, (uint8_t *)iedata, *ielen);
  debug_printf(DEBUG_INT, "\n");
#else
  debug_printf(DEBUG_NORMAL, "WPA2 isn't implemented in this version of the "
	       "wireless extensions!  Please upgrade to the latest version "
	       "of wireless extensions, or specify the driver to use with the"
	       " -D option!\n");

  iedata = NULL;
  *ielen = 0;
#endif
  return XENONE;
}

/**
 *  Generate the WPA1 Information Element
 **/
int cardif_linux_wext_get_wpa_ie(context *thisint, 
				 char *iedata, int *ielen)
{
  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((iedata != NULL), "iedata != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((ielen != NULL), "ielen != NULL", FALSE))
    return XEMALLOC;

#if WIRELESS_EXT > 17

  wpa_gen_ie(thisint, iedata);
  *ielen = 24;

  debug_printf(DEBUG_INT, "Setting WPA IE : ");
  debug_hex_printf(DEBUG_INT, (uint8_t *)iedata, *ielen);
  debug_printf(DEBUG_INT, "\n");
#else
  debug_printf(DEBUG_NORMAL, "WPA isn't implemented in this version of the "
	       "wireless extensions!  Please upgrade to the latest version "
	       "of wireless extensions, or specify the driver to use with the"
	       " -D option!\n");

  iedata = NULL;
  *ielen = 0;
#endif
  return XENONE;
}

/**
 * Set encryption to open on the wireless card.
 **/
int cardif_linux_wext_enc_open(context *thisint)
{
  int rc = 0;
  struct iwreq wrq;
  struct lin_sock_data *sockData;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  memset((struct iwreq *)&wrq, 0x00, sizeof(struct iwreq));

  sockData = thisint->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  if (sockData->sockInt <= 0)
    return XENOSOCK;

  Strncpy(wrq.ifr_name, sizeof(wrq.ifr_name), thisint->intName, 
	  strlen(thisint->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  // We got some data, so see if we have encryption or not.
  wrq.u.encoding.flags = IW_ENCODE_OPEN;
  wrq.u.encoding.length = 0;
  wrq.u.encoding.pointer = (caddr_t)NULL;

  rc = ioctl(sockData->sockInt, SIOCSIWENCODE, &wrq);
  if (rc < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't disable encryption!\n");
    } else {
      debug_printf(DEBUG_INT, "Encryption set to Open!\n");
    }
 
  return rc;
}


/**
 * Set a WEP key.  Also, based on the index, we may change the transmit
 * key.
 **/
int cardif_linux_wext_set_WEP_key(context *thisint, uint8_t *key, 
				  int keylen, int index)
{
  int rc = 0;
  int settx = 0;
  struct iwreq wrq;
  struct lin_sock_data *sockData;
  wireless_ctx *wctx = NULL;
  char seq[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
  uint8_t addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((thisint->intTypeData != NULL), "thisint->intTypeData != NULL",
		   FALSE))
    return XEMALLOC;

  wctx = (wireless_ctx *)thisint->intTypeData;

  if (index & 0x80) settx = 1;

#if WIRELESS_EXT > 17
  rc = cardif_linux_wext_set_key_ext(thisint, IW_ENCODE_ALG_WEP, addr,
				     (index & 0x7f), settx, seq, 6, (char *)key,
				     keylen);


  if (rc == XENONE) return rc;
#endif

  debug_printf(DEBUG_INT, "Couldn't use extended key calls to set keys. \n");
  debug_printf(DEBUG_INT, "Trying old method.\n");

  memset(&wrq, 0x00, sizeof(wrq));

  if (thisint->intType != ETH_802_11_INT)
    {
      debug_printf(DEBUG_NORMAL, "Interface isn't wireless, but an attempt"
		   " to set a key was made!\n");
      return XENOWIRELESS;
    }

  sockData = thisint->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  if (sockData->sockInt <= 0)
    return XENOSOCK;

  Strncpy(wrq.ifr_name, sizeof(wrq.ifr_name), thisint->intName, 
	  strlen(thisint->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  wrq.u.data.flags = ((index & 0x7f)+1);

  if (TEST_FLAG(wctx->flags, WIRELESS_DONT_USE_TEMP))
    wrq.u.data.flags |= IW_ENCODE_OPEN;
  else
    wrq.u.data.flags |= IW_ENCODE_OPEN | IW_ENCODE_TEMP;

  wrq.u.data.length = keylen;
  wrq.u.data.pointer = (caddr_t)key;

  if ((rc = ioctl(sockData->sockInt, SIOCSIWENCODE, &wrq)) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Failed to set WEP key [%d], error %d : %s\n",
		   (index & 0x7f) + 1, errno, strerror(errno));

      rc = XENOKEYSUPPORT;
    } else {
      debug_printf(DEBUG_INT, "Successfully set WEP key [%d]\n",
		   (index & 0x7f)+1);

      if (index & 0x80)
	{
	  // This is a unicast key, use it for transmissions.
	  Strncpy(wrq.ifr_name, sizeof(wrq.ifr_name), thisint->intName, 
		  strlen(thisint->intName)+1);

	  if (strlen(wrq.ifr_name) == 0)
	    {
	      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
			   __FUNCTION__, __LINE__);
	      return XEGENERROR;
	    }

	  wrq.u.data.flags = (((index & 0x7f) + 1) & IW_ENCODE_INDEX) | IW_ENCODE_NOKEY;
	  if (TEST_FLAG(wctx->flags, WIRELESS_DONT_USE_TEMP))
	    wrq.u.data.flags |= IW_ENCODE_OPEN;
	  else
	    wrq.u.data.flags |= IW_ENCODE_OPEN | IW_ENCODE_TEMP;

	  wrq.u.data.length = 0;
	  wrq.u.data.pointer = (caddr_t)NULL;

	  if (ioctl(sockData->sockInt, SIOCSIWENCODE, &wrq) < 0)
	    {
	      debug_printf(DEBUG_NORMAL, "Failed to set the WEP transmit key ID [%d]\n", (index & 0x7f)+1);
	      rc = XENOKEYSUPPORT;
	    } else {
	      debug_printf(DEBUG_INT, "Successfully set the WEP transmit key [%d]\n", (index & 0x7f)+1);
	    }
	}  
    }
 
  return rc;
}

/**
 * Set the SSID of the wireless card.
 **/
int cardif_linux_wext_set_ssid(context *thisint, char *ssid_name)
{
  struct iwreq iwr;
  struct lin_sock_data *sockData;
  char newssid[100];
  wireless_ctx *wctx = NULL;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEGENERROR;

  if (!xsup_assert((ssid_name != NULL), "ssid_name != NULL", FALSE))
    return XEGENERROR;

  if (!xsup_assert((thisint->intTypeData != NULL), "thisint->intTypeData != NULL",
		   FALSE))
    return XEGENERROR;

  wctx = (wireless_ctx *)thisint->intTypeData;

  memset(&iwr, 0x00, sizeof(iwr));

  sockData = thisint->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  if (sockData->sockInt <= 0)
    return XENOSOCK;

  if (thisint->intType != ETH_802_11_INT)
    {
      return XENOWIRELESS;
    }

  // Specify the interface name we are asking about.
  Strncpy(iwr.ifr_name, sizeof(iwr.ifr_name), thisint->intName, 
	  strlen(thisint->intName)+1);

  if (strlen(iwr.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  memset(newssid, 0x00, 100);
  strcpy(newssid, ssid_name);

  iwr.u.essid.pointer = (caddr_t) newssid;
  iwr.u.essid.length = strlen(newssid);

  // Starting in WE 21, we don't NULL terminate SSIDs we set.
  if (cardif_linux_rtnetlink_get_we_ver(thisint) < 21)
    iwr.u.essid.length++;

  iwr.u.essid.flags = 1;

  if (ioctl(sockData->sockInt, SIOCSIWESSID, &iwr) < 0) return XENOWIRELESS;

  // Allow us to correlate SSID set events.
  SET_FLAG(wctx->flags, WIRELESS_SM_SSID_CHANGE);

  debug_printf(DEBUG_INT, "Requested SSID be set to '%s'\n", newssid);

  UNSET_FLAG(thisint->flags, WAS_DOWN);

  return XENONE;
}

/**
 * Set the Broadcast SSID (MAC address) of the AP we are connected to.
 **/
int cardif_linux_wext_set_bssid(context *intdata, uint8_t *bssid)
{
  struct iwreq wrq;
  struct lin_sock_data *sockData;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEGENERROR;

  if (!xsup_assert((bssid != NULL), "bssid != NULL", FALSE))
    return XEGENERROR;

  debug_printf(DEBUG_INT, "Setting BSSID : ");
  debug_hex_printf(DEBUG_INT, bssid, 6);

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  memset(&wrq, 0x00, sizeof(wrq));

  Strncpy((char *)&wrq.ifr_name, sizeof(wrq.ifr_name), intdata->intName, 
	  strlen(intdata->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  memcpy(&wrq.u.ap_addr.sa_data, bssid, 6);
  wrq.u.ap_addr.sa_family = ARPHRD_ETHER;

  if (ioctl(sockData->sockInt, SIOCSIWAP, &wrq) < 0)
    {
      // If we couldn't set the BSSID, it isn't the end of the world.  The
      // driver just may not need it.
      debug_printf(DEBUG_NORMAL, "Error setting BSSID!  We may not associate/"
      		   "authenticate correctly!\n");
      return XESOCKOP;
    }

  return XENONE;
}


/**
 * Get the Broadcast SSID (MAC address) of the Access Point we are connected 
 * to.  If this is not a wireless card, or the information is not available,
 * we should return an error.
 **/
int cardif_linux_wext_get_bssid(context *thisint, 
				char *bssid_dest)
{
  struct iwreq iwr;
  struct lin_sock_data *sockData;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((bssid_dest != NULL), "bssid_dest != NULL", FALSE))
    return XEMALLOC;

  // If we are a wired interface, don't bother.
  if (thisint->intType != ETH_802_11_INT) return XENONE;

  sockData = thisint->sockData;
  
  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  memset(&iwr, 0x00, sizeof(iwr));

  // Specify the interface name we are asking about.
  Strncpy(iwr.ifr_name, sizeof(iwr.ifr_name), thisint->intName, 
	  strlen(thisint->intName)+1);

  if (strlen(iwr.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  if (ioctl(sockData->sockInt, SIOCGIWAP, &iwr) < 0) 
    {
      debug_printf(DEBUG_NORMAL, "Couldn't get MAC address for AP!\n");
      return XENOWIRELESS;
    }

  memcpy(bssid_dest, iwr.u.ap_addr.sa_data, 6);
  return XENONE;
}

/**
 * Ask the wireless card for the ESSID that we are currently connected to.  If
 * this is not a wireless card, or the information is not available, we should
 * return an error.
 **/
int cardif_linux_wext_get_ssid(context *thisint, char *ssid_name,
			       unsigned int ssid_len)
{
  struct iwreq iwr;
  struct lin_sock_data *sockData;
  char newssid[100];

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((ssid_name != NULL), "ssid_name != NULL", FALSE))
    return XEMALLOC;

  sockData = thisint->sockData;

  memset(&iwr, 0x00, sizeof(iwr));

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);
 
  if (thisint->intType != ETH_802_11_INT)
    {
      // We want to verify that the interface is in fact, not wireless, and
      // not that we are in a situation where the interface has just been 
      // down.
      debug_printf(DEBUG_NORMAL, "This interface isn't wireless!\n");
      return XENOWIRELESS;
    } 

  // Specify the interface name we are asking about.
  if (strlen(thisint->intName) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  Strncpy(iwr.ifr_name, sizeof(iwr.ifr_name), thisint->intName, 
	  sizeof(iwr.ifr_name)+1);

  memset(newssid, 0x00, IW_ESSID_MAX_SIZE+1);
  iwr.u.essid.pointer = (caddr_t) newssid;
  iwr.u.essid.length = 100;
  iwr.u.essid.flags = 0;

  if (ioctl(sockData->sockInt, SIOCGIWESSID, &iwr) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't get ESSID!\n");
      debug_printf(DEBUG_NORMAL, "Error (%d) : %s\n", errno, strerror(errno));
      return XENOWIRELESS;
    }

  UNSET_FLAG(thisint->flags, WAS_DOWN);

  Strncpy(ssid_name, ssid_len, newssid, iwr.u.essid.length+1);

  if (strlen(ssid_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  return XENONE;
}

/**
 *  This function is called when we roam, or disassociate.  It should
 *  reset the card to a state where it can associate with a new AP.
 **/
int cardif_linux_wext_wep_associate(context *intdata, 
				    int zero_keys)
{
  uint8_t *bssid;
  struct config_globals *globals;
  wireless_ctx *wctx = NULL;
  uint32_t alg;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  if (!xsup_assert((intdata->intTypeData != NULL), "intdata->intTypeData != NULL",
		   FALSE))
    return XEMALLOC;

  wctx = (wireless_ctx *)intdata->intTypeData;

  cardif_linux_wext_wpa_state(intdata, 0);

#if WIRELESS_EXT > 17
  // Determine the type of association we want, and set it up if we are using
  // the proper versions of WEXT.

  switch (intdata->conn->association.association_type)
    {
    case ASSOC_TYPE_WPA1:
    case ASSOC_TYPE_WPA2:
    case ASSOC_TYPE_OPEN:
      alg = IW_AUTH_ALG_OPEN_SYSTEM;
      break;

    case ASSOC_TYPE_SHARED:
      alg = IW_AUTH_ALG_SHARED_KEY;
      break;

    case ASSOC_TYPE_LEAP:
      alg = IW_AUTH_ALG_LEAP;
      break;

    default:
      debug_printf(DEBUG_NORMAL, "Unknown 802.11 authetication alg.  Defaulting "
		   "to Open System.\n");
      debug_printf(DEBUG_NORMAL, "Type was %d.\n", intdata->conn->association.association_type);

      alg = IW_AUTH_ALG_OPEN_SYSTEM;
      break;
    }

  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_80211_AUTH_ALG,
                                   alg, "802.11 auth. alg to open") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't set 802.11 auth. alg.\n");
    }
#endif

  cardif_linux_wext_set_ssid(intdata, wctx->cur_essid);

  if (zero_keys == 0)
    {
      debug_printf(DEBUG_INT, "WEP: turning encryption off.\n");
      return cardif_linux_wext_enc_disable(intdata);
    } else if (zero_keys == 1)
      {
      debug_printf(DEBUG_INT, "WEP: zero keys.\n");
	cardif_linux_wext_zero_keys(intdata);
	return XENONE;
      } else
	{
	  debug_printf(DEBUG_NORMAL, "Invalid association value.\n");
	}

  bssid = config_ssid_get_mac(wctx);
  if (bssid != NULL)
    {
      debug_printf(DEBUG_INT, "Dest. BSSID : ");
      debug_hex_printf(DEBUG_INT, bssid, 6);
    }

  globals = config_get_globals();

  if ((!globals) || (!TEST_FLAG(globals->flags, CONFIG_GLOBALS_FIRMWARE_ROAM)))
    {
      cardif_linux_wext_set_bssid(intdata, bssid);
    }

  return XENOTHING_TO_DO;
}

#if WIRELESS_EXT > 17
int cardif_linux_wext_mlme(context *thisint, uint16_t mlme_type,
			   uint16_t mlme_reason)
{
  struct iwreq iwr;
  struct lin_sock_data *sockData;
  struct iw_mlme iwm;

  if (!xsup_assert((thisint != NULL), "thisint != NULL", FALSE))
    return XEMALLOC;

  sockData = thisint->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  // If we get here, and isWireless == FALSE, then we need to double
  // check that our interface is really not wireless.
  if (thisint->intType != ETH_802_11_INT)
    {
      return XENOWIRELESS;
    }  

  memset(&iwr, 0, sizeof(iwr));
  // Specify the interface name we are asking about.
  Strncpy(iwr.ifr_name, sizeof(iwr.ifr_name), thisint->intName, 
	  strlen(thisint->intName));

  if (strlen(iwr.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  memset(&iwm, 0, sizeof(iwm));
  
  // Set up our MLME struct.
  iwm.cmd = mlme_type;
  iwm.reason_code = mlme_reason;
  iwm.addr.sa_family = ARPHRD_ETHER;

  // Need to specify the MAC address that we want to do this MLME for.
  memcpy(iwm.addr.sa_data, thisint->source_mac, 6);
  iwr.u.data.pointer = (caddr_t)&iwm;
  iwr.u.data.length = sizeof(iwm);

  if (ioctl(sockData->sockInt, SIOCSIWMLME, &iwr) != 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't issue MLME request!  (Error (%d) : %s\n", errno, strerror(errno));
    }
  
  return XENONE;
}
#endif

// Forward decl to keep the compiler from screaming.
void cardif_linux_clear_keys(context *);

int cardif_linux_wext_disassociate(context *intdata, int reason)
{
#if WIRELESS_EXT > 17
  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  cardif_linux_wext_mlme(intdata, IW_MLME_DEAUTH, reason);
  cardif_linux_wext_mlme(intdata, IW_MLME_DISASSOC, reason);
#endif

  debug_printf(DEBUG_NORMAL, "!!!!!!!!! Setting invalid SSID!\n");
  cardif_linux_wext_set_ssid(intdata, "unlikelyssid");
  cardif_linux_clear_keys(intdata);

  return XENONE;
}

int cardif_linux_wext_set_key_ext(context *intdata, int alg, 
				   unsigned char *addr, int keyidx, int settx, 
				   char *seq,  int seqlen, char *key, 
				   int keylen)
{
#if WIRELESS_EXT > 17
  struct iwreq wrq;
  struct lin_sock_data *sockData;
  struct iw_encode_ext *iwee;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  memset(&wrq, 0x00, sizeof(wrq));
  Strncpy((char *)&wrq.ifr_name, sizeof(wrq.ifr_name), intdata->intName, 
	  sizeof(intdata->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  // Allocate enough memory to hold our iw_encode_ext struct, and the
  // key itself.
  iwee = (struct iw_encode_ext *)Malloc(sizeof(struct iw_encode_ext) + keylen);
  if (iwee == NULL) 
    {
      debug_printf(DEBUG_NORMAL,
        "Error with malloc of iwee in cardif_linux_wext_set_key_ext()\n");
      return XEMALLOC;
    }

  iwee->alg = alg;
  iwee->ext_flags = keyidx+1;

  if ((seq != NULL) && (seqlen > 0))
    {
      printf("Setting seq!\n");
      iwee->ext_flags |= IW_ENCODE_EXT_RX_SEQ_VALID;
      memcpy(iwee->rx_seq, seq, seqlen);
    }
  
  if (settx) 
    {
      printf("TX Key!\n");
      iwee->ext_flags |= IW_ENCODE_EXT_SET_TX_KEY;
      if ((addr != NULL) && 
	  (memcmp(addr, "\0xff\0xff\0xff\0xff\0xff\0xff", 6) != 0))
	{
	  memcpy(iwee->addr.sa_data, addr, 6);
	} else {
	  memcpy(iwee->addr.sa_data, intdata->dest_mac, 6);
	}
    } else {
      iwee->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;

      memset(iwee->addr.sa_data, 0xff, 6);
    }
  iwee->addr.sa_family = ARPHRD_ETHER;

  if (keylen != 0)
    {
      iwee->key_len = keylen;
      memcpy(iwee->key, key, keylen);
      printf("Key (%d) : \n", iwee->key_len);
      debug_hex_printf(DEBUG_NORMAL, iwee->key, iwee->key_len);
    }

  debug_printf(DEBUG_INT, "Key Index : %d   Length : %d\n", keyidx, keylen);
  debug_printf(DEBUG_INT, "Destination MAC : ");
  debug_hex_printf(DEBUG_INT, (uint8_t *)iwee->addr.sa_data, 6);
  
  debug_printf(DEBUG_INT, "Setting key : ");
  debug_hex_printf(DEBUG_INT, iwee->key, keylen);

  wrq.u.encoding.pointer = (caddr_t)iwee;
  wrq.u.encoding.flags = (keyidx + 1);
  
  if (alg == IW_ENCODE_ALG_NONE)
    wrq.u.encoding.flags |= (IW_ENCODE_DISABLED | IW_ENCODE_NOKEY);

  wrq.u.encoding.length = sizeof(struct iw_encode_ext) + keylen +1;

  if (ioctl(sockData->sockInt, SIOCSIWENCODEEXT, &wrq) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Error setting key!! (IOCTL "
		   "failure.)\n");
      debug_printf(DEBUG_NORMAL, "Error %d : %s\n", errno, strerror(errno));
    }

  FREE(iwee);

#else
  debug_printf(DEBUG_NORMAL, "%s : Not supported by WE(%d)!\n", __FUNCTION__,
	       WIRELESS_EXT);
#endif

  return XENONE;
}


int cardif_linux_wext_set_tkip_key(context *intdata, 
				   unsigned char *addr, int keyidx, int settx, 
				   char *key, int keylen)
{
#if WIRELESS_EXT > 17
  char seq[6] = {0x00,0x00,0x00,0x00,0x00,0x00};

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  return cardif_linux_wext_set_key_ext(intdata, IW_ENCODE_ALG_TKIP, addr,
				       keyidx, settx, seq, 6, key, keylen);
#else
  debug_printf(DEBUG_NORMAL, "%s : Not supported by WE(%d)!\n", __FUNCTION__,
	       WIRELESS_EXT);
#endif
  return XENONE;
}

int cardif_linux_wext_set_ccmp_key(context *intdata,
				   unsigned char *addr, int keyidx, int settx,
				   char *key, int keylen)
{
#if WIRELESS_EXT > 17
  char seq[6] = {0x00,0x00,0x00,0x00,0x00,0x00};

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  // According to 802.11i, section 8.3.3.4.3e says we should set the PN to
  // 0 when a CCMP key is set. 
  return cardif_linux_wext_set_key_ext(intdata, IW_ENCODE_ALG_CCMP, addr,
				       keyidx, settx, seq, 6, key,
				       keylen);
#else
  debug_printf(DEBUG_NORMAL, "%s : Not supported by WE(%d)!\n", __FUNCTION__,
	       WIRELESS_EXT);
#endif
  return XENONE;
}

int cardif_linux_wext_wpa(context *intdata, char state)
{
#if WIRELESS_EXT > 17
  int retval = 0;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  retval = cardif_linux_wext_set_iwauth(intdata, IW_AUTH_WPA_ENABLED,
					state, "change WPA state");

  if (retval == 0)
    {
      retval = cardif_linux_wext_set_iwauth(intdata, 
					    IW_AUTH_TKIP_COUNTERMEASURES,
					    FALSE, "TKIP countermeasures");

      if (retval == 0)
	{
	  retval = cardif_linux_wext_set_iwauth(intdata, 
						IW_AUTH_DROP_UNENCRYPTED,
						TRUE, "drop unencrypted");
	}
    }

  return retval;
#endif
  return XENONE;
}

int cardif_linux_wext_set_wpa_ie(context *intdata, 
				 unsigned char *wpaie, unsigned int wpalen)
{
#if WIRELESS_EXT > 17
  struct iwreq wrq;
  struct lin_sock_data *sockData;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  memset(&wrq, 0x00, sizeof(wrq));

  Strncpy((char *)&wrq.ifr_name, sizeof(wrq.ifr_name), intdata->intName, 
	  sizeof(intdata->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  wrq.u.data.pointer = (caddr_t) wpaie;
  wrq.u.data.length = wpalen;
  wrq.u.data.flags = 0;

  if (ioctl(sockData->sockInt, SIOCSIWGENIE, &wrq) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Error setting WPA IE!\n");
    } 
#endif
  return XENONE;
}


int cardif_linux_wext_wpa_state(context *intdata, char state)
{

  // If we have wireless extensions 18 or higher, we can support WPA/WPA2
  // with standard ioctls.

#if WIRELESS_EXT > 17
  char wpaie[24];

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  if (state)
    {
      // Enable WPA if the interface doesn't already have it.
      cardif_linux_wext_wpa(intdata, TRUE);
    } else {
      cardif_linux_wext_wpa(intdata, FALSE);

      // Otherwise, make sure we don't have an IE set.
      memset(wpaie, 0x00, sizeof(wpaie));
      if (cardif_linux_wext_set_wpa_ie(intdata, (unsigned char *)wpaie, 0) < 0)
	{
	  debug_printf(DEBUG_NORMAL, "Couldn't clear WPA IE on device %s!\n",
		       intdata->intName);
	}
    }
#endif

  return XENONE;
}

/**
 * Set values for IWAUTH.
 **/
int cardif_linux_wext_set_iwauth(context *intdata,
				 int setting, uint32_t value, 
				 char *setting_name)
{
#if WIRELESS_EXT > 17
  struct iwreq wrq;
  struct lin_sock_data *sockData;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  memset(&wrq, 0x00, sizeof(wrq));
  Strncpy((char *)&wrq.ifr_name, sizeof(wrq.ifr_name), intdata->intName, 
	  sizeof(intdata->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return XEGENERROR;
    }

  wrq.u.param.flags = setting & IW_AUTH_INDEX;
  wrq.u.param.value = value;

  if (ioctl(sockData->sockInt, SIOCSIWAUTH, &wrq) < 0)
    {
      if (errno != ENOTSUP)
	{
	  if (xsup_assert((setting_name != NULL), "setting_name != NULL",
			  FALSE))
	    {
	      debug_printf(DEBUG_NORMAL, "Error changing setting for '%s'! "
			   "It is possible that your driver does not support "
			   "the needed functionality.\n", setting_name);
	      debug_printf(DEBUG_NORMAL, "Error was (%d) : %s\n", errno,
			   strerror(errno));
	    }
	  return -1;
	}
    }
#endif
  return XENONE;
}

/**
 *  Set if we should allow unencrypted EAPoL messages or not.
 **/
int cardif_linux_wext_unencrypted_eapol(context *intdata,
					int state)
{
#if WIRELESS_EXT > 17
  struct lin_sock_data *sockData;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  return cardif_linux_wext_set_iwauth(intdata, IW_AUTH_RX_UNENCRYPTED_EAPOL,
				      state, "RX unencrypted EAPoL");
#endif
  return XENONE;
}



/**
 * Convert our cipher designator to something that will be understood by
 * the linux wireless extensions.
 */
int cardif_linux_wext_iw_cipher(int cipher)
{
#if WIRELESS_EXT > 17
  switch (cipher)
    {
    case CIPHER_NONE:
      return 0;
      break;

    case CIPHER_WEP40:
      return IW_AUTH_CIPHER_WEP40;
      break;

    case CIPHER_TKIP:
      return IW_AUTH_CIPHER_TKIP;
      break;

    case CIPHER_WRAP:
      debug_printf(DEBUG_NORMAL, "WRAP is not supported!\n");
      return -1;
      break;

    case CIPHER_CCMP:
      return IW_AUTH_CIPHER_CCMP;
      break;
      
    case CIPHER_WEP104:
      return IW_AUTH_CIPHER_WEP104;
      break;

    default:
      debug_printf(DEBUG_NORMAL, "Unknown cipher value of %d!\n", cipher);
      return -1;
      break;
    }
#else
  return -1;
#endif
}

/**
 * Set all of the card settings that are needed in order to complete an
 * association, so that we can begin the authentication.
 **/
void cardif_linux_wext_associate(context *intdata)
{
  uint8_t *bssid;
#if WIRELESS_EXT > 17
  int len = 0, akm = 0;
  uint32_t cipher, alg;
  uint8_t wpaie[255];
#endif
  struct config_globals *globals;
  wireless_ctx *wctx = NULL;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return;

  if (!xsup_assert((intdata->intTypeData != NULL), "intdata->intTypeData != NULL",
		   FALSE))
    return;

  wctx = (wireless_ctx *)intdata->intTypeData;

#if WIRELESS_EXT > 17
  // Determine the type of association we want, and set it up if we are using
  // the proper versions of WEXT.

  switch (intdata->conn->association.association_type)
    {
    case ASSOC_TYPE_WPA1:
    case ASSOC_TYPE_WPA2:
    case ASSOC_TYPE_OPEN:
      alg = IW_AUTH_ALG_OPEN_SYSTEM;
      break;

    case ASSOC_TYPE_SHARED:
      alg = IW_AUTH_ALG_SHARED_KEY;
      break;

    case ASSOC_TYPE_LEAP:
      alg = IW_AUTH_ALG_LEAP;
      break;

    default:
      debug_printf(DEBUG_NORMAL, "Unknown 802.11 authetication alg.  Defaulting "
                   "to Open System.\n");

      debug_printf(DEBUG_NORMAL, "Type was %d.\n", intdata->conn->association.association_type);

      alg = IW_AUTH_ALG_OPEN_SYSTEM;
      break;
    }

  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_80211_AUTH_ALG,
                                   alg, "802.11 auth. alg to open") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't set 802.11 auth. alg.\n");
    }

  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_DROP_UNENCRYPTED,
                                   TRUE, "drop unencrypted data") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't enable dropping of unencrypted"
                   " data!\n");
    }
#endif

  if (config_ssid_get_ssid_abilities(wctx) & RSN_IE)
    {
#if WIRELESS_EXT > 17
      cardif_linux_wext_get_wpa2_ie(intdata, (char *) wpaie, &len);
      if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_WPA_VERSION,
				       IW_AUTH_WPA_VERSION_WPA2,
				       "WPA2 version") < 0)
	{
	  debug_printf(DEBUG_NORMAL, "Couldn't set WPA2 version!\n");
	}
      wctx->groupKeyType = wpa2_get_group_crypt(intdata);
      wctx->pairwiseKeyType = wpa2_get_pairwise_crypt(intdata);
#endif
    } else if (config_ssid_get_ssid_abilities(wctx) & WPA_IE)
      {
#if WIRELESS_EXT > 17
	cardif_linux_wext_get_wpa_ie(intdata, (char *) wpaie, &len);
	if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_WPA_VERSION,
					 IW_AUTH_WPA_VERSION_WPA,
					 "WPA version") < 0)
	  {
	    debug_printf(DEBUG_NORMAL, "Couldn't set WPA version!\n");
	  }
	wctx->groupKeyType = wpa_get_group_crypt(intdata);
	wctx->pairwiseKeyType = wpa_get_pairwise_crypt(intdata);
#endif
      }

#if WIRELESS_EXT > 17
  if (cardif_linux_wext_set_wpa_ie(intdata, wpaie, len) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't set WPA IE on device %s!\n",
		   intdata->intName);
    }
  
  // For drivers that require something other than just setting a
  // WPA IE, we will set the components for the IE instead.
  cipher = cardif_linux_wext_iw_cipher(wctx->pairwiseKeyType);
  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_CIPHER_PAIRWISE,
				   cipher, "pairwise cipher") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't set pairwise cipher to %d.\n",
		   cipher);
    }
  
  cipher = cardif_linux_wext_iw_cipher(wctx->groupKeyType);
  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_CIPHER_GROUP,
				   cipher, "group cipher") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't set group cipher to %d.\n",
		   cipher);
    }
  
  if (intdata->conn->association.auth_type == AUTH_PSK)
    {
      akm = IW_AUTH_KEY_MGMT_802_1X;
    } else {
      akm = IW_AUTH_KEY_MGMT_PSK;
    }
  
  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_KEY_MGMT, akm,
				   "Authenticated Key Management Suite") 
      < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't set Authenticated Key "
		   "Management Suite.\n");
    }

  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_PRIVACY_INVOKED,
				   TRUE, "Privacy Invoked") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't invoke privacy!\n");
    }

  if (cardif_linux_wext_set_iwauth(intdata, IW_AUTH_RX_UNENCRYPTED_EAPOL,
                                   TRUE, "RX unencrypted EAPoL") < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't enable RX of unencrypted"
                   " EAPoL.\n");
    }

#endif

  cardif_linux_wext_set_ssid(intdata, wctx->cur_essid);

  bssid = config_ssid_get_mac(wctx);
  if (bssid != NULL)
    {
      debug_printf(DEBUG_INT, "Dest. BSSID : ");
      debug_hex_printf(DEBUG_INT, bssid, 6);
    }

  globals = config_get_globals();

  if ((!globals) || (!TEST_FLAG(globals->flags, CONFIG_GLOBALS_FIRMWARE_ROAM)))
    {
      cardif_linux_wext_set_bssid(intdata, bssid);
    }
  
  if ((wctx->wpa_ie == NULL) && (wctx->rsn_ie == NULL))
    {
      // We need to set up the card to allow unencrypted EAPoL frames.
      cardif_linux_wext_unencrypted_eapol(intdata, TRUE);
    }
  
  return;
}

int cardif_linux_wext_countermeasures(context *intdata,
				      char endis)
{
#if WIRELESS_EXT > 17
  struct lin_sock_data *sockData;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  return cardif_linux_wext_set_iwauth(intdata, IW_AUTH_TKIP_COUNTERMEASURES, 
				      endis,
				      "enable/disable TKIP countermeasures");
#endif
  return XENONE;
}

int cardif_linux_wext_drop_unencrypted(context *intdata, char endis)
{
#if WIRELESS_EXT > 17
  struct lin_sock_data *sockData;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  return cardif_linux_wext_set_iwauth(intdata, IW_AUTH_DROP_UNENCRYPTED,
				      endis, "drop unencrypted frames");
#endif
  return XENONE;
}

void cardif_linux_wext_enc_capabilities(context *intdata)
{
#if WIRELESS_EXT > 17
  struct iwreq wrq;
  struct lin_sock_data *sockData;
  struct iw_range *range;
  wireless_ctx *wctx = NULL;
  char buffer[sizeof(struct iw_range) * 2];
  int i;

  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return;

  if (!xsup_assert((intdata->intTypeData != NULL), "intdata->intTypeData != NULL",
		   FALSE))
    return;

  wctx = (wireless_ctx *)intdata->intTypeData;

  sockData = intdata->sockData;

  xsup_assert((sockData != NULL), "sockData != NULL", TRUE);

  wctx->enc_capa = 0;

  memset(&wrq, 0x00, sizeof(wrq));
  Strncpy((char *)&wrq.ifr_name, sizeof(wrq.ifr_name), intdata->intName, 
	  sizeof(intdata->intName)+1);

  if (strlen(wrq.ifr_name) == 0)
    {
      debug_printf(DEBUG_NORMAL, "Invalid interface name in %s():%d\n",
                   __FUNCTION__, __LINE__);
      return;
    }

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = sizeof(buffer);
  wrq.u.data.flags = 0;

  if (!xsup_assert((sockData->sockInt > 0), "sockData->sockInt > 0", FALSE))
    return;

  if (ioctl(sockData->sockInt, SIOCGIWRANGE, &wrq) < 0)
    {
      debug_printf(DEBUG_NORMAL, "Couldn't get encryption capabilites!\n");
      return;
    }

  // Otherwise, determine what we have.
  range = (struct iw_range *)buffer;

  for (i=0; i<range->num_encoding_sizes; i++)
    {
      if (range->encoding_size[i] == 5) wctx->enc_capa |= DOES_WEP40;
      if (range->encoding_size[i] == 13) wctx->enc_capa |= DOES_WEP104;
    }

  if (range->enc_capa & IW_ENC_CAPA_WPA) wctx->enc_capa |= DOES_WPA;
  if (range->enc_capa & IW_ENC_CAPA_WPA2) wctx->enc_capa |= DOES_WPA2;
  if (range->enc_capa & IW_ENC_CAPA_CIPHER_TKIP)
    wctx->enc_capa |= DOES_TKIP;
  if (range->enc_capa & IW_ENC_CAPA_CIPHER_CCMP)
    wctx->enc_capa |= DOES_CCMP;
#else
  debug_printf(DEBUG_NORMAL, "You need wireless extensions > 17 in order to"
	       " support detection of encryption methods.\n");
  wctx->enc_capa = 0;
#endif
}

int cardif_linux_wext_delete_key(context *intdata, int key_idx, int set_tx)
{
#if WIRELESS_EXT > 17
  if (!xsup_assert((intdata != NULL), "intdata != NULL", FALSE))
    return XEMALLOC;

  debug_printf(DEBUG_INT, "Deleting key %d, with tx set to %d.\n", key_idx,
	       set_tx);

  return cardif_linux_wext_set_key_ext(intdata, IW_ENCODE_ALG_NONE, NULL,
				       key_idx, set_tx, NULL, 0, NULL, 0);
#else
  debug_printf(DEBUG_NORMAL, "%s : Not supported by WE(%d)!\n", __FUNCTION__,
	       WIRELESS_EXT);
#endif
  return XENONE;
}


struct cardif_funcs cardif_linux_wext_driver = {
  .scan = cardif_linux_wext_scan,
  .disassociate = cardif_linux_wext_disassociate,
  .set_wep_key = cardif_linux_wext_set_WEP_key,
  .set_tkip_key = cardif_linux_wext_set_tkip_key,
  .set_ccmp_key = cardif_linux_wext_set_ccmp_key,
  .delete_key = cardif_linux_wext_delete_key,
  .associate = cardif_linux_wext_associate,
  .get_ssid = cardif_linux_wext_get_ssid,
  .get_bssid = cardif_linux_wext_get_bssid,
  .wpa_state = cardif_linux_wext_wpa_state,
  .wpa = cardif_linux_wext_wpa,
  .wep_associate = cardif_linux_wext_wep_associate,
  .countermeasures = cardif_linux_wext_countermeasures,
  .drop_unencrypted = cardif_linux_wext_drop_unencrypted,
  .get_wpa_ie = cardif_linux_wext_get_wpa_ie,
  .get_wpa2_ie = cardif_linux_wext_get_wpa2_ie,
  .enc_disable = cardif_linux_wext_enc_disable,
  .enc_capabilities = cardif_linux_wext_enc_capabilities,
  .setbssid = cardif_linux_wext_set_bssid,
  .set_operstate = cardif_linux_rtnetlink_set_operstate,
  .set_linkmode = cardif_linux_rtnetlink_set_linkmode,
};