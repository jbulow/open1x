SUBDIRS = platform
CORELIBS = @CORELIBS@
MORELIBS = @MORELIBS@ -L${top_builddir}/lib/libxsupconfig -lxsupconfig \
	   -L${top_builddir}/lib/libxsupconfwrite -lxsupconfwrite \
	   -L${top_builddir}/lib/liblist -llist \
	   -L${top_builddir}/lib/libcrashdump -lcrashdump \
	   -L${top_builddir}/lib/libsupdetect -lsupdetect \
	   -L${top_builddir}/lib/libxsupconfcheck -lxsupconfcheck @DARWINLIBS@
MOREDEFS = @MOREDEFS@
DEFS = @DEFS@ $(MOREDEFS) -DDEBUG_LOG_PLUGINS

CLEANFILES= config_grammar.c config_lexicon.c config_grammar.h

sbin_PROGRAMS = xsupplicant

xsupplicant_CFLAGS = -I${top_srcdir}/src/platform @DARWININCS@

xsupplicant_SOURCES = backend_sm.h config_ssid.h context.h eapol.h \
			eapol_key_type1.h eapol_key_type254.h \
			eapol_key_type2.h eap_sm.h error_prequeue.h \
			event_core.h event_core_win.h frame_structs.h \
			getopts.h hmac.h interfaces.h ipc_callout.h \
                        ipc_callout_helper.h \
			ipc_events_catalog.h ipc_events.h ipc_events_index.h \
			key_statemachine.h mic.h pmksa.h plugins.h profile.h psk.h \
			snmp.h statemachine.h stdintwin.h timer.h \
			wireless_sm.h wpa2.h wpa_common.h wpa.h xsup_common.h \
			xsup_debug.h xsup_err.h xsupgui_ipc_events_index.h \
			xsup_ipc.h logon_creds.h \
			\
			platform/linux/platform.c \
			\
			eap_types/eap_type_common.h \
			eap_types/md5/eapmd5.h eap_types/md5/md5.h \
			eap_types/mschapv2/eapmschapv2.h \
			eap_types/mschapv2/mschapv2.h \
			eap_types/psk/eappsk.h eap_types/psk/psk_crypt.h \
			eap_types/tls/eaptls.h eap_types/tls/tls_funcs.h \
			eap_types/tls/certificates.h \
			eap_types/ttls/eapttls.h eap_types/ttls/ttlsphase2.h \
			eap_types/ttls/phase2_common.h \
			eap_types/ttls/ttls_eap.h \
			eap_types/ttls/pap.h eap_types/ttls/chap.h \
			eap_types/ttls/mschap.h eap_types/ttls/p2mschapv2.h \
			eap_types/otp/eapotp.h eap_types/tnc/eaptnc.h \
			eap_types/peap/eappeap.h eap_types/peap/peap_phase2.h \
			eap_types/peap/peap_extensions.h \
			eap_types/leap/eapleap.h \
			eap_types/sim/eapsim.h eap_types/sim/fips.h \
			eap_types/sim/sha1.h eap_types/sim/sim.h \
			eap_types/sim/simd11.h eap_types/sim/simd5.h \
			eap_types/sim/sm_handler.h \
			eap_types/sim/sim_reader_plugin_hook.h \
			eap_types/aka/eapaka.h eap_types/aka/aka.h \
			eap_types/eap_type_common.h \
			eap_types/fast/eapfast.h eap_types/fast/eapfast_xml.h \
			eap_types/fast/eapfast_phase2.h \
			eap_types/tnc/tnc_compliance_funcs.h \
			eap_types/tnc/tnc_compliance_callbacks.h \
			eap_types/tnc/tnc_compliance_options.h \
			\
			xsup_driver.c xsup_debug.c context.c config_ssid.c \
			getopts.c eapol.c statemachine.c eap_sm.c snmp.c wpa.c \
			wireless_sm.c event_core.c xsup_common.c \
			key_statemachine.c eapol_key_type1.c wpa_common.c \
			eapol_key_type254.c mic.c pmksa.c psk.c backend_sm.c \
			eapol_key_type2.c wpa2.c timer.c interfaces.c \
			ipc_events.c error_prequeue.c plugins.c xsup_ipc.c \
			ipc_callout.c logon_creds.c \
                        ipc_callout_helper.c \
			\
			eap_types/md5/eapmd5.c eap_types/md5/md5.c \
			eap_types/mschapv2/eapmschapv2.c \
			eap_types/mschapv2/mschapv2.c \
			eap_types/psk/eappsk.c \
			eap_types/psk/psk_crypt.c \
			eap_types/tls/eaptls.c eap_types/tls/gnu_tls_funcs.c \
			eap_types/tls/ossl_tls_funcs.c \
			eap_types/tls/certificates.c \
			eap_types/ttls/eapttls.c eap_types/ttls/ttlsphase2.c \
			eap_types/ttls/phase2_common.c \
			eap_types/ttls/ttls_eap.c \
			eap_types/ttls/pap.c eap_types/ttls/chap.c \
			eap_types/ttls/mschap.c eap_types/ttls/p2mschapv2.c \
			eap_types/otp/eapotp.c eap_types/tnc/eaptnc.c \
			eap_types/peap/eappeap.c eap_types/peap/peap_phase2.c \
			eap_types/peap/peap_extensions.c \
			eap_types/leap/eapleap.c \
			eap_types/sim/eapsim.c eap_types/sim/fips.c \
			eap_types/sim/sha1.c eap_types/sim/sim.c \
			eap_types/sim/simd11.c eap_types/sim/simd5.c \
			eap_types/sim/sm_handler.c \
			eap_types/sim/sim_reader_plugin_hook.c \
			eap_types/aka/eapaka.c eap_types/aka/aka.c \
			eap_types/eap_type_common.c \
			eap_types/fast/eapfast.c eap_types/fast/eapfast_xml.c \
			eap_types/fast/eapfast_phase2.c \
			eap_types/tnc/tnc_compliance_funcs.c \
			eap_types/tnc/tnc_compliance_callbacks.c
			
xsupplicant_LDADD = platform/libplatform.a $(CORELIBS) $(MORELIBS) $(TNCLIB)

