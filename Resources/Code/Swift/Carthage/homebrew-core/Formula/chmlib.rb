class Chmlib < Formula
  desc "Library for dealing with Microsoft ITSS/CHM files"
  homepage "http://www.jedrea.com/chmlib"
  url "http://www.jedrea.com/chmlib/chmlib-0.40.tar.gz"
  mirror "https://download.tuxfamily.org/slitaz/sources/packages/c/chmlib-0.40.tar.gz"
  sha256 "512148ed1ca86dea051ebcf62e6debbb00edfdd9720cde28f6ed98071d3a9617"
  license "LGPL-2.1"

  bottle do
    cellar :any
    rebuild 2
    sha256 "af369d3e427b36281f053f65a0d5be2a269c2a0fb80c87443baa066892d0652c" => :big_sur
    sha256 "96d7cb33260c72012f24f383054b7f2505f815f0e3e24298229b5712f8a66cfa" => :catalina
    sha256 "1718a0a9343788718b4207596ebff457f5214879319292cc1608254374720944" => :mojave
    sha256 "426b95744d071ad76399ee240400ab74bcec9057735cbfeb2d433501105060ef" => :high_sierra
    sha256 "9781c76f933beca002df542d2db0644e51766568d9399f9e73dc39b9e896f539" => :sierra
    sha256 "6b834a6ae6e95f8daaa726fd6ae1a2d3e60335f98862fea9e790c24e5a6411d1" => :el_capitan
    sha256 "bdc19058cbf1690e960bd88d06f6c8b2ff47f8b743947eb82c259ba394881a65" => :yosemite
    sha256 "366c564a2cd0185d84ff6892f5d773f80ddee50f6db39e763060b3ebb31413b3" => :mavericks
  end

  def install
    system "./configure", "--disable-io64", "--enable-examples", "--prefix=#{prefix}"
    system "make", "install"
  end
end
