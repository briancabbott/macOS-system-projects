class OpenOcd < Formula
  desc "On-chip debugging, in-system programming and boundary-scan testing"
  homepage "http://openocd.org/"
  url "https://downloads.sourceforge.net/project/openocd/openocd/0.10.0/openocd-0.10.0.tar.bz2"
  sha256 "7312e7d680752ac088b8b8f2b5ba3ff0d30e0a78139531847be4b75c101316ae"
  license "GPL-2.0-or-later"

  livecheck do
    url :stable
    regex(%r{url=.*?/openocd[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    rebuild 2
    sha256 "3b90b2e58a0b0ba8d9c93909ae5735d5c0e19e6e7ba1af0339c78bc5ee8df452" => :big_sur
    sha256 "b4695aad1ca7722c171bbf1df79c476d159289bd23fb895f1f440e606ab80cdd" => :arm64_big_sur
    sha256 "73738a0c3bfffa98beea25c441d5eddfd743dc5c7c79418685519354975bb840" => :catalina
    sha256 "491bec9acdc4e446a6515975041f21dec919ba330f88b5a69e8651ddd9c07468" => :mojave
    sha256 "0258f4d658907060d890c978a4d122ac5501119c4d28bb272e4bf5bc59bd8852" => :high_sierra
    sha256 "790605e83cc22ab4a455a382f7b6a434d44c19f82e0b8a0ee6a3bf28ac6f9f31" => :sierra
  end

  head do
    url "https://github.com/ntfreak/openocd.git"

    depends_on "autoconf" => :build
    depends_on "automake" => :build
    depends_on "libtool" => :build
    depends_on "texinfo" => :build
  end

  depends_on "pkg-config" => :build
  depends_on "hidapi"
  depends_on "libftdi"
  depends_on "libusb"
  depends_on "libusb-compat"

  def install
    ENV["CCACHE"] = "none"

    system "./bootstrap", "nosubmodule" if build.head?
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--enable-buspirate",
                          "--enable-stlink",
                          "--enable-dummy",
                          "--enable-jtag_vpi",
                          "--enable-remote-bitbang"
    system "make", "install"
  end
end
