bin_PROGRAMS = xfce4-screenshooter
	
xfce4_screenshooter_CFLAGS = \
	-I$(top_srcdir)/lib \
	@GTK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@LIBXFCE4UTIL_CFLAGS@ \
	@LIBXFCEGUI4_CFLAGS@ \
	@GTHREAD_CFLAGS@ \
	@SOUP_CFLAGS@ \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"
	
xfce4_screenshooter_LDFLAGS = \
	$(top_builddir)/lib/libscreenshooter.la \
	@GTK_LIBS@ \
	@GLIB_LIBS@ \
	@GTHREAD_LIBS@ \
	@SOUP_LIBS@ \
	@LIBXFCE4UTIL_LIBS@ \
	@LIBXFCEGUI4_LIBS@

	
xfce4_screenshooter_SOURCES	= \
	main.c		

desktopdir = $(datadir)/applications
desktop_in_in_files = xfce4-screenshooter.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

# get full path into .desktop file
xfce4-screenshooter.desktop.in: xfce4-screenshooter.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/applications^" 	\
		$< > $@
	
EXTRA_DIST =								\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)

