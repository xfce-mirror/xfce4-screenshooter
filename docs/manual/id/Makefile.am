# Copied from Thunar, original author: Benedict Meurer

SUBDIRS = 					\
	images

noinst_DATA = \
	xfce4-screenshooter.xml

docdir=$(datadir)/xfce4/doc/id

doc_DATA= \
	xfce4-screenshooter.html

EXTRA_DIST = $(noinst_DATA) $(doc_DATA)

if MAINTAINER_MODE
if ENABLE_XSLTPROC

xfce4-screenshooter.html: $(srcdir)/xfce4-screenshooter.xml
	$(XSLTPROC) $(top_srcdir)/docs/manual/xfce-nochunk.xsl $(srcdir)/xfce4-screenshooter.xml

endif
endif
