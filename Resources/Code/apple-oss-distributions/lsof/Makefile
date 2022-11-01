##
# Makefile for lsof
##
# Allan Nathanson <ajn@apple.com>
##

##
# Usage:
#   xcrun --sdk macosx.internal make -- UNAME_RELEASE="20.0"
##

# Project info
Project  = lsof
UserType = Administrator
ToolType = Commands

# It's a GNU Source project
# Well, not really but we can make it work.
GnuNoChown = YES
GnuAfterInstall = add_supporting_files
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install-strip

Configure = $(BuildDirectory)/Configure
Configure_Flags = -n darwin
Configure_Products = Makefile

UNIQUE := $(shell echo $$$$)

##
# The "Configure" script wants to create symlinks within the source
# tree. Since we're not supposed to modify the sources we'll create
# a 'shadow tree' and use that directory instead.
##
lazy_install_source:: install-patched-source

##
# Change the "Configure" script
##

LSOF_CONFIGURE  = $(OBJROOT)/Configure
LSOF_MAKEFILE3  = $(OBJROOT)/dialects/darwin/libproc/Makefile

install-patched-source: shadow_source
	$(_v) echo "*** patching Configure"
	$(_v) $(CAT) $(LSOF_CONFIGURE)							>  /tmp/build.lsof.$(UNIQUE)
	$(_v) echo '/^[ 	]*900|1000|1100|1200|1300|1400|1500|1600)/n'		>  /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^[ 	]*;;/i'							>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      if [ -n "$${SDKROOT}" ]; then'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_AR="`xcrun -sdk $${SDKROOT} -find ar` cr"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CC="`xcrun -sdk $${SDKROOT} -find cc`"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CFGF="$$LSOF_CFGF -isysroot $${SDKROOT}"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_INCLUDE="$${SDKROOT}/usr/include"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_RANLIB="`xcrun -sdk $${SDKROOT} -find ranlib`"'	>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      else'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_AR="`xcrun -sdk / -find ar` cr"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CC="`xcrun -sdk / -find cc`"'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_RANLIB="`xcrun -sdk / -find ranlib`"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      fi'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.'									>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^[ 	]*1700|1800|1900|2000)/n'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^[ 	]*;;/i'							>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      if [ -n "$${SDKROOT}" ]; then'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_AR="`xcrun -sdk $${SDKROOT} -find ar` cr"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CC="`xcrun -sdk $${SDKROOT} -find cc`"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CFGF="$$LSOF_CFGF -isysroot $${SDKROOT}"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_INCLUDE="$${SDKROOT}/usr/include"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_RANLIB="`xcrun -sdk $${SDKROOT} -find ranlib`"'	>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      else'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_AR="`xcrun -sdk / -find ar` cr"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CC="`xcrun -sdk / -find cc`"'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_RANLIB="`xcrun -sdk / -find ranlib`"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      fi'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.'									>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^[ 	]*2100)/n'						>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^[ 	]*;;/i'							>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      LSOF_UNSUP=""'						>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      LSOF_TSTBIGF=" "'						>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      if [ -n "$${SDKROOT}" ]; then'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_AR="`xcrun -sdk $${SDKROOT} -find ar` cr"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CC="`xcrun -sdk $${SDKROOT} -find cc`"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CFGF="$$LSOF_CFGF -isysroot $${SDKROOT}"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_INCLUDE="$${SDKROOT}/usr/include"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_RANLIB="`xcrun -sdk $${SDKROOT} -find ranlib`"'	>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      else'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_AR="`xcrun -sdk / -find ar` cr"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_CC="`xcrun -sdk / -find cc`"'				>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '        LSOF_RANLIB="`xcrun -sdk / -find ranlib`"'			>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '      fi'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.'									>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^.* -mdynamic-no-pic/d'						>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^.* -lcurses/d'							>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.,$$s/DARWIN_XNU_HEADERS/SDKROOT/'					>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^#include <time.h>/n'						>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^main(){/i'							>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo 'int'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.'									>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo 'w'									>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) ed - /tmp/build.lsof.$(UNIQUE)						<  /tmp/build.lsof.$(UNIQUE)-ed	\
											>  /dev/null
	$(_v) $(RM) /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) $(MV) /tmp/build.lsof.$(UNIQUE) $(LSOF_CONFIGURE)
	$(_v) $(CHMOD) +x $(LSOF_CONFIGURE)

	$(_v) echo "*** patching dialects/darwin/libproc/Makefile"
	$(_v) $(CAT) $(LSOF_MAKEFILE3) |							\
		$(SED)	-E									\
			-e 's@(#define[ 	]+LSOF_CC[ 	]+)"@\1"" // "@'		\
			-e 's@(#define[ 	]+LSOF_CCDATE[ 	]+)"@\1"" // "@'		\
			-e 's@(#define[ 	]+LSOF_CCFLAGS[ 	]+)"@\1"" // "@'	\
			-e 's@(#define[ 	]+LSOF_CCV[ 	]+)"@\1"" // "@'		\
			-e 's@(#define[ 	]+LSOF_HOST[ 	]+)"@\1"" // "@'		\
			-e 's@(#define[ 	]+LSOF_LDFLAGS[ 	]+)"@\1"" // "@'	\
			-e 's@(#define[ 	]+LSOF_LOGNAME[ 	]+)"@\1"" // "@'	\
			-e 's@(#define[ 	]+LSOF_SYSINFO[ 	]+)"@\1"" // "@'	\
			-e 's@(#define[ 	]+LSOF_USER[ 	]+)"@\1"" // "@'		\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) echo '/^DOC=/i'									>  /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo 'CODESIGN=		"'`xcrun -sdk $${SDKROOT} -find codesign`'"'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo 'CODESIGN_ALLOCATE=	"'`xcrun -sdk $${SDKROOT} -find codesign_allocate`'"'	>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo ''										>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.'										>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^install-strip:/n'								>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '/^[ 	]*mkdir -p $${DOC}/i'						>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '	  CODESIGN_ALLOCATE=$${CODESIGN_ALLOCATE}		\\'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '	  $${CODESIGN}						\\'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '		--force						\\'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '		--sign -					\\'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '		--entitlements $${SRCROOT}/entitlements.plist	\\'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '		$${BIN}/$${PROG};				\\'		>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo '.'										>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) echo 'w'										>> /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) ed - /tmp/build.lsof.$(UNIQUE)							<  /tmp/build.lsof.$(UNIQUE)-ed	\
												>  /dev/null
	$(_v) $(RM) /tmp/build.lsof.$(UNIQUE)-ed
	$(_v) $(RM) $(LSOF_MAKEFILE3)
	$(_v) $(MV) /tmp/build.lsof.$(UNIQUE) $(LSOF_MAKEFILE3)

##
# Change a few of compile time definitions
##

LSOF_MAKEFILE   = $(OBJROOT)/Makefile
LSOF_MAKEFILE2  = $(OBJROOT)/lib/Makefile
LSOF_MACHINE_H1 = $(OBJROOT)/dialects/darwin/kmem/machine.h
LSOF_MACHINE_H2 = $(OBJROOT)/dialects/darwin/libproc/machine.h

ConfigStamp2 = $(ConfigStamp)2

configure:: $(ConfigStamp2)

$(ConfigStamp2): $(ConfigStamp)
	$(_v) echo "*** patching Makefile"
	$(_v) $(CAT) $(LSOF_MAKEFILE) |								\
		$(SED)	-e 's@^\(DEBUG=	\).*@\1 -Os -g@'					\
			-e 's@^\(DS=	\).*@\1 '`xcrun -sdk $${SDKROOT} -find dsymutil`'@'	\
		        -e 's@install -c@xcrun -sdk $${SDKROOT} install -c@'			\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(RM) $(LSOF_MAKEFILE)
	$(_v) $(MV) /tmp/build.lsof.$(UNIQUE) $(LSOF_MAKEFILE)

	$(_v) echo "*** patching lib/Makefile"
	$(_v) $(CAT) $(LSOF_MAKEFILE2) |					\
		$(SED)	-e 's@^\(DEBUG=	\).*@\1 -Os -g@'			\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(RM) $(LSOF_MAKEFILE2)
	$(_v) $(MV) /tmp/build.lsof.$(UNIQUE) $(LSOF_MAKEFILE2)

	$(_v) echo "*** patching dialects/darwin/kmem/machine.h"
	$(_v) $(CAT) $(LSOF_MACHINE_H1) |					\
		$(SED)	-e 's@^.*\(#define.*HASSECURITY.*1\).*@\1@'		\
			-e 's@^.*\(#define.*HASKERNIDCK.*1\).*@/* \1 */@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(RM) $(LSOF_MACHINE_H1)
	$(_v) $(MV) /tmp/build.lsof.$(UNIQUE) $(LSOF_MACHINE_H1)

	$(_v) echo "*** patching dialects/darwin/libproc/machine.h"
	$(_v) $(CAT) $(LSOF_MACHINE_H2) |					\
		$(SED)	-e 's@^.*\(#define.*HASKERNIDCK.*1\).*@/* \1 */@'	\
		> /tmp/build.lsof.$(UNIQUE)
	$(_v) $(RM) $(LSOF_MACHINE_H2)
	$(_v) $(MV) /tmp/build.lsof.$(UNIQUE) $(LSOF_MACHINE_H2)

	$(_v) $(TOUCH) $(ConfigStamp2)

# Open Source support files
OSV = ${DSTROOT}/usr/local/OpenSourceVersions
OSL = ${DSTROOT}/usr/local/OpenSourceLicenses

add_supporting_files:
	@echo "Adding Open Source support files"
	$(_v) $(MKDIR) ${OSV}
	$(_v) $(INSTALL_FILE) $(SRCROOT)/${Project}.plist ${OSV}/${Project}.plist
	$(_v) $(MKDIR) ${OSL}
	$(_v) $(CAT) ${SRCROOT}/lsof/00README				\
		| $(SED) -n -e '/^License/,/\*\//p'			\
		> ${OSL}/${Project}.txt
	$(_v) echo ""										>> ${OSL}/${Project}.txt
	$(_v) echo "In addition, the following copyright is included with the files"		>> ${OSL}/${Project}.txt
	$(_v) echo "associated with the \"darwin\" dialect that use the <libproc.h> APIs."	>> ${OSL}/${Project}.txt
	$(_v) echo ""										>> ${OSL}/${Project}.txt
	$(_v) $(CAT) ${SRCROOT}/lsof/dialects/darwin/libproc/dproc.c	\
		| $(SED) -n -e '6,34p'					\
		| $(SED)    -e 's/^/    /'				\
		>> ${OSL}/${Project}.txt
	$(_v) $(CHOWN) $(Install_File_User):$(Install_File_Group) ${OSL}/${Project}.txt
	$(_v) $(CHMOD) $(Install_File_Mode) ${OSL}/${Project}.txt
