# $FreeBSD: src/lib/libmd/Makefile,v 1.42 2005/03/09 19:23:04 cperciva Exp $

LIB=	md
SHLIBDIR?= /lib
SRCS=	md2c.c md4c.c md5c.c md2hl.c md4hl.c md5hl.c \
	rmd160c.c rmd160hl.c \
	sha1hl.c \
	#sha256hl.c
INCS=	md2.h md4.h md5.h ripemd.h sha.h sha256.h

CLEANFILES+=	md[245]c.c md[245]hl.c md[245].ref mddriver \
		rmd160.ref rmd160hl.c rmddriver \
		sha0.ref sha0hl.c sha1.ref sha1hl.c shadriver \
		sha256.ref sha256hl.c
CFLAGS+= -I${.CURDIR}

md2c.c: mdXc.c
	sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' ${.ALLSRC} > ${.TARGET}

md4c.c: mdXc.c
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' ${.ALLSRC} > ${.TARGET}

md5c.c: mdXc.c
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' ${.ALLSRC} > ${.TARGET}

md2hl.c: mdXhl.c
	(echo '#define LENGTH 16'; \
		sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' ${.ALLSRC}) > ${.TARGET}

md4hl.c: mdXhl.c
	(echo '#define LENGTH 16'; \
		sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' ${.ALLSRC}) > ${.TARGET}

md5hl.c: mdXhl.c
	(echo '#define LENGTH 16'; \
		sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' ${.ALLSRC}) > ${.TARGET}

sha1hl.c: mdXhl.c
	(echo '#define LENGTH 20'; \
		sed -e 's/mdX/sha/g' -e 's/MDX/SHA1_/g' -e 's/SHA1__/SHA1_/g' \
		${.ALLSRC}) > ${.TARGET}

sha256hl.c: mdXhl.c
	(echo '#define LENGTH 32'; \
		sed -e 's/mdX/sha256/g' -e 's/MDX/SHA256_/g'	\
			-e  's/SHA256__/SHA256_/g' \
		${.ALLSRC}) > ${.TARGET}

rmd160hl.c: mdXhl.c
	(echo '#define LENGTH 20'; \
		sed -e 's/mdX/ripemd/g' -e 's/MDX/RIPEMD160_/g' \
			-e 's/RIPEMD160__/RIPEMD160_/g' \
			${.ALLSRC}) > ${.TARGET}

md2.ref:
	echo 'MD2 test suite:' > ${.TARGET}
	@echo 'MD2 ("") = 8350e5a3e24c153df2275c9f80692773' >> ${.TARGET}
	@echo 'MD2 ("a") = 32ec01ec4a6dac72c0ab96fb34c0b5d1' >> ${.TARGET}
	@echo 'MD2 ("abc") = da853b0d3f88d99b30283a69e6ded6bb' >> ${.TARGET}
	@echo 'MD2 ("message digest") = ab4f496bfb2a530b219ff33031fe06b0' >> ${.TARGET}
	@echo 'MD2 ("abcdefghijklmnopqrstuvwxyz") = 4e8ddff3650292ab5a4108c3aa47940b' >> ${.TARGET}
	@echo 'MD2 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = da33def2a42df13975352846c30338cd' >> ${.TARGET}
	@echo 'MD2 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = d5976f79d83d3a0dc9806c3c66f3efd8' >> ${.TARGET}

md4.ref:
	echo 'MD4 test suite:' > ${.TARGET}
	@echo 'MD4 ("") = 31d6cfe0d16ae931b73c59d7e0c089c0' >> ${.TARGET}
	@echo 'MD4 ("a") = bde52cb31de33e46245e05fbdbd6fb24' >> ${.TARGET}
	@echo 'MD4 ("abc") = a448017aaf21d8525fc10ae87aa6729d' >> ${.TARGET}
	@echo 'MD4 ("message digest") = d9130a8164549fe818874806e1c7014b' >> ${.TARGET}
	@echo 'MD4 ("abcdefghijklmnopqrstuvwxyz") = d79e1c308aa5bbcdeea8ed63df412da9' >> ${.TARGET}
	@echo 'MD4 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = 043f8582f241db351ce627e153e7f0e4' >> ${.TARGET}
	@echo 'MD4 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = e33b4ddc9c38f2199c3e7b164fcc0536' >> ${.TARGET}

md5.ref:
	echo 'MD5 test suite:' > ${.TARGET}
	@echo 'MD5 ("") = d41d8cd98f00b204e9800998ecf8427e' >> ${.TARGET}
	@echo 'MD5 ("a") = 0cc175b9c0f1b6a831c399e269772661' >> ${.TARGET}
	@echo 'MD5 ("abc") = 900150983cd24fb0d6963f7d28e17f72' >> ${.TARGET}
	@echo 'MD5 ("message digest") = f96b697d7cb7938d525a2f31aaf161d0' >> ${.TARGET}
	@echo 'MD5 ("abcdefghijklmnopqrstuvwxyz") = c3fcd3d76192e4007dfb496cca67e13b' >> ${.TARGET}
	@echo 'MD5 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = d174ab98d277d9f5a5611c2c9f419d9f' >> ${.TARGET}
	@echo 'MD5 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = 57edf4a22be3c955ac49da2e2107b67a' >> ${.TARGET}

sha1.ref:
	(echo 'SHA-1 test suite:'; \
	echo 'SHA-1 ("") = da39a3ee5e6b4b0d3255bfef95601890afd80709'; \
	echo 'SHA-1 ("abc") = a9993e364706816aba3e25717850c26c9cd0d89d'; \
	echo 'SHA-1 ("message digest") =' \
		'c12252ceda8be8994d5fa0290a47231c1d16aae3'; \
	echo 'SHA-1 ("abcdefghijklmnopqrstuvwxyz") =' \
		'32d10c7b8cf96570ca04ce37f2a19d84240d3a89'; \
	echo 'SHA-1 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'761c457bf73b14d27e9e9265c46f4b4dda11f940'; \
	echo 'SHA-1 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'50abf5706a150990a08b2c5ea40fa0e585554732' ) > ${.TARGET}

sha256.ref:
	echo 'SHA-256 test suite:' > ${.TARGET}
	@echo 'SHA-256 ("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855' >> ${.TARGET}
	@echo 'SHA-256 ("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad' >> ${.TARGET}
	@echo 'SHA-256 ("message digest") = f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650' >> ${.TARGET}
	@echo 'SHA-256 ("abcdefghijklmnopqrstuvwxyz") = 71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73' >> ${.TARGET}
	@echo 'SHA-256 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = db4bfcbd4da0cd85a60c3c37d3fbd8805c77f15fc6b1fdfe614ee0a7c8fdb4c0' >> ${.TARGET}
	@echo 'SHA-256 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e' >> ${.TARGET}

rmd160.ref:
	(echo 'RIPEMD160 test suite:'; \
	echo 'RIPEMD160 ("") = 9c1185a5c5e9fc54612808977ee8f548b2258d31'; \
	echo 'RIPEMD160 ("abc") = 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc'; \
	echo 'RIPEMD160 ("message digest") =' \
		'5d0689ef49d2fae572b881b123a85ffa21595f36'; \
	echo 'RIPEMD160 ("abcdefghijklmnopqrstuvwxyz") =' \
		'f71c27109c692c1b56bbdceb5b9d2865b3708dbc'; \
	echo 'RIPEMD160 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'b0e20b6e3116640286ed3a87a5713079b21f5189'; \
	echo 'RIPEMD160 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'9b752e45573d4b39f4dbd3323cab82bf63326bfb' ) > ${.TARGET}

test:	md2.ref md4.ref md5.ref rmd160.ref sha1.ref sha256.ref
	@${ECHO} if any of these test fail, the code produces wrong results
	@${ECHO} and should NOT be used.
	${CC} ${CFLAGS} ${LDFLAGS} -DMD=2 -o mddriver ${.CURDIR}/mddriver.c -L. -lmd
	./mddriver | cmp md2.ref -
	@${ECHO} MD2 passed test
	${CC} ${CFLAGS} ${LDFLAGS} -DMD=4 -o mddriver ${.CURDIR}/mddriver.c -L. -lmd
	./mddriver | cmp md4.ref -
	@${ECHO} MD4 passed test
	${CC} ${CFLAGS} ${LDFLAGS} -DMD=5 -o mddriver ${.CURDIR}/mddriver.c -L. -lmd
	./mddriver | cmp md5.ref -
	@${ECHO} MD5 passed test
	-rm -f mddriver
	${CC} ${CFLAGS} ${LDFLAGS} -o rmddriver ${.CURDIR}/rmddriver.c -L. -lmd
	./rmddriver | cmp rmd160.ref -
	@${ECHO} RIPEMD160 passed test
	-rm -f rmddriver
	${CC} ${CFLAGS} ${LDFLAGS} -DSHA_VERSION=1 -o shadriver ${.CURDIR}/shadriver.c -L. -lmd
	./shadriver | cmp sha1.ref -
	@${ECHO} SHA-1 passed test
	#${CC} ${CFLAGS} ${LDFLAGS} -DSHA_VERSION=256 -o shadriver ${.CURDIR}/shadriver.c -L. -lmd
	#./shadriver | cmp sha256.ref -
	#@${ECHO} SHA-256 passed test
	-rm -f shadriver

.include <bsd.lib.mk>
