##
# Makefile for emacs
##

Extra_CC_Flags = -Wno-pointer-sign
Extra_LD_Flags = -Wl,-headerpad,0x1000 -Wl,-no_pie -Wl,-no_function_starts
Extra_Configure_Flags = --without-x --without-carbon ac_cv_host=mac-apple-darwin ac_cv_func_posix_memalign=no

# Project info
Project  = emacs
GNUVersion = 22.1
UserType = Developer
ToolType = Commands
#CommonNoInstallSource = YES
GnuAfterInstall = remove-dir install-dumpemacs cleanup install-plist install-default install-dsyms
GnuNoBuild = YES
# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
DSYMUTIL?=dsymutil
RSYNC?=rsync
Environment+= SHELL="$(SRCROOT)"/pipefail.sh
NJOBS=$(shell sysctl -n hw.activecpu)

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = $(GNUVersion)
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Apple.diff files.el.diff \
	CVE-2007-6109.diff darwin.h.diff vcdiff.diff lread.c.diff \
	fast-lock.el.diff python.el.diff src_Makefile.in.diff \
	lisp_Makefile.in.diff xdisp.c.diff lib-src_update-game-score.c.diff \
	src_fileio.c.diff src_search.c.diff lib-src_etags.c.diff \
	src_regex.c.diff \
	src_eval.c.25331628.diff

# Extract the source.
install_source::
	$(TAR) --exclude "*.elc" --exclude info -C "$(SRCROOT)" -zxf "$(SRCROOT)/$(AEP_Filename)"
	$(RMDIR) "$(SRCROOT)/$(AEP_Project)"
	$(MV) "$(SRCROOT)/$(AEP_ExtractDir)" "$(SRCROOT)/$(AEP_Project)"
	$(CHMOD) -R go-w "$(SRCROOT)/$(Project)"
	for patchfile in $(AEP_Patches); do \
		cd "$(SRCROOT)/$(Project)" && patch -lp0 -F0 < "$(SRCROOT)/patches/$$patchfile" || exit 1; \
	done
	$(INSTALL_FILE) "$(SRCROOT)/mac.h" "$(SRCROOT)/$(AEP_Project)/src/m"
	$(INSTALL_FILE) "$(SRCROOT)/unexmacosx.c" "$(SRCROOT)/$(AEP_Project)/src"
	for f in $(EXTRAEL); do $(RM) "$(SRCROOT)/$$f"; done

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

EXTRAEL=	emacs/lisp/cus-load.el \
		emacs/lisp/eshell/esh-groups.el \
		emacs/lisp/finder-inf.el \
		emacs/lisp/loaddefs.el \
		emacs/lisp/mh-e/mh-loaddefs.el \
		emacs/lisp/subdirs.el

#installsrc :
#	if test ! -d $(SRCROOT) ; then mkdir -p $(SRCROOT); fi;
# --include to workaround .cvsignores
#	rsync -aC --include '*.ps' --include configure --include Resources --include index.texi ./ $(SRCROOT)/

remove-dir :
	rm $(DSTROOT)/usr/share/info/dir

$(OBJROOT)/bo.h:
	printf "char bo[] = {\n" > $@
	hexdump -ve '1/1 "0x%02x,"' < $(OBJROOT)/src/buildobj.lst >> $@
	printf "};\n" >> $@

$(OBJROOT)/runit.o: $(SRCROOT)/runit.c
	$(CC) -c $(CFLAGS) -o $@ $^ 

$(OBJROOT)/dumpemacs.o: $(OBJROOT)/bo.h $(OBJROOT)/src/version.h
	$(CC) -I $(OBJROOT) -DkEmacsVersion=$(patsubst %,'"%"', $(GNUVersion)) $(CFLAGS) -o $@ -g -c $(SRCROOT)/dumpemacs.c

$(SYMROOT)/dumpemacs: $(OBJROOT)/dumpemacs.o $(OBJROOT)/runit.o
	$(CC) $(CFLAGS) -o $@ -g $^

install-dumpemacs: $(SYMROOT)/dumpemacs
	$(INSTALL) -s -o root -g wheel -m 555 $(SYMROOT)/dumpemacs $(DSTROOT)/usr/libexec/dumpemacs
	$(DSYMUTIL) $(SYMROOT)/dumpemacs
	$(INSTALL) -s -o root -g wheel -m 555 $(SYMROOT)/emacs $(DSTROOT)/usr/bin/emacs
	$(INSTALL) -d "$(DSTROOT)"/usr/share/man/man{1,8}
	$(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/dumpemacs.8 "$(DSTROOT)"/usr/share/man/man8
	$(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/emacs-undumped.1 "$(DSTROOT)"/usr/share/man/man1

build::
	find "$(OBJROOT)" -name Makefile -exec $(SRCROOT)/fixMakefile {} \;
	@echo "Bootstraping $(Project)..."
	$(MKDIR) $(OBJROOT)/src/arch
ifeq (ppc,$(filter ppc,$(RC_ARCHS)))
	$(CC) $(CFLAGS) -c -o $(OBJROOT)/emacs-ppc.o $(SRCROOT)/emacs-ppc.c
	$(CC) $(CFLAGS) -o $(OBJROOT)/src/arch/emacs-ppc $(OBJROOT)/emacs-ppc.o
	lipo $(OBJROOT)/src/arch/emacs-ppc -extract_family ppc -output $(OBJROOT)/src/arch/emacs-ppc
endif
	ruby version.rb emacs > $(OBJROOT)/src/version.h
	$(_v) $(MAKE) -j $(NJOBS) -C $(BuildDirectory) $(Environment) bootstrap
	$(CP) $(OBJROOT)/src/emacs $(SYMROOT)
	$(DSYMUTIL) $(SYMROOT)/emacs # Use current objs
	@echo "Change default LoadPath for installed emacs-undumped"
	$(MV) $(OBJROOT)/src/lread.o $(OBJROOT)/src/lread.o+save
	$(MV) $(OBJROOT)/src/temacs $(OBJROOT)/src/temacs+save
	$(RM) $(DSTROOT)/usr/bin/emacs-undumped
	$(_v) $(MAKE) -C $(BuildDirectory)/src $(Environment) MYCPPFLAGS=-DEMACS_UNDUMPED temacs
	$(INSTALL_FILE) $(OBJROOT)/src/temacs $(OBJROOT)/src/emacs-undumped
	$(INSTALL_FILE) $(OBJROOT)/src/emacs-undumped $(SYMROOT)
	$(DSYMUTIL) $(SYMROOT)/emacs-undumped
# Don't rebuild anything else
	$(MV) -f $(OBJROOT)/src/lread.o+save $(OBJROOT)/src/lread.o
	$(MV) -f $(OBJROOT)/src/temacs+save $(OBJROOT)/src/temacs

cleanup:			# Return sources to pristine state
	@echo "Cleaning $(Project)..."
	find $(SRCROOT) -type f -name '*.el[c~]' -delete
	$(RM) -r $(SRCROOT)/emacs/info
	$(RM) "$(DSTROOT)/usr/bin/ctags" "$(DSTROOT)/usr/share/man/man1/ctags.1"
	$(RM) "$(DSTROOT)/usr/bin/"{b2m,ebrowse,grep-changelog,rcs-checkin}
	$(RM) -r "$(DSTROOT)/usr/var"
	for f in $(EXTRAEL); do $(RM) -f "$(SRCROOT)/$$f"; done

install-plist:
	$(INSTALL) -d $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(INSTALL) -d $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

install-default:
	$(INSTALL) -o root -g wheel -m 644 default.el "$(DSTROOT)"/usr/share/emacs/site-lisp
	$(INSTALL) -o root -g wheel -m 644 site-start.el "$(DSTROOT)"/usr/share/emacs/site-lisp

install-dsyms:
	for f in emacsclient etags cvtmail digest-doc fakemail hexl movemail profile sorted-doc update-game-score; do \
		$(RSYNC) -ia $(OBJROOT)/lib-src/$$f $(SYMROOT)/; \
		$(DSYMUTIL) $(SYMROOT)/$$f; \
	done

