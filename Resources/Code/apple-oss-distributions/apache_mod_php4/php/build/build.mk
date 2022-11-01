#  +----------------------------------------------------------------------+
#  | PHP Version 4                                                        |
#  +----------------------------------------------------------------------+
#  | Copyright (c) 1997-2006 The PHP Group                                |
#  +----------------------------------------------------------------------+
#  | This source file is subject to version 3.01 of the PHP license,      |
#  | that is bundled with this package in the file LICENSE, and is        |
#  | available through the world-wide-web at the following url:           |
#  | http://www.php.net/license/3_01.txt                                  |
#  | If you did not receive a copy of the PHP license and are unable to   |
#  | obtain it through the world-wide-web, please send a note to          |
#  | license@php.net so we can mail you a copy immediately.               |
#  +----------------------------------------------------------------------+
#  | Author: Sascha Schumann <sascha@schumann.cx>                         |
#  +----------------------------------------------------------------------+
#
# $Id: build.mk,v 1.18.2.1.4.2 2006/01/01 13:46:49 sniper Exp $ 
#
#
# Makefile to generate build tools
#

ZENDDIR = Zend

SUBDIRS = $(ZENDDIR) TSRM

STAMP = buildmk.stamp

ALWAYS = generated_lists


all: $(STAMP) $(ALWAYS)
	@$(MAKE) -s -f build/build2.mk

generated_lists:
	@echo makefile_am_files = $(ZENDDIR)/Makefile.am \
		TSRM/Makefile.am > $@
	@echo config_h_files = $(ZENDDIR)/acconfig.h TSRM/acconfig.h >> $@
	@echo config_m4_files = $(ZENDDIR)/Zend.m4 TSRM/tsrm.m4 TSRM/threads.m4 \
		$(ZENDDIR)/acinclude.m4 ext/*/config*.m4 sapi/*/config.m4 >> $@

$(STAMP): build/buildcheck.sh
	@build/buildcheck.sh $(STAMP)

snapshot:
	distname='$(DISTNAME)'; \
	if test -z "$$distname"; then \
		distname='php4-snapshot'; \
	fi; \
	myname=`basename \`pwd\`` ; \
	cd .. && cp -rp $$myname $$distname; \
	cd $$distname; \
	rm -f $(SUBDIRS) 2>/dev/null || true; \
	for i in $(SUBDIRS); do \
		test -d $$i || (test -d ../$$i && cp -rp ../$$i $$i); \
	done; \
	find . -type l -exec rm {} \; ; \
	$(MAKE) -f build/build.mk; \
	cd ..; \
	tar cf $$distname.tar $$distname; \
	rm -rf $$distname $$distname.tar.*; \
	bzip2 -9 $$distname.tar; \
	md5sum $$distname.tar.bz2; \
	sync; sleep 2; \
	md5sum $$distname.tar.bz2; \
	bzip2 -t $$distname.tar.bz2

cvsclean-work:
	@for i in `find . -name .cvsignore`; do \
		(cd `dirname $$i` 2>/dev/null && rm -rf `cat .cvsignore | grep -v config.nice` *.o *.a .libs || true); \
	done

.PHONY: $(ALWAYS) snapshot cvsclean
