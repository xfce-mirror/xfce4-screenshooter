noinst_LTLIBRARIES = 							\
	libscreenshooter.la

libscreenshooter_la_SOURCES =							\
	libscreenshooter.h \
	screenshooter-utils.c screenshooter-utils.h \
	screenshooter-dialogs.c screenshooter-dialogs.h

libscreenshooter_la_CFLAGS = \
	-I$(top_srcdir)	\
  @GTK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@LIBXFCE4UTIL_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

libscreenshooter_la_LIBADD = \
  @GTK_LIBS@ \
	@LIBXFCE4UTIL_LIBS@ \
	@LIBXFCEGUI4_LIBS@
	
if HAVE_GIO
  
libscreenshooter_la_CFLAGS += \
  @GIO_CFLAGS@
  
libscreenshooter_la_LIBADD += \
  @GIO_LIBS@
  
endif
