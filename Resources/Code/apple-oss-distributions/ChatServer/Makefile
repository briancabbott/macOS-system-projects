#
# Copyright (c) 2009 Apple, Inc.
#
# Wrapper Makefile for ChatServer
#
# ChatServer consists of two main packages: jabberd2 and libidn.  The libidn
# package in built and installed into a temporary location prior to configuring
# jabberd2.  There is no install target for libidn -- only jabberd2 installs 
# any files in the system.
#

# These includes provide the proper paths to system utilities
include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

# Include common server build variables
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

#
# Special variables for make targets
#
# The temporary libidn folder structure is created as follows:
#    $(OBJROOT)/
#        libidn.build/
#            src/
#                ppc/
#                i386/
#                ...
#            lib/
#                ppc/
#                i386/ 
#                ...
#            include/
# NOTE: Since the value of $(OBJROOT) varies depending on whether ~rc/bin/buildit
#       is invoked or make from the ChatServer2 directory, the location of the 
#       libidn temp directories will vary accordinly.  As additional architectures
#       are added to the Makefile, additional sub-folders will be created under 
#       src/ and lib/ as needed.
#
PROJECT_NAME	= ChatServer

# STAGING DIR
#
# As of the current release, jabberd2 and certain other package sources are build
# into a "staging" directory, which contains all the build products, but is not
# organized in the final intallation structure.  The contents of STAGING_DIR
# will be copied in the install phase (see below) to create the structure that
# will exists when Mac OS X Server is intalled.
STAGING_DIR 	:= staging

# common root directories with absolute paths
SQLITE_LIB_DIR	= /usr/lib
SQLITE_INCLUDE_DIR	= /usr/include
BUILD_DIR	= /usr
JABBERD_MODULES_DIR = $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/jabberd/modules

# common directories with relative paths (may be contained inside build directories)
ETC_DIR	= private/etc
VAR_DIR=private/var
USRBIN_DIR=usr/bin
USRLIB_DIR=usr/lib
USRLOCAL_DIR=usr/local
USRSHARE_DIR=usr/share
LIBEXEC_DIR=usr/libexec
SYSCONFIG_DIR	= etc
MAN_DIR	= usr/share/man
INFO_DIR	= usr/share/info
MIGRATION_EXTRAS_DIR	= System/Library/ServerSetup/MigrationExtras
PROMOTION_EXTRAS_DIR	= System/Library/ServerSetup/PromotionExtras
COMMON_EXTRAS_DIR	= System/Library/ServerSetup/CommonExtras
RESTORE_EXTRAS_DIR	= System/Library/ServerSetup/RestoreExtras
JABBERD_BIN_DIR=$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/jabberd

# project directories and tools used while building, relative to the source root
TOOLS_DIR=tools
OPENSOURCE_PKG_DIR=opensource_pkgs
JABBERD_PATCH_TOOL=$(TOOLS_DIR)/patch_jabberd.pl
JABBERD_DIST_TAR=$(OPENSOURCE_PKG_DIR)/jabberd-2.2.17.tar.gz
PATCH_DIR=apple_patch
PATCH_DIR_PRECONF=$(PATCH_DIR)/pre_configure
PATCH_DIR_POSTCONF=$(PATCH_DIR)/post_configure

# tool path defines
INSTALL		= /usr/bin/install
DITTO		= /usr/bin/ditto
TAR		= /usr/bin/tar
GZIP		= /usr/bin/gzip
MKDIR		= /bin/mkdir
PATCH		=/usr/bin/patch
GCC		=/usr/bin/gcc
LS		=/bin/ls
APPLELIBTOOL		=/usr/bin/libtool
FIND		=/usr/bin/find


#-------------------------------
# Define the default build rule
#-------------------------------
#
default: build


#-------------------------------
# LIBIDN MODULE
#-------------------------------
#
# NOTE: There is no install target for libidn.  It's make output is copied 
#       to $(OBJROOT)/$(LIBIDN_BUILD_DIR) so that the jabberd2 target 
#       statically links against the library during the build, 
#       but the intermediate build results are discarded after the build completes.
#
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(LIBIDN_NAME)/
#          make sources (ppc): $(OBJROOT)/$(LIBIDN_SRC_DIR)/ppc/
#          make sources (i386): $(OBJROOT)/$(LIBIDN_SRC_DIR)/i386/
#       Build Products:
#          libraries (ppc): $(OBJROOT)/$(LIBIDN_LIB_DIR)/ppc/
#          libraries (i386): $(OBJROOT)/$(LIBIDN_LIB_DIR)/i386/
#          libraries (universal): $(OBJROOT)/$(LIBIDN_LIB_DIR)/
#          includes (universal): $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)/
#
LIBIDN_VERSION	= libidn-0.6.14
LIBIDN_NAME	= libidn
LIBIDN_A	= libidn.a
LIBIDN_LA	= libidn.la
LIBIDN_DYLIB	=	libidn.dylib

LIBIDN_BUILD_DIR	:= libidn.build
LIBIDN_SRC_DIR	:= $(LIBIDN_BUILD_DIR)/src
LIBIDN_LIB_DIR	:= $(LIBIDN_BUILD_DIR)/lib
LIBIDN_INCLUDE_DIR	:= $(LIBIDN_BUILD_DIR)/include

LIBIDN_ARCHS	:= $(ARCHS)

# libidn build rules
#

.PHONY: libidn/untar libidn/build libidn/build-lib libidn/clean libidn/clean-all

$(LIBIDN_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(LIBIDN_SRC_DIR); $(MKDIR) -p -m 755 $(OBJROOT)/$(LIBIDN_SRC_DIR)
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR); $(TAR) -xzf $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/$(LIBIDN_VERSION).tgz; $(MV) $(LIBIDN_VERSION) $$i; \
	done

libidn/untar: $(LIBIDN_NAME)

$(LIBIDN_NAME)/config.status: libidn/untar
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: CONFIGURING"
	@echo "#"
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i; env VALGRIND="/usr/bin/false" ./configure --disable-valgrind-tests --disable-shared --prefix=$(BUILD_DIR) --build=$$i-apple-darwin --host=$$i-apple-darwin CFLAGS="$(CFLAGS) -arch $$i"; \
	done

$(LIBIDN_NAME)/lib/.libs/libidn.dylib:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: BUILDING"
	@echo "#"
	for i in $(LIBIDN_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i; make; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: INSTALLING"
	@echo "#"
	for i in $(LIBIDN_ARCHS) ; do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...installing - caching make results: libs; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)/$$i; \
		$(SILENT) $(DITTO) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$$i/lib/.libs/ $(OBJROOT)/$(LIBIDN_LIB_DIR)/$$i/; \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...installing - caching make results: includes"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)
	$(SILENT) $(DITTO) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$(firstword $(LIBIDN_ARCHS))/lib/*.h $(OBJROOT)/$(LIBIDN_INCLUDE_DIR)/
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: ...installing - creating univeral library"
	$(SILENT) $(LIPO) -create \
		$(foreach arch, $(LIBIDN_ARCHS), -arch $(arch) $(OBJROOT)/$(LIBIDN_LIB_DIR)/$(arch)/$(LIBIDN_A) ) \
		-output $(OBJROOT)/$(LIBIDN_LIB_DIR)/$(LIBIDN_A)
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(LIBIDN_LIB_DIR)/$(LIBIDN_A)
	$(SILENT) $(DITTO) $(OBJROOT)/$(LIBIDN_SRC_DIR)/$(firstword $(LIBIDN_ARCHS))/lib/$(LIBIDN_LA) $(OBJROOT)/$(LIBIDN_LIB_DIR)/

libidn/build: $(LIBIDN_NAME)/config.status $(LIBIDN_NAME)/lib/.libs/libidn.dylib

libidn/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: cleaning sources"
	@echo "#"
	$(SILENT) $(RM) -rf $(LIBIDN_VERSION) $(LIBIDN_NAME) $(OBJROOT)/$(LIBIDN_BUILD_DIR)
	$(SILENT) $(CD) $(SRCROOT); $(SILENT) $(TAR) -xzf $(OPENSOURCE_PKG_DIR)/$(LIBIDN_VERSION).tgz; $(SILENT) $(MV) $(LIBIDN_VERSION) $(LIBIDN_NAME)

libidn/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [libidn]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(LIBIDN_BUILD_DIR)


#-------------------------------
# UDNS LIBRARY
#-------------------------------
#
# NOTE: jabberd 2.2 requires the UDNS library for asynchronous DNS  
#       resolution.
#       There is no install target for udns.  It's make output is copied 
#       to $(OBJROOT)/$(UDNS_BUILD_DIR)/install so that the jabberd target can 
#       use the binary during the build, but the intermediate build results are 
#       discarded after the build completes.
# DIRECTORIES:
#       Sources:
#          tar output: $(SRCROOT)/$(UDNS_NAME)/
#          make sources (arch): $(OBJROOT)/$(UDNS_SRC_DIR)/(arch)/
#       Build Products:
#          /usr/lib/* (arch): $(OBJROOT)/$(UDNS_SRC_DIR)/(arch)/$(UDNS_A)
#          /usr/lib/* (universal): $(OBJROOT)/$(UDNS_BUILD_DIR)/$(UDNS_A)

UDNS_VERSION	= udns-0.2
UDNS_NAME_ORIG	= udns-0.2
UDNS_NAME	= udns
UDNS_A	=	libudns.a
UDNS_H	=	udns.h
UDNS_BUILD_DIR	:= udns.build
UDNS_SRC_DIR	:= $(UDNS_BUILD_DIR)/src
UDNS_LIB_DIR	:= $(UDNS_BUILD_DIR)/lib
UDNS_INCLUDE_DIR	:= $(UDNS_BUILD_DIR)/include

# UDNS_LIB_FILES_TO_LIPO  List of all thin libraries that need to be lipo'd 
#                         into universal libraries.
UDNS_LIB_FILES_TO_LIPO	= UDNS_A

UDNS_ARCHS := $(ARCHS)

# udns build rules
#
.PHONY: udns/untar udns/config.status udns/build

$(UDNS_NAME): $(OBJROOT)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: UNPACKING"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(UDNS_SRC_DIR)
	for i in $(UDNS_ARCHS); do \
		$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(UDNS_SRC_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(UDNS_SRC_DIR)/$$i; $(TAR) -xzf $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/$(UDNS_VERSION).tar.gz; $(MV) $(UDNS_NAME_ORIG) $(UDNS_NAME); $(CHOWN) -R $(USER):$(GROUP) $(UDNS_NAME); \
	done

udns/untar: $(UDNS_NAME)

udns/config.status: udns/untar
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: PATCHING"
	@echo "#"
	for i in $(UDNS_ARCHS); do \
	$(PATCH) $(OBJROOT)/$(UDNS_SRC_DIR)/x86_64/$(UDNS_NAME)/configure.lib < $(SRCROOT)/$(PATCH_DIR_PRECONF)/$(UDNS_NAME)/configure.lib.patch; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: CONFIGURING"
	@echo "#"
	for i in $(UDNS_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(UDNS_SRC_DIR)/$$i/$(UDNS_NAME); ./configure --enable-ipv6; \
	done

udns/udns:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: BUILDING"
	@echo "#"
	for i in $(UDNS_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: ...building; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(OBJROOT)/$(UDNS_SRC_DIR)/$$i/$(UDNS_NAME); \
		make CFLAGS="-arch $$i" LDFLAGS="-arch $$i"; \
	done
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: INSTALLING"
	@echo "#"
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: ...installing - creating univeral library"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(UDNS_LIB_DIR)
	$(SILENT) $(LIPO) -create \
		$(foreach arch, $(UDNS_ARCHS), -arch $(arch) $(OBJROOT)/$(UDNS_SRC_DIR)/$(arch)/$(UDNS_NAME)/$(UDNS_A) ) \
		-output $(OBJROOT)/$(UDNS_LIB_DIR)/$(UDNS_A); \
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(UDNS_LIB_DIR)/$(UDNS_A)
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [udns]: ...installing - installing header files"
	@echo "#"
	$(SILENT) $(MKDIR) -p -m 777  $(OBJROOT)/$(UDNS_INCLUDE_DIR)
	$(SILENT) $(DITTO) $(OBJROOT)/$(UDNS_SRC_DIR)/$(firstword $(LIBIDN_ARCHS))/$(UDNS_NAME)/$(UDNS_H) $(OBJROOT)/$(UDNS_INCLUDE_DIR)/

udns/build: udns/config.status udns/udns

#-------------------------------
# JABBER_OD_AUTH MODULE
#-------------------------------
#
# NOTE: There is no install target for jabber_od_auth.  It's make output is built 
#       into $(OBJROOT)/$(STAGING_DIR)/$(ODAUTH_LIB_DIR) so that the jabber target can 
#       use this library during it's own build phase, then discarded after the 
#       build is finished.
#
# DIRECTORIES:
#       Sources:
#          make sources (universal): $(SRCROOT)/$(ODAUTH_SRC_DIR)/
#       Build Products:
#          objects (ppc):        $(OBJROOT)/$(ODAUTH_BUILD_DIR)/ppc/
#          objects (i386):       $(OBJROOT)/$(ODAUTH_BUILD_DIR)/i386/
#          library (ppc):        $(OBJROOT)/$(ODAUTH_BUILD_DIR)/ppc/
#          library (i386):       $(OBJROOT)/$(ODAUTH_BUILD_DIR)/i386/
#          library (universal):  $(OBJROOT)/$(ODAUTH_LIB_DIR)/
#          includes (universal): $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
#

ODAUTH_SRC_DIR	:= jabber_od_auth
ODAUTH_BUILD_DIR	:= jabber_od_auth.build
ODAUTH_LIB_DIR	:= $(ODAUTH_BUILD_DIR)/lib
ODAUTH_INCLUDE_DIR	:= $(ODAUTH_BUILD_DIR)/include

OD_AUTH_ARCHS := $(ARCHS)

#
# od_auth/build rules
#   od_auth/build       : builds the jabber_od_auth library
#
.PHONY: od_auth/build

od_auth/build:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabber_od_auth]: BUILDING; DEST=$(OBJROOT)/$(ODAUTH_BUILD_DIR)"
	@echo "#"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(ODAUTH_LIB_DIR)
	$(SILENT) $(LN) -sf $(PROJECT_DIR)/$(ODAUTH_SRC_DIR)/apple_authenticate.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	$(SILENT) $(LN) -sf $(PROJECT_DIR)/$(ODAUTH_SRC_DIR)/apple_authorize.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	$(SILENT) $(LN) -sf $(PROJECT_DIR)/$(ODAUTH_SRC_DIR)/sasl_switch_hit.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	$(SILENT) $(LN) -sf $(PROJECT_DIR)/$(ODAUTH_SRC_DIR)/odkerb.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	$(SILENT) $(LN) -sf $(PROJECT_DIR)/$(ODAUTH_SRC_DIR)/auth_event.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	$(SILENT) $(LN) -sf $(PROJECT_DIR)/$(ODAUTH_SRC_DIR)/odckit.h $(OBJROOT)/$(ODAUTH_INCLUDE_DIR)/
	# use best version available
	if [ -f $(OBJROOT)/UninstalledProducts/libxmppodauth.a ]; then \
	    $(SILENT) $(LN) -sf $(OBJROOT)/UninstalledProducts/libxmppodauth.a $(OBJROOT)/$(ODAUTH_LIB_DIR)/ ;\
	else \
	    $(SILENT) $(LN) -sf $(OBJROOT)/$(CONFIGURATION)/libxmppodauth.a $(OBJROOT)/$(ODAUTH_LIB_DIR)/ ;\
	fi
	$(SILENT) $(LIPO) -info $(OBJROOT)/$(ODAUTH_LIB_DIR)/libxmppodauth.a

od_auth/clean:
	$(SILENT) $(RM) -rf $(OBJROOT)/$(ODAUTH_BUILD_DIR)


#-------------------------------
# JABBERD2 MODULE
#-------------------------------
#
JABBERD2_VERSION	= jabberd-2.2.17
JABBERD2_SAVE	= jabberd2-save
JABBERD2_NAME	= jabberd2

.PHONY: jabberd2/jabberd2 jabberd2/makeinstall jabberd2/build jabberd2/clean jabberd2/clean-all \
	jabberd2/extract_and_patch jabberd2/post-configure_patch

JABBERD2_BUILD_DIR 	:= jabberd2.build
JABBERD2_SRC_DIR	:= $(JABBERD2_BUILD_DIR)/src
JABBERD2_INSTALL_DIR	:= $(JABBERD2_BUILD_DIR)/install

JABBERD2_ARCHS 	:= $(ARCHS)

JABBERD2_CONFIG_OPTS= \
		--infodir=/$(INFO_DIR) \
		--mandir=/$(MAN_DIR) \
		--sysconfdir=/$(SYSCONFIG_DIR)/jabberd \
		--bindir=$(JABBERD_BIN_DIR) \
		--libdir=$(JABBERD_MODULES_DIR) \
		--disable-mysql \
		--enable-apple \
		--enable-sqlite \
		--enable-mio=kqueue \
		--enable-debug \
		--enable-developer \
		--enable-tests=no \
		--without-subst \
		--with-sasl=cyrus \
		--with-extra-include-path=$(OBJROOT)/$(LIBIDN_INCLUDE_DIR):$(SASL_INCLUDE_DIR):$(SQLITE_INCLUDE_DIR):$(OBJROOT)/$(ODAUTH_INCLUDE_DIR):$(OBJROOT)/$(UDNS_INCLUDE_DIR) \
		--with-extra-library-path=$(OBJROOT)/$(LIBIDN_LIB_DIR):$(SQLITE_LIB_DIR):$(OBJROOT)/$(ODAUTH_LIB_DIR):$(OBJROOT)/$(UDNS_LIB_DIR) \
                CFLAGS="$(CFLAGS) -D HAVE_MEMMOVE -arch $$i" \
                LIBS="-L$(OBJROOT)/$(ODAUTH_LIB_DIR) -lobjc -lldap -lxmppodauth -framework OpenDirectory -framework DirectoryService -framework CoreFoundation -F$(SERVER_INSTALL_PATH_PREFIX)/System/Library/PrivateFrameworks -F/System/Library/PrivateFrameworks -framework CoreSymbolication -framework ServerFoundation -framework Foundation -framework CoreDaemon"

# JABBERD2_EXEC_FILES_TO_LIPO  List of all thin binaries that need to be lipo'd 
#                         into universal binaries for the jabberd2 component.
JABBERD2_EXEC_FILES_TO_LIPO	= c2s router s2s sm
# JABBERD2_LIB_FILES_TO_LIPO  List of all thin jabberd library files to lipo into universal.  This list depends on how jabberd2 is configured.
#   Jabberd accesses the lib files via symlinks; we just ditto the symlinks from one of the thin build dirs.
JABBERD2_LIB_FILES_TO_LIPO = \
authreg_apple_od.so \
mod_active.0.so \
mod_amp.0.so \
mod_announce.0.so \
mod_autobuddy.0.so \
mod_deliver.0.so \
mod_disco.0.so \
mod_echo.0.so \
mod_help.0.so \
mod_iq-last.0.so \
mod_iq-ping.0.so \
mod_iq-private.0.so \
mod_iq-time.0.so \
mod_iq-vcard.0.so \
mod_iq-version.0.so \
mod_offline.0.so \
mod_pep.0.so \
mod_presence.0.so \
mod_privacy.0.so \
mod_roster-publish.0.so \
mod_roster.0.so \
mod_session.0.so \
mod_status.0.so \
mod_template-roster.0.so \
mod_vacation.0.so \
mod_validate.0.so \
storage_sqlite.so

# jabberd2 build rules
#
#	jabberd2/extract_and_patch		: Expands the jabberd2 source tarball and applies Apple (pre-configure) patches
#   jabberd2/config.status  		: runs the configure script for the jabberd2 sources
#	jabberd2/post-configure_patch	: Applies Apple post-configure patches to the jabberd2 source
#   jabberd2/jabberd2          		: Patches and configures the jabberd2 source to prepare for makeinstall
#   jabberd2/makeinstall    		: creates installed file and directoried in an intermediate directory
#   jabberd2/build         		 	: wrapper rule for jabberd2 and makeinstall
#

$(JABBERD2_NAME)/extract_and_patch:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: EXTRACT AND PATCH; SRCROOT=$(SRCROOT)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...extracting; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...extracting - SRCROOT=$(SRCROOT)"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...extracting - creating $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i"; \
		$(SILENT) $(MKDIR) -p -m 777 $(OBJROOT)/$(JABBERD2_SRC_DIR); \
		$(SILENT) $(SRCROOT)/$(JABBERD_PATCH_TOOL) -a -f $(SRCROOT)/$(JABBERD_DIST_TAR) -p $(SRCROOT)/$(PATCH_DIR_PRECONF)/$(JABBERD2_NAME) -n $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i -t $(OBJROOT)/$(JABBERD2_BUILD_DIR); \
	done

$(JABBERD2_NAME)/config.status:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: CONFIGURING; SRCROOT=$(SRCROOT)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring; arch=$$i"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - SRCROOT=$(SRCROOT)"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - creating $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; $(RM) -rf ac-stdint.h; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...configuring - creating final sources; arch=$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; \
		$(SILENT) $(CP) $(SRCROOT)/config_cache/jabberd2/$$i/config.cache $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i/config.cache; \
		./configure --prefix=$(BUILD_DIR) --build=$$i-apple-darwin --host=$$i-apple-darwin --config-cache $(JABBERD2_CONFIG_OPTS); \
	done

$(JABBERD2_NAME)/post-configure_patch:

jabberd2/jabberd2: $(JABBERD2_NAME)/extract_and_patch $(JABBERD2_NAME)/config.status $(JABBERD2_NAME)/post-configure_patch
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: BUILDING; SRCROOT=$(SRCROOT)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...building; arch=$$i; src=$(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i"; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; \
		make; \
	done

jabberd2/makeinstall:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: INSTALLING; DESTDIR=$(OBJROOT)/$(JABBERD2_BUILD_DIR)"
	@echo "#"
	for i in $(JABBERD2_ARCHS); do \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing; arch=$$i"; \
		$(SILENT) $(MKDIR) -p -m 777 $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$$i; \
		$(SILENT) $(CD) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$$i; make install DESTDIR=$(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$$i; \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing - creating universal binaries"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)
	$(SILENT) $(MKDIRS) $(SYMROOT)/$(JABBERD_BIN_DIR)
	for i in $(JABBERD2_EXEC_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			$(foreach arch, $(JABBERD2_ARCHS), -arch $(arch) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(arch)/$(JABBERD_BIN_DIR)/$$i )  \
			-output $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)/$$i; \
		$(SILENT) $(CP) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)/$$i $(SYMROOT)/$(JABBERD_BIN_DIR)/; \
		$(SILENT) $(STRIP) -S $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)/$$i; \
	done
	$(SILENT) $(INSTALL) -m 755 -o root -g wheel $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(JABBERD_BIN_DIR)/jabberd $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_BIN_DIR)
	$(SILENT) $(MKDIRS) $(SYMROOT)/$(JABBERD_MODULES_DIR)
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)
# we don't want to ship .la files
	$(SILENT) $(RM) -f $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(JABBERD_MODULES_DIR)/*.la
# authreg_sqlite gets built, but we don't want it.
	$(SILENT) $(RM) -f $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(JABBERD_MODULES_DIR)/authreg_sqlite.so
# Copy symlinks for jabberd2 modules
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(JABBERD_MODULES_DIR) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)
#  This is not working, but it would be nice to make the list of shared libs dynamic based on what jabberd2 builds.
#	JABBERD2_LIB_FILES_TO_LIPO = $(shell $(FIND) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(JABBERD_MODULES_DIR) -name "*.so" -type f|awk -F "/" '{print $NF}')
	for i in $(JABBERD2_LIB_FILES_TO_LIPO); do \
		$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)/$$i; \
		$(SILENT) $(LIPO) -create \
			$(foreach arch, $(JABBERD2_ARCHS), -arch $(arch) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(arch)/$(JABBERD_MODULES_DIR)/$$i ) \
			-output $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)/$$i; \
		$(SILENT) $(CP) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)/$$i $(SYMROOT)/$(JABBERD_MODULES_DIR)/; \
		$(SILENT) $(STRIP) -S $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)/$$i > /dev/null 2>&1; \
		$(SILENT) $(LIPO) -info $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR)/$$i; \
	done
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing version and license info"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceVersions
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/ChatServer.plist $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceVersions
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/ChatServer_jabberd.txt $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/ChatServer_proxy65.txt $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/ChatServer_libidn.txt $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/$(OPENSOURCE_PKG_DIR)/ChatServer_udns.txt $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(USRLOCAL_DIR)/OpenSourceLicenses

	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: ...installing - copying man pages for executables"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(MAN_DIR)
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(MAN_DIR) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(MAN_DIR)

jabberd2/build: jabberd2/jabberd2 jabberd2/makeinstall

# jabberd2/clean rules
#   jabberd2/clean     : erase all except pre-defined jabberd2 sources (DEFAULT)
#   jabberd2/clean-all : wipe the jabberd2 directory for later restore from archive

jabberd2/clean:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: cleaning OBJECTS"
	@echo "#"
	$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_SRC_DIR)

jabberd2/clean-all:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [jabberd2]: cleaning ALL"
	@echo "#"
	$(SILENT) $(RM) -rf $(JABBERD2_NAME) $(OBJROOT)/$(JABBERD2_SRC_DIR)

#-------------------------------
# TOP-LEVEL MAKE RULES
#-------------------------------
#

BACKUPRESTORE_SRC_DIR=backup_restore
MIGRATION_SRC_DIR=migration
PROMOTION_SRC_DIR=common_extras
RESTORE_SRC_DIR=restore_extras
INITIALIZATION_SRC_DIR=initialization

# FILES_TO_INSTALL  Used by the installsrc target.
#                   WARNING! Make sure to update this will ALL project sources 
#                   (files AND folders), or the B&I build may fail!!!

FILES_TO_INSTALL		=  \
Makefile $(TOOLS_DIR) $(OPENSOURCE_PKG_DIR)\
apple_patch cfg-apple config_cache \
$(AUTOBUDDY_SRC_DIR) $(BACKUPRESTORE_SRC_DIR) $(ODAUTH_SRC_DIR) \
$(MIGRATION_SRC_DIR) \
$(PROMOTION_SRC_DIR) \
$(JABBERD2_NAME)

#.PHONY: all build configure install untar clean clean-all installhdrs installsrc \
#	install/intro_banner install/jabber_usr_dir install/autobuddy \
#	install/custom_configs install/sbsbackup install/jabber_var_dirs \
#	install/file_proxy install/migration install/promotion install/restore_extras install/initialization install/copy_dstroot
.PHONY: all build configure install untar clean clean-all installhdrs installsrc \
	install/intro_banner install/jabber_usr_dir \
	install/runtime_scripts install/man_pages \
	install/custom_configs install/sbsbackup install/jabber_var_dirs \
	install/file_proxy install/migration install/promotion install/restore_extras install/initialization install/copy_dstroot

all: build

build: od_auth/build libidn/build udns/build jabberd2/build

configure: $(LIBIDN_NAME)/config.status libidn/build $(JABBERD2_NAME)/config.status

#
# Create the final install image for DSTROOT
#
# The install target takes all the intermediate build products from jabberd2 and the other 
# components and creates STAGING_DIR, which is used to intall all the build products into
# their final directories and perform any final patches.  Once STAGING_DIR is fully
# processed, it is then copied to DSTROOT as the final set of files to be installed in the OS.
#
# - Executables are lipo'd from each installed architecture and combined into a single universal 
#   binary in the common staging directory.
# - Non-executables generated by the configure script are installed from the ppc build into the 
#   common staging directory.
# - Other files are copied from the common source directory into the common staging directory.
# - Miscellaneous patch are applied here before final installation in DSTROOT.
#

JABBER_VAR_DIR=$(VAR_DIR)/jabberd

#
# Install sub-targets
#

install/intro_banner:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: COPYING; STAGING_DIR=$(OBJROOT)/$(STAGING_DIR)"
	@echo "#"
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(STAGING_DIR)/$(USRBIN_DIR)

install/jabber_usr_dir:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying jabber binaries"
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/usr $(OBJROOT)/$(STAGING_DIR)/usr
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/Applications $(OBJROOT)/$(STAGING_DIR)/Applications

install/runtime_scripts:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying runtime scripts"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/server_backup
	$(SILENT) $(CP) $(SRCROOT)/$(BACKUPRESTORE_SRC_DIR)/MessageServer_* $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/server_backup/
	$(SILENT) $(MKDIR) -p -m 700 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/tmp
	
install/man_pages:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying additional man pages"
	$(SILENT) $(MKDIR) -p -m 755  $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_config_migrator.pl.8 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/
	$(SILENT) $(CHMOD) 644 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/jabber_config_migrator.pl.8
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_data_migrate_2.0-2.2.pl.8 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/
	$(SILENT) $(CHMOD) 644 $(OBJROOT)/$(STAGING_DIR)/$(MAN_DIR)/man8/jabber_data_migrate_2.0-2.2.pl.8

install/custom_configs:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying Apple configuration files"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)
	$(SILENT) $(CP) -r $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(SYSCONFIG_DIR)/jabberd $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/
	$(SILENT) $(RM) -rf $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(firstword $(JABBERD2_ARCHS))/$(SYSCONFIG_DIR)/jabberd
	$(SILENT) $(RM) -rf $(OBJROOT)/$(STAGING_DIR)/$(SYSCONFIG_DIR)
	$(SILENT) $(CP) $(SRCROOT)/cfg-apple/*.xml $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/
	$(SILENT) $(CP) $(SRCROOT)/cfg-apple/jabberd.cfg $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/
	$(SILENT) $(CP) $(SRCROOT)/cfg-apple/*.dist $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/
	$(SILENT) $(CHOWN) _jabber:wheel $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/*.xml
	$(SILENT) $(CHOWN) _jabber:wheel $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/jabberd.cfg
	$(SILENT) $(CHOWN) _jabber:wheel $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/*.dist
	$(SILENT) $(CHMOD) 600 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/*.xml
	$(SILENT) $(CHMOD) 600 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/jabberd.cfg
	$(SILENT) $(CHMOD) 600 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/jabberd/*.dist

install/sbsbackup:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying SBS Backup files"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/server_backup
	$(SILENT) $(CP) $(SRCROOT)/$(BACKUPRESTORE_SRC_DIR)/75-MessageServer.plist $(OBJROOT)/$(STAGING_DIR)/$(ETC_DIR)/server_backup/

install/migration:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying migration tools"
	$(SILENT) $(MKDIR) -p -m 700 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/tmp
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(MIGRATION_EXTRAS_DIR)
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/58_jabbermigrator.pl $(OBJROOT)/$(STAGING_DIR)/$(MIGRATION_EXTRAS_DIR)/
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_config_migrator.pl $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/
	$(SILENT) $(CP) $(SRCROOT)/$(MIGRATION_SRC_DIR)/jabber_data_migrate_2.0-2.2.pl $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/

install/promotion:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying CommonExtras"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(COMMON_EXTRAS_DIR)
	$(SILENT) $(CP) $(SRCROOT)/$(PROMOTION_SRC_DIR)/70-message_server_common_extra.pl $(OBJROOT)/$(STAGING_DIR)/$(COMMON_EXTRAS_DIR)/

install/restore_extras:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying RestoreExtras"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(RESTORE_EXTRAS_DIR)
	$(SILENT) $(CP) $(SRCROOT)/$(RESTORE_SRC_DIR)/75_MessageServerRestoreExtra.pl $(OBJROOT)/$(STAGING_DIR)/$(RESTORE_EXTRAS_DIR)/

install/initialization:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying initialization script"
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)
	$(SILENT) $(CP) $(SRCROOT)/$(INITIALIZATION_SRC_DIR)/copy_message_server_config_files.sh $(OBJROOT)/$(STAGING_DIR)/$(LIBEXEC_DIR)/

install/jabber_var_dirs:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying - creating Jabberd /var directories"
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)
	$(SILENT) $(CP) $(OBJROOT)/$(JABBERD2_SRC_DIR)/$(firstword $(JABBERD2_ARCHS))/tools/db-setup.sqlite $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(JABBERD_MODULES_DIR)
	$(SILENT) $(DITTO) $(OBJROOT)/$(JABBERD2_INSTALL_DIR)/$(JABBERD_MODULES_DIR) $(OBJROOT)/$(STAGING_DIR)/$(JABBERD_MODULES_DIR)
	$(SILENT) $(CHOWN) -R root:wheel $(OBJROOT)/$(STAGING_DIR)/$(JABBERD_MODULES_DIR)
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/muc_room_logs
	$(SILENT) $(CHOWN) _jabber:_jabber $(OBJROOT)/$(STAGING_DIR)/$(JABBER_VAR_DIR)/muc_room_logs

install/file_proxy:
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying File Proxy files"
	$(SILENT) $(MKDIR) -p -m 750 $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/log
	$(SILENT) $(MKDIR) -p -m 755 $(OBJROOT)/$(STAGING_DIR)/$(USRSHARE_DIR)/proxy65
	$(SILENT) $(CP) $(SRCROOT)/modules/filetransfer/proxy65.py $(OBJROOT)/$(STAGING_DIR)/$(USRSHARE_DIR)/proxy65
	$(SILENT) $(CHMOD) 644 $(OBJROOT)/$(STAGING_DIR)/$(USRSHARE_DIR)/proxy65/proxy65.py
	$(SILENT) $(CP) $(SRCROOT)/modules/filetransfer/socks5.py $(OBJROOT)/$(STAGING_DIR)/$(USRSHARE_DIR)/proxy65
	$(SILENT) $(CHMOD) 644 $(OBJROOT)/$(STAGING_DIR)/$(USRSHARE_DIR)/proxy65/socks5.py
	$(SILENT) $(CHOWN) -R root:wheel $(OBJROOT)/$(STAGING_DIR)/$(USRSHARE_DIR)/proxy65

install/copy_dstroot:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...copying staged content to DSTROOT"
	@echo "#"
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/private/etc $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc
	$(SILENT) $(MKDIR) -p -m 750 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/var/jabberd
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/db-setup.sqlite $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/var/jabberd/db-setup.sqlite
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/log $(DSTROOT)/private/var/jabberd/log
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/muc_room_logs $(DSTROOT)/private/var/jabberd/muc_room_logs
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/private/var/jabberd/tmp $(DSTROOT)/private/var/jabberd/tmp
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/usr/local $(DSTROOT)/usr/local
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/usr/libexec $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/Applications $(DSTROOT)/Applications
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/usr/bin $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/System $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System
	$(SILENT) $(DITTO) $(OBJROOT)/$(STAGING_DIR)/usr/share $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: [staging]: ...fixing DESTROOT ownership & permissions"
	$(SILENT) $(CHOWN) _jabber:_jabber $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/var/jabberd
	$(SILENT) $(CHOWN) _jabber:_jabber $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/var/jabberd/db-setup.sqlite
	$(SILENT) $(CHOWN) _jabber:_jabber $(DSTROOT)/private/var/jabberd
	$(SILENT) $(CHOWN) -R _jabber:_jabber $(DSTROOT)/private/var/jabberd/log
	$(SILENT) $(CHOWN) -R _jabber:_jabber $(DSTROOT)/private/var/jabberd/tmp
	$(SILENT) $(CHOWN) -R root:wheel $(DSTROOT)/$(JABBERD_MODULES_DIR)
	$(SILENT) $(CHOWN) root:wheel $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/$(MAN_DIR)/man8/*

install/end_banner:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: BUILD COMPLETE."
	@echo "#"
	@echo "### The latest information about Jabber 2.0 is available on the web"
	@echo "### at http://jabberd.jabberstudio.org/2/.  Use Server Preferences"
	@echo "### or Server Admin app to set up and start Message Server."

#
# MAIN INSTALL TARGET
#
# Executes all sub-targets and displays the completion banner
#
#install: build \
#		install/intro_banner install/jabber_usr_dir install/autobuddy \
#		install/custom_configs install/sbsbackup install/jabber_var_dirs \
#		install/file_proxy install/migration install/promotion install/restore_extras install/initialization \
#		install/copy_dstroot install/end_banner
install: build \
		install/intro_banner install/jabber_usr_dir \
		install/runtime_scripts install/man_pages \
		install/custom_configs install/sbsbackup install/jabber_var_dirs \
		install/file_proxy install/migration install/promotion install/restore_extras install/initialization \
		install/copy_dstroot install/end_banner

untar: libidn/untar jabberd2/untar

clean: libidn/clean jabberd2/clean

clean-all: libidn/clean-all jabberd2/clean-all

installhdrs:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: No headers to install in $(SRCROOT)"
	@echo "#"

# installsrc target -- make sure that all source files to be installed 
#                      are included in the FILES_TO_INSTALL list
installsrc:
	@echo "#"
	@echo "# `date +%Y/%m/%d\ %H:%M:%S` ChatServer: Installing sources into $(SRCROOT)"
	@echo "#"
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) -r $(FILES_TO_INSTALL) $(SRCROOT)
#	$(SILENT) install-src-run=1

#
# Rules for special build directories
#
#
$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@
