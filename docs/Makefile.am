# -*- Mode: Makefile -*-
#
# Makefile.am - docs automake file for Redland
#
# $Id$
#
# Copyright (C) 2000-2004 David Beckett - http://purl.org/net/dajobe/
# Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
# University of Bristol - http://www.bristol.ac.uk/
# 
# This package is Free Software or Open Source available under the
# following licenses (these are alternatives):
#   1. GNU Lesser General Public License (LGPL)
#   2. GNU General Public License (GPL)
#   3. Mozilla Public License (MPL)
# 
# See LICENSE.html or LICENSE.txt at the top of this package for the
# full license terms.
# 
# 

EXTRA_DIST=api.sgml.in api.sgml README.html README \
build-docs api \
kernel-doc

DOC_CLASSES=general literal expr query

DOC_C_FILES=$(DOC_CLASSES:%=$(srcdir)/../rasqal%.c)
API_DOCBOOK_FILES=$(DOC_CLASSES:%=%.sgml)
API_HTML_FILES=$(DOC_CLASSES:%=api_%.html)

CLEANFILES=README
MAINTAINERCLEANFILES=api.sgml api $(API_DOCBOOK_FILES) \
$(API_HTML_FILES)

PERL=perl

# Create some text files from HTML sources
LYNX=lynx
LYNXARGS=-dump -nolist

SUFFIXES = .html .txt .sgml .ps .pdf .pm .pod .3

api: api/index.html

# Generates a tree of HTML files, not 1 file
api/index.html: api.sgml
	rm -rf api.junk DBTOHTML*
	db2html $<
	rm -rf api.junk

.html.txt:
	$(LYNX) $(LYNXARGS) $< > $@

# These don't work for me
.sgml.ps:
	db2ps $<

.sgml.pdf:
	db2pdf $<

if MAINTAINER_MODE
$(API_DOCBOOK_FILES): %.sgml: $(srcdir)/../rasqal_%.c $(srcdir)/kernel-doc
	$(PERL) $(srcdir)/kernel-doc -docbook $< > $@

api.sgml: $(srcdir)/build-docs api.sgml.in $(API_DOCBOOK_FILES)
	$(PERL) $(srcdir)/build-docs api.sgml.in > api.sgml

$(API_HTML_FILES): api_%.html: $(srcdir)/../rasqal_%.c $(srcdir)/kernel-doc
	$(PERL) $(srcdir)/kernel-doc -html $< > $@

api.html: $(srcdir)/build-docs api.html.in $(API_HTML_FILES)
	$(PERL) $(srcdir)/build-docs api.html.in > api.html

clean-local:
	rm -rf api.junk DBTOHTML* pod2htm*

distclean-local:
	rm -rf $(API_DOCBOOK_FILES)

else
clean-local:
	rm -rf $(API_DOCBOOK_FILES) api.junk DBTOHTML* pod2htm*
endif

README: README.html
	$(LYNX) $(LYNXARGS) $< > $@

@SET_MAKE@
