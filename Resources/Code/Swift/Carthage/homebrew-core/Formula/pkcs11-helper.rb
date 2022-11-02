class Pkcs11Helper < Formula
  desc "Library to simplify the interaction with PKCS#11"
  homepage "https://github.com/OpenSC/OpenSC/wiki/pkcs11-helper"
  url "https://github.com/OpenSC/pkcs11-helper/releases/download/pkcs11-helper-1.27/pkcs11-helper-1.27.0.tar.bz2"
  sha256 "653730f0c561bbf5941754c0783976113589b2dc64a0661c908dc878bfa4e58b"
  license any_of: ["BSD-3-Clause", "GPL-2.0-or-later"]
  head "https://github.com/OpenSC/pkcs11-helper.git"

  livecheck do
    url :stable
    strategy :github_latest
    regex(%r{href=.*?/tag/pkcs11-helper[._-]v?(\d+(?:\.\d+)+)["' >]}i)
  end

  bottle do
    cellar :any
    sha256 "84c49ac08cc1c9f222742d7aa3bd628b32673d2376efbe7059fc8d355ff540ad" => :big_sur
    sha256 "79eaec51f13a0bda941703a652f790a2306233428878fd9c2beaca7fcbdb9422" => :arm64_big_sur
    sha256 "5cdee7e99d40242d5026b2fbb448f7390e272bb610f8f7a125ab599941c73a06" => :catalina
    sha256 "3bc3ca9909c0cc67a51ab579ed498dbc9c9dc2842d572b5adc4c715405f78ada" => :mojave
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "pkg-config" => :build
  depends_on "openssl@1.1"

  def install
    args = %W[
      --disable-debug
      --disable-dependency-tracking
      --prefix=#{prefix}
    ]

    system "autoreconf", "--verbose", "--install", "--force"
    system "./configure", *args
    system "make", "install"
  end

  test do
    (testpath/"test.c").write <<~EOS
      #include <stdio.h>
      #include <stdlib.h>
      #include <pkcs11-helper-1.0/pkcs11h-core.h>

      int main() {
        printf("Version: %08x", pkcs11h_getVersion ());
        return 0;
      }
    EOS
    system ENV.cc, testpath/"test.c", "-I#{include}", "-L#{lib}",
                   "-lpkcs11-helper", "-o", "test"
    system "./test"
  end
end
