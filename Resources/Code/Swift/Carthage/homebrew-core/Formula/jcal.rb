class Jcal < Formula
  desc "UNIX-cal-like tool to display Jalali calendar"
  homepage "https://savannah.nongnu.org/projects/jcal/"
  url "https://download.savannah.gnu.org/releases/jcal/jcal-0.4.1.tar.gz"
  sha256 "e8983ecad029b1007edc98458ad13cd9aa263d4d1cf44a97e0a69ff778900caa"
  license "GPL-3.0"

  livecheck do
    url "https://download.savannah.gnu.org/releases/jcal/"
    regex(/href=.*?jcal[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "00a9eec192b14b6b4a442e1268bd7727df19923901d36ca225a32e69477df5de" => :big_sur
    sha256 "6995c49236be96cf2adcf11cd03a88f46436ca66061de24d087c1c69aa4b9f6c" => :arm64_big_sur
    sha256 "0544ee162b480d5999a312cf721b40007901f964b20edbdd8e062b2e95c64157" => :catalina
    sha256 "4274c678ae3c2110c94b474aa56fcbb6b121645f9a91352b7c24bf028750f3d9" => :mojave
    sha256 "348fdd02ce58859bf75ebe00feaf5c90e1f4f052d531e7667343f4c220d8e7bb" => :high_sierra
    sha256 "d6f50844723751f0de8181f751ffc0912013b518b5ac60777a3ade7e1aaa3179" => :sierra
    sha256 "4d876e18cb50c7aa31211b60b66e42637ca3c9eeed9c688c1945dc4755977597" => :el_capitan
    sha256 "3640b058b034b519a5aa3bb1dde36b4efb2ec7bb8124bdbd106617202bf87b22" => :yosemite
    sha256 "f3c61ee0a88644c66be60de5d0d0c3ec0118aa4762797baab398363c948a0536" => :mavericks
  end

  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  def install
    system "/bin/sh", "autogen.sh"
    system "./configure", "--prefix=#{prefix}",
                          "--disable-debug",
                          "--disable-dependency-tracking"
    system "make"
    system "make", "install"
  end

  test do
    system "#{bin}/jcal", "-y"
    system "#{bin}/jdate"
  end
end
