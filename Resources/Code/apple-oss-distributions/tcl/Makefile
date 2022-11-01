##
# Makefile for tcl
##

Project = tcl

TEMPROOT = $(SRCROOT)/temp
export PATH := $(TEMPROOT)/usr/bin:$(PATH)

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

## Build settings ##

TCL_DSTROOT           = $(if $(DSTROOT),$(DSTROOT),/tmp/tcl/Release)

TCL_FRAMEWORK_DIR     = $(TCL_DSTROOT)$(NSFRAMEWORKDIR)
TCLSH                 = $(TCL_DSTROOT)$(USRBINDIR)/tclsh
WISH                  = $(TCL_DSTROOT)$(USRBINDIR)/wish

TCL_EXT_DIR           = $(NSLIBRARYDIR)/Tcl
TCL_SRC_DIR           = $(SRCROOT)/tcl
TCL_VERSION           = $(shell $(GREP) "TCL_VERSION=" "$(TCL_SRC_DIR)/tcl/unix/configure" | $(CUT) -d '=' -f 2)
TCL_INIT              = $(TCL_FRAMEWORK_DIR)/Tcl.framework/Versions/$(TCL_VERSION)/Resources/Scripts/init.tcl

ext : TCL_EXT_DIR= $(NSLIBRARYDIR)/Tcl/$(TCL_VERSION)
ext:   TCL_VERSCHECK  = [catch {package present Tcl $(TCL_VERSION)-}]
fetch:: SRCROOT       = $(CURDIR)

TCL_CONFIG_DIR        = $(OBJROOT)
AC_VALS               = ac_cv_path_tclsh=$(TCLSH) ac_cv_path_wish=$(WISH) ac_cv_header_stdc=yes

MAKE_ARGS             = TclExtLibDir=$(TCL_EXT_DIR) LicenseInstallDir=$(OSL) Plist=$(PLIST) \
                        TclFramework=$(TCL_FRAMEWORK_DIR)/Tcl.framework Tclsh=$(TCLSH) \
                        TkFramework=$(TCL_FRAMEWORK_DIR)/Tk.framework Wish=$(WISH) \
                        CONFIG_SITE=$(TCL_CONFIG_DIR)/config.site

ext:   EXT_MAKE_ARGS  = PureTclExt=NO Tcl84Ext=NO
ext_puretcl: EXT_MAKE_ARGS = PureTclExt=YES

PLIST                 = $(SRCROOT)/$(Project).plist
OSV                   = /usr/local/OpenSourceVersions
OSL                   = /usr/local/OpenSourceLicenses

export PATH          := $(PATH):/usr/X11/bin
Cruft                += .git

SubProjects          := tcl tk tcl_ext
Actions              := almostclean extract fetch install-license
Actions_nodeps       := install
include tcl_ext/SubprojActions.make

## Targets ##

core                 := install-tcl install-tk ext ext_puretcl
ext                  := install-tcl_ext

install_source:: install_temp_autoconf extract remove_temp_autoconf

AUTOCONFARCHIVE = $(SRCROOT)/autoconf-18-root.cpio.gz
install_temp_autoconf:
	$(_v) $(MKDIR) $(TEMPROOT)
	$(_v) ditto -x -z $(AUTOCONFARCHIVE) $(TEMPROOT)
	$(_v) $(RM) $(AUTOCONFARCHIVE)

remove_temp_autoconf:
	$(_v) $(RMDIR) $(TEMPROOT)

build::
	$(_v) $(MKDIR) $(TCL_CONFIG_DIR)
	$(_v) echo "cache_file=$(TCL_CONFIG_DIR)/config.cache" > "$(TCL_CONFIG_DIR)/config.site"
	$(_v) for v in $(AC_VALS); do echo "$$v" >> "$(TCL_CONFIG_DIR)/config.site"; done

install:: $(core) install-plist

install1:: build $(core) addentitlements
install2:: install-plist 

ext:
	$(_v) $(MAKE) $(ext) $(EXT_MAKE_ARGS) \
		TCL_EXT_DIR=$(TCL_EXT_DIR) TCL_CONFIG_DIR=$(OBJROOT) \
		OBJROOT=$(OBJROOT)/$@ SYMROOT=$(SYMROOT)/$@ 
	$(_v) printf '%s\n%s%s\n' 'if {$(TCL_VERSCHECK)} {return}' \
		'if {[lsearch -exact $$::auto_path $$dir] == -1} {' \
		'lappend ::auto_path $$dir; lappend ::tcl_pkgPath $$dir}' \
		> $(OBJROOT)/$@/pkgIndex.tcl
	$(_v) printf '\n%s\n    %s%s\n\t%s\n\t    %s%s\n\t%s\n    %s\n' \
		'proc tcl::DarwinFixAutoPath {} {' \
		'if {[lsearch -exact $$::auto_path $(TCL_EXT_DIR)] == -1 && ' \
		'[lsearch -exact $$::auto_path $(shell dirname $(TCL_EXT_DIR))] != -1} {' \
		'foreach g {::auto_path ::tcl_pkgPath} {' \
		'set $$g [linsert [set $$g] [expr {[lsearch -exact [set $$g] ' \
		'$(shell dirname $(TCL_EXT_DIR))]+1}] $(TCL_EXT_DIR)]' '}' '}'\
	        >> $(TCL_INIT)
	$(_v) if [ -n "$(TCL_AUTOPATH)" ]; then printf '%s%s\n' \
		'if {[lsearch -exact $$::auto_path $(TCL_AUTOPATH)] == -1} {' \
		'lappend ::auto_path $(TCL_AUTOPATH)}' \
		>> $(OBJROOT)/$@/pkgIndex.tcl; \
		printf '    %s%s\n\t%s%s\n    %s\n' \
		'if {[lsearch -exact $$::auto_path $(TCL_AUTOPATH)] == -1 && ' \
		'[lsearch -exact $$::auto_path $(NSFRAMEWORKDIR)] != -1} {' \
		'set ::auto_path [linsert $$::auto_path [expr {[lsearch -exact ' \
		'$$::auto_path $(NSFRAMEWORKDIR)]+1}] $(TCL_AUTOPATH)]' '}' \
	        >> $(TCL_INIT); fi
	$(_v) printf '%s\n%s\n%s\n' \
		'}' 'tcl::DarwinFixAutoPath' 'rename tcl::DarwinFixAutoPath {}' \
	        >> $(TCL_INIT)
	$(_v) $(INSTALL_FILE) $(OBJROOT)/$@/pkgIndex.tcl $(DSTROOT)$(TCL_EXT_DIR)

ext_puretcl:
	$(_v) $(MAKE) $(ext) $(EXT_MAKE_ARGS)

install-plist: install-license
	$(_v) $(MKDIR) $(DSTROOT)$(OSV) && $(INSTALL_FILE) $(PLIST) $(DSTROOT)$(OSV)/$(Project).plist

CODESIGN_ALLOCATE=$(DT_TOOLCHAIN_DIR)/usr/bin/codesign_allocate
addentitlements:
	CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) /usr/bin/codesign --force --sign - --entitlements $(SRCROOT)/entitlements.plist --timestamp=none $(DSTROOT)/System/Library/Frameworks/Tcl.framework/Versions/$(TCL_VERSION)/tclsh$(TCL_VERSION)
	CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) /usr/bin/codesign --force --sign - --entitlements $(SRCROOT)/entitlements.plist --timestamp=none $(DSTROOT)/System/Library/Frameworks/Tk.framework/Versions/$(TCL_VERSION)/Resources/Wish.app/Contents/MacOS/Wish
	CODESIGN_ALLOCATE=$(CODESIGN_ALLOCATE) /usr/bin/codesign --force --sign - --entitlements $(SRCROOT)/entitlements.plist --timestamp=none $(DSTROOT)/System/Library/Frameworks/Tk.framework/Versions/$(TCL_VERSION)/Resources/Wish.app/Contents/MacOS/Wish\ Shell

extract::
	$(_v) $(FIND) "$(SRCROOT)" $(Find_Cruft) -depth -exec $(RMDIR) "{}" \;

.PHONY: ext ext_puretcl install-plist
.NOTPARALLEL:
