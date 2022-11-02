class LcdfTypetools < Formula
  desc "Manipulate OpenType and multiple-master fonts"
  homepage "https://www.lcdf.org/type/"
  url "https://www.lcdf.org/type/lcdf-typetools-2.108.tar.gz"
  sha256 "fb09bf45d98fa9ab104687e58d6e8a6727c53937e451603662338a490cbbcb26"
  license "GPL-2.0"
  head "https://github.com/kohler/lcdf-typetools.git"

  livecheck do
    url :homepage
    regex(/href=.*?lcdf-typetools[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    sha256 "45ff77c662edb2a238d2c07be58cf242685ed85c926847a381e9a2acf7035b3a" => :big_sur
    sha256 "bea41132d0d057aef10d64fe53b52bb198c3ce49b8548c72d5430f1411f68afe" => :arm64_big_sur
    sha256 "e7ce2d4d16d2b79e482cb862231519653e6d3c09cd5e310573b04f804323e1e3" => :catalina
    sha256 "0fd983396dbcf027e560753e6f25797500d085762edcf59a1a2034cd55c24cfd" => :mojave
    sha256 "cdff1c16d03fd920033f85dd2e2180f91791057729fbd26b6f193ac7cd0ce9f4" => :high_sierra
    sha256 "2bfe28f9e869eec676cada56bcf6efe97024e0e1f93b126a7b26ac2a292db2af" => :sierra
  end

  def install
    system "./configure", "--disable-dependency-tracking",
                          "--prefix=#{prefix}",
                          "--without-kpathsea"
    system "make", "install"
  end

  test do
    font_name = (MacOS.version >= :catalina) ? "Arial\\ Unicode.ttf" : "Arial.ttf"
    assert_include shell_output("#{bin}/otfinfo -p /Library/Fonts/#{font_name}"), "Arial"
  end
end
