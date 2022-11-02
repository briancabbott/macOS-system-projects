class Showkey < Formula
  desc "Simple keystroke visualizer"
  homepage "http://www.catb.org/~esr/showkey/"
  url "http://www.catb.org/~esr/showkey/showkey-1.8.tar.gz"
  sha256 "31b6b064976a34d7d8e7a254db0397ba2dc50f1bb6e283038b17c48a358d50d3"
  license "MIT"
  head "https://gitlab.com/esr/showkey.git"

  livecheck do
    url :homepage
    regex(/showkey[._-]v?(\d+(?:\.\d+)+)/i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "712671c9ed8023a91e5f170d40db86fed18d70703981e1397f63d1bf40f0010f" => :big_sur
    sha256 "077e85f756df5b32424c7e552963e2d76d3c9165be10217c19f9c36592b7b469" => :arm64_big_sur
    sha256 "7565c213c9d1e4d0859164eaa92ec1e6f0cdf4d9e28f8e3a4f7a156971292de1" => :catalina
    sha256 "7bb7683a2f338db50eaed3bb2079308f32d30a43ce0ac37d16b7a9ae98235678" => :mojave
    sha256 "2eb4e4da78137fc93f5558e9448744e7dfdef298e6259f562093f956656c86a6" => :high_sierra
  end

  depends_on "xmlto" => :build

  def install
    ENV["XML_CATALOG_FILES"] = etc/"xml/catalog"
    system "make", "showkey", "showkey.1"
    bin.install "showkey"
    man1.install "showkey.1"
  end

  test do
    require "expect"

    output = Utils.safe_popen_write("script", "-q", "/dev/null", bin/"showkey") do |pipe|
      pipe.expect /interrupt .*? or quit .*? character\.\r?\n$/
      pipe.write "Hello Homebrew!"
      sleep 1
      pipe.write "\cC\cD"
    end
    assert_match /^Hello<SP>Homebrew!<CTL-D=EOT>/, output
  end
end
