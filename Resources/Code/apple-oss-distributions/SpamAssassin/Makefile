#
# xbs-compatible wrapper Makefile for SpamAssassin
#

Project         = SpamAssassin
OPEN_SOURCE_VER = 3.3.2
PROJECT_VERSION = $(Project)-$(OPEN_SOURCE_VER)

# Configuration values we customize
OPEN_SOURCE_DIR = Mail-$(PROJECT_VERSION)
SPAM_TAR_GZ     = $(OPEN_SOURCE_DIR).tar.gz

PROJECT_BIN_DIR		= $(Project).Bin
PROJECT_CONF_DIR	= $(Project).Config
PROJECT_LD_DIR		= $(Project).LaunchDaemons
PROJECT_OS_DIR		= $(Project).OpenSourceInfo
PROJECT_PATCH_DIR	= $(Project).Patch
PROJECT_SETUP_DIR	= $(Project).SetupExtras

DATA_DIR		= /Library/Server/Mail/Data/scanner/spamassassin
CONFIG_DIR		= /Library/Server/Mail/Config/spamassassin
V310_PRE		= $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail/spamassassin/v310.pre
V310_PRE_TMP	= $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail/spamassassin/v310.pre.tmp

PROMO_DIR			= $(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/ServerSetup/PromotionExtras
SRC_PROMO_SCRIPT	= service_promotion.pl
DST_PROMO_SCRIPT	= 62-mail_services_filter.pl

HEADER_TEXT	   = header.txt
SA_LEARN_PATH  = $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/spamassassin/sa_learn.pl
SA_UPDATE_PATH = $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/spamassassin/sa_update.pl

CONFIG_ENV	= MAKEOBJDIR="$(BuildDirectory)" \
				INSTALL_ROOT="$(DSTROOT)" \
				TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"

CFLAGS		= -g -Os $(RC_CFLAGS)
LDFLAGS		= $(CFLAGS)

DSYMUTIL	= /usr/bin/dsymutil

# multi-version support
VERSIONER_DIR		:= /usr/local/versioner

# Perl multi-version support
PERL_VERSIONS		:= $(VERSIONER_DIR)/perl/versions
PERL_SUB_DEFAULT	:= $(shell sed -n '/^DEFAULT = /s///p' $(PERL_VERSIONS))
PERL_DEFAULT		:= $(shell grep '^$(PERL_SUB_DEFAULT)' $(PERL_VERSIONS))
PERL_UNORDERED_VERS := $(shell grep -v '^DEFAULT' $(PERL_VERSIONS))

# do default version last
PERL_BUILD_VERS		:= $(filter-out $(PERL_DEFAULT),$(PERL_UNORDERED_VERS)) $(PERL_DEFAULT)

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= MAKEOBJDIR="$(BuildDirectory)" \
				INSTALL_ROOT="$(DSTROOT)" \
				TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"

# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment =

Make_Flags	=

ProjectConfig		= $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/$(Project)-config

Common_Configure_Flags  = Makefile.PL PREFIX=$(SERVER_INSTALL_PATH_PREFIX)/ \
					LOCALSTATEDIR=/Library/Server/Mail/Data/scanner/spamassassin \
					INSTALLSITEDATA=/Library/Server/Mail/Data/scanner/spamassassin/3.003002 \
					INSTALLSITECONF=/Library/Server/Mail/Config/spamassassin \
					ENABLE_SSL=no

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

XCODEBUILD		= $(USRBINDIR)/xcodebuild

# Override settings from above includes
BUILD_DIR		= $(OBJROOT)/Build
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
TMPDIR			= $(OBJROOT)/Build/tmp

# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"

# Typically defined in GNUSource.make; duplicated here to effect similar functionality.
Sources				= $(SRCROOT)
Configure			= perl
ConfigureProject	= $(Configure)
ProjectConfigStamp	= $(BuildDirectory)/$(Project)/configure-stamp

LIB_TOOL			= $(BuildDirectory)/$(Project)/libtool

.PHONY: build-sa
.PHONY: archive-strip-binaries install-extras install-man install-startup-files
.PHONY: install-open-source-files

# Unbundling install paths
include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

default : clean build-sa

install :: build-sa install-extras install-startup-files \
			do-cleanup normalize-directories archive-strip-binaries

install-no-clean :: build-sa

###################

build-sa : $(BUILD_DIR) $(TMPDIR)
	@echo "***** Building $(Project)"
	$(_v) for vers in $(PERL_BUILD_VERS); do \
		export VERSIONER_PERL_VERSION=$${vers}; \
		cd $(BuildDirectory) && /usr/bin/tar -xzpf $(Sources)/$(PROJECT_BIN_DIR)/$(SPAM_TAR_GZ); \
		$(MV) $(BuildDirectory)/$(OPEN_SOURCE_DIR) $(BuildDirectory)/$(Project)-$${vers}; \
		cd "$(BuildDirectory)/$(Project)-$${vers}" && \
				$(PATCH) -p1 < "$(SRCROOT)/$(PROJECT_PATCH_DIR)/patch-1.diff"; \
		cd $(BuildDirectory)/$(Project)-$${vers} && \
				$(Environment) $(ConfigureProject) $(Common_Configure_Flags) \
					LIB=$(SERVER_INSTALL_PATH_PREFIX)/System/Library/Perl/Extras/$${vers} PERL_VERSION=$${vers}; \
		$(_v) $(MAKE) -C $(BuildDirectory)/$(Project)-$${vers} CFLAGS="$(CFLAGS)" $(Make_Flags) $(Install_Flags) $(Install_Target); \
	done
	@echo "***** Building $(Project) complete."

$(ProjectConfig): $(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config
	$(_v) $(CP) "$(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config" $@
$(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config:
	$(_v) $(MAKE) build-sa

# Custom configuration

do-cleanup :
	@echo "***** Cleaning up files not intended for installation"
	# remove installed local.cf file
	$(_v) $(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail/spamassassin/local.cf

	# cleanup perl directories
	$(_v) find $(DSTROOT) -name \*.bs -delete
	$(_v) find $(DSTROOT) -name perllocal.pod -delete
	$(_v) find $(DSTROOT) -type d -empty -delete
	$(_v) find $(DSTROOT) -name darwin-thread-multi-2level -delete
	$(_v) $(RMDIR) $(SYMROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)
	$(_v) $(RMDIR) $(SYMROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)
	@echo "***** Cleaning up complete."

archive-strip-binaries: $(SYMROOT)
	@echo "***** Archiving, dSYMing and stripping binaries..."
	$(_v) if [ -e "$(BUILD_DIR)/$(Project)-$(PERL_SUB_DEFAULT)/spamc/spamc" ]; then \
		$(CP) "$(BUILD_DIR)/$(Project)-$(PERL_SUB_DEFAULT)/spamc/spamc" "$(SYMROOT)/"; \
	fi
	$(_v) if [ -e "$(BUILD_DIR)/$(Project)-$(PERL_SUB_DEFAULT)/spamc/spamc.dSYM" ]; then \
		$(CP) "$(BUILD_DIR)/$(Project)-$(PERL_SUB_DEFAULT)/spamc/spamc.dSYM" "$(SYMROOT)/"; \
	fi
	$(_v) if [ -e "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)/spamc" ]; then \
		$(STRIP) -S "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)/spamc";	\
	fi
	@echo "***** Archiving, dSYMing and stripping binaries complete."

normalize-directories :
	@echo "***** Making standard directory paths..."
	# Create & merge into /private/etc
	$(_v) if [ ! -d "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)";		\
		$(MKDIR) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)/etc" -a "$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)" != "/etc" ]; then	\
		echo "$(MV) $(DSTROOT)/etc/* $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)";	\
		$(MV) "$(DSTROOT)/etc/"* "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)/etc";	\
		$(RMDIR) "$(DSTROOT)/etc";	\
	fi

	# Create & merge into /usr/bin
	$(_v) if [ ! -d "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)";		\
		$(MKDIR) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/bin" -a "$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)" != "/bin" ]; then	\
		echo "$(MV) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/bin/* $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)";	\
		$(MV) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/bin/"* "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/bin";	\
		$(RMDIR) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/bin";	\
	fi

	# Create & merge into /usr/share
	$(_v) if [ ! -d "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)" ]; then	\
		echo "**: $(MKDIR) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)";		\
		$(MKDIR) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/share" -a "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)" != "/share" ]; then	\
		echo "$(MV) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/share/* $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)";	\
		$(MV) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/share/"* "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/share";	\
		$(RMDIR) "$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/share";	\
	fi
	@echo "***** Making standard directory paths complete."

install-open-source-files :
	@echo "***** Installing open source configuration files..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceVersions
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceLicenses
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_OS_DIR)/$(Project).plist" "$(DSTROOT)$(USRDIR)/local/OpenSourceVersions"
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_OS_DIR)/$(Project).txt" "$(DSTROOT)$(USRDIR)/local/OpenSourceLicenses"
	@echo "***** Installing open source configuration files complete."

install-extras : install-open-source-files
	@echo "***** Installing extras..."
	# move promotion data
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail
	$(_v) $(MV) $(DSTROOT)$(CONFIG_DIR) \
		$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail/
	$(_v) $(MV) $(DSTROOT)$(DATA_DIR)/3.003002 \
		$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail/spamassassin/
	$(_v) $(RM) -rf $(DSTROOT)/Library

	# install directories
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/ServerSetup/PromotionExtras

	# Service configuration file
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(PROJECT_CONF_DIR)/local.cf.default \
			$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/mail/spamassassin/local.cf.default

	# Service setup script
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(PROMO_DIR)
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_SETUP_DIR)/$(SRC_PROMO_SCRIPT)" "$(DSTROOT)/$(PROMO_DIR)/$(DST_PROMO_SCRIPT)"
	$(_v) (/bin/chmod 755 "$(DSTROOT)/$(PROMO_DIR)/$(DST_PROMO_SCRIPT)")

	# Service runtime scripts
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/spamassassin
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_SETUP_DIR)/sa_learn.pl" "$(DSTROOT)/$(SA_LEARN_PATH)"
	$(_v) (/bin/chmod 755 "$(DSTROOT)/$(SA_LEARN_PATH)")

	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_SETUP_DIR)/sa_update.pl" "$(DSTROOT)/$(SA_UPDATE_PATH)"
	$(_v) (/bin/chmod 755 "$(DSTROOT)/$(SA_UPDATE_PATH)")

	$(_v) $(SED) -e 's/#loadplugin Mail::SpamAssassin::Plugin::TextCat/loadplugin Mail::SpamAssassin::Plugin::TextCat/' \
			"$(V310_PRE)" > "$(V310_PRE_TMP)"
	$(_v) $(RM) "$(V310_PRE)"
	$(_v) $(MV) "$(V310_PRE_TMP)" "$(V310_PRE)"
	@echo "***** Installing extras complete."

install-startup-files :
	@echo "***** Installing Startup Item..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/LaunchDaemons
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(PROJECT_LD_DIR)/com.apple.salearn.plist \
			$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/LaunchDaemons/com.apple.salearn.plist
	/usr/libexec/PlistBuddy -c 'Set :Program $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/spamassassin/sa_learn.pl' \
			"$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/LaunchDaemons/com.apple.salearn.plist"
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(PROJECT_LD_DIR)/com.apple.saupdate.plist \
			$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/LaunchDaemons/com.apple.saupdate.plist
	/usr/libexec/PlistBuddy -c 'Set :Program $(SERVER_INSTALL_PATH_PREFIX)/usr/libexec/spamassassin/sa_update.pl' \
			"$(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(NSLIBRARYDIR)/LaunchDaemons/com.apple.saupdate.plist"
	@echo "***** Installing Startup Item complete."

$(DSTROOT) $(TMPDIR) :
	$(_v) if [ ! -d $@ ]; then	\
		$(MKDIR) $@;	\
	fi

$(OBJROOT) $(BUILD_DIR) :
	$(_v) if [ ! -d $@ ]; then	\
		$(MKDIR) $@;	\
	fi
