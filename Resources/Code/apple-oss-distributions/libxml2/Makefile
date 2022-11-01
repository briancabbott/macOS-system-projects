##
# Makefile for libxml2
##

# Project info
Project               = libxml2
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --enable-static=no --with-python=no --with-iconv=no --with-icu=yes
GnuAfterInstall       = extract-symbols fix-libraries remove-gtk-doc install-plist remove-autom4te-cache

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

# Extract the source.
install_source::
	cd $(SRCROOT)/$(Project) && aclocal && glibtoolize --force && autoconf && automake --add-missing
	rm -rf $(SRCROOT)/$(Project)/autom4te.cache


extract-symbols:
	for binary in xmllint xmlcatalog libxml2.2.dylib; do \
		$(CP) $(OBJROOT)/.libs/$${binary} $(SYMROOT)/; \
		dsymutil $(SYMROOT)/$${binary}; \
	done


fix-libraries:
	# <rdar://problem/5077277>: change library_names to acommodate that we don't install a dylib with minor.micro versioning.
	sed -i "" -e 's/\(library_names=.*\) libxml2\.2\.[^.]*\.[^.]*\.dylib/\1/' $(DSTROOT)/usr/lib/libxml2.la
	$(RM) $(DSTROOT)/usr/lib/libxml2.2.*.dylib

remove-gtk-doc:
	$(RM) -rf $(DSTROOT)/usr/share/gtk-doc/


OSV     = $(DSTROOT)/usr/local/OpenSourceVersions
OSL     = $(DSTROOT)/usr/local/OpenSourceLicenses
install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/Copyright $(OSL)/$(Project).txt


remove-autom4te-cache:
	$(RM) -rf $(SRCROOT)/$(Project)/autom4te.cache
