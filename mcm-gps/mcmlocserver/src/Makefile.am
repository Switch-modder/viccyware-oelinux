AM_CFLAGS = \
            -I. \
            $(QMIF_CFLAGS) \
            $(LOCPLA_CFLAGS) \
            $(LOCHAL_CFLAGS) \
            $(GPSUTILS_CFLAGS) \
            $(LOCSERVICE_CFLAGS) \
            $(MCMCORE_CFLAGS) \
            $(MCMLOCTYPECONV_CFLAGS)

requiredlibs = $(QMIF_LIBS) \
               $(LOCPLA_LIBS) \
               $(LOCHAL_LIBS) \
               $(GPSUTILS_LIBS) \
               $(LOCSERVICE_LIBS) \
               $(MCMCORE_LIBS) \
               $(MCMLOCTYPECONV_LIBS)

h_sources = \
            loc_srv_utils.h \
            loc_srv.h \
            loc_srv_fwrk_proc_req_hdlrs.h \
            loc_srv_fwrk_requests.h \
            loc_srv_hal_indications.h \
            loc_srv_proc_req_hdlrs.h \
            loc_srv_send_indications.h \
            loc_srv_log.h
c_sources = \
            loc_srv.cpp \
            loc_srv_fwrk_proc_req_hdlrs.c \
            loc_srv_fwrk_requests.c \
            loc_srv_hal_indications.cpp \
            loc_srv_proc_req_hdlrs.cpp \
            loc_srv_send_indications.cpp

mcmlocserver_SOURCES = $(c_sources) $(h_sources)

if USE_GLIB
mcmlocserver_CFLAGS = -DUSE_GLIB $(AM_CFLAGS) @GLIB_CFLAGS@
mcmlocserver_LDFLAGS = -lstdc++ -lpthread @GLIB_LIBS@ -shared -version-info 1:0:0
mcmlocserver_CPPFLAGS = -DUSE_GLIB $(AM_CFLAGS) $(AM_CPPFLAGS) @GLIB_CFLAGS@
else
mcmlocserver_CFLAGS = $(AM_CFLAGS)
mcmlocserver_LDFLAGS = -shared -version-info 1:0:0
mcmlocserver_CPPFLAGS = $(AM_CFLAGS) $(AM_CPPFLAGS)
endif

init_ddir = $(sysconfdir)/init.d
init_d_SCRIPTS = mcmlocserverd

mcmlocserver_LDADD = $(requiredlibs)

bin_PROGRAMS = mcmlocserver
