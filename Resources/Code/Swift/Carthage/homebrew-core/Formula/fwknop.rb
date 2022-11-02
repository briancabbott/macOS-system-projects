class Fwknop < Formula
  desc "Single Packet Authorization and Port Knocking"
  homepage "https://www.cipherdyne.org/fwknop/"
  url "https://github.com/mrash/fwknop/archive/2.6.10.tar.gz"
  sha256 "a7c465ba84261f32c6468c99d5512f1111e1bf4701477f75b024bf60b3e4d235"
  license "GPL-2.0-or-later"
  head "https://github.com/mrash/fwknop.git"

  livecheck do
    url :stable
    regex(/^v?(\d+(?:\.\d+)+)$/i)
  end

  bottle do
    sha256 "61ec8e446a6dbae400dcf40a8db9e066a8d64937bf82e84ee1c1e5a86d163934" => :big_sur
    sha256 "74003c23c04e207543afa2e957df46dc72b3067c8d9100503546d828007c14a8" => :arm64_big_sur
    sha256 "3a4ff22b7de484deb6473ffdad63d3e927290925af925b5dbf1b868648824493" => :catalina
    sha256 "2e56267215274c15f322335f86aa92b671f5600cbfd2275949cef03ec47d390e" => :mojave
    sha256 "a36cd65fe358a6b156b2b5276bcdf629b2d777ac8a803e7cd40ee9e3c75512e4" => :high_sierra
    sha256 "7472ea129bbb0d5a1187d08e4a9770d66d480a9bc284a62db11f6dec90b770cf" => :sierra
    sha256 "ec59a9d13d78f441a695776767038fb830acc4cdbfe28b30cc41ec2b7ea76f1f" => :el_capitan
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build
  depends_on "gpgme"

  def install
    system "./autogen.sh"
    system "./configure", "--disable-dependency-tracking", "--disable-silent-rules",
                          "--prefix=#{prefix}", "--with-gpgme", "--sysconfdir=#{etc}",
                          "--with-gpg=#{Formula["gnupg"].opt_bin}/gpg"
    system "make", "install"
  end

  test do
    touch testpath/".fwknoprc"
    chmod 0600, testpath/".fwknoprc"
    system "#{bin}/fwknop", "--version"
  end
end
