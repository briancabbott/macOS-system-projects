class Lsdvd < Formula
  desc "Read the content info of a DVD"
  homepage "https://sourceforge.net/projects/lsdvd"
  url "https://downloads.sourceforge.net/project/lsdvd/lsdvd/lsdvd-0.17.tar.gz"
  sha256 "7d2c5bd964acd266b99a61d9054ea64e01204e8e3e1a107abe41b1274969e488"
  revision 3

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "78c4f868f8b534b59db6820fe0f303d42c0f908c49be6e5a5896da76441d0e0d" => :big_sur
    sha256 "0c85243ae07f9fc4f0dba6bea2ca2ae6865b6bb9612f570034dad469c460e7b3" => :arm64_big_sur
    sha256 "63e52d4264e765ca6095153de37b08dee02d59676ae98b2f0c36b29c0fdae48e" => :catalina
    sha256 "1489bcdd29d86d725cbf76a68ff77335729c676fcf1155555da799d7096ee933" => :mojave
    sha256 "d5a945ffb9e6172df4625d0ed5b3e5c6aecd2dfc6c9ab5b2a83d88267027edc8" => :high_sierra
  end

  depends_on "pkg-config" => :build
  depends_on "libdvdcss"
  depends_on "libdvdread"

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    system bin/"lsdvd", "--help"
  end
end
