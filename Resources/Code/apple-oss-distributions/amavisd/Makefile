#
# xbs-compatible wrapper Makefile for SpamAssassin
#

Project=amavisd

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=
CFLAGS=-Os $(RC_CFLAGS)

# Configuration values we customize
#

PROJECT_VERS	= 2.8.0
PROJECT_PATH	= amavisd-new-

PROJECT_BIN_DIR		= $(Project).Bin
PROJECT_MAN_DIR		= $(Project).Man
PROJECT_CONF_DIR	= $(Project).Conf
PROJECT_SETUP_DIR	= $(Project).SetupExtras
PROJECT_LD_DIR		= $(Project).LaunchDaemons
PROJECT_OS_DIR		= $(Project).OpenSourceInfo
PROJECT_PATCH_DIR	= $(Project).Patches
PROJECT_EXTRAS_DIR	= $(Project).InstallExtras

USR_BIN			= $(SERVER_INSTALL_PATH_PREFIX)/usr/bin
ETC_DIR			= $(SERVER_INSTALL_PATH_PREFIX)/private/etc
AMAVIS_DIR		= $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/amavisd
LAUNCHD_DIR		= $(SERVER_INSTALL_PATH_PREFIX)/System/Library/LaunchDaemons
MAN8_DST_DIR	= $(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man8
PROMO_DIR		= $(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/PromotionExtras

LANGUAGES_DIR	= $(ETC_DIR)/mail/amavisd/languages/en.lproj
USR_OS_VERSION	= /usr/local/OpenSourceVersions
USR_OS_LICENSE	= /usr/local/OpenSourceLicenses

SRC_PROMO_SCRIPT	= service_promotion.pl
DST_PROMO_SCRIPT	= 61-mail_services_interface.pl

CHOWN	= /usr/sbin/chown

# These includes provide the proper paths to system utilities
#

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

default:: make_amavisd

install :: make_amavisd_install

installhdrs :
	$(SILENT) $(ECHO) "No headers to install"

installsrc :
	[ ! -d $(SRCROOT)/$(Project) ] && mkdir -p $(SRCROOT)/$(Project)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

make_amavisd_install : $(DSTROOT)$(ETC_DIR) $(DSTROOT)$(USR_BIN)
	$(SILENT) $(ECHO) "-------------- Amavisd-new --------------"

	# install launchd plist
	install -d -m 0755 "$(DSTROOT)$(LAUNCHD_DIR)"
	install -m 0644 "$(SRCROOT)/$(PROJECT_LD_DIR)/org.amavis.amavisd.plist" "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd.plist"
	/usr/libexec/PlistBuddy -c 'Set :Program $(SERVER_INSTALL_PATH_PREFIX)/usr/bin/amavisd' "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd.plist"
	/usr/libexec/PlistBuddy -c 'Set :ProgramArguments:0 $(SERVER_INSTALL_PATH_PREFIX)/usr/bin/amavisd' "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd.plist"
	install -m 0644 "$(SRCROOT)/$(PROJECT_LD_DIR)/org.amavis.amavisd_cleanup.plist" "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd_cleanup.plist"
	/usr/libexec/PlistBuddy -c 'Set :Program $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/amavisd/amavisd_cleanup' "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd_cleanup.plist"
	/usr/libexec/PlistBuddy -c 'Set :ProgramArguments:0 $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/amavisd/amavisd_cleanup' "$(DSTROOT)/$(LAUNCHD_DIR)/org.amavis.amavisd_cleanup.plist"

	# apply patch
	$(SILENT) ($(CD) "$(SRCROOT)/$(Project)/$(PROJECT_PATH)$(PROJECT_VERS)" && /usr/bin/patch -p1 < "$(SRCROOT)/$(PROJECT_PATCH_DIR)/apple-mods.diff")

	# install amavis config and scripts
	install -m 0644 "$(SRCROOT)/$(PROJECT_CONF_DIR)/amavisd.conf" "$(DSTROOT)/$(ETC_DIR)/amavisd.conf.default"
	install -m 0755 "$(SRCROOT)/$(Project)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd" "$(DSTROOT)/$(USR_BIN)/amavisd"
	install -m 0755 "$(SRCROOT)/$(Project)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd-agent" "$(DSTROOT)/$(USR_BIN)/amavisd-agent"
	install -m 0755 "$(SRCROOT)/$(Project)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd-nanny" "$(DSTROOT)/$(USR_BIN)/amavisd-nanny"
	install -m 0755 "$(SRCROOT)/$(Project)/$(PROJECT_PATH)$(PROJECT_VERS)/amavisd-release" "$(DSTROOT)/$(USR_BIN)/amavisd-release"

	# install amavis  directories
	install -d -m 0750 "$(DSTROOT)$(AMAVIS_DIR)"
	install -d -m 0755 "$(DSTROOT)$(LANGUAGES_DIR)"

	# install default language files
	install -m 0644 "$(SRCROOT)/$(PROJECT_EXTRAS_DIR)/languages/en.lproj/"* "$(DSTROOT)/$(LANGUAGES_DIR)/"

	# setup & migration extras
	install -d -m 0755 "$(DSTROOT)$(PROMO_DIR)"
	install -m 0755 "$(SRCROOT)/$(PROJECT_SETUP_DIR)/$(SRC_PROMO_SCRIPT)" "$(DSTROOT)/$(PROMO_DIR)/$(DST_PROMO_SCRIPT)"

	install -o _amavisd -m 0755 "$(SRCROOT)$)/$(PROJECT_SETUP_DIR)/amavisd_cleanup" "$(DSTROOT)/$(AMAVIS_DIR)/amavisd_cleanup"
	$(SILENT) ($(CHOWN) -R root:wheel "$(DSTROOT)$(AMAVIS_DIR)")

	install -d -m 0755 "$(DSTROOT)$(USR_OS_VERSION)"
	install -d -m 0755 "$(DSTROOT)$(USR_OS_LICENSE)"
	install -m 0755 "$(SRCROOT)/$(PROJECT_OS_DIR)/amavisd.plist" "$(DSTROOT)/$(USR_OS_VERSION)/amavisd.plist"
	install -m 0755 "$(SRCROOT)/$(PROJECT_OS_DIR)/amavisd.txt" "$(DSTROOT)/$(USR_OS_LICENSE)/amavisd.txt"

	install -d -m 0755 "$(DSTROOT)$(MAN8_DST_DIR)"
	install -m 0444 "$(SRCROOT)/$(PROJECT_MAN_DIR)/amavisd.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd.8"
	install -m 0444 "$(SRCROOT)/$(PROJECT_MAN_DIR)/amavisd-agent.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd-agent.8"
	install -m 0444 "$(SRCROOT)/$(PROJECT_MAN_DIR)/amavisd-nanny.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd-nanny.8"
	install -m 0444 "$(SRCROOT)/$(PROJECT_MAN_DIR)/amavisd-release.8" "$(DSTROOT)$(MAN8_DST_DIR)/amavisd-release.8"

	$(SILENT) $(ECHO) "---- Building Amavisd-new complete."

.PHONY: clean installhdrs installsrc build install 


$(DSTROOT) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(ETC_DIR) :
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(USR_BIN) :
	$(SILENT) $(MKDIRS) $@
