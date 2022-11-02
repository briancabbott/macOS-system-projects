class Pwgen < Formula
  desc "Password generator"
  homepage "https://pwgen.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/pwgen/pwgen/2.08/pwgen-2.08.tar.gz"
  sha256 "dab03dd30ad5a58e578c5581241a6e87e184a18eb2c3b2e0fffa8a9cf105c97b"
  license "GPL-2.0"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "0a47de6eec09b1a2e938da0bebca8386261bb63040f9ca77fadfc3d28db7efc8" => :big_sur
    sha256 "cc4d1e845384c5170c9fd6e9c5b054e152b8690763a55b3c9a1a0e51fbee31c4" => :arm64_big_sur
    sha256 "725911d1fd71b259acb7b907c09ef86a03545afe95e161856130992fc0789ffc" => :catalina
    sha256 "2f35a2d575e16a2ab0497cabfc927a7b40aba68edba574889bf9bbdf03572c12" => :mojave
    sha256 "185f2f56eb03da60277520734452204ec2e0059cbc1f0af5d0fec1e7fa837658" => :high_sierra
    sha256 "01a0709f74923e7b86d680f03d3ec056d3175cb7e54be176a26d5bfae890fd21" => :sierra
    sha256 "7dade70b172cb91635afffe8bb1eadb251f9dbd3368ab9e4a37f98a7c0a14b01" => :el_capitan
    sha256 "1799bdbb42974d10e2ff3a4e382351b1f03f0a3be31c15ff718d8935d1226101" => :yosemite
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--mandir=#{man}"
    system "make", "install"
  end

  test do
    system "#{bin}/pwgen", "--secure", "20", "10"
  end
end
