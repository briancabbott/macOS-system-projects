class Smake < Formula
  desc "Portable make program with automake features"
  homepage "https://s-make.sourceforge.io/"
  url "https://downloads.sourceforge.net/project/s-make/smake-1.2.5.tar.bz2"
  sha256 "27566aa731a400c791cd95361cc755288b44ff659fa879933d4ea35d052259d4"

  livecheck do
    url :stable
  end

  bottle do
    sha256 "91320cb3802a9b395c25e93efc7162ebdf59514ec70fe82a7476b045120d7adc" => :big_sur
    sha256 "43034c1f8106c8f34f94e6ffbfc143521f1688929227b28a0c2c35d15a36e1a2" => :arm64_big_sur
    sha256 "c09f4bc9cdcaa26dddc33ec021083885ed7d9236b2af2c87713446ad1a0cb538" => :catalina
    sha256 "6dd776264c5583a982b9a8270956c84274387719aeae7b057d7c581ebc438c70" => :mojave
    sha256 "5b1860ab709b7a27201f781f31a34ccf6db6da600ef60741fd918a95c3beedb7" => :high_sierra
    sha256 "b1afe84c5a7b535738d2b2ee3f2abf879c908cf4f3b9c5a6f9f9cdd3fc403536" => :sierra
    sha256 "a5cb6ea4fab2d0ce67342f482fd0efb4dcc20483722e56ae120880d2a97ebab0" => :el_capitan
    sha256 "c1420a59ceba43481eac2b2046a7d3c4aac967a12ff52bccb3b4697eca8d5c8f" => :yosemite
    sha256 "4e8157c27f8ab0d5ad2c9673a86357f38acfabea1ac4eef80c54e8141dfdb336" => :mavericks
  end

  def install
    # The bootstrap smake does not like -j
    ENV.deparallelize
    # Xcode 9 miscompiles smake if optimization is enabled
    # https://sourceforge.net/p/schilytools/tickets/2/
    ENV.O1 if DevelopmentTools.clang_build_version >= 900

    system "make", "GMAKE_NOWARN=true", "INS_BASE=#{libexec}", "INS_RBASE=#{libexec}", "install"
    bin.install_symlink libexec/"bin/smake"
    man1.install_symlink Dir["#{libexec}/share/man/man1/*.1"]
    man5.install_symlink Dir["#{libexec}/share/man/man5/*.5"]
  end

  test do
    system "#{bin}/smake", "-version"
  end
end
