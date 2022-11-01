#!/bin/sh
#
# opensource version
#

project=Heimdal

if [ ! -d build/.osx-build ] ; then
    sudo rm -rf build
    mkdir -p build/.osx-build
fi

version=$(sw_vers -productVersion | perl -pi -e 's/(\d+\.\d+).*/$1/')
developerdir=$(xcode-select -print-path)

toolchainvers=$(echo $version | perl -pi -e 's/(\.)/_/')
toolchain="com.apple.dt.toolchain.OSX${toolchainvers}"
DT_TOOLCHAIN_DIR="\$(DEVELOPER_DIR)/Toolchains/OSX${version}.xctoolchain"


ROOT=/tmp/$project.dst


sudo rm -rf /Heimdal/{Compilers,Frameworks,Foundation,Executables,Merged}
test -d /Heimdal || sudo mkdir /Heimdal

(sudo rm -rf $ROOT && \
    time xcodebuild installhdrs \
	-target HeimdalFrameworks \
	-arch x86_64 -arch i386 \
    	TOOLCHAINS=$toolchain \
    	DT_TOOLCHAIN_DIR="${DT_TOOLCHAIN_DIR}" \
    	CODE_SIGN_IDENTITY="-" \
	GCC_OPTIMIZATION_LEVEL=0 \
	"$@" \
    && \
    sudo chown -R root:wheel $ROOT && \
    sudo ditto $ROOT /Heimdal/Frameworks && \
    sudo ditto $ROOT /Heimdal/Merged && \
    sudo ditto $ROOT ${developerdir}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${version}.Internal.sdk && \
    sudo ditto $ROOT/System/Library ${developerdir}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${version}.sdk/System/Library && \
    sudo ditto $ROOT / ) || exit 1


(sudo rm -rf $ROOT && \
    time xcodebuild installhdrs \
	-target HeimdalFrameworksFoundation \
	-arch i386 -arch x86_64 \
    	TOOLCHAINS=$toolchain \
    	DT_TOOLCHAIN_DIR="${DT_TOOLCHAIN_DIR}" \
    	CODE_SIGN_IDENTITY="-" \
	GCC_OPTIMIZATION_LEVEL=0 \
	"$@" \
    && \
    sudo chown -R root:wheel $ROOT && \
    sudo ditto $ROOT /Heimdal/Foundation && \
    sudo ditto $ROOT /Heimdal/Merged && \
    sudo ditto $ROOT ${developerdir}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${version}.Internal.sdk && \
    sudo ditto $ROOT/System/Library ${developerdir}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX${version}.sdk/System/Library && \
    sudo ditto $ROOT / ) || exit 1

#endif
