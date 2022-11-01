#!/usr/bin/env bash
#
# Install dependencies required to build and test Git on Linux and macOS
#

. ${0%/*}/lib.sh

P4WHENCE=http://filehost.perforce.com/perforce/r$LINUX_P4_VERSION
LFSWHENCE=https://github.com/github/git-lfs/releases/download/v$LINUX_GIT_LFS_VERSION

case "$jobname" in
linux-clang|linux-gcc)
	sudo apt-add-repository -y "ppa:ubuntu-toolchain-r/test"
	sudo apt-get -q update
	sudo apt-get -q -y install language-pack-is libsvn-perl apache2
	case "$jobname" in
	linux-gcc)
		sudo apt-get -q -y install gcc-8
		;;
	esac

	mkdir --parents "$P4_PATH"
	pushd "$P4_PATH"
		wget --quiet "$P4WHENCE/bin.linux26x86_64/p4d"
		wget --quiet "$P4WHENCE/bin.linux26x86_64/p4"
		chmod u+x p4d
		chmod u+x p4
	popd
	mkdir --parents "$GIT_LFS_PATH"
	pushd "$GIT_LFS_PATH"
		wget --quiet "$LFSWHENCE/git-lfs-linux-amd64-$LINUX_GIT_LFS_VERSION.tar.gz"
		tar --extract --gunzip --file "git-lfs-linux-amd64-$LINUX_GIT_LFS_VERSION.tar.gz"
		cp git-lfs-$LINUX_GIT_LFS_VERSION/git-lfs .
	popd
	;;
osx-clang|osx-gcc)
	export HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1
	# Uncomment this if you want to run perf tests:
	# brew install gnu-time
	test -z "$BREW_INSTALL_PACKAGES" ||
	brew install $BREW_INSTALL_PACKAGES
	brew link --force gettext
	brew cask install perforce || {
		# Update the definitions and try again
		git -C "$(brew --repository)"/Library/Taps/homebrew/homebrew-cask pull &&
		brew cask install perforce
	} ||
	brew install caskroom/cask/perforce
	case "$jobname" in
	osx-gcc)
		brew link gcc ||
		brew link gcc@8
		;;
	esac
	;;
StaticAnalysis)
	sudo apt-get -q update
	sudo apt-get -q -y install coccinelle libcurl4-openssl-dev libssl-dev \
		libexpat-dev gettext
	;;
Documentation)
	sudo apt-get -q update
	sudo apt-get -q -y install asciidoc xmlto docbook-xsl-ns

	test -n "$ALREADY_HAVE_ASCIIDOCTOR" ||
	gem install --version 1.5.8 asciidoctor
	;;
esac

if type p4d >/dev/null && type p4 >/dev/null
then
	echo "$(tput setaf 6)Perforce Server Version$(tput sgr0)"
	p4d -V | grep Rev.
	echo "$(tput setaf 6)Perforce Client Version$(tput sgr0)"
	p4 -V | grep Rev.
fi
if type git-lfs >/dev/null
then
	echo "$(tput setaf 6)Git-LFS Version$(tput sgr0)"
	git-lfs version
fi
