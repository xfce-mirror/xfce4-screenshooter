plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-screenshooter-plugin

xfce4_screenshooter_plugin_CFLAGS =	\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"	\
	-I$(top_srcdir)	\
	-I$(top_srcdir)/lib	\
	@LIBXFCE4PANEL_CFLAGS@ \
	@GTHREAD_CFLAGS@ \
	@SOUP_CFLAGS@

xfce4_screenshooter_plugin_LDFLAGS = \
	$(top_builddir)/lib/libscreenshooter.la \
	@LIBXFCE4PANEL_LIBS@ \
	@GTHREAD_LIBS@ \
	@SOUP_LIBS@

xfce4_screenshooter_plugin_SOURCES = \
	screenshooter-plugin.c
	
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
screenshooter.desktop.in: screenshooter.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" 	\
		$< > $@

EXTRA_DIST =								\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)
