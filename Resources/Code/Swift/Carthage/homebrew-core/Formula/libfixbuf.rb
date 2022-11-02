class Libfixbuf < Formula
  desc "Implements the IPFIX Protocol as a C library"
  homepage "https://tools.netsa.cert.org/fixbuf/"
  url "https://tools.netsa.cert.org/releases/libfixbuf-2.4.1.tar.gz"
  sha256 "8c535d48120b08df1731de709f2dbd2ba8bce568ad64cac34826102caf594d84"
  license "LGPL-3.0-only"

  livecheck do
    url "https://tools.netsa.cert.org/fixbuf/download.html"
    regex(%r{releases/libfixbuf[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    sha256 "1cafffbb92c6bba1c328d01d1c00fa4ed1191a93eec966a6a3dce31219ef0d05" => :big_sur
    sha256 "d29c4e4bd8a1d13508214f781306188fe89e78579747aea4ab1166a9d33a4402" => :arm64_big_sur
    sha256 "4e7eb768bb5499147e8c630e4620c356abba37fff0c54d69daf8fe2c9f752771" => :catalina
    sha256 "576d20e23954424a4132ed8b1dd9387f78ce0cf3b401d1fa1ed0d436016a3713" => :mojave
  end

  depends_on "pkg-config" => :build

  depends_on "glib"
  depends_on "openssl@1.1"

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--with-openssl=#{Formula["openssl@1.1"].opt_prefix}",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make"
    system "make", "install"
  end
end
