noinst_LTLIBRARIES = 							\
	libscreenshooter.la

libscreenshooter_la_SOURCES =							\
	libscreenshooter.h \
	screenshooter-global.h \
	screenshooter-utils.c screenshooter-utils.h \
  sexy-url-label.c sexy-url-label.h \
	screenshooter-capture.c screenshooter-capture.h \
	screenshooter-dialogs.c screenshooter-dialogs.h \
	screenshooter-actions.c screenshooter-actions.h \
	screenshooter-zimagez.c screenshooter-zimagez.h

libscreenshooter_la_CFLAGS = \
	-I$(top_srcdir)	\
	$(XMLRPC_CFLAGS) \
  @GTK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@LIBXFCE4UTIL_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
  -DPACKAGE_LOCALE_DIR=\"$(localedir)\"
	
libscreenshooter_la_LIBADD = \
	$(XMLRPC_LIBS) -lxmlrpc_client\
  @GTK_LIBS@ \
	@LIBXFCE4UTIL_LIBS@ \
	@LIBXFCEGUI4_LIBS@ \
  @GLIB_LIBS@
