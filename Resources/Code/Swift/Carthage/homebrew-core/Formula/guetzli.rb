class Guetzli < Formula
  desc "Perceptual JPEG encoder"
  homepage "https://github.com/google/guetzli"
  url "https://github.com/google/guetzli/archive/v1.0.1.tar.gz"
  sha256 "e52eb417a5c0fb5a3b08a858c8d10fa797627ada5373e203c196162d6a313697"
  license "Apache-2.0"
  head "https://github.com/google/guetzli.git"

  bottle do
    cellar :any
    sha256 "9f1fb787b3b21f795dd2a5d0399bf4f1263ca0e5c28f04e4d101ac33ea22503b" => :big_sur
    sha256 "4337b23e3a80393c75f23df48034d14e408b72647ec918caf5ae524ee6716a99" => :arm64_big_sur
    sha256 "14605bd3ba2aa89d0030d3935eb5ffa022712fc6eef43f969bc301beda218af6" => :catalina
    sha256 "1599d5e292f5ca4ade99ab2627d3a0d2a3450011317dff9d5a46d779af20b01a" => :mojave
    sha256 "1b3a1b5544b7a8c30553b2e8ac669d8e924d0164feb5355b0a7c2ef5807aca46" => :high_sierra
    sha256 "c059346fa601885f550b50752d6d1a23eced66388b18e1c1db5169a0951dcad6" => :sierra
    sha256 "a77327b3964a88a84879943171e0d10d6661cc72c5ceaa12ee2091f02930da1a" => :el_capitan
    sha256 "04864f5c52c77f2d382247a57bf082052599a2bc9bd8fa28592ab17657342b08" => :yosemite
  end

  depends_on "pkg-config" => :build
  depends_on "libpng"

  resource "test_image" do
    url "https://github.com/google/guetzli/releases/download/v1.0/bees.png"
    sha256 "2c1784bf4efb90c57f00a3ab4898ac8ec4784c60d7a0f70d2ba2c00af910520b"
  end

  def install
    system "make"
    bin.install "bin/Release/guetzli"
  end

  test do
    resource("test_image").stage { system "#{bin}/guetzli", "bees.png", "bees.jpg" }
  end
end
