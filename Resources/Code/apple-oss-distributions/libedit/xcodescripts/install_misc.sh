#!/bin/sh

set -ex

mkdir -p $DSTROOT/usr/include/readline $DSTROOT/usr/include/editline
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m $ALTERNATE_MODE $SRCROOT/src/histedit.h $DSTROOT/usr/include
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m $ALTERNATE_MODE $SRCROOT/src/editline/readline.h $DSTROOT/usr/include/editline
for h in readline.h history.h ; do
	ln -s -f ../editline/readline.h $DSTROOT/usr/include/readline/$h
done

[ "$ACTION" == "installhdrs" ] && exit 0

if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	# Don't keep static libraries for the simulator SDK
	rm -rf ${DSTROOT}/usr/local/lib
fi

mkdir -p $DSTROOT/usr/lib

for l in libedit.2.tbd libedit.3.0.tbd libedit.tbd libreadline.tbd ; do
	ln -s -f libedit.3.tbd $DSTROOT/usr/lib/$l
done

[ "$ACTION" == "installapi" ] && exit 0

for l in libedit.2.dylib libedit.3.0.dylib libedit.dylib libreadline.dylib ; do
	ln -s -f libedit.3.dylib $DSTROOT/usr/lib/$l
done

if [ "${RC_ProjectName%_Sim}" == "${RC_ProjectName}" ] ; then
	mkdir -p $DSTROOT/usr/share/man/man{3,5}
	install -m 0644 $SRCROOT/doc/editrc.5 $DSTROOT/usr/share/man/man5
	install -m 0644 $SRCROOT/doc/editline.3 $DSTROOT/usr/share/man/man3
	for m in el_deletestr.3 el_end.3 el_get.3 el_getc.3 el_gets.3 el_history.3 el_history_end.3 el_history_init.3 el_init.3 el_insertstr.3 el_line.3 el_parse.3 el_push.3 el_reset.3 el_resize.3 el_set.3 el_source.3 el_tok_end.3 el_tok_init.3 el_tok_line.3 el_tok_reset.3 el_tok_str.3 ; do
		ln -s -f editline.3 $DSTROOT/usr/share/man/man3/$m
	done
fi

mkdir -p $DSTROOT/usr/local/OpenSource{Licenses,Versions}
install -g $ALTERNATE_GROUP -o $ALTERNATE_OWNER -m a=r $SRCROOT/local/libedit.plist $DSTROOT/usr/local/OpenSourceVersions
/usr/bin/sed -e '1,2d' -e '/^$/,$d' $SRCROOT/src/el.c >$DSTROOT/usr/local/OpenSourceLicenses/libedit.txt
