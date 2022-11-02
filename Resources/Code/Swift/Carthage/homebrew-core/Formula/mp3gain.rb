class Mp3gain < Formula
  desc "Lossless mp3 normalizer with statistical analysis"
  homepage "https://mp3gain.sourceforge.io"
  url "https://downloads.sourceforge.net/project/mp3gain/mp3gain/1.6.2/mp3gain-1_6_2-src.zip"
  version "1.6.2"
  sha256 "5cc04732ef32850d5878b28fbd8b85798d979a025990654aceeaa379bcc9596d"

  livecheck do
    url :stable
  end

  bottle do
    cellar :any
    sha256 "d31ec490fe52fd92457325ec9d1161104283d1c16cee1c73c2d083a847d187e1" => :big_sur
    sha256 "d4e92ab9bfc8143f4442f6d7c3f78a3ef92677d44198402ef5d05a604481b414" => :arm64_big_sur
    sha256 "27dbf67d73a4f63cd06cc568b8a40d09e3fec5e858c447da1750b2093046d795" => :catalina
    sha256 "6db408b86b074e8713476fa60ea252ad3f4213dbf63cdca3342ffe989bd372d5" => :mojave
    sha256 "5aa37ac4ab2013f5365da14969494111500337cae3c6d7614b72dfb9e94352f2" => :high_sierra
    sha256 "66684a469ee1de432a00f1264c89b3921d3558854fa736b24a3942e351617c47" => :sierra
    sha256 "4c97894216600ba8ac03094a45fe68a7d107f69adbcd638d40c967ad10e95480" => :el_capitan
  end

  depends_on "mpg123"

  def install
    system "make"
    bin.install "mp3gain"
  end

  test do
    system "#{bin}/mp3gain", "-v"
  end
end
