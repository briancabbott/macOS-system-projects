class Libart < Formula
  desc "Library for high-performance 2D graphics"
  homepage "https://people.gnome.org/~mathieu/libart/libart.html"
  url "https://download.gnome.org/sources/libart_lgpl/2.3/libart_lgpl-2.3.21.tar.bz2"
  sha256 "fdc11e74c10fc9ffe4188537e2b370c0abacca7d89021d4d303afdf7fd7476fa"

  # We use a common regex because libart doesn't use GNOME's "even-numbered
  # minor is stable" version scheme.
  livecheck do
    url :stable
    regex(/libart_lgpl[._-]v?(\d+(?:\.\d+)+)\.t/i)
  end

  bottle do
    cellar :any
    sha256 "1204889805658d4aeb3ec37251e8ab064e995654008be97588bc034505b77314" => :big_sur
    sha256 "8daf6e0691d2fc7f919716cb760a80bbba53295aa5c92d8b05aef4aa1172b09b" => :arm64_big_sur
    sha256 "54ca46ebc37bba1fdc39e8b28c166202e7d488d93cc5b4acfb042a14adec84f9" => :catalina
    sha256 "5fc8b240a975efcb5bd3992afd4d01c0a393a306a4a66192cb9a10e580bcf4d3" => :mojave
    sha256 "c5ae59f4955fd1b4e3c49976b06609d56c5079d2b0f6e0675b356b1eb09181cd" => :high_sierra
    sha256 "e9e14623ba0284a89dd09c7be72393619582c5d0489891cd1f654b6c26b0fabc" => :sierra
    sha256 "18fb7a842650151fef102efadefa52aa12dc3f597ace95b8e25efe6518a65d2e" => :el_capitan
    sha256 "006a9bf5e40ea99cdb4a10b7a2a2ac6a249f511254be1954a937dac0e50a6f0d" => :yosemite
    sha256 "276eafd432499ab988a21d9e87104d744e82cf1adf4cc55d47639b1f72ab410a" => :mavericks
  end

  def install
    system "./configure", "--disable-debug", "--disable-dependency-tracking",
                          "--prefix=#{prefix}"
    system "make", "install"
  end
end
