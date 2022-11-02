class Oggz < Formula
  desc "Command-line tool for manipulating Ogg files"
  homepage "https://www.xiph.org/oggz/"
  url "https://downloads.xiph.org/releases/liboggz/liboggz-1.1.1.tar.gz"
  sha256 "6bafadb1e0a9ae4ac83304f38621a5621b8e8e32927889e65a98706d213d415a"
  license "BSD-3-Clause"

  bottle do
    cellar :any
    sha256 "e9f424566678f728990a41c130ae2682069b608d642aecdab827440fc56ef363" => :big_sur
    sha256 "286192f997ec0e02994b70cdc03d06d0616b10bea980b1aee7f3322f1d58735c" => :arm64_big_sur
    sha256 "6a107479a443028d27afcfa51b68899449120637dcbe8e6987ce0e5191b1ee59" => :catalina
    sha256 "21ee59402b2854a91629c96c0e3540a1e97e9661984800d4d80d650069fcf0be" => :mojave
    sha256 "f444304f94866179ffcbe6322d6f25193b4fcd2dc49ad71f9c9527b0d85934de" => :high_sierra
    sha256 "a0fad22ba18930be45c7226f2db0fe8b39c988c84c392807ddc75e2d40b3a9ad" => :sierra
    sha256 "4c1819dbc134981faf5e2e03dc69d210deb8dabd59b71969c1f479fa32322635" => :el_capitan
    sha256 "c6076111f111c5d77dc608bcb4892f10dffb84e5b4f5ebdfba311ec332fa6623" => :yosemite
    sha256 "a3aa5e741dd3e7a9aebb65748f80f45947549a79915b68161a79f12cb37b4b12" => :mavericks
  end

  depends_on "pkg-config" => :build
  depends_on "libogg"

  def install
    system "./configure", "--prefix=#{prefix}",
                          "--disable-debug",
                          "--disable-dependency-tracking"
    system "make", "install"
  end

  test do
    system "#{bin}/oggz", "known-codecs"
  end
end
