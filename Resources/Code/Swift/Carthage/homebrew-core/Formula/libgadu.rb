class Libgadu < Formula
  desc "Library for ICQ instant messenger protocol"
  homepage "https://libgadu.net/"
  url "https://github.com/wojtekka/libgadu/releases/download/1.12.2/libgadu-1.12.2.tar.gz"
  sha256 "28e70fb3d56ed01c01eb3a4c099cc84315d2255869ecf08e9af32c41d4cbbf5d"
  license "LGPL-2.1"

  bottle do
    cellar :any
    sha256 "d9f8198b7a7640ec47933ebbb7d4cab50bc0f29fe20fa88126e6ecd6b116d62b" => :big_sur
    sha256 "e556444015bb575c2d7efc07815f72141da829fcc67262238f72257110226c99" => :arm64_big_sur
    sha256 "afe9b94a62b55c700f57d853d077be96a901b450faa7ff9585a43397cacf838a" => :catalina
    sha256 "394b7c3b78e1aa4f7960d7ffc62cefe91069a0e50b7442b62f68d2e68f5d01ad" => :mojave
    sha256 "65f828f98715efbb7bb351d47e11df0fd0279b8c060233138721c119abf0879f" => :high_sierra
    sha256 "4cf4bb4fa157bff6ce4e1fa58a79c372df6b0a00c5e5fd621f6396b3d55451e6" => :sierra
    sha256 "1feb9c3c574632f9324fdfc8bc5ed49f2817e7a58ae280e44b0ae8735e89caca" => :el_capitan
    sha256 "845c258af465001dcdfad1f09e7659e86d6d006b9381c6e3cfaf0461e432ab46" => :yosemite
  end

  def install
    system "./configure", "--prefix=#{prefix}", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--without-pthread"
    system "make", "install"
  end
end
