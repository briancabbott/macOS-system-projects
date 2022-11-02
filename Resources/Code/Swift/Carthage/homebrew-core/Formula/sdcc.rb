class Sdcc < Formula
  desc "ANSI C compiler for Intel 8051, Maxim 80DS390, and Zilog Z80"
  homepage "https://sdcc.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/sdcc/sdcc/4.0.0/sdcc-src-4.0.0.tar.bz2"
  sha256 "489180806fc20a3911ba4cf5ccaf1875b68910d7aed3f401bbd0695b0bef4e10"
  license "GPL-2.0"
  head "https://svn.code.sf.net/p/sdcc/code/trunk/sdcc"

  livecheck do
    url :stable
    regex(%r{url=.*?/sdcc-src[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    sha256 "5f6504340cb02b7de8cd7562e5b91533c9ff090e620f1e458a4843ebe6460ad3" => :big_sur
    sha256 "e002b79aa971132f16eca044273a2048fca8c162a4231b69f7d90086315e3d76" => :arm64_big_sur
    sha256 "876e548b2a8c31c2d45d753b59e528c82101d193398d8c158270849fe9703ece" => :catalina
    sha256 "214547215aa0b7598ecfd80cd291bbc64bd8b2d95c867fca9653e5d0aef042d6" => :mojave
    sha256 "1f2423cb4c4f66c34b8a68f9c7a967c4256ca438646260dbf50e7f4c0b5f8f59" => :high_sierra
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "boost"
  depends_on "gputils"

  def install
    system "./configure", "--prefix=#{prefix}"
    system "make", "all"
    system "make", "install"
    rm Dir["#{bin}/*.el"]
  end

  test do
    system "#{bin}/sdcc", "-v"
  end
end
