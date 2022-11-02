class Libpcl < Formula
  desc "C library and API for coroutines"
  homepage "http://xmailserver.org/libpcl.html"
  url "http://xmailserver.org/pcl-1.12.tar.gz"
  sha256 "e7b30546765011575d54ae6b44f9d52f138f5809221270c815d2478273319e1a"
  license "GPL-2.0"

  bottle do
    cellar :any
    rebuild 1
    sha256 "2ed8a2eb0ff0c53cb2a2653991386ceded74a41a8a215e0d641221092917e361" => :big_sur
    sha256 "11984be842d85e685f2e52d4d5155f24123a44e0f1855970c5fed1e8cb2172f5" => :catalina
    sha256 "3eb3bf64576a13da02b76cf21bfd37a9889e48d3e7c0df06bd5767c61cc09d06" => :mojave
    sha256 "2d7ce1c2a11e762dacf0e28f92a1b1f6b6a45ea4564ac579b4c0683c61ac61f7" => :high_sierra
    sha256 "525c0925d7d3234cf5da86a892d15aa4f6d4417f302ed821e2bfd6e7cb06ef43" => :sierra
    sha256 "1975baf018352fd1f1ca88bd39fc02db384e2f6be4017976184dda3365c60608" => :el_capitan
    sha256 "e9c6f7bc1efab583e44879426a5abb2ff5e7f3eb30261a81a7be723c3280c3a3" => :yosemite
    sha256 "8f8e6669f9a552618b5578ad649e0b2a5f0860922e756c79a609b2eb21b5d4b4" => :mavericks
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
