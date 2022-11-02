class Libnxml < Formula
  desc "C library for parsing, writing, and creating XML files"
  homepage "https://www.autistici.org/bakunin/libnxml/"
  url "https://www.autistici.org/bakunin/libnxml/libnxml-0.18.3.tar.gz"
  sha256 "0f9460e3ba16b347001caf6843f0050f5482e36ebcb307f709259fd6575aa547"
  license "LGPL-2.1"

  bottle do
    cellar :any
    rebuild 1
    sha256 "646e960c9d78476dd4102b5ede1aac8bf0ea3dd06f51de6cec429f0851b4f1ec" => :big_sur
    sha256 "c9fb3bcc767561392f500093ca5549248153ba874b7d3df6ae17a9a94c9135b7" => :arm64_big_sur
    sha256 "af92d830dbb7a103cd5a512c03c1cf2777742ea72c998ecbf1fc80912679cb47" => :catalina
    sha256 "61e076a06cab737a7410a8a2adf9c29c3d32e44467caaef25d54c7be63093bd6" => :mojave
    sha256 "a6b51b3ed4d09a603b7d232040b7e53fb26013a16ea9b4b86f415c45200faf43" => :high_sierra
    sha256 "ddeb6f19f803f29eb44f498ed687dd76a5bdeb0b6416c67759e1690ab9fa4f14" => :sierra
    sha256 "de106efa2da60ccb8567403547f904485c1c6431dd492ce4e1bbd66599c7f961" => :el_capitan
    sha256 "7c2bff9c49c93ef6a3901050212671c60e0cb4e72f2faf968eb4ae57f3d6fbeb" => :yosemite
    sha256 "49cfdc9ab57c78deed6b2fc3ce1c13b48a943384b2d366f9c37cfb673528b637" => :mavericks
  end

  def install
    system "./configure", "--disable-debug",
                          "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
