##
# Makefile for Apple Release Control (common)
#
# Wilfredo Sanchez | wsanchez@apple.com
# Copyright (c) 1997-1999 Apple Computer, Inc.
#
# @APPLE_LICENSE_HEADER_START@
# 
# Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.1 (the "License").  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
# 
# The Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
# Set these variables as needed, then include this file, then:
#
#  Project           [ UNTITLED_PROJECT ]
#  ProjectName       [ $(Project)       ]
#  SubProjects
#  Extra_Environment
#  Passed_Targets
#
# Additional variables inherited from Standard/Standard.make
##

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/Standard/Standard.make

##
# Some reasonable defaults for RC variables
##

RC_ARCHS   = $(shell for i in `file /usr/lib/libSystem.B.dylib | grep 'shared library ' | sed 's|.*shared library ||'`; do $(CC) -arch $$i -E -x c /dev/null > /dev/null 2>&1 && echo $$i; done)
RC_RELEASE = unknown
RC_VERSION = unknown

SRCROOT = /tmp/$(ProjectName)/Sources
OBJROOT = /tmp/$(ProjectName)/Build
SYMROOT = /tmp/$(ProjectName)/Debug
DSTROOT = /tmp/$(ProjectName)/Release

##
# My variables
##

ifndef Project
Project = UNTITLED_PROJECT
endif

ifndef ProjectName
ProjectName = $(Project)
endif

ifneq ($(RC_VERSION),unknown)
Version = RC_VERSION
else
Version = $(RC_ProjectSourceVersion)
ifeq ($(Version),)
Version = 0
endif
endif

Sources        = $(SRCROOT)
Platforms      = $(patsubst %,%-apple-rhapsody$(RhapsodyVersion),$(RC_ARCHS:ppc=powerpc))
BuildDirectory = $(OBJROOT)

CC_Archs      = $(RC_ARCHS:%=-arch %)
#CPP_Defines += -DPROJECT_VERSION=\"$(Project)-$(Version)\"

Environment += CC="$(CC)"
Environment += CXX="$(CXX)"

ifneq "$(strip $(CFLAGS))" ""
Environment += CFLAGS="$(CFLAGS)"
endif
ifneq "$(strip $(CXXFLAGS))" ""
Environment += CCFLAGS="$(CXXFLAGS)" CXXFLAGS="$(CXXFLAGS)"
endif
ifneq "$(strip $(LDFLAGS))" ""
Environment += LDFLAGS="$(LDFLAGS)"
endif
ifneq "$(strip $(CPPFLAGS))" ""
Environment += CPPFLAGS="$(CPPFLAGS)"
endif
Environment += $(Extra_Environment)

VPATH=$(Sources)

ManPageDirectories = /usr/share/man

##
# Targets
##

.PHONY: all install installhdrs install_headers lazy_installsrc lazy_install_source installsrc install_source build clean recurse

all: build

$(DSTROOT): install

install:: install_headers build

# For RC
installhdrs:: install_headers

install_headers::

lazy_install_source::
	$(_v) if [ ! -f "$(SRCROOT)/Makefile" ]; then $(MAKE) install_source; fi

install_source::
ifneq ($(CommonNoInstallSource),YES)
	@echo "Installing source for $(Project)..."
	$(_v) $(MKDIR) "$(SRCROOT)"
	$(_v) $(TAR) -cp $(Tar_Cruft) . | $(TAR) -pox -C "$(SRCROOT)"
endif

ifndef ShadowTestFile
ShadowTestFile = $(BuildDirectory)/Makefile
endif

shadow_source:: $(ShadowTestFile)

$(ShadowTestFile):
	echo "Creating pseudo-copy of sources in the build directory...";
	$(_v) mkdir -p $(BuildDirectory);
	$(_v) for dir in $$( cd $(Sources) && $(FIND) . -type d ); do			\
	        cd $(BuildDirectory) && if [ ! -d $$dir ]; then $(MKDIR) $$dir; fi;	\
	      done
	$(_v) for file in $$( cd $(Sources) && $(FIND) . -type f ); do			\
	        cd $(BuildDirectory) && $(LN) -fs $(Sources)/$$file $$file;		\
	      done

# For RC
installsrc: install_source

build:: lazy_install_source

clean::
	@echo "Cleaning $(Project)..."
	$(_v) $(RMDIR) -f "$(BuildDirectory)" || true

$(Passed_Targets) $(Extra_Passed_Targets):
	$(_v) umask $(Install_Mask) ; $(MAKE) -C $(BuildDirectory) $(Environment) $@

recurse:
ifdef SubProjects
	$(_v) for SubProject in $(SubProjects); do				\
		$(MAKE) -C $$SubProject $(TARGET)				\
			OBJROOT=$(OBJROOT)					\
			SYMROOT=$(SYMROOT)					\
			DSTROOT=$(DSTROOT)					\
		        BuildDirectory=$(BuildDirectory)/$${SubProject}		\
		               Sources=$(Sources)/$${SubProject}		\
		       CoreOSMakefiles=$(CoreOSMakefiles) || exit 1 ;		\
	      done
endif

rshowvar: showvar
	$(_v) $(MAKE) recurse TARGET=rshowvar

compress_man_pages:
	@echo "Man pages are no longer compressed.  Please remove the compress_man_pages target from your Makefile."
