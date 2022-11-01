##
# Makefile for CVS
##

# Project info
Project           = cvs
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = post-install install-plist copy-strip

Extra_Configure_Flags = --with-gssapi \
                        --enable-pam \
                        --enable-encryption \
                        --with-external-zlib \
                        --enable-case-sensitivity \
                        --with-editor=vim \
                        ac_cv_func_working_mktime=no

Extra_CC_Flags = -D_DARWIN_NO_64_BIT_INODE

ifndef DEVELOPER_DIR
DEVELOPER_DIR = /Developer
endif
Install_Prefix = $(DEVELOPER_DIR)/usr/local
Install_Man = $(DEVELOPER_DIR)/usr/local/share/man

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	ed - $(OBJROOT)/config.h < $(SRCROOT)/patches/config.h.ed
	echo TESTDIR=/private/tmp/cvs-sanity >> $(OBJROOT)/src/sanity.config.sh
	echo TMPDIR=/private/tmp/cvs-sanity/tmp >> $(OBJROOT)/src/sanity.config.sh
	touch $(ConfigStamp2)

Install_Target = install

post-install:
	$(INSTALL_FILE) $(Sources)/contrib/rcs2log.1 $(DSTROOT)$(Install_Man)/man1/

copy-strip:
	$(CP) $(DSTROOT)$(Install_Prefix)/bin/cvs $(SYMROOT)
	$(STRIP) -x $(DSTROOT)$(Install_Prefix)/bin/cvs

OSV = $(DSTROOT)$(DEVELOPER_DIR)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)$(DEVELOPER_DIR)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP_Project    = $(Project)
AEP_Version    = 1.12.13
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = ea.diff \
                 tag.diff \
                 nopic.diff \
                 i18n.diff \
                 endian.diff \
                 zlib.diff \
                 PR5178707.diff \
                 fixtest-recase.diff \
                 fixtest-client-20.diff \
                 initgroups.diff \
                 remove-libcrypto.diff \
                 remove-info.diff

# Extract the source.
install_source::
	$(TAR) -C $(SRCROOT) -jxf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	@set -x && \
	cd $(SRCROOT)/$(Project) && \
	for patchfile in $(AEP_Patches); do \
		patch -p0 -F0 -i $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
