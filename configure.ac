dnl configure.ac
dnl
dnl xfce4-test-plugin - Test plugin for the XFce4 panel
dnl

m4_define([test_version], [1.0.0])

AC_INIT([xfce4-screenshooter-plugin], [test_version()],
	[dbobadil@dcc.uchile.cl])


TEST_VERSION=test_version()
AM_INIT_AUTOMAKE([xfce4-screenshooter-plugin], [$TEST_VERSION])
AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

dnl Check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_INTLTOOL
AC_PROG_LIBTOOL

dnl Check for standard header files
AC_HEADER_STDC

dnl configure the panel plugin
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-1.0], [4.3.22])

dnl Translations
XDT_I18N([cs de fr hu pl])

dnl Check for debugging support
XDT_FEATURE_DEBUG()

AC_OUTPUT([
Makefile
panel-plugin/Makefile
po/Makefile.in
])

