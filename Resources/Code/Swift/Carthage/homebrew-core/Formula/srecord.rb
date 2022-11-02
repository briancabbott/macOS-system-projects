class Srecord < Formula
  desc "Tools for manipulating EPROM load files"
  homepage "https://srecord.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/srecord/srecord/1.64/srecord-1.64.tar.gz"
  sha256 "49a4418733c508c03ad79a29e95acec9a2fbc4c7306131d2a8f5ef32012e67e2"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "5c5129ae228ef644b5ed2a26516295feabd198aea270eab03c5c6d3e418980b1" => :big_sur
    sha256 "2531dde4b69ae50e0cb15b498b2729862e64d00b98be831e581989d9e907f36a" => :arm64_big_sur
    sha256 "cc4e1e89835954876853f5f7bcccbfd172adbb5651c1f2790ea3da10e4347845" => :catalina
    sha256 "6b3b825b501d1ea1635d107fb62021dde713f6da375f53f1a1fdcb59070df63a" => :mojave
    sha256 "f6341ba9022e6cbc057c519fcdc7c7518247c850025777b80d2463341315d88c" => :high_sierra
    sha256 "0601896fc392a13f7ef861fc3840fadfc7ddc7313763c1d374555129f4301c0d" => :sierra
    sha256 "6a0df3e5fb40699d9b1198562b3b3a4e1745c3a0d12923c461246b7784b8324c" => :el_capitan
    sha256 "c3c29b357c44bc3da2dbb8f23a6d83aeb637aa374fe0564eb9454e5e6b53d54c" => :yosemite
    sha256 "10a04c2aca5e6f554c00aa57bd05f9c3cbe46238c9af66678dc1e6a3323c5cdb" => :mavericks
  end

  depends_on "libtool" => :build
  depends_on "boost"
  depends_on "libgcrypt"

  # Use macOS's pstopdf
  patch do
    url "https://raw.githubusercontent.com/Homebrew/formula-patches/85fa66a9/srecord/1.64.patch"
    sha256 "140e032d0ffe921c94b19145e5904538233423ab7dc03a9c3c90bf434de4dd03"
  end

  def install
    system "./configure", "--prefix=#{prefix}", "LIBTOOL=glibtool"
    system "make", "install"
  end
end
