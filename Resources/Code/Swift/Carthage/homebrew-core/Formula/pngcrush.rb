class Pngcrush < Formula
  desc "Optimizer for PNG files"
  homepage "https://pmt.sourceforge.io/pngcrush"
  url "https://downloads.sourceforge.net/project/pmt/pngcrush/1.8.13/pngcrush-1.8.13.tar.xz"
  sha256 "8fc18bcbcc65146769241e20f9e21e443b0f4538d581250dce89b1e969a30705"

  livecheck do
    url :stable
    regex(%r{url=.*?/pngcrush[._-]v?(\d+(?:\.\d+)+)\.t}i)
  end

  bottle do
    cellar :any_skip_relocation
    sha256 "4f7a3810130d10dc7b448aeb8c53cf8b52da9312863ff12edeb3c1268eaf6ea6" => :big_sur
    sha256 "ba3aa0d156954d41cb43b96bd5529c3a68e56a67a751b3a9cc153e3ed47e2425" => :arm64_big_sur
    sha256 "f6b31e35011fd69dc4ee678e4529fd5a76ee7be8faba88bb7c9cb0b7cbfafacb" => :catalina
    sha256 "904e958b1198e2931ab233981764b1ec66b26da793445c0fa10182588b5369a7" => :mojave
    sha256 "db13f642eae1815e00e1a80d363228e0311d85ca510e9c9de94dba8483fa2d87" => :high_sierra
    sha256 "f648ad0c664699f67bba8ba791358e8b294d0c1d975f026aa67fc1635badbc73" => :sierra
    sha256 "2633aff1e7cec8bb6c55da5c4daf9f555c74e516ebcc5f3027589588f76d3e17" => :el_capitan
    sha256 "5505ea179a71996eb4fab04feebd09ebbef7e8ea4c1efba1e0184333c1883d1b" => :yosemite
  end

  def install
    system "make", "CC=#{ENV.cc}",
                   "LD=#{ENV.cc}",
                   "CFLAGS=#{ENV.cflags}",
                   "LDFLAGS=#{ENV.ldflags}"
    bin.install "pngcrush"
  end

  test do
    system "#{bin}/pngcrush", test_fixtures("test.png"), "/dev/null"
  end
end
