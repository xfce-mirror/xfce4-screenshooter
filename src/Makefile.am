plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-screenshooter-plugin
bin_PROGRAMS = xfce4-screenshooter

xfce4_screenshooter_plugin_CFLAGS =						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBXFCE4PANEL_CFLAGS@ -Werror

xfce4_screenshooter_plugin_LDFLAGS =						\
	@LIBXFCE4PANEL_LIBS@

xfce4_screenshooter_plugin_SOURCES =						\
	screenshooter-plugin.c																\
	screenshooter-utils.c screenshooter-utils.h
	
xfce4_screenshooter_CFLAGS = \
	@GTK_CFLAGS@ \
	@GLIB_CFLAGS@ \
	@LIBXFCE4UTIL_CFLAGS@ \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" -Werror
	
xfce4_screenshooter_LDFLAGS =                       \
	@GTK_LIBS@ \
	@LIBXFCE4UTIL_LIBS@
	
xfce4_screenshooter_SOURCES	= \
	main.c	\
	screenshooter-utils.c screenshooter-utils.h					

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
# 
desktop_in_in_files = screenshooter.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
	
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" 	\
		$< > $@
	
EXTRA_DIST =								\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)

