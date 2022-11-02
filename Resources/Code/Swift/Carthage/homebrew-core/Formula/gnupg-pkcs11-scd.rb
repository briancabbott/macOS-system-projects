class GnupgPkcs11Scd < Formula
  desc "Enable the use of PKCS#11 tokens with GnuPG"
  homepage "https://gnupg-pkcs11.sourceforge.io"
  url "https://github.com/alonbl/gnupg-pkcs11-scd/releases/download/gnupg-pkcs11-scd-0.9.2/gnupg-pkcs11-scd-0.9.2.tar.bz2"
  sha256 "fddd798f8b5f9f960d2a7f6961b00ef7b49b00e8bf069c113a4d42b5e44fd0d5"
  license "BSD-3-Clause"
  revision 1

  livecheck do
    url :stable
    strategy :github_latest
    regex(%r{href=.*?/tag/gnupg-pkcs11-scd[._-]v?(\d+(?:\.\d+)+)["' >]}i)
  end

  bottle do
    cellar :any
    sha256 "039a425a56fce6b9495361e626925b46a5c6569ef0bed3512ff12da6148ab221" => :big_sur
    sha256 "f7d8c8919b4411b11a53c503ae03db90ac561332c3cdb97da4d57ef9165aa352" => :arm64_big_sur
    sha256 "78f09618378fd89be78ff9e10af2e1d33ac5dc06fcca6474994662434b7b3dc1" => :catalina
    sha256 "78537d1ee3285a604aae1d683db56da1b9ec76bf71262ff234e758efda63f885" => :mojave
    sha256 "1f4264ac76b36c453a3c5a000d1b1269f331e88420efc5591274ccbb8dc8b85c" => :high_sierra
    sha256 "83748a14d87233e8a2cf4744d0353c01176536b5cd9e1b317f741f824416453f" => :sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "libassuan"
  depends_on "libgcrypt"
  depends_on "libgpg-error"
  depends_on "pkcs11-helper"

  def install
    system "autoreconf", "-fiv"
    system "./configure", "--disable-dependency-tracking",
                          "--with-libgpg-error-prefix=#{Formula["libgpg-error"].opt_prefix}",
                          "--with-libassuan-prefix=#{Formula["libassuan"].opt_prefix}",
                          "--with-libgcrypt-prefix=#{Formula["libgcrypt"].opt_prefix}",
                          "--prefix=#{prefix}"
    system "make"
    system "make", "check"
    system "make", "install"
  end

  test do
    system "#{bin}/gnupg-pkcs11-scd", "--help"
  end
end
