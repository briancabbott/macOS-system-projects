# Makefile orchestrating CPAN

include $(SRCROOT)/$(VERS).inc
ifneq ($(wildcard $(SRCROOT)/Platforms/$(RC_TARGET_CONFIG)/$(VERS).inc),)
include $(SRCROOT)/Platforms/$(RC_TARGET_CONFIG)/$(VERS).inc
endif

# These variables cause installation into the Extras directory, adds RC_CFLAGS
# to the compile and linking arguments, and sets DESTDIR to DSTROOT
installarchlib := $(shell perl -MConfig -e 'print $$Config::Config{installextrasarch}')
installbin := $(shell perl -MConfig -e 'print $$Config::Config{installbin}')
installprivlib := $(shell perl -MConfig -e 'print $$Config::Config{installextraslib}')
EXTRASARCH := $(DSTROOT)/$(shell perl -MConfig -e 'print $$Config::Config{extrasarch}')
EXTRASLIB := $(DSTROOT)/$(shell perl -MConfig -e 'print $$Config::Config{extraslib}')
PLARGS := INSTALLDIRS=perl INSTALLARCHLIB='$(installarchlib)' INSTALLPRIVLIB='$(installprivlib)' INSTALLBIN='$(installbin)' INSTALLSCRIPT='$(installbin)'
PLBARGS := --installdirs core --install_path arch='$(installarchlib)' --install_path lib='$(installprivlib)' --install_path bin='$(installbin)' --install_path script='$(installbin)'
make := perl $(SRCROOT)/make.pl
MAKEARGS := DESTDIR=$(DSTROOT)
BUILDARGS := --destdir $(DSTROOT)
export PERL5LIB := $(EXTRASLIB)
export NO_PERL_PREPENDTOPATH := 1

# <rdar://problem/42682363> CPAN builds against ssl incorrectly
export OPENSSL_PREFIX := $(SDKROOT)/usr/local/libressl-3.3
export OPENSSL_BINDIR := /usr/bin

no_target:
	@set -x && for i in $(PROJECTS); do \
	    echo "===== $$i =====" && \
	    $(make) -C Modules/$$i unpack installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" && \
	    $(make) -C Modules/$$i installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done

install:
	@set -x && for i in $(PROJECTS); do \
	    echo "===== $$i =====" && \
	    $(make) -C Modules/$$i unpack installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" && \
	    $(make) -C Modules/$$i install installarchlib="$(installarchlib)" \
		installprivlib="$(installprivlib)" PLARGS="$(PLARGS)" \
		PLBARGS="$(PLBARGS)" BUILDARGS="$(BUILDARGS)"\
		MAKEARGS="$(MAKEARGS)" || exit 1; \
	done
	@echo ================ post-install fixups ================
	@set -x && \
	cd $(EXTRASARCH)/auto && \
	for b in `find . -name \*.bundle | sed 's,^\./,,'`; do \
	    rsync -R $$b $(SYMROOT) && \
	    dsymutil $(SYMROOT)/$$b && \
	    strip -x $$b || exit 1; \
	done
	@set -x && \
	cd $(DSTROOT) && \
	for b in usr/bin/*; do \
	    if file $$b | fgrep -q Mach-O; then \
		rsync -R $$b $(SYMROOT) && \
		dsymutil $(SYMROOT)/$$b && \
		strip -x $$b || exit 1; \
	    fi \
	done
	rm -f $(EXTRASARCH)/perllocal.pod
	find $(EXTRASARCH)/auto -name .packlist -print -delete
	rm -f $(DSTROOT)/usr/share/man/man3/Test\:\:Tester\:\:CaptureRunner.3pm
	rm -f $(DSTROOT)/usr/share/man/man3/Test\:\:Tester\:\:Capture.3pm
	rm -f $(DSTROOT)/usr/share/man/man3/Test\:\:use\:\:ok.3pm
	rm -f $(DSTROOT)/usr/share/man/man3/ok.3pm
	rm -f $(DSTROOT)/usr/share/man/man3/Test\:\:Tester.3pm
	rm -f $(DSTROOT)/usr/share/man/man3/Internals.3pm
