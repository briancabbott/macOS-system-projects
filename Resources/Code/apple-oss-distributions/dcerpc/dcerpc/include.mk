dce_includes=-I$(top_srcdir)/include -I$(top_builddir)/include @DCETHREADINCLUDES@

BASE_CPPFLAGS=$(dce_includes)

AM_CPPFLAGS=$(BASE_CPPFLAGS)
AM_CFLAGS=$(BASE_CFLAGS)

SUFFIXES=.idl

# TODO-2008/0124-dalmeida - More correct naming of variable.
IDL_CPPFLAGS=$(IDL_CFLAGS)

IDL=$(top_builddir)/idl_compiler/dceidl
IDL_INCLUDE_DIR=$(top_srcdir)/include/dce
IDL_INCLUDES=-I$(top_srcdir)/include -I$(top_builddir)/include

MODULELDFLAGS=-module -avoid-version -export-dynamic -export-symbols-regex rpc__module_init_func

# ylwrap is provided by automake as a wrapper to allow multiple invocations in
# a single directory.

YLWRAP = $(top_srcdir)/build/ylwrap

%.h: %.idl
	$(IDL) $(IDL_INCLUDES) $(IDL_CFLAGS) -cepv -client none -server none -no_mepv $<

# Create default message strings from a msg file
#%_defmsg.h:	%.msg
#	echo $(RM) $(RMFLAGS) $@
#	echo $(SED) -e '/^\$$/d;/^$$/d;s/^[^ ]* /"/;s/$$/",/;' $< > $@

#%.cat:	%.msg
#	$(RM) $(RMFLAGS) $@
#	$(GENCAT) $@ $<
